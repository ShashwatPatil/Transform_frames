#include "FirestoreManager.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <chrono>
#include <firebase/auth.h>

namespace uwb_bridge {

FirestoreManager::FirestoreManager()
    : app_(nullptr),
      auth_(nullptr),
      db_(nullptr),
      initialized_(false),
      project_id_("") {
}

FirestoreManager::~FirestoreManager() {
    stopTransformListener();
    
    if (db_) {
        delete db_;
        db_ = nullptr;
    }
    
    if (auth_) {
        delete auth_;
        auth_ = nullptr;
    }
    
    if (app_) {
        delete app_;
        app_ = nullptr;
    }
}

bool FirestoreManager::initialize() {
    try {
        spdlog::info("Initializing Firebase App with environment credentials...");
        
        // Check for GOOGLE_APPLICATION_CREDENTIALS (preferred for service accounts)
        const char* creds_path = std::getenv("GOOGLE_APPLICATION_CREDENTIALS");
        if (creds_path) {
            spdlog::info("Using service account from: {}", creds_path);
            return initializeWithServiceAccount(creds_path);
        }
        
        // Fallback to API key authentication
        spdlog::info("Using API key authentication (GOOGLE_APPLICATION_CREDENTIALS not set)");
        
        // Get Firebase project ID from environment variable or default
        const char* project_id_env = std::getenv("FIREBASE_PROJECT_ID");
        if (project_id_env) {
            project_id_ = project_id_env;
        } else {
            spdlog::error("FIREBASE_PROJECT_ID not set!");
            return false;
        }
        
        // Create Firebase options
        firebase::AppOptions options;
        options.set_project_id(project_id_.c_str());
        
        // Check for API key in environment
        const char* api_key = std::getenv("FIREBASE_API_KEY");
        if (api_key) {
            options.set_api_key(api_key);
        } else {
            spdlog::warn("FIREBASE_API_KEY not set - authentication may fail");
        }
        
        // Initialize Firebase App
        app_ = firebase::App::Create(options);
        if (!app_) {
            spdlog::error("Failed to create Firebase App");
            return false;
        }
        
        spdlog::info("Firebase App created successfully");
        
        // Initialize Firestore
        spdlog::info("Initializing Firestore...");
        db_ = firebase::firestore::Firestore::GetInstance(app_);
        if (!db_) {
            spdlog::error("Failed to get Firestore instance");
            return false;
        }
        
        spdlog::info("Firestore initialized successfully");
        initialized_ = true;
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Exception during Firebase initialization: {}", e.what());
        return false;
    }
}

bool FirestoreManager::initializeWithServiceAccount(const std::string& credentials_path) {
    try {
        spdlog::info("Initializing Firebase with service account: {}", credentials_path);
        
        // Set the environment variable for the Firebase SDK
        setenv("GOOGLE_APPLICATION_CREDENTIALS", credentials_path.c_str(), 1);
        
        // Read project ID from credentials file
        std::ifstream creds_file(credentials_path);
        if (!creds_file.is_open()) {
            spdlog::error("Failed to open credentials file: {}", credentials_path);
            return false;
        }
        
        // Parse JSON to get project_id
        nlohmann::json creds_json;
        try {
            creds_file >> creds_json;
            project_id_ = creds_json["project_id"].get<std::string>();
            spdlog::info("Project ID from credentials: {}", project_id_);
        } catch (const std::exception& e) {
            spdlog::error("Failed to parse credentials JSON: {}", e.what());
            return false;
        }
        
        // Create Firebase options
        firebase::AppOptions options;
        options.set_project_id(project_id_.c_str());
        
        // Set API key and App ID (required for Firebase Auth)
        // Try to read from google-services-desktop.json
        std::ifstream google_services("google-services-desktop.json");
        if (google_services.is_open()) {
            try {
                nlohmann::json services_json;
                google_services >> services_json;
                std::string api_key = services_json["client"][0]["api_key"][0]["current_key"].get<std::string>();
                std::string app_id = services_json["client"][0]["client_info"]["mobilesdk_app_id"].get<std::string>();
                options.set_api_key(api_key.c_str());
                options.set_app_id(app_id.c_str());
                spdlog::info("Loaded API key and App ID from google-services-desktop.json");
            } catch (const std::exception& e) {
                spdlog::warn("Could not parse google-services-desktop.json: {}", e.what());
            }
        } else {
            spdlog::warn("google-services-desktop.json not found - auth may fail");
        }
        
        // Initialize Firebase App (will use GOOGLE_APPLICATION_CREDENTIALS)
        app_ = firebase::App::Create(options);
        if (!app_) {
            spdlog::error("Failed to create Firebase App");
            return false;
        }
        
        spdlog::info("Firebase App created successfully with service account");
        
        // Initialize Firebase Auth (REQUIRED for authentication)
        spdlog::info("Initializing Firebase Auth...");
        auth_ = firebase::auth::Auth::GetAuth(app_);
        if (!auth_) {
            spdlog::error("Failed to get Auth instance");
            return false;
        }
        
        // Sign in as Robot User (proper way for C++ Client SDK)
        const char* robot_email = std::getenv("FIREBASE_ROBOT_EMAIL");
        const char* robot_password = std::getenv("FIREBASE_ROBOT_PASSWORD");
        
        if (!robot_email || !robot_password) {
            spdlog::error("FIREBASE_ROBOT_EMAIL and FIREBASE_ROBOT_PASSWORD must be set!");
            spdlog::error("Example: export FIREBASE_ROBOT_EMAIL=uwb-bridge-robot@nova-40708.firebaseapp.com");
            return false;
        }
        
        spdlog::info("Signing in as robot user: {}", robot_email);
        firebase::Future<firebase::auth::AuthResult> login_future = 
            auth_->SignInWithEmailAndPassword(robot_email, robot_password);
        
        // Wait for sign-in to complete (blocking)
        while (login_future.status() == firebase::kFutureStatusPending) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (login_future.error() != firebase::auth::kAuthErrorNone) {
            spdlog::error("Authentication failed: {}", login_future.error_message());
            spdlog::error("Check that the robot user exists in Firebase Console and credentials are correct");
            return false;
        }
        
        spdlog::info("Successfully authenticated!");
        spdlog::info("  User UID: {}", auth_->current_user().uid());
        spdlog::info("  Email: {}", auth_->current_user().email());
        
        // Initialize Firestore
        spdlog::info("Initializing Firestore...");
        db_ = firebase::firestore::Firestore::GetInstance(app_);
        if (!db_) {
            spdlog::error("Failed to get Firestore instance");
            return false;
        }
        
        spdlog::info("Firestore initialized successfully");
        initialized_ = true;
        return true;
        
    } catch (const std::exception& e) {
        spdlog::error("Exception during Firebase initialization: {}", e.what());
        return false;
    }
}

std::future<AppConfig> FirestoreManager::fetchAppConfig() {
    if (!initialized_) {
        throw std::runtime_error("FirestoreManager not initialized. Call initialize() first.");
    }
    
    auto promise = std::make_shared<std::promise<AppConfig>>();
    auto future = promise->get_future();
    
    spdlog::info("Fetching AppConfig from Firestore...");
    
    // Get reference to the pozyx document (same path as transform listener)
    // Path: setups/&GSP&Office&29607/environment/pozyx
    auto doc_ref = db_->Collection("setups")
                       .Document("&GSP&Office&29607")
                       .Collection("environment")
                       .Document("pozyx");
    
    // Perform async Get operation
    doc_ref.Get().OnCompletion(
        [this, promise](const firebase::Future<firebase::firestore::DocumentSnapshot>& result) {
            if (result.error() != firebase::firestore::Error::kErrorOk) {
                spdlog::error("Failed to fetch AppConfig: {} (code: {})", 
                             result.error_message(), result.error());
                promise->set_exception(
                    std::make_exception_ptr(
                        std::runtime_error("Failed to fetch AppConfig from Firestore")
                    )
                );
                return;
            }
            
            const firebase::firestore::DocumentSnapshot* snapshot = result.result();
            if (!snapshot->exists()) {
                spdlog::error("AppConfig document does not exist in Firestore");
                promise->set_exception(
                    std::make_exception_ptr(
                        std::runtime_error("AppConfig document not found in Firestore")
                    )
                );
                return;
            }
            
            try {
                AppConfig config = parseAppConfig(*snapshot);
                spdlog::info("Successfully fetched AppConfig from Firestore");
                promise->set_value(config);
            } catch (const std::exception& e) {
                spdlog::error("Failed to parse AppConfig: {}", e.what());
                promise->set_exception(std::current_exception());
            }
        }
    );
    
    return future;
}

bool FirestoreManager::startTransformListener(std::shared_ptr<uwb_transform::FloorplanTransformer> transformer) {
    if (!initialized_) {
        spdlog::error("FirestoreManager not initialized. Call initialize() first.");
        return false;
    }
    
    if (!transformer) {
        spdlog::error("Invalid transformer pointer");
        return false;
    }
    
    spdlog::info("Starting real-time listener for TransformConfig...");
    
    // Get reference to the pozyx document in your Firestore structure
    // Path: setups/&GSP&Office&29607/environment/pozyx
    auto doc_ref = db_->Collection("setups")
                       .Document("&GSP&Office&29607")
                       .Collection("environment")
                       .Document("pozyx");
    
    // Add OnSnapshot listener for real-time updates
    transform_listener_ = doc_ref.AddSnapshotListener(
        [this, transformer](const firebase::firestore::DocumentSnapshot& snapshot,
                           firebase::firestore::Error error,
                           const std::string& error_message) {
            
            if (error != firebase::firestore::Error::kErrorOk) {
                spdlog::error("Firestore listener error: {} (code: {})", error_message, error);
                return;
            }
            
            if (!snapshot.exists()) {
                spdlog::warn("TransformConfig document does not exist");
                return;
            }
            
            try {
                // Parse the new transform config
                uwb_bridge::TransformConfig new_config = parseTransformConfig(snapshot);
                
                // Convert to uwb_transform::TransformConfig
                uwb_transform::TransformConfig transform_config;
                transform_config.origin_x = new_config.origin_x;
                transform_config.origin_y = new_config.origin_y;
                transform_config.scale = new_config.scale;
                transform_config.rotation_rad = new_config.rotation_rad;
                transform_config.x_flipped = new_config.x_flipped;
                transform_config.y_flipped = new_config.y_flipped;
                
                // Thread-safe update to the transformer
                transformer->updateConfig(transform_config);
                
                spdlog::info("Updated transform matrix from Firestore");
                spdlog::debug("New config - Origin: ({}, {}), Scale: {}, Rotation: {} rad",
                             transform_config.origin_x, transform_config.origin_y,
                             transform_config.scale, transform_config.rotation_rad);
                
            } catch (const std::exception& e) {
                spdlog::error("Failed to parse TransformConfig update: {}", e.what());
            }
        }
    );
    
    spdlog::info("TransformConfig listener started successfully");
    return true;
}

void FirestoreManager::stopTransformListener() {
    spdlog::info("Stopping TransformConfig listener...");
    transform_listener_.Remove();
}

AppConfig FirestoreManager::parseAppConfig(const firebase::firestore::DocumentSnapshot& snapshot) {
    AppConfig config;
    
    // Parse MQTT configuration from nested maps
    try {
        // Enable dual broker mode (separate source and dest brokers)
        config.mqtt.dual_mode = true;
        
        // Parse source_broker map
        auto source_map = snapshot.Get("source_broker").map_value();
        config.mqtt.source_broker.broker_address = source_map["broker_address"].string_value();
        config.mqtt.source_broker.port = static_cast<int>(source_map["port"].integer_value());
        config.mqtt.source_broker.client_id = source_map["client_id"].string_value();
        config.mqtt.source_broker.username = source_map["username"].string_value();
        config.mqtt.source_broker.password = source_map["password"].string_value();
        config.mqtt.source_broker.source_topic = source_map["source_topic"].string_value();
        config.mqtt.source_broker.qos = static_cast<int>(source_map["qos"].integer_value());
        config.mqtt.source_broker.keepalive_interval = static_cast<int>(source_map["keepalive_interval"].integer_value());
        config.mqtt.source_broker.clean_session = source_map["clean_session"].boolean_value();
        config.mqtt.source_broker.use_ssl = source_map["use_ssl"].boolean_value();
        config.mqtt.source_broker.use_websockets = source_map["use_websockets"].boolean_value();
        config.mqtt.source_broker.ws_path = source_map["ws_path"].string_value();
        
        // Parse dest_broker map
        auto dest_map = snapshot.Get("dest_broker").map_value();
        config.mqtt.dest_broker.broker_address = dest_map["broker_address"].string_value();
        config.mqtt.dest_broker.port = static_cast<int>(dest_map["port"].integer_value());
        config.mqtt.dest_broker.client_id = dest_map["client_id"].string_value();
        config.mqtt.dest_broker.username = dest_map["username"].string_value();
        config.mqtt.dest_broker.password = dest_map["password"].string_value();
        config.mqtt.dest_broker.dest_topic_prefix = dest_map["dest_topic_prefix"].string_value();
        config.mqtt.dest_broker.qos = static_cast<int>(dest_map["qos"].integer_value());
        config.mqtt.dest_broker.keepalive_interval = static_cast<int>(dest_map["keepalive_interval"].integer_value());
        config.mqtt.dest_broker.clean_session = dest_map["clean_session"].boolean_value();
        config.mqtt.dest_broker.use_ssl = dest_map["use_ssl"].boolean_value();
        config.mqtt.dest_broker.use_websockets = dest_map["use_websockets"].boolean_value();
        config.mqtt.dest_broker.ws_path = dest_map["ws_path"].string_value();
        
        // Parse transform configuration (same document)
        uwb_bridge::TransformConfig transform = parseTransformConfig(snapshot);
        config.transform = transform;
        
        // Set default logging configuration (not in Firestore)
        config.log_level = "info";
        config.log_file = "/var/log/uwb_bridge/uwb_bridge.log";
        config.log_rotation_size_mb = 10;
        config.log_rotation_count = 3;
        
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to parse AppConfig fields: ") + e.what());
    }
    
    return config;
}

uwb_bridge::TransformConfig FirestoreManager::parseTransformConfig(
    const firebase::firestore::DocumentSnapshot& snapshot) {
    
    uwb_bridge::TransformConfig config;
    
    try {
        // Get the nested "transform" map
        auto transform_map = snapshot.Get("transform").map_value();
        
        config.origin_x = transform_map["origin_x"].double_value();
        config.origin_y = transform_map["origin_y"].double_value();
        config.scale = transform_map["scale"].double_value();
        config.rotation_rad = transform_map["rotation"].double_value();
        
        // Convert flip values: 1 means flipped (true), -1 means not flipped (false)
        int x_flip_val = static_cast<int>(transform_map["x_flip"].integer_value());
        int y_flip_val = static_cast<int>(transform_map["y_flip"].integer_value());
        
        config.x_flipped = (x_flip_val == 1);
        config.y_flipped = (y_flip_val == -1);  // -1 means flipped for y
        
        // Set defaults for fields not in your DB
        config.frame_id = "floorplan_pixel_frame";
        config.output_units = "meters";
        
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to parse TransformConfig fields: ") + e.what());
    }
    
    return config;
}

} // namespace uwb_bridge

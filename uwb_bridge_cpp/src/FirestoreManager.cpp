#include "FirestoreManager.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <cstdlib>
#include <fstream>

namespace uwb_bridge {

FirestoreManager::FirestoreManager()
    : app_(nullptr),
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
        
        // Initialize Firebase App (will use GOOGLE_APPLICATION_CREDENTIALS)
        app_ = firebase::App::Create(options);
        if (!app_) {
            spdlog::error("Failed to create Firebase App");
            return false;
        }
        
        spdlog::info("Firebase App created successfully with service account");
        
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
    
    // Get reference to the app_config document
    auto doc_ref = db_->Collection("configs").Document("app_config");
    
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
    
    // Get reference to the transform_config document
    auto doc_ref = db_->Collection("configs").Document("transform_config");
    
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
    
    // Parse MQTT configuration
    try {
        // Check if dual broker mode
        config.mqtt.dual_mode = snapshot.Get("mqtt_dual_mode").boolean_value();
        
        if (config.mqtt.dual_mode) {
            // Parse source broker
            config.mqtt.source_broker.broker_address = 
                snapshot.Get("mqtt_source_broker").string_value();
            config.mqtt.source_broker.client_id = 
                snapshot.Get("mqtt_source_client_id").string_value();
            config.mqtt.source_broker.username = 
                snapshot.Get("mqtt_source_username").string_value();
            config.mqtt.source_broker.password = 
                snapshot.Get("mqtt_source_password").string_value();
            config.mqtt.source_broker.source_topic = 
                snapshot.Get("mqtt_source_topic").string_value();
            config.mqtt.source_broker.qos = 
                static_cast<int>(snapshot.Get("mqtt_source_qos").integer_value());
            config.mqtt.source_broker.keepalive_interval = 
                static_cast<int>(snapshot.Get("mqtt_source_keepalive").integer_value());
            config.mqtt.source_broker.clean_session = 
                snapshot.Get("mqtt_source_clean_session").boolean_value();
            config.mqtt.source_broker.use_ssl = 
                snapshot.Get("mqtt_source_use_ssl").boolean_value();
            
            // Parse destination broker
            config.mqtt.dest_broker.broker_address = 
                snapshot.Get("mqtt_dest_broker").string_value();
            config.mqtt.dest_broker.client_id = 
                snapshot.Get("mqtt_dest_client_id").string_value();
            config.mqtt.dest_broker.username = 
                snapshot.Get("mqtt_dest_username").string_value();
            config.mqtt.dest_broker.password = 
                snapshot.Get("mqtt_dest_password").string_value();
            config.mqtt.dest_broker.dest_topic_prefix = 
                snapshot.Get("mqtt_dest_topic_prefix").string_value();
            config.mqtt.dest_broker.qos = 
                static_cast<int>(snapshot.Get("mqtt_dest_qos").integer_value());
            config.mqtt.dest_broker.keepalive_interval = 
                static_cast<int>(snapshot.Get("mqtt_dest_keepalive").integer_value());
            config.mqtt.dest_broker.clean_session = 
                snapshot.Get("mqtt_dest_clean_session").boolean_value();
            config.mqtt.dest_broker.use_ssl = 
                snapshot.Get("mqtt_dest_use_ssl").boolean_value();
        } else {
            // Single broker mode
            config.mqtt.source_broker.broker_address = 
                snapshot.Get("mqtt_broker").string_value();
            config.mqtt.source_broker.client_id = 
                snapshot.Get("mqtt_client_id").string_value();
            config.mqtt.source_broker.username = 
                snapshot.Get("mqtt_username").string_value();
            config.mqtt.source_broker.password = 
                snapshot.Get("mqtt_password").string_value();
            config.mqtt.source_broker.source_topic = 
                snapshot.Get("mqtt_source_topic").string_value();
            config.mqtt.source_broker.dest_topic_prefix = 
                snapshot.Get("mqtt_dest_topic_prefix").string_value();
            config.mqtt.source_broker.qos = 
                static_cast<int>(snapshot.Get("mqtt_qos").integer_value());
            config.mqtt.source_broker.keepalive_interval = 
                static_cast<int>(snapshot.Get("mqtt_keepalive").integer_value());
            config.mqtt.source_broker.clean_session = 
                snapshot.Get("mqtt_clean_session").boolean_value();
            config.mqtt.source_broker.use_ssl = 
                snapshot.Get("mqtt_use_ssl").boolean_value();
            
            // Copy to dest_broker for compatibility
            config.mqtt.dest_broker = config.mqtt.source_broker;
        }
        
        // Parse transform configuration (will be updated via listener, but set initial values)
        uwb_bridge::TransformConfig transform = parseTransformConfig(snapshot);
        config.transform = transform;
        
        // Parse logging configuration
        config.log_level = snapshot.Get("log_level").string_value();
        config.log_file = snapshot.Get("log_file").string_value();
        config.log_rotation_size_mb = 
            static_cast<int>(snapshot.Get("log_rotation_size_mb").integer_value());
        config.log_rotation_count = 
            static_cast<int>(snapshot.Get("log_rotation_count").integer_value());
        
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to parse AppConfig fields: ") + e.what());
    }
    
    return config;
}

uwb_bridge::TransformConfig FirestoreManager::parseTransformConfig(
    const firebase::firestore::DocumentSnapshot& snapshot) {
    
    uwb_bridge::TransformConfig config;
    
    try {
        config.origin_x = snapshot.Get("origin_x").double_value();
        config.origin_y = snapshot.Get("origin_y").double_value();
        config.scale = snapshot.Get("scale").double_value();
        config.rotation_rad = snapshot.Get("rotation_rad").double_value();
        config.x_flipped = snapshot.Get("x_flipped").boolean_value();
        config.y_flipped = snapshot.Get("y_flipped").boolean_value();
        config.frame_id = snapshot.Get("frame_id").string_value();
        config.output_units = snapshot.Get("output_units").string_value();
        
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to parse TransformConfig fields: ") + e.what());
    }
    
    return config;
}

} // namespace uwb_bridge

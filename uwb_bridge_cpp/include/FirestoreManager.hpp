#ifndef FIRESTORE_MANAGER_HPP
#define FIRESTORE_MANAGER_HPP

#include "ConfigLoader.hpp"
#include "FloorplanTransformer.hpp"
#include <firebase/app.h>
#include <firebase/auth.h>
#include <firebase/firestore.h>
#include <memory>
#include <future>
#include <string>

namespace uwb_bridge {

/**
 * @brief Manager class for Firebase Firestore integration
 * 
 * Handles Firebase App initialization, one-time AppConfig fetching,
 * and real-time listening for TransformConfig updates.
 * 
 * Architecture:
 * - AppConfig (MQTT settings, etc.): Fetched once on startup using Get()
 * - TransformConfig: Real-time listener using OnSnapshot() that updates transformer
 */
class FirestoreManager {
public:
    /**
     * @brief Constructor
     */
    FirestoreManager();

    /**
     * @brief Destructor - cleans up Firebase resources
     */
    ~FirestoreManager();

    /**
     * @brief Initialize Firebase App and Firestore with environment variables
     * 
     * Loads Firebase credentials from environment variables:
     * - FIREBASE_PROJECT_ID: Your Firebase project ID
     * - FIREBASE_API_KEY: Your Firebase API key (optional)
     * - GOOGLE_APPLICATION_CREDENTIALS: Path to service account JSON (preferred)
     * 
     * @return true if initialization successful, false otherwise
     */
    bool initialize();

    /**
     * @brief Initialize Firebase App with service account file
     * 
     * Loads Firebase credentials from a service account JSON file.
     * This is the recommended approach for production deployments.
     * 
     * @param credentials_path Path to service account JSON file
     * @return true if initialization successful, false otherwise
     */
    bool initializeWithServiceAccount(const std::string& credentials_path);

    /**
     * @brief Fetch AppConfig from Firestore (one-time read)
     * 
     * Retrieves the application configuration (MQTT broker settings, logging config)
     * from the "configs/app_config" document. This is a blocking call that waits
     * for the result.
     * 
     * @return std::future<AppConfig> Future that resolves to the application configuration
     * @throws std::runtime_error if fetch fails or document doesn't exist
     */
    std::future<AppConfig> fetchAppConfig();

    /**
     * @brief Start real-time listener for TransformConfig updates
     * 
     * Sets up an OnSnapshot listener on "configs/transform_config" document.
     * Whenever the document changes in Firestore, automatically calls
     * transformer->updateConfig() with the new values.
     * 
     * @param transformer Shared pointer to the FloorplanTransformer to update
     * @return true if listener successfully attached, false otherwise
     */
    bool startTransformListener(std::shared_ptr<uwb_transform::FloorplanTransformer> transformer);

    /**
     * @brief Stop the transform config listener
     */
    void stopTransformListener();

    /**
     * @brief Check if Firebase is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return initialized_; }

private:
    /**
     * @brief Parse Firestore document to AppConfig struct
     * @param snapshot Document snapshot from Firestore
     * @return AppConfig structures
     */
    AppConfig parseAppConfig(const firebase::firestore::DocumentSnapshot& snapshot);

    /**
     * @brief Parse Firestore document to TransformConfig struct
     * @param snapshot Document snapshot from Firestore
     * @return TransformConfig structure
     */
    uwb_bridge::TransformConfig parseTransformConfig(const firebase::firestore::DocumentSnapshot& snapshot);

    firebase::App* app_;                                        ///< Firebase App instance
    firebase::auth::Auth* auth_;                                ///< Firebase Auth instance (required for service account auth)
    firebase::firestore::Firestore* db_;                        ///< Firestore database instance
    firebase::firestore::ListenerRegistration transform_listener_; ///< Transform config listener
    bool initialized_;                                          ///< Initialization state
    std::string project_id_;                                    ///< Firebase project ID
};

} // namespace uwb_bridge

#endif // FIRESTORE_MANAGER_HPP

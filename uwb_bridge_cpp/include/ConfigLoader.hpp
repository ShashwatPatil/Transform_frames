#ifndef CONFIG_LOADER_HPP
#define CONFIG_LOADER_HPP

#include <string>
#include <nlohmann/json.hpp>

namespace uwb_bridge {

/**
 * @brief Configuration structure for MQTT connection
 */
struct MqttConfig {
    std::string broker_address;  ///< MQTT broker address (e.g., "tcp://localhost:1883")
    std::string client_id;       ///< MQTT client identifier
    std::string username;        ///< MQTT username (optional)
    std::string password;        ///< MQTT password (optional)
    std::string source_topic;    ///< Topic pattern to subscribe to (e.g., "tags/#")
    std::string dest_topic_prefix; ///< Prefix for published topics (e.g., "processed/")
    int qos;                     ///< Quality of Service (0, 1, or 2)
    int keepalive_interval;      ///< Keep-alive interval in seconds
    bool clean_session;          ///< Clean session flag
    bool use_ssl;                ///< Enable SSL/TLS
};

/**
 * @brief Configuration structure for dual MQTT brokers (source + destination)
 */
struct DualMqttConfig {
    MqttConfig source_broker;  ///< Source broker for subscribing
    MqttConfig dest_broker;    ///< Destination broker for publishing
    bool dual_mode;            ///< True if using separate brokers for source/dest
};

/**
 * @brief Configuration structure for UWB transformation
 */
struct TransformConfig {
    double origin_x;      ///< X location of image top-left corner in UWB frame (mm)
    double origin_y;      ///< Y location of image top-left corner in UWB frame (mm)
    double scale;         ///< Pixels per UWB unit (pixels/mm)
    double rotation_rad;  ///< Rotation of UWB frame in radians
    bool x_flipped;       ///< True if UWB X axis opposes Image X axis
    bool y_flipped;       ///< True if UWB Y axis opposes Image Y axis
    std::string frame_id; ///< Frame ID to add to output coordinates
    std::string output_units; ///< Output units: "meters", "millimeters", or "pixels"
};

/**
 * @brief Complete application configuration
 */
struct AppConfig {
    DualMqttConfig mqtt;
    TransformConfig transform;
    std::string log_level;        ///< Logging level (trace, debug, info, warn, error)
    std::string log_file;         ///< Log file path (empty for console only)
    int log_rotation_size_mb;     ///< Log rotation size in MB
    int log_rotation_count;       ///< Number of rotated log files to keep
};

/**
 * @brief Configuration loader for UWB Bridge application
 * 
 * Loads and validates configuration from JSON file. Provides
 * sensible defaults for optional parameters.
 */
class ConfigLoader {
public:
    /**
     * @brief Load configuration from JSON file
     * @param config_path Path to JSON configuration file
     * @return AppConfig structure with loaded configuration
     * @throws std::runtime_error if file cannot be read or parsed
     */
    static AppConfig loadFromFile(const std::string& config_path);

    /**
     * @brief Validate configuration parameters
     * @param config Configuration to validate
     * @return true if configuration is valid
     * @throws std::invalid_argument if configuration is invalid
     */
    static bool validate(const AppConfig& config);

private:
    /**
     * @brief Parse MQTT configuration section (single or dual broker)
     */
    static DualMqttConfig parseMqttConfig(const nlohmann::json& j);
    
    /**
     * @brief Parse single broker MQTT config
     */
    static MqttConfig parseSingleBrokerConfig(const nlohmann::json& j);

    /**
     * @brief Parse Transform configuration section
     */
    static TransformConfig parseTransformConfig(const nlohmann::json& j);
};

} // namespace uwb_bridge

#endif // CONFIG_LOADER_HPP

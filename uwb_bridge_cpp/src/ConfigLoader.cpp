#include "ConfigLoader.hpp"
#include <fstream>
#include <stdexcept>
#include <spdlog/spdlog.h>

namespace uwb_bridge {

AppConfig ConfigLoader::loadFromFile(const std::string& config_path) {
    spdlog::info("Loading configuration from: {}", config_path);

    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + config_path);
    }

    nlohmann::json j;
    try {
        config_file >> j;
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("Failed to parse JSON config: " + std::string(e.what()));
    }

    AppConfig config;

    // Parse sections
    if (j.contains("mqtt")) {
        config.mqtt = parseMqttConfig(j["mqtt"]);
    } else {
        throw std::runtime_error("Missing 'mqtt' section in configuration");
    }

    if (j.contains("transform")) {
        config.transform = parseTransformConfig(j["transform"]);
    } else {
        throw std::runtime_error("Missing 'transform' section in configuration");
    }

    // Parse logging configuration (with defaults)
    config.log_level = j.value("log_level", "info");
    config.log_file = j.value("log_file", "");
    config.log_rotation_size_mb = j.value("log_rotation_size_mb", 10);
    config.log_rotation_count = j.value("log_rotation_count", 3);

    // Validate configuration
    validate(config);

    spdlog::info("Configuration loaded successfully");
    return config;
}

DualMqttConfig ConfigLoader::parseMqttConfig(const nlohmann::json& j) {
    DualMqttConfig config;
    
    // Check if using dual broker mode (new format)
    if (j.contains("source_broker") && j.contains("dest_broker")) {
        spdlog::info("Detected dual MQTT broker configuration");
        config.dual_mode = true;
        config.source_broker = parseSingleBrokerConfig(j["source_broker"]);
        config.dest_broker = parseSingleBrokerConfig(j["dest_broker"]);
    } 
    // Legacy single broker mode (old format)
    else {
        spdlog::info("Detected single MQTT broker configuration (legacy mode)");
        config.dual_mode = false;
        MqttConfig single = parseSingleBrokerConfig(j);
        config.source_broker = single;
        config.dest_broker = single;  // Use same broker for both
    }

    return config;
}

MqttConfig ConfigLoader::parseSingleBrokerConfig(const nlohmann::json& j) {
    MqttConfig config;

    // Required parameters
    if (!j.contains("broker_address")) {
        throw std::runtime_error("Missing required parameter: broker_address");
    }
    config.broker_address = j["broker_address"].get<std::string>();

    // Source topic (required for source broker, optional for dest)
    config.source_topic = j.value("source_topic", "");

    // Optional parameters with defaults
    config.client_id = j.value("client_id", "uwb_bridge_cpp");
    config.username = j.value("username", "");
    config.password = j.value("password", "");
    config.dest_topic_prefix = j.value("dest_topic_prefix", "processed/");
    config.qos = j.value("qos", 1);
    config.keepalive_interval = j.value("keepalive_interval", 60);
    config.clean_session = j.value("clean_session", true);
    config.use_ssl = j.value("use_ssl", false);

    return config;
}

TransformConfig ConfigLoader::parseTransformConfig(const nlohmann::json& j) {
    TransformConfig config;

    // All parameters are required
    if (!j.contains("origin_x") || !j.contains("origin_y") ||
        !j.contains("scale") || !j.contains("rotation_rad")) {
        throw std::runtime_error("Missing required transformation parameters");
    }

    config.origin_x = j["origin_x"].get<double>();
    config.origin_y = j["origin_y"].get<double>();
    config.scale = j["scale"].get<double>();
    config.rotation_rad = j["rotation_rad"].get<double>();
    config.x_flipped = j.value("x_flipped", false);
    config.y_flipped = j.value("y_flipped", false);

    return config;
}

bool ConfigLoader::validate(const AppConfig& config) {
    // Validate MQTT configuration
    if (config.mqtt.source_broker.broker_address.empty()) {
        throw std::invalid_argument("Source MQTT broker address cannot be empty");
    }

    if (config.mqtt.source_broker.source_topic.empty()) {
        throw std::invalid_argument("MQTT source topic cannot be empty");
    }

    if (config.mqtt.source_broker.qos < 0 || config.mqtt.source_broker.qos > 2) {
        throw std::invalid_argument("MQTT QoS must be 0, 1, or 2");
    }

    if (config.mqtt.source_broker.keepalive_interval < 1) {
        throw std::invalid_argument("MQTT keepalive interval must be positive");
    }
    
    // Validate destination broker if in dual mode
    if (config.mqtt.dual_mode) {
        if (config.mqtt.dest_broker.broker_address.empty()) {
            throw std::invalid_argument("Destination MQTT broker address cannot be empty");
        }
    }

    // Validate transform configuration
    if (config.transform.scale == 0.0) {
        throw std::invalid_argument("Transform scale cannot be zero");
    }

    // Validate logging configuration
    const std::vector<std::string> valid_levels = {
        "trace", "debug", "info", "warn", "error", "critical", "off"
    };
    if (std::find(valid_levels.begin(), valid_levels.end(), config.log_level) == valid_levels.end()) {
        throw std::invalid_argument("Invalid log level: " + config.log_level);
    }

    return true;
}

} // namespace uwb_bridge

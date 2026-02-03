#include "BridgeCore.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <thread>

namespace uwb_bridge {

BridgeCore::BridgeCore(const AppConfig& config)
    : config_(config), 
      start_time_(std::chrono::system_clock::now()) {
    spdlog::info("BridgeCore initialized");
}

BridgeCore::~BridgeCore() {
    if (running_) {
        stop();
    }
}

bool BridgeCore::initialize() {
    try {
        spdlog::info("Initializing BridgeCore components...");

        // Initialize coordinate transformer
        spdlog::info("Creating FloorplanTransformer...");
        
        uwb_transform::TransformConfig tf_config;
        tf_config.origin_x = config_.transform.origin_x;
        tf_config.origin_y = config_.transform.origin_y;
        tf_config.scale = config_.transform.scale;
        tf_config.rotation_rad = config_.transform.rotation_rad;
        tf_config.x_flipped = config_.transform.x_flipped;
        tf_config.y_flipped = config_.transform.y_flipped;

        transformer_ = std::make_unique<uwb_transform::FloorplanTransformer>(tf_config);
        spdlog::info("FloorplanTransformer created successfully");
        spdlog::info("  Origin: ({}, {}) mm", tf_config.origin_x, tf_config.origin_y);
        spdlog::info("  Scale: {} px/mm", tf_config.scale);
        spdlog::info("  Rotation: {} rad", tf_config.rotation_rad);

        // Initialize MQTT handler with callback
        spdlog::info("Creating MQTT handler...");
        
        mqtt_handler_ = std::make_unique<MqttHandler>(
            config_.mqtt,
            [this](const std::string& topic, const std::string& payload) {
                this->onMessageReceived(topic, payload);
            }
        );
        
        spdlog::info("MQTT handler created successfully");

        initialized_ = true;
        spdlog::info("BridgeCore initialization complete");
        
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize BridgeCore: {}", e.what());
        return false;
    }
}

bool BridgeCore::start() {
    if (!initialized_) {
        spdlog::error("Cannot start - BridgeCore not initialized");
        return false;
    }

    if (running_) {
        spdlog::warn("BridgeCore already running");
        return true;
    }

    spdlog::info("Starting BridgeCore...");

    // Connect to MQTT broker
    if (!mqtt_handler_->connect()) {
        spdlog::error("Failed to connect to MQTT broker");
        return false;
    }

    running_ = true;
    start_time_ = std::chrono::system_clock::now();
    
    spdlog::info("BridgeCore started successfully");
    spdlog::info("Listening for messages on topic: {}", config_.mqtt.source_topic);
    
    return true;
}

void BridgeCore::stop() {
    if (!running_) {
        return;
    }

    spdlog::info("Stopping BridgeCore...");
    
    running_ = false;

    // Print final statistics
    printStats();

    // Disconnect MQTT
    if (mqtt_handler_) {
        mqtt_handler_->disconnect();
    }

    spdlog::info("BridgeCore stopped");
}

void BridgeCore::onMessageReceived(const std::string& topic, const std::string& payload) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    spdlog::debug("BridgeCore::onMessageReceived called - Topic: {}, Payload: {}", topic, payload);
    total_messages_++;

    try {
        // Parse message
        double uwb_x, uwb_y, uwb_z = 0.0;
        std::string tag_id;

        spdlog::debug("Attempting to parse message...");
        if (!parseMessage(payload, uwb_x, uwb_y, uwb_z, tag_id)) {
            malformed_messages_++;
            spdlog::warn("Malformed message on topic {}", topic);
            return;
        }
        
        spdlog::debug("Parsed: tag_id='{}', x={}, y={}, z={}", tag_id, uwb_x, uwb_y, uwb_z);

        // If tag_id not in JSON, try to extract from topic
        if (tag_id.empty()) {
            tag_id = extractTagIdFromTopic(topic);
            spdlog::debug("Extracted tag_id from topic: '{}'", tag_id);
        }

        // Transform coordinates
        spdlog::debug("Transforming coordinates...");
        double meter_x, meter_y;
        if (!transformCoordinates(uwb_x, uwb_y, meter_x, meter_y)) {
            failed_transforms_++;
            spdlog::error("Transformation failed for tag {}", tag_id);
            return;
        }

        // Create output message
        uint64_t timestamp = getCurrentTimestampMs();
        std::string output_json = createOutputMessage(tag_id, meter_x, meter_y, uwb_z, timestamp);
        
        spdlog::debug("Created output JSON: {}", output_json);

        // Publish transformed data
        std::string output_topic = config_.mqtt.dest_topic_prefix + tag_id;
        spdlog::debug("Publishing to topic: {}", output_topic);
        
        // Publish in detached thread to avoid deadlock from MQTT callback thread
        std::thread([this, output_topic, output_json]() {
            if (mqtt_handler_->publish(output_topic, output_json)) {
                successful_transforms_++;
                spdlog::debug("Published successfully!");
            } else {
                failed_transforms_++;
                spdlog::error("Failed to publish message");
            }
        }).detach();

        // Update processing time statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        total_processing_time_us_ += duration.count();

        // Log at debug level (avoid spam in production)
        spdlog::debug("Processed tag {}: ({:.2f}, {:.2f}) mm -> ({:.3f}, {:.3f}) m in {} μs",
                     tag_id, uwb_x, uwb_y, meter_x, meter_y, duration.count());

    } catch (const std::exception& e) {
        spdlog::error("Exception in message processing: {}", e.what());
        failed_transforms_++;
    }
}

bool BridgeCore::parseMessage(const std::string& payload,
                             double& uwb_x, double& uwb_y, double& uwb_z,
                             std::string& tag_id) {
    try {
        auto j = nlohmann::json::parse(payload);

        // Try different possible field names for coordinates
        // Pozyx format: {"coordinates": {"x": ..., "y": ..., "z": ...}}
        if (j.contains("coordinates") && j["coordinates"].is_object()) {
            auto coords = j["coordinates"];
            uwb_x = coords["x"].get<double>();
            uwb_y = coords["y"].get<double>();
            uwb_z = coords.value("z", 0.0);
        }
        // Simple format: {"x": ..., "y": ..., "z": ...}
        else if (j.contains("x") && j.contains("y")) {
            uwb_x = j["x"].get<double>();
            uwb_y = j["y"].get<double>();
            uwb_z = j.value("z", 0.0);
        }
        // Alternative formats
        else if (j.contains("posX") && j.contains("posY")) {
            uwb_x = j["posX"].get<double>();
            uwb_y = j["posY"].get<double>();
            uwb_z = j.value("posZ", 0.0);
        } else if (j.contains("position")) {
            auto pos = j["position"];
            uwb_x = pos["x"].get<double>();
            uwb_y = pos["y"].get<double>();
            uwb_z = pos.value("z", 0.0);
        } else {
            spdlog::warn("Message missing coordinate fields");
            return false;
        }

        // Extract tag ID if present
        // Pozyx format: {"tagData": {"tagId": "111"}}
        if (j.contains("tagData") && j["tagData"].is_object() && j["tagData"].contains("tagId")) {
            tag_id = j["tagData"]["tagId"].get<std::string>();
        }
        // Simple formats
        else if (j.contains("tag_id")) {
            tag_id = j["tag_id"].get<std::string>();
        } else if (j.contains("tagId")) {
            tag_id = j["tagId"].get<std::string>();
        } else if (j.contains("id")) {
            tag_id = j["id"].get<std::string>();
        }

        return true;

    } catch (const nlohmann::json::exception& e) {
        spdlog::error("JSON parsing error: {}", e.what());
        return false;
    }
}

bool BridgeCore::transformCoordinates(double uwb_x, double uwb_y,
                                     double& meter_x, double& meter_y) {
    try {
        spdlog::debug("Calling transformer_->transformToPixel({}, {})", uwb_x, uwb_y);
        auto result = transformer_->transformToPixel(uwb_x, uwb_y);
        meter_x = result(0);
        meter_y = result(1);
        spdlog::debug("Transform result: ({}, {})", meter_x, meter_y);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Transform error: {}", e.what());
        return false;
    }
}

std::string BridgeCore::createOutputMessage(const std::string& tag_id,
                                           double meter_x, double meter_y, double uwb_z,
                                           uint64_t timestamp) {
    nlohmann::json j;
    j["tag_id"] = tag_id;
    j["x"] = meter_x;
    j["y"] = meter_y;
    j["z"] = uwb_z;
    j["timestamp"] = timestamp;
    j["units"] = "meters";
    
    return j.dump();
}

std::string BridgeCore::extractTagIdFromTopic(const std::string& topic) {
    // Extract last part of topic (e.g., "tags/0x1234" -> "0x1234")
    size_t last_slash = topic.find_last_of('/');
    if (last_slash != std::string::npos && last_slash < topic.length() - 1) {
        return topic.substr(last_slash + 1);
    }
    return "unknown";
}

uint64_t BridgeCore::getCurrentTimestampMs() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

BridgeStats BridgeCore::getStats() const {
    BridgeStats stats;
    stats.total_messages = total_messages_.load();
    stats.successful_transforms = successful_transforms_.load();
    stats.failed_transforms = failed_transforms_.load();
    stats.malformed_messages = malformed_messages_.load();
    stats.start_time = start_time_;
    
    uint64_t total_time = total_processing_time_us_.load();
    uint64_t success_count = successful_transforms_.load();
    stats.avg_processing_time_us = success_count > 0 ? 
        static_cast<double>(total_time) / success_count : 0.0;
    
    return stats;
}

void BridgeCore::printStats() const {
    auto stats = getStats();
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - stats.start_time);
    
    spdlog::info("=================================================");
    spdlog::info("  UWB Bridge Statistics");
    spdlog::info("=================================================");
    spdlog::info("Uptime:               {} seconds", uptime.count());
    spdlog::info("Total Messages:       {}", stats.total_messages);
    spdlog::info("Successful:           {}", stats.successful_transforms);
    spdlog::info("Failed:               {}", stats.failed_transforms);
    spdlog::info("Malformed:            {}", stats.malformed_messages);
    
    if (stats.successful_transforms > 0) {
        double success_rate = 100.0 * stats.successful_transforms / stats.total_messages;
        spdlog::info("Success Rate:         {:.2f}%", success_rate);
        spdlog::info("Avg Processing Time:  {:.2f} μs", stats.avg_processing_time_us);
        
        if (uptime.count() > 0) {
            double throughput = static_cast<double>(stats.successful_transforms) / uptime.count();
            spdlog::info("Throughput:           {:.2f} msg/sec", throughput);
        }
    }
    
    spdlog::info("=================================================");
}

} // namespace uwb_bridge

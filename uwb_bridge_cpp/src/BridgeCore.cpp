#include "BridgeCore.hpp"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>

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

        // Initialize MQTT handlers
        spdlog::info("Creating MQTT handlers...");
        dual_mqtt_mode_ = config_.mqtt.dual_mode;
        
        if (dual_mqtt_mode_) {
            spdlog::info("Dual MQTT mode: separate source and destination brokers");
            
            // Source MQTT handler (subscribe only)
            mqtt_source_handler_ = std::make_unique<MqttHandler>(
                config_.mqtt.source_broker,
                [this](const std::string& topic, const std::string& payload) {
                    this->onMessageReceived(topic, payload);
                }
            );
            spdlog::info("Source MQTT handler created: {}", config_.mqtt.source_broker.broker_address);
            
            // Destination MQTT handler (publish only, no callback needed)
            mqtt_dest_handler_ = std::make_unique<MqttHandler>(
                config_.mqtt.dest_broker,
                nullptr  // No message callback for publish-only handler
            );
            spdlog::info("Destination MQTT handler created: {}", config_.mqtt.dest_broker.broker_address);
        } else {
            spdlog::info("Single MQTT mode: same broker for source and destination");
            
            // Single broker for both subscribe and publish
            mqtt_source_handler_ = std::make_unique<MqttHandler>(
                config_.mqtt.source_broker,
                [this](const std::string& topic, const std::string& payload) {
                    this->onMessageReceived(topic, payload);
                }
            );
            // In single mode, both pointers point to the same handler (no second unique_ptr)
            spdlog::info("MQTT handler created: {}", config_.mqtt.source_broker.broker_address);
        }

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

    // Connect to MQTT broker(s)
    if (dual_mqtt_mode_) {
        spdlog::info("Connecting to source MQTT broker...");
        if (!mqtt_source_handler_->connect()) {
            spdlog::error("Failed to connect to source MQTT broker");
            return false;
        }
        
        spdlog::info("Connecting to destination MQTT broker...");
        if (!mqtt_dest_handler_->connect()) {
            spdlog::error("Failed to connect to destination MQTT broker");
            // Disconnect source on failure to avoid partial state
            mqtt_source_handler_->disconnect();
            return false;
        }
    } else {
        spdlog::info("Connecting to MQTT broker...");
        if (!mqtt_source_handler_->connect()) {
            spdlog::error("Failed to connect to MQTT broker");
            return false;
        }
    }

    running_ = true;
    start_time_ = std::chrono::system_clock::now();
    
    spdlog::info("BridgeCore started successfully");
    spdlog::info("Listening for messages on topic: {}", config_.mqtt.source_broker.source_topic);
    
    return true;
}

void BridgeCore::stop() {
    if (!running_) {
        return;
    }

    spdlog::info("Stopping BridgeCore...");
    
    // Signal shutdown to all threads first
    shutdown_requested_ = true;
    
    // Give publish threads time to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    running_ = false;

    // Print final statistics
    printStats();

    // Disconnect MQTT broker(s)
    if (dual_mqtt_mode_) {
        if (mqtt_source_handler_) {
            mqtt_source_handler_->disconnect();
        }
        if (mqtt_dest_handler_) {
            mqtt_dest_handler_->disconnect();
        }
    } else {
        if (mqtt_source_handler_) {
            mqtt_source_handler_->disconnect();
        }
    }

    spdlog::info("BridgeCore stopped");
}

void BridgeCore::onMessageReceived(const std::string& topic, const std::string& payload) {
    // Ignore messages if bridge not fully running (both brokers connected) or shutting down
    if (!running_ || shutdown_requested_) {
        spdlog::debug("Ignoring message - bridge not ready or shutting down");
        return;
    }
    
    // Capture arrival timestamp immediately for end-to-end latency measurement
    auto arrival_time = std::chrono::high_resolution_clock::now();
    auto start_time = arrival_time;
    
    spdlog::debug("BridgeCore::onMessageReceived called - Topic: {}, Payload: {}", topic, payload);
    total_messages_++;

    try {
        // Check if payload is an array of tags
        auto j_check = nlohmann::json::parse(payload);
        if (j_check.is_array() && !j_check.empty()) {
            // Array of tags - process each one
            // spdlog::debug("Processing array of {} tags", j_check.size());
            
            // Process and transform all tags in the array
            std::string output_json = processAndModifyMessageArray(payload);
            
            if (output_json.empty()) {
                spdlog::error("Failed to process tag array");
                failed_transforms_++;
                return;
            }
            
            // spdlog::debug("Created output JSON array: {}", output_json);
            
            // Calculate processing latency
            auto transform_end = std::chrono::high_resolution_clock::now();
            auto transform_duration = std::chrono::duration_cast<std::chrono::microseconds>(transform_end - start_time);
            auto total_latency = std::chrono::duration_cast<std::chrono::microseconds>(transform_end - arrival_time);
            total_processing_time_us_ += transform_duration.count();
            
            // Publish transformed data to the common topic
            std::string output_topic = config_.mqtt.dest_broker.dest_topic_prefix + "tags";
            // spdlog::debug("Publishing array to topic: {}", output_topic);
            
            // Capture needed data for thread
            bool is_dual_mode = dual_mqtt_mode_;
            auto* source_ptr = mqtt_source_handler_.get();
            auto* dest_ptr = mqtt_dest_handler_.get();
            auto shutdown_flag = &shutdown_requested_;
            
            // Publish in detached thread
            std::thread([is_dual_mode, source_ptr, dest_ptr, output_topic, output_json, arrival_time, shutdown_flag, this]() {
                if (shutdown_flag->load()) {
                    return;
                }
                
                auto publish_start = std::chrono::high_resolution_clock::now();
                MqttHandler* pub_handler = is_dual_mode ? dest_ptr : source_ptr;
                
                if (pub_handler && pub_handler->publish(output_topic, output_json)) {
                    successful_transforms_++;
                } else {
                    failed_transforms_++;
                    spdlog::error("Failed to publish array message");
                }
            }).detach();
            
            return;
        }
        
        // Single tag processing (original logic)
        double uwb_x, uwb_y, uwb_z = 0.0;
        std::string tag_id;

        spdlog::debug("Attempting to parse single tag message...");
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

        // Transform Z coordinate (simple unit conversion)
        double transformed_z = uwb_z;
        if (config_.transform.output_units == "meters") {
            transformed_z = uwb_z / 1000.0;  // mm to meters
        } else if (config_.transform.output_units == "pixels") {
            transformed_z = uwb_z * config_.transform.scale;  // mm to pixels
        }
        // else keep in millimeters

        // Process and modify the original message to preserve nested structure
        std::string output_json = processAndModifyMessage(payload, meter_x, meter_y, transformed_z);
        
        spdlog::debug("Created output JSON: {}", output_json);

        // Calculate processing latency (transform time)
        auto transform_end = std::chrono::high_resolution_clock::now();
        auto transform_duration = std::chrono::duration_cast<std::chrono::microseconds>(transform_end - start_time);
        auto total_latency = std::chrono::duration_cast<std::chrono::microseconds>(transform_end - arrival_time);
        total_processing_time_us_ += transform_duration.count();
        
        // Log latency prominently
        // spdlog::info("[LATENCY] Tag {}: Transform={}μs, Total={}μs | ({:.2f}, {:.2f})mm -> ({:.3f}, {:.3f})m",
        //              tag_id, transform_duration.count(), total_latency.count(),
        //              uwb_x, uwb_y, meter_x, meter_y);
        
        // Publish transformed data
        std::string output_topic = config_.mqtt.dest_broker.dest_topic_prefix + tag_id;
        spdlog::debug("Publishing to topic: {}", output_topic);
        
        // Capture needed data for thread - avoid capturing 'this' to prevent use-after-free
        bool is_dual_mode = dual_mqtt_mode_;
        auto* source_ptr = mqtt_source_handler_.get();
        auto* dest_ptr = mqtt_dest_handler_.get();
        auto shutdown_flag = &shutdown_requested_;
        
        // Publish in detached thread to avoid deadlock from MQTT callback thread
        std::thread([is_dual_mode, source_ptr, dest_ptr, output_topic, output_json, arrival_time, shutdown_flag, this]() {
            // Check if shutdown requested
            if (shutdown_flag->load()) {
                return;
            }
            
            auto publish_start = std::chrono::high_resolution_clock::now();
            
            // Use dest_handler in dual mode, source_handler in single mode
            MqttHandler* pub_handler = is_dual_mode ? dest_ptr : source_ptr;
            
            if (pub_handler && pub_handler->publish(output_topic, output_json)) {
                auto publish_end = std::chrono::high_resolution_clock::now();
                auto publish_latency = std::chrono::duration_cast<std::chrono::microseconds>(publish_end - publish_start);
                auto end_to_end = std::chrono::duration_cast<std::chrono::microseconds>(publish_end - arrival_time);
                
                successful_transforms_++;
                // spdlog::info("[PUBLISH] Publish={}μs, End-to-end={}μs", publish_latency.count(), end_to_end.count());
            } else {
                failed_transforms_++;
                spdlog::error("Failed to publish message");
            }
        }).detach();

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
        
        // Note: Array handling moved to onMessageReceived for proper multi-tag processing
        // This function now only handles single tag objects

        // Try different possible field names for coordinates
        // Pozyx nested format: {"data": {"coordinates": {"x": ..., "y": ..., "z": ...}}}
        if (j.contains("data") && j["data"].is_object() && 
            j["data"].contains("coordinates") && j["data"]["coordinates"].is_object()) {
            auto coords = j["data"]["coordinates"];
            uwb_x = coords["x"].get<double>();
            uwb_y = coords["y"].get<double>();
            uwb_z = coords.value("z", 0.0);
        }
        // Pozyx format: {"coordinates": {"x": ..., "y": ..., "z": ...}}
        else if (j.contains("coordinates") && j["coordinates"].is_object()) {
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
        // Check simple top-level formats first (most common)
        if (j.contains("tagId")) {
            tag_id = j["tagId"].get<std::string>();
        } else if (j.contains("tag_id")) {
            tag_id = j["tag_id"].get<std::string>();
        } else if (j.contains("id")) {
            tag_id = j["id"].get<std::string>();
        }
        // Nested Pozyx format: {"tagData": {"tagId": "111"}}
        else if (j.contains("tagData") && j["tagData"].is_object() && j["tagData"].contains("tagId")) {
            tag_id = j["tagData"]["tagId"].get<std::string>();
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
    j["processing_timestamp"] = getCurrentTimestampMs();
    j["units"] = "meters";
    
    return j.dump();
}

std::string BridgeCore::processAndModifyMessageArray(const std::string& payload) {
    try {
        auto j = nlohmann::json::parse(payload);
        
        if (!j.is_array() || j.empty()) {
            spdlog::error("processAndModifyMessageArray called with non-array payload");
            return "";
        }
        
        // Process each tag in the array
        for (size_t i = 0; i < j.size(); ++i) {
            auto& tag_obj = j[i];
            
            // Extract coordinates from this tag
            double uwb_x, uwb_y, uwb_z = 0.0;
            std::string tag_id = "unknown";
            
            // Parse coordinates
            if (tag_obj.contains("data") && tag_obj["data"].is_object() && 
                tag_obj["data"].contains("coordinates") && tag_obj["data"]["coordinates"].is_object()) {
                auto& coords = tag_obj["data"]["coordinates"];  // Reference so modifications stick!
                uwb_x = coords["x"].get<double>();
                uwb_y = coords["y"].get<double>();
                uwb_z = coords.value("z", 0.0);
                
                // Extract tag ID
                if (tag_obj.contains("tagId")) {
                    tag_id = tag_obj["tagId"].get<std::string>();
                }
                
                // Transform coordinates
                double meter_x, meter_y;
                if (!transformCoordinates(uwb_x, uwb_y, meter_x, meter_y)) {
                    spdlog::error("Transformation failed for tag {} in array (index {})", tag_id, i);
                    continue;  // Skip this tag but process others
                }
                
                // Transform Z coordinate
                double transformed_z = uwb_z;
                if (config_.transform.output_units == "meters") {
                    transformed_z = uwb_z / 1000.0;  // mm to meters
                } else if (config_.transform.output_units == "pixels") {
                    transformed_z = uwb_z * config_.transform.scale;  // mm to pixels
                }
                
                // Update coordinates in place
                coords["x"] = meter_x;
                coords["y"] = meter_y;
                coords["z"] = transformed_z;
                coords["frame_id"] = config_.transform.frame_id;
                coords["processing_timestamp"] = getCurrentTimestampMs();
                coords["units"] = config_.transform.output_units;
                
                // Remove anchor data to save bandwidth
                if (tag_obj["data"].contains("anchorData")) {
                    tag_obj["data"].erase("anchorData");
                }
                
                // spdlog::info("Transformed tag {} (index {}): ({:.2f}, {:.2f}, {:.2f})mm -> ({:.3f}, {:.3f}, {:.3f})m",
                //             tag_id, i, uwb_x, uwb_y, uwb_z, meter_x, meter_y, transformed_z);
            } else {
                spdlog::warn("Tag at index {} has unexpected format, skipping", i);
                spdlog::info("Tag object: {}", tag_obj.dump());
            }
        }
        
        return j.dump();
        
    } catch (const std::exception& e) {
        spdlog::error("Error processing array message: {}", e.what());
        return "";
    }
}

std::string BridgeCore::processAndModifyMessage(const std::string& payload,
                                               double transformed_x, 
                                               double transformed_y, 
                                               double transformed_z) {
    try {
        auto j = nlohmann::json::parse(payload);
        
        // Single tag processing only (array handling moved to processAndModifyMessageArray)
        // Check if this is nested format (has data.coordinates)
        bool is_nested = j.contains("data") && j["data"].is_object() && 
                        j["data"].contains("coordinates");
        
        if (is_nested) {
            // Modify existing nested structure in-place
            auto& coords = j["data"]["coordinates"];
            coords["x"] = transformed_x;
            coords["y"] = transformed_y;
            coords["z"] = transformed_z;
            coords["frame_id"] = config_.transform.frame_id;
            coords["processing_timestamp"] = getCurrentTimestampMs();
            coords["units"] = config_.transform.output_units;
            
            // Remove anchor data to save bandwidth
            if (j["data"].contains("anchorData")) {
                j["data"].erase("anchorData");
            }
            
            return j.dump();
        } else {
            // Not nested format - use createOutputMessage for backward compatibility
            std::string tag_id = "unknown";
            if (j.contains("tagId")) {
                tag_id = j["tagId"].get<std::string>();
            } else if (j.contains("tag_id")) {
                tag_id = j["tag_id"].get<std::string>();
            }
            return createOutputMessage(tag_id, transformed_x, transformed_y, transformed_z, getCurrentTimestampMs());
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Error processing JSON message: {}", e.what());
        // Fallback to simple output
        return createOutputMessage("unknown", transformed_x, transformed_y, transformed_z, getCurrentTimestampMs());
    }
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

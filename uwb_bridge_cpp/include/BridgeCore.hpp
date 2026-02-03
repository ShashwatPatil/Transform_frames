#ifndef BRIDGE_CORE_HPP
#define BRIDGE_CORE_HPP

#include "ConfigLoader.hpp"
#include "MqttHandler.hpp"
#include "FloorplanTransformer.hpp"
#include <memory>
#include <atomic>
#include <chrono>

namespace uwb_bridge {

/**
 * @brief Statistics for the bridge operations
 */
struct BridgeStats {
    uint64_t total_messages;
    uint64_t successful_transforms;
    uint64_t failed_transforms;
    uint64_t malformed_messages;
    double avg_processing_time_us;
    std::chrono::system_clock::time_point start_time;
    
    BridgeStats() 
        : total_messages(0), 
          successful_transforms(0),
          failed_transforms(0),
          malformed_messages(0),
          avg_processing_time_us(0.0),
          start_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Core business logic for the UWB bridge
 * 
 * Handles the complete data flow:
 * 1. Receives raw UWB data from MQTT
 * 2. Parses JSON payload
 * 3. Applies coordinate transformation
 * 4. Publishes transformed data back to MQTT
 * 
 * Thread-safe and exception-safe to ensure the service never crashes.
 */
class BridgeCore {
public:
    /**
     * @brief Construct bridge with configuration
     * @param config Application configuration
     */
    explicit BridgeCore(const AppConfig& config);

    /**
     * @brief Destructor
     */
    ~BridgeCore();

    /**
     * @brief Initialize bridge components (transformer, MQTT)
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Start the bridge service
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the bridge service
     */
    void stop();

    /**
     * @brief Check if bridge is running
     * @return true if service is active
     */
    bool isRunning() const { return running_; }

    /**
     * @brief Get current statistics
     * @return BridgeStats structure with current stats
     */
    BridgeStats getStats() const;

    /**
     * @brief Print statistics to log
     */
    void printStats() const;

private:
    /**
     * @brief Callback for incoming MQTT messages
     * @param topic MQTT topic
     * @param payload Message payload (JSON string)
     */
    void onMessageReceived(const std::string& topic, const std::string& payload);

    /**
     * @brief Parse incoming JSON message
     * @param payload JSON string
     * @param uwb_x Output: UWB X coordinate
     * @param uwb_y Output: UWB Y coordinate
     * @param uwb_z Output: UWB Z coordinate (optional)
     * @param tag_id Output: Tag identifier
     * @return true if parsing successful
     */
    bool parseMessage(const std::string& payload, 
                     double& uwb_x, double& uwb_y, double& uwb_z,
                     std::string& tag_id);

    /**
     * @brief Transform coordinates using the configured transformer
     * @param uwb_x Input UWB X coordinate (mm)
     * @param uwb_y Input UWB Y coordinate (mm)
     * @param meter_x Output transformed X coordinate (meters)
     * @param meter_y Output transformed Y coordinate (meters)
     * @return true if transformation successful
     */
    bool transformCoordinates(double uwb_x, double uwb_y,
                            double& meter_x, double& meter_y);

    /**
     * @brief Create output JSON message
     * @param tag_id Tag identifier
     * @param meter_x Transformed X coordinate
     * @param meter_y Transformed Y coordinate
     * @param uwb_z Original Z coordinate
     * @param timestamp Unix timestamp in milliseconds
     * @return JSON string
     */
    std::string createOutputMessage(const std::string& tag_id,
                                   double meter_x, double meter_y, double uwb_z,
                                   uint64_t timestamp);

    /**
     * @brief Extract tag ID from MQTT topic
     * @param topic Full MQTT topic (e.g., "tags/0x1234")
     * @return Tag ID string
     */
    std::string extractTagIdFromTopic(const std::string& topic);

    /**
     * @brief Get current Unix timestamp in milliseconds
     */
    static uint64_t getCurrentTimestampMs();

    AppConfig config_;
    std::unique_ptr<uwb_transform::FloorplanTransformer> transformer_;
    std::unique_ptr<MqttHandler> mqtt_handler_;
    
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    
    // Statistics (atomic for thread safety)
    std::atomic<uint64_t> total_messages_{0};
    std::atomic<uint64_t> successful_transforms_{0};
    std::atomic<uint64_t> failed_transforms_{0};
    std::atomic<uint64_t> malformed_messages_{0};
    std::atomic<uint64_t> total_processing_time_us_{0};
    
    std::chrono::system_clock::time_point start_time_;
};

} // namespace uwb_bridge

#endif // BRIDGE_CORE_HPP

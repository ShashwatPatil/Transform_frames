#ifndef MQTT_HANDLER_HPP
#define MQTT_HANDLER_HPP

#include "ConfigLoader.hpp"
#include <mqtt/async_client.h>
#include <functional>
#include <memory>
#include <atomic>

namespace uwb_bridge {

/**
 * @brief Callback function type for incoming MQTT messages
 * @param topic The topic the message was published to
 * @param payload The message payload as a string
 */
using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

/**
 * @brief Callback action listener for MQTT operations
 * 
 * Handles async callbacks for connect, disconnect, and publish operations
 */
class ActionListener : public virtual mqtt::iaction_listener {
private:
    std::string name_;
    
    void on_failure(const mqtt::token& tok) override;
    void on_success(const mqtt::token& tok) override;

public:
    ActionListener(const std::string& name) : name_(name) {}
};

/**
 * @brief Callback handler for incoming MQTT messages
 */
class Callback : public virtual mqtt::callback, public virtual mqtt::iaction_listener {
private:
    mqtt::async_client& client_;
    mqtt::connect_options& conn_opts_;
    ActionListener& sub_listener_;
    std::string source_topic_;
    int qos_;
    MessageCallback message_callback_;
    std::atomic<int> reconnect_attempts_{0};
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr int RECONNECT_DELAY_MS = 5000;

    void reconnect();
    
    // mqtt::callback interface
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr token) override;
    
    // mqtt::iaction_listener interface (for reconnection)
    void on_failure(const mqtt::token& tok) override;
    void on_success(const mqtt::token& tok) override;

public:
    Callback(mqtt::async_client& client,
             mqtt::connect_options& conn_opts,
             ActionListener& sub_listener,
             const std::string& source_topic,
             int qos,
             MessageCallback callback);
};

/**
 * @brief Asynchronous MQTT handler with automatic reconnection
 * 
 * Wraps Eclipse Paho MQTT C++ client with robust error handling,
 * automatic reconnection, and clean callback-based architecture.
 * Thread-safe for concurrent publishing.
 */
class MqttHandler {
public:
    /**
     * @brief Construct MQTT handler from configuration
     * @param config MQTT configuration
     * @param callback Callback function for incoming messages
     */
    MqttHandler(const MqttConfig& config, MessageCallback callback);

    /**
     * @brief Destructor - ensures clean disconnection
     */
    ~MqttHandler();

    /**
     * @brief Connect to MQTT broker
     * @return true if connection initiated successfully
     */
    bool connect();

    /**
     * @brief Disconnect from MQTT broker
     */
    void disconnect();

    /**
     * @brief Publish message to MQTT topic
     * @param topic Topic to publish to
     * @param payload Message payload
     * @param qos Quality of Service (default: use config value)
     * @return true if publish initiated successfully
     */
    bool publish(const std::string& topic, const std::string& payload, int qos = -1);

    /**
     * @brief Check if client is connected
     * @return true if connected to broker
     */
    bool isConnected() const;

    /**
     * @brief Get connection statistics
     * @return String with connection info
     */
    std::string getStats() const;

private:
    MqttConfig config_;
    MessageCallback message_callback_;
    
    std::unique_ptr<mqtt::async_client> client_;
    mqtt::connect_options conn_opts_;
    std::unique_ptr<ActionListener> sub_listener_;
    std::unique_ptr<Callback> callback_;
    
    std::atomic<uint64_t> messages_received_{0};
    std::atomic<uint64_t> messages_published_{0};
    std::atomic<bool> connected_{false};
};

} // namespace uwb_bridge

#endif // MQTT_HANDLER_HPP

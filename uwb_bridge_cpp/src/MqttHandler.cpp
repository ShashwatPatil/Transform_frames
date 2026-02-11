#include "MqttHandler.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

namespace uwb_bridge {

// ActionListener Implementation
void ActionListener::on_failure(const mqtt::token& tok) {
    spdlog::error("MQTT action '{}' failed", name_);
    if (tok.get_message_id() != 0) {
        spdlog::error("  Message ID: {}", tok.get_message_id());
    }
}

void ActionListener::on_success(const mqtt::token& tok) {
    spdlog::debug("MQTT action '{}' succeeded", name_);
    if (tok.get_message_id() != 0) {
        spdlog::debug("  Message ID: {}", tok.get_message_id());
    }
}

// Callback Implementation
Callback::Callback(mqtt::async_client& client,
                   mqtt::connect_options& conn_opts,
                   ActionListener& sub_listener,
                   const std::string& source_topic,
                   int qos,
                   MessageCallback callback)
    : client_(client),
      conn_opts_(conn_opts),
      sub_listener_(sub_listener),
      source_topic_(source_topic),
      qos_(qos),
      message_callback_(std::move(callback)) {}

void Callback::reconnect() {
    std::this_thread::sleep_for(std::chrono::milliseconds(RECONNECT_DELAY_MS));
    
    if (reconnect_attempts_ >= MAX_RECONNECT_ATTEMPTS) {
        spdlog::critical("Max reconnection attempts ({}) reached. Giving up.", MAX_RECONNECT_ATTEMPTS);
        return;
    }

    reconnect_attempts_++;
    spdlog::warn("Attempting to reconnect (attempt {}/{})...", 
                 reconnect_attempts_.load(), MAX_RECONNECT_ATTEMPTS);

    try {
        client_.connect(conn_opts_, nullptr, *this);
    } catch (const mqtt::exception& exc) {
        spdlog::error("Reconnection failed: {}", exc.what());
    }
}

void Callback::connection_lost(const std::string& cause) {
    spdlog::error("Connection lost!");
    if (!cause.empty()) {
        spdlog::error("  Cause: {}", cause);
    }
    
    spdlog::info("Attempting to reconnect...");
    reconnect();
}

void Callback::message_arrived(mqtt::const_message_ptr msg) {
    try {
        std::string topic = msg->get_topic();
        std::string payload = msg->to_string();
        
        spdlog::debug("Message arrived - Topic: {}, Payload size: {} bytes", 
                     topic, payload.size());
        
        // Call the user's callback function
        if (message_callback_) {
            spdlog::debug("Invoking user message callback...");
            message_callback_(topic, payload);
        } else {
            spdlog::error("Message callback is NULL! Cannot process message.");
        }
    } catch (const std::exception& e) {
        spdlog::error("Error processing message: {}", e.what());
    }
}

void Callback::delivery_complete(mqtt::delivery_token_ptr token) {
    spdlog::trace("Delivery complete for message ID: {}", token->get_message_id());
}

void Callback::on_failure(const mqtt::token& tok) {
    spdlog::error("Connection attempt failed");
    reconnect();
}

void Callback::on_success(const mqtt::token& tok) {
    spdlog::info("Successfully reconnected!");
    reconnect_attempts_ = 0;
    
    // Resubscribe to topic only if not empty
    if (source_topic_.empty()) {
        spdlog::info("No topic to resubscribe - publish-only mode");
        return;
    }
    
    // Resubscribe to topic
    try {
        client_.subscribe(source_topic_, qos_, nullptr, sub_listener_);
        spdlog::info("Resubscribed to topic: {}", source_topic_);
    } catch (const mqtt::exception& exc) {
        spdlog::error("Failed to resubscribe: {}", exc.what());
    }
}

// MqttHandler Implementation
MqttHandler::MqttHandler(const MqttConfig& config, MessageCallback callback)
    : config_(config), message_callback_(std::move(callback)) {
    
    spdlog::info("Initializing MQTT Handler");
    spdlog::info("  Broker: {}", config_.broker_address);
    spdlog::info("  Client ID: {}", config_.client_id);
    spdlog::info("  Source Topic: {}", config_.source_topic);
    spdlog::info("  Destination Prefix: {}", config_.dest_topic_prefix);

    // Build server URI with WebSocket support
    std::string server_uri;
    if (config_.use_websockets) {
        // WebSocket mode: construct ws:// or wss:// URI
        std::string protocol = config_.use_ssl ? "wss://" : "ws://";
        std::string ws_path = config_.ws_path.empty() ? "/mqtt" : config_.ws_path;
        
        // Remove any protocol prefix from broker_address if present
        std::string broker = config_.broker_address;
        size_t proto_pos = broker.find("://");
        if (proto_pos != std::string::npos) {
            broker = broker.substr(proto_pos + 3);
        }
        
        // Build URI: wss://host:port/path
        if (config_.port > 0) {
            server_uri = protocol + broker + ":" + std::to_string(config_.port) + ws_path;
        } else {
            server_uri = protocol + broker + ws_path;
        }
        spdlog::info("WebSocket mode: {}", server_uri);
    } else {
        // Standard MQTT: use broker_address as-is or construct tcp:// URI
        server_uri = config_.broker_address;
        if (server_uri.find("://") == std::string::npos) {
            // No protocol specified, add tcp:// or ssl://
            std::string protocol = config_.use_ssl ? "ssl://" : "tcp://";
            if (config_.port > 0) {
                server_uri = protocol + server_uri + ":" + std::to_string(config_.port);
            } else {
                server_uri = protocol + server_uri;
            }
        }
        spdlog::info("Standard MQTT mode: {}", server_uri);
    }

    // Create MQTT client
    client_ = std::make_unique<mqtt::async_client>(
        server_uri,
        config_.client_id);

    // Configure connection options
    conn_opts_.set_keep_alive_interval(config_.keepalive_interval);
    conn_opts_.set_clean_session(config_.clean_session);
    conn_opts_.set_automatic_reconnect(true);

    // Set credentials if provided
    if (!config_.username.empty()) {
        conn_opts_.set_user_name(config_.username);
        if (!config_.password.empty()) {
            conn_opts_.set_password(config_.password);
        }
    }

    // SSL/TLS configuration
    if (config_.use_ssl) {
        mqtt::ssl_options ssl_opts;
        ssl_opts.set_trust_store("/etc/ssl/certs/ca-certificates.crt");
        conn_opts_.set_ssl(ssl_opts);
        spdlog::info("SSL/TLS enabled");
    }

    // Create action listener
    sub_listener_ = std::make_unique<ActionListener>("subscription");

    // Create callback handler
    callback_ = std::make_unique<Callback>(
        *client_,
        conn_opts_,
        *sub_listener_,
        config_.source_topic,
        config_.qos,
        message_callback_);

    client_->set_callback(*callback_);
}

MqttHandler::~MqttHandler() {
    try {
        if (client_ && client_->is_connected()) {
            spdlog::info("Disconnecting MQTT client...");
            disconnect();
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in MqttHandler destructor: {}", e.what());
    }
}

bool MqttHandler::connect() {
    try {
        spdlog::info("Connecting to MQTT broker: {}", config_.broker_address);
        
        auto tok = client_->connect(conn_opts_);
        tok->wait();  // Wait for connection to complete
        
        spdlog::info("Connected to MQTT broker successfully");
        
        // Subscribe to source topic only if it's not empty (for publish-only handlers)
        if (!config_.source_topic.empty()) {
            spdlog::info("Subscribing to topic: {} (QoS {})", 
                         config_.source_topic, config_.qos);
            
            client_->subscribe(config_.source_topic, config_.qos, nullptr, *sub_listener_);
            
            spdlog::info("Subscribed successfully");
        } else {
            spdlog::info("No source topic configured - publish-only mode");
        }
        
        connected_ = true;
        
        return true;
        
    } catch (const mqtt::exception& e) {
        spdlog::error("MQTT connection failed: {}", e.what());
        connected_ = false;
        return false;
    }
}

void MqttHandler::disconnect() {
    try {
        if (client_ && client_->is_connected()) {
            spdlog::info("Disconnecting from MQTT broker...");
            client_->disconnect()->wait();
            connected_ = false;
            spdlog::info("Disconnected successfully");
        }
    } catch (const mqtt::exception& e) {
        spdlog::error("Error during disconnect: {}", e.what());
    }
}

bool MqttHandler::publish(const std::string& topic, const std::string& payload, int qos) {
    spdlog::debug("MqttHandler::publish() called - topic: {}, payload size: {}", topic, payload.size());
    
    if (!client_->is_connected()) {
        spdlog::warn("Cannot publish - not connected to broker");
        return false;
    }

    try {
        int actual_qos = (qos >= 0) ? qos : config_.qos;
        
        spdlog::debug("Creating MQTT message...");
        mqtt::message_ptr pubmsg = mqtt::make_message(topic, payload);
        pubmsg->set_qos(actual_qos);
        
        spdlog::debug("Calling client_->publish() (async, no wait)...");
        // Use async publish without wait() to avoid deadlock in callback thread
        auto token = client_->publish(pubmsg);
        
        spdlog::debug("Publish initiated (token created)");
        messages_published_++;
        spdlog::trace("Published to {}: {} bytes", topic, payload.size());
        
        return true;
        
    } catch (const mqtt::exception& e) {
        spdlog::error("Failed to publish message: {}", e.what());
        return false;
    }
}

bool MqttHandler::isConnected() const {
    return client_ && client_->is_connected();
}

std::string MqttHandler::getStats() const {
    std::ostringstream oss;
    oss << "MQTT Stats:\n"
        << "  Connected: " << (isConnected() ? "Yes" : "No") << "\n"
        << "  Messages Received: " << messages_received_.load() << "\n"
        << "  Messages Published: " << messages_published_.load();
    return oss.str();
}

} // namespace uwb_bridge

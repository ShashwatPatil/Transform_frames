#include "BridgeCore.hpp"
#include "ConfigLoader.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <csignal>
#include <memory>
#include <thread>
#include <chrono>

// Global pointer for signal handling
std::unique_ptr<uwb_bridge::BridgeCore> g_bridge;
std::atomic<bool> g_shutdown_requested{false};

/**
 * @brief Signal handler for graceful shutdown
 */
void signalHandler(int signum) {
    spdlog::warn("Received signal {}, initiating graceful shutdown...", signum);
    g_shutdown_requested = true;
    
    if (g_bridge) {
        g_bridge->stop();
    }
}

/**
 * @brief Setup logging based on configuration
 */
void setupLogging(const uwb_bridge::AppConfig& config) {
    std::vector<spdlog::sink_ptr> sinks;

    // Always add console sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::from_str(config.log_level));
    sinks.push_back(console_sink);

    // Add file sink if specified
    if (!config.log_file.empty()) {
        try {
            size_t max_size = config.log_rotation_size_mb * 1024 * 1024;
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config.log_file, max_size, config.log_rotation_count);
            file_sink->set_level(spdlog::level::from_str(config.log_level));
            sinks.push_back(file_sink);
        } catch (const spdlog::spdlog_ex& e) {
            spdlog::error("Failed to create log file: {}", e.what());
        }
    }

    // Create and register logger
    auto logger = std::make_shared<spdlog::logger>("uwb_bridge", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::from_str(config.log_level));
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
    spdlog::set_default_logger(logger);

    spdlog::info("Logging initialized - Level: {}", config.log_level);
    if (!config.log_file.empty()) {
        spdlog::info("Logging to file: {}", config.log_file);
    }
}

/**
 * @brief Print application banner
 */
void printBanner() {
    spdlog::info("=================================================");
    spdlog::info("  UWB MQTT Bridge Service");
    spdlog::info("  Version: 1.0.0");
    spdlog::info("  High-Performance C++ Edition");
    spdlog::info("=================================================");
}

/**
 * @brief Print usage information
 */
void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -c, --config <file>    Configuration file path (default: config/app_config.json)\n";
    std::cout << "  -h, --help             Show this help message\n";
    std::cout << "  -v, --version          Show version information\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " -c /etc/uwb_bridge/config.json\n";
    std::cout << "  " << program_name << " --config ./my_config.json\n";
    std::cout << "\n";
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    // Default configuration path
    std::string config_path = "config/app_config.json";

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "UWB Bridge v1.0.0\n";
            return 0;
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_path = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << "\n\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    try {
        // Load configuration
        spdlog::info("Loading configuration from: {}", config_path);
        auto config = uwb_bridge::ConfigLoader::loadFromFile(config_path);

        // Setup logging with loaded configuration
        setupLogging(config);

        // Print banner
        printBanner();

        // Register signal handlers for graceful shutdown
        std::signal(SIGINT, signalHandler);   // Ctrl+C
        std::signal(SIGTERM, signalHandler);  // systemctl stop

        // Create and initialize bridge
        spdlog::info("Creating UWB Bridge...");
        g_bridge = std::make_unique<uwb_bridge::BridgeCore>(config);

        if (!g_bridge->initialize()) {
            spdlog::critical("Failed to initialize bridge");
            return 1;
        }

        // Start bridge service
        if (!g_bridge->start()) {
            spdlog::critical("Failed to start bridge");
            g_bridge.reset();  // Clean shutdown before exit
            return 1;
        }

        spdlog::info("UWB Bridge running. Press Ctrl+C to stop.");

        // Main loop - keep service alive
        while (!g_shutdown_requested && g_bridge->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Optionally print stats periodically (every 60 seconds)
            static int stats_counter = 0;
            if (++stats_counter >= 60) {
                g_bridge->printStats();
                stats_counter = 0;
            }
        }

        // Graceful shutdown
        spdlog::info("Shutting down...");
        g_bridge->stop();
        g_bridge.reset();

        spdlog::info("Shutdown complete. Goodbye!");
        return 0;

    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        return 1;
    } catch (...) {
        spdlog::critical("Unknown fatal error occurred");
        return 1;
    }
}

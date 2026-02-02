#include "FloorplanTransformer.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>

using namespace uwb_transform;

// Helper function to print test results
void printTestResult(const std::string& test_name, 
                     const Eigen::Vector2d& result,
                     const Eigen::Vector2d& expected,
                     double tolerance = 0.01) {
    double error = (result - expected).norm();
    bool passed = error < tolerance;
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n" << test_name << ":\n";
    std::cout << "  Result:   (" << result(0) << ", " << result(1) << ")\n";
    std::cout << "  Expected: (" << expected(0) << ", " << expected(1) << ")\n";
    std::cout << "  Error:    " << error << " mm\n";
    std::cout << "  Status:   " << (passed ? "✓ PASSED" : "✗ FAILED") << "\n";
}

int main(int argc, char** argv) {
    try {
        std::cout << "=================================================\n";
        std::cout << "  UWB-to-Floorplan Coordinate Transformer Test\n";
        std::cout << "=================================================\n";

        // Determine config file path
        std::string config_path = "config/transform_config.json";
        if (argc > 1) {
            config_path = argv[1];
        }

        std::cout << "\nLoading configuration from: " << config_path << "\n";

        // Load transformer from config file
        FloorplanTransformer transformer = FloorplanTransformer::fromConfigFile(config_path);

        // Display configuration
        const auto& config = transformer.getConfig();
        std::cout << "\nConfiguration loaded:\n";
        std::cout << "  Origin:        (" << config.origin_x << ", " << config.origin_y << ") mm\n";
        std::cout << "  Scale:         " << config.scale << " px/mm\n";
        std::cout << "  Rotation:      " << config.rotation_rad << " rad ("
                  << (config.rotation_rad * 180.0 / M_PI) << "°)\n";
        std::cout << "  X Flipped:     " << (config.x_flipped ? "Yes" : "No") << "\n";
        std::cout << "  Y Flipped:     " << (config.y_flipped ? "Yes" : "No") << "\n";

        std::cout << "\n=================================================\n";
        std::cout << "  Test 1: Forward Transform (UWB -> Pixel)\n";
        std::cout << "=================================================\n";

        // Test point from Python script
        double uwb_test_x = 4396.0;  // mm
        double uwb_test_y = 17537.0; // mm

        std::cout << "\nInput UWB coordinates: (" << uwb_test_x << ", " 
                  << uwb_test_y << ") mm\n";

        Eigen::Vector2d pixel_coords = transformer.transformToPixel(uwb_test_x, uwb_test_y);

        std::cout << "Output Floorplan coordinates: (" 
                  << pixel_coords(0) << ", " 
                  << pixel_coords(1) << ") meters\n";

        std::cout << "\n=================================================\n";
        std::cout << "  Test 2: Inverse Transform (Pixel -> UWB)\n";
        std::cout << "=================================================\n";

        std::cout << "\nInput Floorplan coordinates: (" 
                  << pixel_coords(0) << ", " 
                  << pixel_coords(1) << ") meters\n";

        Eigen::Vector2d uwb_back = transformer.transformToUWB(
            pixel_coords(0), pixel_coords(1));

        std::cout << "Output UWB coordinates: (" 
                  << uwb_back(0) << ", " 
                  << uwb_back(1) << ") mm\n";

        // Verify round-trip accuracy
        Eigen::Vector2d expected(uwb_test_x, uwb_test_y);
        printTestResult("Round-trip Accuracy Test", uwb_back, expected, 0.01);

        std::cout << "\n=================================================\n";
        std::cout << "  Test 3: Multiple Test Points\n";
        std::cout << "=================================================\n";

        struct TestPoint {
            double uwb_x, uwb_y;
            std::string description;
        };

        std::vector<TestPoint> test_points = {
            {0.0, 0.0, "Origin"},
            {10000.0, 10000.0, "Point (10m, 10m)"},
            {5000.0, 15000.0, "Point (5m, 15m)"},
            {config.origin_x, config.origin_y, "Image Origin"}
        };

        for (const auto& point : test_points) {
            std::cout << "\n" << point.description << " - UWB: (" 
                      << point.uwb_x << ", " << point.uwb_y << ") mm\n";
            
            auto pixel = transformer.transformToPixel(point.uwb_x, point.uwb_y);
            std::cout << "  -> Floorplan: (" << pixel(0) << ", " << pixel(1) << ") m\n";
            
            auto uwb_verify = transformer.transformToUWB(pixel(0), pixel(1));
            double error = std::sqrt(
                std::pow(uwb_verify(0) - point.uwb_x, 2) + 
                std::pow(uwb_verify(1) - point.uwb_y, 2)
            );
            std::cout << "  -> Round-trip error: " << error << " mm "
                      << (error < 0.01 ? "✓" : "✗") << "\n";
        }

        std::cout << "\n=================================================\n";
        std::cout << "  Test 4: Performance Benchmark\n";
        std::cout << "=================================================\n";

        const int iterations = 1000000;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            transformer.transformToPixel(4396.0 + i * 0.001, 17537.0 + i * 0.001);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double avg_time = static_cast<double>(duration.count()) / iterations;
        std::cout << "\nPerformed " << iterations << " transformations in "
                  << duration.count() / 1000.0 << " ms\n";
        std::cout << "Average time per transformation: " << avg_time << " μs\n";
        std::cout << "Throughput: " << (1000000.0 / avg_time) << " transforms/second\n";

        std::cout << "\n=================================================\n";
        std::cout << "  All Tests Completed Successfully!\n";
        std::cout << "=================================================\n\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << "\n\n";
        return 1;
    }
}

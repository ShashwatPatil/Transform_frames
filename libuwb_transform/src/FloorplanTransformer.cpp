#include "FloorplanTransformer.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <cmath>

using json = nlohmann::json;

namespace uwb_transform {

FloorplanTransformer::FloorplanTransformer(const TransformConfig& config)
    : config_(config) {
    transform_matrix_ = calculateTransformMatrix();
    inverse_matrix_ = transform_matrix_.inverse();
}

FloorplanTransformer FloorplanTransformer::fromConfigFile(const std::string& config_path) {
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + config_path);
    }

    json j;
    try {
        config_file >> j;
    } catch (const json::parse_error& e) {
        throw std::runtime_error("Failed to parse JSON config: " + std::string(e.what()));
    }

    TransformConfig config;
    
    // Load required parameters
    if (!j.contains("origin_x") || !j.contains("origin_y") || 
        !j.contains("scale") || !j.contains("rotation_rad")) {
        throw std::runtime_error("Missing required configuration parameters");
    }

    config.origin_x = j["origin_x"].get<double>();
    config.origin_y = j["origin_y"].get<double>();
    config.scale = j["scale"].get<double>();
    config.rotation_rad = j["rotation_rad"].get<double>();
    
    // Optional parameters with defaults
    config.x_flipped = j.value("x_flipped", false);
    config.y_flipped = j.value("y_flipped", false);

    return FloorplanTransformer(config);
}

Eigen::Matrix3d FloorplanTransformer::calculateTransformMatrix() const {
    // --- 1. Translation Matrix (T) ---
    // Shifts points so the Image Origin becomes (0,0)
    Eigen::Matrix3d T = Eigen::Matrix3d::Identity();
    T(0, 2) = -config_.origin_x;
    T(1, 2) = -config_.origin_y;

    // --- 2. Rotation Matrix (R) ---
    // Rotates the axes to align UWB with Image
    // Note: For pozyx we have to negate the rotation. IDK why (might change if we switch to other RTLS)
    double theta = -config_.rotation_rad;
    double c = std::cos(theta);
    double s = std::sin(theta);
    
    Eigen::Matrix3d R = Eigen::Matrix3d::Identity();
    R(0, 0) = c;
    R(0, 1) = -s;
    R(1, 0) = s;
    R(1, 1) = c;

    // --- 3. Scale Matrix (S) ---
    // Handles Scaling (px/unit) and Axis Flipping
    double scale_x = config_.scale * (config_.x_flipped ? -1.0 : 1.0);
    double scale_y = config_.scale * (config_.y_flipped ? -1.0 : 1.0);
    
    Eigen::Matrix3d S = Eigen::Matrix3d::Identity();
    S(0, 0) = scale_x;
    S(1, 1) = scale_y;

    // --- 4. Combine (Order: S * R * T) ---
    // We apply T first (rightmost), then R, then S
    return S * R * T;
}

Eigen::Vector2d FloorplanTransformer::transformToPixel(double uwb_x, double uwb_y) const {
    // Create homogeneous coordinate vector
    Eigen::Vector3d uwb_point(uwb_x, uwb_y, 1.0);
    
    // Apply transformation
    Eigen::Vector3d pixel_point = transform_matrix_ * uwb_point;
    
    // Convert to meters: res[0], res[1] is in pixels
    // Divide by scale to get it in mm, then by 1000 to get meters
    double meter_x = (pixel_point(0) / config_.scale) / 1000.0;
    double meter_y = (pixel_point(1) / config_.scale) / 1000.0;
    
    return Eigen::Vector2d(meter_x, meter_y);
}

Eigen::Vector2d FloorplanTransformer::transformToUWB(double meter_x, double meter_y) const {
    // Convert meters to pixels for reverse transform
    // meters * 1000 = mm, mm * scale = pixels
    double pixel_x = meter_x * 1000.0 * config_.scale;
    double pixel_y = meter_y * 1000.0 * config_.scale;
    
    // Create homogeneous coordinate vector
    Eigen::Vector3d pixel_point(pixel_x, pixel_y, 1.0);
    
    // Apply inverse transformation
    Eigen::Vector3d uwb_point = inverse_matrix_ * pixel_point;
    
    // Return UWB coordinates (x, y) in mm
    return Eigen::Vector2d(uwb_point(0), uwb_point(1));
}

} // namespace uwb_transform

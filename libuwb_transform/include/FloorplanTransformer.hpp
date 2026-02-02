#ifndef FLOORPLAN_TRANSFORMER_HPP
#define FLOORPLAN_TRANSFORMER_HPP

#include <Eigen/Dense>
#include <string>

namespace uwb_transform {

/**
 * @brief Configuration structure for FloorplanTransformer
 * 
 * This structure holds all parameters needed to configure the coordinate
 * transformation between UWB global frame and floorplan pixel frame.
 */
struct TransformConfig {
    double origin_x;      ///< X location of image top-left corner in UWB frame (mm)
    double origin_y;      ///< Y location of image top-left corner in UWB frame (mm)
    double scale;         ///< Pixels per UWB unit (pixels/mm)
    double rotation_rad;  ///< Rotation of UWB frame in radians (counter-clockwise)
    bool x_flipped;       ///< True if UWB X axis opposes Image X axis
    bool y_flipped;       ///< True if UWB Y axis opposes Image Y axis

    TransformConfig()
        : origin_x(0.0), origin_y(0.0), scale(1.0), 
          rotation_rad(0.0), x_flipped(false), y_flipped(false) {}
};

/**
 * @brief High-performance coordinate transformer for UWB-to-Floorplan mapping
 * 
 * This class implements a 3x3 homogeneous transformation matrix that converts
 * between UWB global coordinates and floorplan pixel coordinates. The transformation
 * pipeline is: Translation -> Rotation -> Scale (with optional axis flipping).
 * 
 * Uses Eigen library for optimized matrix operations with SIMD support.
 * 
 * @note Thread-safe for read operations after construction
 */
class FloorplanTransformer {
public:
    /**
     * @brief Construct transformer from configuration structure
     * @param config Configuration parameters for the transformation
     */
    explicit FloorplanTransformer(const TransformConfig& config);

    /**
     * @brief Load configuration from JSON file and construct transformer
     * @param config_path Path to JSON configuration file
     * @return FloorplanTransformer instance
     * @throws std::runtime_error if file cannot be read or parsed
     */
    static FloorplanTransformer fromConfigFile(const std::string& config_path);

    /**
     * @brief Transform UWB coordinates to floorplan coordinates in meters
     * 
     * Applies the full transformation matrix to convert from UWB global frame (mm)
     * to floorplan coordinates in meters. This matches the Python implementation
     * which outputs meters for display/plotting purposes.
     * 
     * @param uwb_x X coordinate in UWB frame (mm)
     * @param uwb_y Y coordinate in UWB frame (mm)
     * @return Eigen::Vector2d Floorplan coordinates (x, y) in meters
     */
    Eigen::Vector2d transformToPixel(double uwb_x, double uwb_y) const;

    /**
     * @brief Transform floorplan coordinates (meters) back to UWB coordinates
     * 
     * Applies the inverse transformation matrix to convert from floorplan
     * coordinates (in meters) back to UWB global frame (in mm).
     * 
     * @param meter_x X coordinate in meters
     * @param meter_y Y coordinate in meters
     * @return Eigen::Vector2d UWB coordinates (x, y) in mm
     */
    Eigen::Vector2d transformToUWB(double meter_x, double meter_y) const;

    /**
     * @brief Get the forward transformation matrix
     * @return const Eigen::Matrix3d& The 3x3 transformation matrix
     */
    const Eigen::Matrix3d& getMatrix() const { return transform_matrix_; }

    /**
     * @brief Get the inverse transformation matrix
     * @return const Eigen::Matrix3d& The 3x3 inverse transformation matrix
     */
    const Eigen::Matrix3d& getInverseMatrix() const { return inverse_matrix_; }

    /**
     * @brief Get the current configuration
     * @return const TransformConfig& The configuration used by this transformer
     */
    const TransformConfig& getConfig() const { return config_; }

private:
    /**
     * @brief Calculate the transformation matrix from configuration
     * @return Eigen::Matrix3d The combined transformation matrix
     */
    Eigen::Matrix3d calculateTransformMatrix() const;

    TransformConfig config_;           ///< Stored configuration
    Eigen::Matrix3d transform_matrix_; ///< Forward transformation matrix
    Eigen::Matrix3d inverse_matrix_;   ///< Cached inverse matrix
};

} // namespace uwb_transform

#endif // FLOORPLAN_TRANSFORMER_HPP

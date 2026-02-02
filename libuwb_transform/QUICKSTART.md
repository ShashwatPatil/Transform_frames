# Quick Start Guide

## 5-Minute Setup

### 1. Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libeigen3-dev nlohmann-json3-dev
```

**macOS:**
```bash
brew install cmake eigen nlohmann-json
```

### 2. Build

```bash
./build.sh all
```

This will:
- Check all dependencies
- Build the library in Release mode
- Run the demo application with test cases

### 3. Customize Configuration

Edit `config/transform_config.json` with your RTLS parameters:

```json
{
  "origin_x": 23469.39,      // ← Your image origin X in UWB frame (mm)
  "origin_y": 30305.22,      // ← Your image origin Y in UWB frame (mm)
  "scale": 0.0414,           // ← Pixels per mm
  "rotation_rad": -0.4363,   // ← Rotation in radians
  "x_flipped": true,         // ← Is X-axis flipped?
  "y_flipped": true          // ← Is Y-axis flipped?
}
```

### 4. Test Your Configuration

```bash
./build.sh run
```

## How to Determine Configuration Parameters

### Finding `origin_x` and `origin_y`

These are the UWB coordinates of your floorplan image's **top-left corner**.

1. Place a tag at the top-left corner of your floorplan
2. Record the UWB X and Y coordinates (in mm)
3. Use these as `origin_x` and `origin_y`

### Finding `scale`

This is the conversion factor from millimeters to pixels.

```
scale = pixel_distance / real_world_distance_mm
```

**Example:**
- Measure a known distance on your floorplan (e.g., wall = 5000mm)
- Count pixels for that distance (e.g., 207 pixels)
- Calculate: `scale = 207 / 5000 = 0.0414`

### Finding `rotation_rad`

If your UWB coordinate system is rotated relative to the image:

```
rotation_rad = angle_in_degrees × (π / 180)
```

**Example:**
- UWB axes rotated 25° counter-clockwise relative to image
- Calculate: `rotation_rad = 25 × (3.14159 / 180) = 0.436`

Use negative values for clockwise rotation.

### Determining Flip Flags

**`x_flipped`**: Set to `true` if:
- Increasing UWB X moves LEFT in the image (instead of right)

**`y_flipped`**: Set to `true` if:
- Increasing UWB Y moves UP in the image (instead of down)

Most image coordinate systems have Y increasing downward, so if your UWB Y increases upward, set `y_flipped = true`.

## Usage in Your Code

### Basic Example

```cpp
#include "FloorplanTransformer.hpp"
#include <iostream>

int main() {
    // Load transformer
    auto transformer = uwb_transform::FloorplanTransformer::fromConfigFile(
        "config/transform_config.json");
    
    // Get tag position from UWB system (in mm)
    double uwb_x = 5420.0;
    double uwb_y = 18230.0;
    
    // Transform to floorplan coordinates in meters
    auto meters = transformer.transformToPixel(uwb_x, uwb_y);
    
    std::cout << "Tag at (" << meters(0) << ", " 
              << meters(1) << ") meters\n";
    
    // Use for display on map (convert meters to pixels if needed)
    // int pixel_x = meters(0) * pixels_per_meter;
    // cv::circle(image, cv::Point(pixel_x, pixel_y), 5, ...);
    
    return 0;
}
```

### Real-Time Tracking Loop

```cpp
#include "FloorplanTransformer.hpp"
#include <vector>

struct TagPosition {
    int tag_id;
    double uwb_x, uwb_y;
};

void updateFloorplanDisplay(
    const std::vector<TagPosition>& tags,
    const uwb_transform::FloorplanTransformer& transformer) {
    
    for (const auto& tag : tags) {
        // Transform to pixels
        auto pixel = transformer.transformToPixel(tag.uwb_x, tag.uwb_y);
        
        // Update display (pseudo-code)
        // drawTagOnMap(tag.tag_id, pixel(0), pixel(1));
    }
}
```

## Common Issues

### "Config file not found"
- Verify path: `config/transform_config.json` exists
- Use absolute path if needed: `FloorplanTransformer::fromConfigFile("/full/path/to/config.json")`

### Coordinates seem flipped or rotated
- Check `x_flipped` and `y_flipped` settings
- Verify `rotation_rad` is in radians, not degrees
- Test with known points (corners of the room)

### Large transformation errors
- Confirm `origin_x`, `origin_y` are in millimeters
- Verify `scale` is pixels/mm (not mm/pixel)
- Check that rotation angle matches your setup

### Performance issues
- Ensure built in Release mode: `./build.sh build Release`
- Check compiler optimizations are enabled (`-O3 -march=native`)
- Library should achieve >10M transforms/second

## Next Steps

- **ROS Integration**: See examples in README for ROS 2 node template
- **Python Bindings**: Use pybind11 to create Python interface
- **Batch Processing**: Process multiple points in parallel for point clouds
- **Visualization**: Integrate with OpenCV for real-time display

## Testing Your Setup

Run the demo and verify:

1. **Round-trip accuracy** < 0.01mm (should see "✓ PASSED")
2. **Performance** > 1M transforms/second
3. **Known points** transform to expected pixel locations

```bash
./build.sh run
```

Look for output like:
```
Round-trip Accuracy Test:
  Result:   (4396.00, 17537.00)
  Expected: (4396.00, 17537.00)
  Error:    0.00 mm
  Status:   ✓ PASSED
```

If errors are > 0.1mm, recheck your configuration parameters.

## Support

For detailed documentation, see [README.md](README.md)

For issues or questions, please open a GitHub issue with:
- Your configuration file
- Expected vs actual results
- Output of `./build.sh run`

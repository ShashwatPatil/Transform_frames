#!/bin/bash

# UWB Transform Library - Quick Build Script
# This script automates the build process for the UWB Transform library

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}======================================${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}======================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

# Check dependencies
check_dependencies() {
    print_header "Checking Dependencies"
    
    MISSING_DEPS=0
    
    # Check CMake
    if command -v cmake &> /dev/null; then
        CMAKE_VERSION=$(cmake --version | head -n1 | awk '{print $3}')
        print_success "CMake found (version $CMAKE_VERSION)"
    else
        print_error "CMake not found"
        MISSING_DEPS=1
    fi
    
    # Check compiler
    if command -v g++ &> /dev/null; then
        GCC_VERSION=$(g++ --version | head -n1 | awk '{print $3}')
        print_success "g++ found (version $GCC_VERSION)"
    elif command -v clang++ &> /dev/null; then
        CLANG_VERSION=$(clang++ --version | head -n1 | awk '{print $4}')
        print_success "clang++ found (version $CLANG_VERSION)"
    else
        print_error "No C++ compiler found"
        MISSING_DEPS=1
    fi
    
    # Check Eigen3
    if pkg-config --exists eigen3 2>/dev/null; then
        EIGEN_VERSION=$(pkg-config --modversion eigen3)
        print_success "Eigen3 found (version $EIGEN_VERSION)"
    else
        print_warning "Eigen3 not found via pkg-config (may still be available)"
    fi
    
    # Check nlohmann-json
    if [ -f "/usr/include/nlohmann/json.hpp" ] || [ -f "/usr/local/include/nlohmann/json.hpp" ]; then
        print_success "nlohmann-json found"
    else
        print_warning "nlohmann-json not found in standard locations"
        echo "  You can install it with:"
        echo "    Ubuntu/Debian: sudo apt-get install nlohmann-json3-dev"
        echo "    macOS: brew install nlohmann-json"
    fi
    
    if [ $MISSING_DEPS -eq 1 ]; then
        print_error "Missing required dependencies"
        echo ""
        echo "Install dependencies:"
        echo "  Ubuntu/Debian: sudo apt-get install build-essential cmake libeigen3-dev nlohmann-json3-dev"
        echo "  macOS: brew install cmake eigen nlohmann-json"
        exit 1
    fi
    
    echo ""
}

# Build function
build_project() {
    print_header "Building Project"
    
    BUILD_TYPE=${1:-Release}
    BUILD_DIR="build"
    
    # Create build directory
    if [ -d "$BUILD_DIR" ]; then
        print_warning "Build directory exists, cleaning..."
        rm -rf "$BUILD_DIR"
    fi
    mkdir -p "$BUILD_DIR"
    
    cd "$BUILD_DIR"
    
    # Configure
    echo ""
    echo "Configuring with CMake (Build Type: $BUILD_TYPE)..."
    if cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" ..; then
        print_success "Configuration successful"
    else
        print_error "Configuration failed"
        exit 1
    fi
    
    # Build
    echo ""
    echo "Building..."
    NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    if make -j"$NPROC"; then
        print_success "Build successful"
    else
        print_error "Build failed"
        exit 1
    fi
    
    cd ..
    echo ""
}

# Run tests
run_demo() {
    print_header "Running Demo"
    
    if [ ! -f "build/uwb_transform_demo" ]; then
        print_error "Demo executable not found. Build first."
        exit 1
    fi
    
    if [ ! -f "config/transform_config.json" ]; then
        print_error "Config file not found: config/transform_config.json"
        exit 1
    fi
    
    echo ""
    ./build/uwb_transform_demo config/transform_config.json
}

# Install function
install_project() {
    print_header "Installing"
    
    if [ ! -d "build" ]; then
        print_error "Build directory not found. Build first."
        exit 1
    fi
    
    cd build
    if sudo make install; then
        print_success "Installation successful"
    else
        print_error "Installation failed"
        exit 1
    fi
    cd ..
    echo ""
}

# Main menu
show_usage() {
    echo "Usage: $0 [command] [options]"
    echo ""
    echo "Commands:"
    echo "  deps              Check dependencies"
    echo "  build [type]      Build project (type: Release|Debug, default: Release)"
    echo "  run               Run demo application"
    echo "  install           Install library system-wide (requires sudo)"
    echo "  clean             Remove build directory"
    echo "  all               Check deps, build, and run demo"
    echo "  help              Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 all                    # Full build and test"
    echo "  $0 build                  # Build in Release mode"
    echo "  $0 build Debug            # Build in Debug mode"
    echo "  $0 run                    # Run the demo"
    echo ""
}

# Clean function
clean_project() {
    print_header "Cleaning"
    
    if [ -d "build" ]; then
        rm -rf build
        print_success "Build directory removed"
    else
        print_warning "Build directory does not exist"
    fi
    echo ""
}

# Main script logic
main() {
    COMMAND=${1:-help}
    
    case $COMMAND in
        deps)
            check_dependencies
            ;;
        build)
            check_dependencies
            BUILD_TYPE=${2:-Release}
            build_project "$BUILD_TYPE"
            print_success "Build complete! Run './build.sh run' to test."
            ;;
        run)
            run_demo
            ;;
        install)
            install_project
            ;;
        clean)
            clean_project
            ;;
        all)
            check_dependencies
            build_project "Release"
            run_demo
            ;;
        help|--help|-h)
            show_usage
            ;;
        *)
            print_error "Unknown command: $COMMAND"
            echo ""
            show_usage
            exit 1
            ;;
    esac
}

# Run main function
main "$@"

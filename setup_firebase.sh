#!/bin/bash
# Firebase Firestore Migration Setup Script
# This script helps set up the Firebase C++ SDK and prepare for Firestore integration

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "Firebase Firestore Setup Script"
echo "========================================="
echo ""

# Configuration
FIREBASE_SDK_VERSION="11.0.0"
FIREBASE_SDK_URL="https://dl.google.com/firebase/sdk/cpp/firebase_cpp_sdk_${FIREBASE_SDK_VERSION}.zip"
INSTALL_DIR="/opt/firebase_cpp_sdk"
TEMP_DIR="/tmp/firebase_setup_$$"

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${YELLOW}Warning: Not running as root. You may need sudo for installation.${NC}"
    echo "Consider running: sudo $0"
    echo ""
fi

# Function to print status
print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_info() {
    echo -e "${YELLOW}[i]${NC} $1"
}

# Step 1: Check prerequisites
echo "Step 1: Checking prerequisites..."

if ! command -v wget &> /dev/null; then
    print_error "wget not found. Please install: sudo apt-get install wget"
    exit 1
fi
print_status "wget found"

if ! command -v unzip &> /dev/null; then
    print_error "unzip not found. Please install: sudo apt-get install unzip"
    exit 1
fi
print_status "unzip found"

if ! command -v cmake &> /dev/null; then
    print_error "cmake not found. Please install: sudo apt-get install cmake"
    exit 1
fi
print_status "cmake found"

echo ""

# Step 2: Check if Firebase SDK already exists
if [ -d "$INSTALL_DIR" ]; then
    print_info "Firebase SDK already exists at $INSTALL_DIR"
    read -p "Do you want to reinstall? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Skipping Firebase SDK installation"
    else
        print_info "Removing existing installation..."
        rm -rf "$INSTALL_DIR"
    fi
fi

# Step 3: Download Firebase C++ SDK
if [ ! -d "$INSTALL_DIR" ]; then
    echo "Step 2: Downloading Firebase C++ SDK..."
    mkdir -p "$TEMP_DIR"
    cd "$TEMP_DIR"
    
    print_info "Downloading Firebase C++ SDK v${FIREBASE_SDK_VERSION}..."
    if wget -q --show-progress "$FIREBASE_SDK_URL" -O firebase_cpp_sdk.zip; then
        print_status "Download complete"
    else
        print_error "Failed to download Firebase SDK"
        rm -rf "$TEMP_DIR"
        exit 1
    fi
    
    echo ""
    echo "Step 3: Extracting Firebase SDK..."
    if unzip -q firebase_cpp_sdk.zip; then
        print_status "Extraction complete"
    else
        print_error "Failed to extract Firebase SDK"
        rm -rf "$TEMP_DIR"
        exit 1
    fi
    
    echo ""
    echo "Step 4: Installing Firebase SDK..."
    mkdir -p "$INSTALL_DIR"
    cp -r firebase_cpp_sdk/* "$INSTALL_DIR/"
    print_status "Firebase SDK installed to $INSTALL_DIR"
    
    # Cleanup
    cd /
    rm -rf "$TEMP_DIR"
    print_status "Cleaned up temporary files"
else
    print_status "Firebase SDK already installed at $INSTALL_DIR"
fi

echo ""

# Step 5: Verify installation
echo "Step 5: Verifying installation..."

if [ -f "$INSTALL_DIR/libs/linux/x86_64/libfirebase_app.a" ]; then
    print_status "firebase_app library found"
else
    print_error "firebase_app library not found"
fi

if [ -f "$INSTALL_DIR/libs/linux/x86_64/libfirebase_firestore.a" ]; then
    print_status "firebase_firestore library found"
else
    print_error "firebase_firestore library not found"
fi

if [ -d "$INSTALL_DIR/include" ]; then
    print_status "Include files found"
else
    print_error "Include files not found"
fi

echo ""

# Step 6: Environment setup
echo "Step 6: Environment setup..."

print_info "Add these to your environment (e.g., ~/.bashrc):"
echo ""
echo "export FIREBASE_CPP_SDK_DIR=\"$INSTALL_DIR\""
echo "export LD_LIBRARY_PATH=\"$INSTALL_DIR/libs/linux/x86_64:\$LD_LIBRARY_PATH\""
echo "export FIREBASE_PROJECT_ID=\"your-firebase-project-id\""
echo "export FIREBASE_API_KEY=\"your-firebase-api-key\""
echo ""

# Step 7: Build instructions
echo "Step 7: Build instructions..."
echo ""
print_info "To build libuwb_transform with Firebase:"
echo "  cd Transform_frames/libuwb_transform/build"
echo "  cmake .. -DFIREBASE_CPP_SDK_DIR=$INSTALL_DIR"
echo "  make -j\$(nproc)"
echo ""

print_info "To build uwb_bridge_cpp with Firebase:"
echo "  cd Transform_frames/uwb_bridge_cpp/build"
echo "  cmake .. -DFIREBASE_CPP_SDK_DIR=$INSTALL_DIR"
echo "  make -j\$(nproc)"
echo ""

# Step 8: Firestore setup reminder
echo "Step 8: Firestore setup..."
echo ""
print_info "Don't forget to:"
echo "  1. Create a Firebase project at https://console.firebase.google.com/"
echo "  2. Enable Firestore Database"
echo "  3. Create the configs collection with app_config and transform_config documents"
echo "  4. Set up security rules"
echo "  5. Get your PROJECT_ID and API_KEY from Project Settings"
echo ""

print_info "See FIRESTORE_SETUP.md for detailed Firestore configuration"
echo ""

# Step 9: Test build (optional)
echo "========================================="
echo "Setup complete!"
echo "========================================="
echo ""

read -p "Do you want to test build the projects now? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    print_info "Testing libuwb_transform build..."
    
    if [ -d "Transform_frames/libuwb_transform" ]; then
        cd Transform_frames/libuwb_transform
        mkdir -p build
        cd build
        
        if cmake .. -DFIREBASE_CPP_SDK_DIR="$INSTALL_DIR" && make -j$(nproc); then
            print_status "libuwb_transform build successful"
        else
            print_error "libuwb_transform build failed"
        fi
        cd ../../..
    else
        print_info "libuwb_transform directory not found, skipping"
    fi
    
    echo ""
    print_info "Testing uwb_bridge_cpp build..."
    
    if [ -d "Transform_frames/uwb_bridge_cpp" ]; then
        cd Transform_frames/uwb_bridge_cpp
        mkdir -p build
        cd build
        
        if cmake .. -DFIREBASE_CPP_SDK_DIR="$INSTALL_DIR" && make -j$(nproc); then
            print_status "uwb_bridge_cpp build successful"
        else
            print_error "uwb_bridge_cpp build failed"
        fi
        cd ../../..
    else
        print_info "uwb_bridge_cpp directory not found, skipping"
    fi
fi

echo ""
print_status "Setup script completed successfully!"
echo ""
print_info "Next steps:"
echo "  1. Set environment variables (see Step 6 above)"
echo "  2. Configure Firestore (see FIRESTORE_SETUP.md)"
echo "  3. Run with: ./bin/uwb_bridge --firestore"
echo ""

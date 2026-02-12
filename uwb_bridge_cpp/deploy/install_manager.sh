#!/bin/bash
# Installation script for UWB Bridge Manager
# This script installs the bridge manager as a systemd service

set -e  # Exit on error

echo "================================================"
echo "  UWB Bridge Manager - Installation Script"
echo "================================================"
echo ""

# Configuration
INSTALL_DIR="/opt/uwb_bridge"
SERVICE_NAME="uwb-bridge-manager"
USER="uwb"
GROUP="uwb"

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Error: This script must be run as root (use sudo)"
    exit 1
fi

# Check if Python 3 is installed
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 is not installed"
    echo "Install with: sudo apt install python3 python3-pip"
    exit 1
fi

# Check if C++ executable exists
if [ ! -f "./bin/uwb_bridge" ]; then
    echo "Error: C++ executable not found at ./bin/uwb_bridge"
    echo "Build it first with: ./build.sh"
    exit 1
fi

# Check if Firebase credentials exist
if [ ! -f "./nova_database_cred.json" ]; then
    echo "Error: Firebase credentials not found at ./nova_database_cred.json"
    echo "Please place your service account JSON file at this location"
    exit 1
fi

echo "✓ Prerequisites check passed"
echo ""

# Create user and group if they don't exist
if ! id "$USER" &>/dev/null; then
    echo "Creating system user: $USER"
    useradd -r -s /bin/false "$USER"
    echo "✓ User created"
else
    echo "✓ User $USER already exists"
fi

# Create installation directory
echo "Creating installation directory: $INSTALL_DIR"
mkdir -p "$INSTALL_DIR"
mkdir -p "$INSTALL_DIR/config"
mkdir -p "$INSTALL_DIR/logs"
mkdir -p "$INSTALL_DIR/bin"

# Copy files
echo "Copying files..."
cp bridge_manager.py "$INSTALL_DIR/"
cp requirements.txt "$INSTALL_DIR/"
cp nova_database_cred.json "$INSTALL_DIR/"
cp -r bin/* "$INSTALL_DIR/bin/"
cp -r include "$INSTALL_DIR/" 2>/dev/null || true
cp -r config "$INSTALL_DIR/" 2>/dev/null || true

# Copy documentation
cp BRIDGE_MANAGER_README.md "$INSTALL_DIR/" 2>/dev/null || true
cp QUICKSTART_BRIDGE_MANAGER.md "$INSTALL_DIR/" 2>/dev/null || true

echo "✓ Files copied"

# Set ownership
echo "Setting permissions..."
chown -R "$USER:$GROUP" "$INSTALL_DIR"
chmod 755 "$INSTALL_DIR/bin/uwb_bridge"
chmod 600 "$INSTALL_DIR/nova_database_cred.json"
chmod 755 "$INSTALL_DIR/bridge_manager.py"
echo "✓ Permissions set"

# Install Python dependencies
echo "Installing Python dependencies..."
pip3 install -r "$INSTALL_DIR/requirements.txt" --quiet
echo "✓ Python dependencies installed"

# Copy systemd service file
echo "Installing systemd service..."
cp deploy/uwb-bridge-manager.service /etc/systemd/system/
chmod 644 /etc/systemd/system/uwb-bridge-manager.service

# Reload systemd
systemctl daemon-reload
echo "✓ Systemd service installed"

# Enable service
echo "Enabling service to start on boot..."
systemctl enable "$SERVICE_NAME"
echo "✓ Service enabled"

echo ""
echo "================================================"
echo "  Installation Complete!"
echo "================================================"
echo ""
echo "Next steps:"
echo ""
echo "1. Start the service:"
echo "   sudo systemctl start $SERVICE_NAME"
echo ""
echo "2. Check status:"
echo "   sudo systemctl status $SERVICE_NAME"
echo ""
echo "3. View logs:"
echo "   sudo journalctl -u $SERVICE_NAME -f"
echo ""
echo "4. Stop the service:"
echo "   sudo systemctl stop $SERVICE_NAME"
echo ""
echo "Installation directory: $INSTALL_DIR"
echo ""

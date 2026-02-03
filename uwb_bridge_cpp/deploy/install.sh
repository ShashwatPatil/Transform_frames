#!/bin/bash
# Installation script for UWB Bridge Service

set -e  # Exit on error

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
SERVICE_USER="uwb"
SERVICE_GROUP="uwb"
INSTALL_DIR="/opt/uwb_bridge"
CONFIG_DIR="/etc/uwb_bridge"
LOG_DIR="/var/log/uwb_bridge"
SERVICE_FILE="uwb-bridge.service"

echo -e "${GREEN}==================================================${NC}"
echo -e "${GREEN}  UWB Bridge Service Installation${NC}"
echo -e "${GREEN}==================================================${NC}"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}Error: This script must be run as root${NC}"
    echo "Usage: sudo ./install.sh"
    exit 1
fi

# Check if build exists
if [ ! -f "../build/bin/uwb_bridge" ]; then
    echo -e "${RED}Error: Binary not found. Please build the project first:${NC}"
    echo "  cd .."
    echo "  mkdir build && cd build"
    echo "  cmake -DCMAKE_BUILD_TYPE=Release .."
    echo "  make -j\$(nproc)"
    exit 1
fi

# Create service user and group
echo -e "\n${YELLOW}Creating service user and group...${NC}"
if ! id "$SERVICE_USER" &>/dev/null; then
    useradd --system --no-create-home --shell /bin/false "$SERVICE_USER"
    echo -e "${GREEN}✓ User '$SERVICE_USER' created${NC}"
else
    echo -e "${GREEN}✓ User '$SERVICE_USER' already exists${NC}"
fi

# Create directories
echo -e "\n${YELLOW}Creating directories...${NC}"
mkdir -p "$INSTALL_DIR/bin"
mkdir -p "$CONFIG_DIR"
mkdir -p "$LOG_DIR"
echo -e "${GREEN}✓ Directories created${NC}"

# Copy binary
echo -e "\n${YELLOW}Installing binary...${NC}"
cp ../build/bin/uwb_bridge "$INSTALL_DIR/bin/"
chmod +x "$INSTALL_DIR/bin/uwb_bridge"
echo -e "${GREEN}✓ Binary installed to $INSTALL_DIR/bin/${NC}"

# Copy configuration
echo -e "\n${YELLOW}Installing configuration...${NC}"
if [ ! -f "$CONFIG_DIR/app_config.json" ]; then
    cp ../config/app_config.json "$CONFIG_DIR/"
    echo -e "${GREEN}✓ Configuration installed to $CONFIG_DIR/${NC}"
else
    echo -e "${YELLOW}⚠ Configuration already exists, skipping${NC}"
    echo -e "${YELLOW}  Backup: $CONFIG_DIR/app_config.json.bak${NC}"
    cp "$CONFIG_DIR/app_config.json" "$CONFIG_DIR/app_config.json.bak"
fi

# Set permissions
echo -e "\n${YELLOW}Setting permissions...${NC}"
chown -R "$SERVICE_USER:$SERVICE_GROUP" "$INSTALL_DIR"
chown -R "$SERVICE_USER:$SERVICE_GROUP" "$LOG_DIR"
chown root:root "$CONFIG_DIR/app_config.json"
chmod 644 "$CONFIG_DIR/app_config.json"
echo -e "${GREEN}✓ Permissions set${NC}"

# Install systemd service
echo -e "\n${YELLOW}Installing systemd service...${NC}"
cp "$SERVICE_FILE" "/etc/systemd/system/"
systemctl daemon-reload
echo -e "${GREEN}✓ Service installed${NC}"

# Completion message
echo -e "\n${GREEN}==================================================${NC}"
echo -e "${GREEN}  Installation Complete!${NC}"
echo -e "${GREEN}==================================================${NC}"
echo ""
echo "Configuration file: $CONFIG_DIR/app_config.json"
echo "Log file:           $LOG_DIR/uwb_bridge.log"
echo ""
echo "Next steps:"
echo "  1. Edit configuration: sudo nano $CONFIG_DIR/app_config.json"
echo "  2. Enable service:     sudo systemctl enable uwb-bridge"
echo "  3. Start service:      sudo systemctl start uwb-bridge"
echo "  4. Check status:       sudo systemctl status uwb-bridge"
echo "  5. View logs:          sudo journalctl -u uwb-bridge -f"
echo ""

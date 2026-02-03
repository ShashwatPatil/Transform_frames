#!/bin/bash
# Quick build script for UWB Bridge

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}==================================================${NC}"
echo -e "${BLUE}  Building UWB Bridge${NC}"
echo -e "${BLUE}==================================================${NC}"

# Create build directory
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# Configure
echo -e "\n${GREEN}Configuring...${NC}"
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
echo -e "\n${GREEN}Building...${NC}"
make -j$(nproc)

echo -e "\n${GREEN}==================================================${NC}"
echo -e "${GREEN}  Build Complete!${NC}"
echo -e "${GREEN}==================================================${NC}"
echo ""
echo "Binary: build/bin/uwb_bridge"
echo ""
echo "Run with: ./bin/uwb_bridge -c ../config/app_config.json"
echo ""

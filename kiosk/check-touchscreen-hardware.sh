#!/bin/bash
# Check Victron Cerbo 50 touchscreen hardware connection

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}   Victron Cerbo 50 Touchscreen Hardware Check${NC}"
echo -e "${BLUE}================================================================${NC}"
echo ""

echo -e "${YELLOW}1. Checking I2C interface...${NC}"
if [ -e /dev/i2c-1 ]; then
    echo -e "  ${GREEN}✓${NC} /dev/i2c-1 exists"
    ls -l /dev/i2c-1
else
    echo -e "  ${RED}✗${NC} /dev/i2c-1 not found"
    echo "  Run: sudo raspi-config -> Interface Options -> I2C -> Enable"
fi
echo ""

echo -e "${YELLOW}2. Checking I2C module...${NC}"
if lsmod | grep -q i2c_dev; then
    echo -e "  ${GREEN}✓${NC} i2c_dev module loaded"
else
    echo -e "  ${YELLOW}⚠${NC} i2c_dev module not loaded"
    echo "  Loading now..."
    sudo modprobe i2c_dev
fi
echo ""

echo -e "${YELLOW}3. Scanning I2C bus 1 for devices...${NC}"
echo "  Looking for FT5316 touchscreen at address 0x38..."
echo ""
sudo i2cdetect -y 1
echo ""

# Check for touchscreen
if sudo i2cdetect -y 1 | grep -q " 38 "; then
    echo -e "${GREEN}✓✓✓ SUCCESS: FT5316 touchscreen detected at 0x38${NC}"
    echo ""
    echo "The touchscreen hardware is connected and working!"
    echo "You can now proceed with the service setup."
else
    echo -e "${RED}✗✗✗ PROBLEM: FT5316 touchscreen NOT detected${NC}"
    echo ""
    echo -e "${YELLOW}Troubleshooting Checklist:${NC}"
    echo ""
    echo "  ${BLUE}Physical Connections:${NC}"
    echo "    [ ] HDMI cable connected from Pi to Victron Cerbo 50 screen"
    echo "    [ ] HDMI cable firmly seated on both ends"
    echo "    [ ] Screen is powered ON (check for display backlight)"
    echo "    [ ] Power cable connected to screen"
    echo ""
    echo "  ${BLUE}Try these steps:${NC}"
    echo "    1. Power cycle the screen (unplug/replug power)"
    echo "    2. Disconnect and reconnect HDMI cable"
    echo "    3. Try a different HDMI cable if available"
    echo "    4. Verify the screen powers on and shows something"
    echo "    5. After fixing, run this script again"
    echo ""
    echo "  ${BLUE}Technical Details:${NC}"
    echo "    - The FT5316 touchscreen uses HDMI's I2C/DDC pins"
    echo "    - I2C address: 0x38 (56 decimal)"
    echo "    - Requires both HDMI data AND screen power"
    echo "    - No HDMI connection = no I2C communication"
    echo ""
fi
echo ""

echo -e "${YELLOW}4. Checking for other I2C devices...${NC}"
DEVICE_COUNT=$(sudo i2cdetect -y 1 | grep -oE ' [0-9a-f]{2} ' | wc -l)
echo "  Devices found on I2C bus 1: $DEVICE_COUNT"
if [ "$DEVICE_COUNT" -gt 0 ]; then
    echo "  This confirms I2C is working - just waiting for touchscreen"
else
    echo "  No I2C devices detected at all - check I2C is enabled"
fi
echo ""

echo -e "${YELLOW}5. Checking HDMI connection...${NC}"
if tvservice -s &>/dev/null; then
    HDMI_STATUS=$(tvservice -s)
    echo "  HDMI status: $HDMI_STATUS"

    if echo "$HDMI_STATUS" | grep -q "state 0x12"; then
        echo -e "  ${GREEN}✓${NC} HDMI display active"
    else
        echo -e "  ${YELLOW}⚠${NC} HDMI may not be active"
    fi
else
    echo "  Cannot check HDMI status (tvservice not available)"
fi
echo ""

echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}   Hardware Check Complete${NC}"
echo -e "${BLUE}================================================================${NC}"
echo ""

if sudo i2cdetect -y 1 | grep -q " 38 "; then
    echo -e "${GREEN}NEXT STEP: Run the service fix script${NC}"
    echo "  cd ~/Projects/test-lvgl-cross-compile/kiosk"
    echo "  sudo ./fix-touchscreen-service.sh"
else
    echo -e "${YELLOW}NEXT STEP: Fix hardware connection first${NC}"
    echo "  1. Check all cables (HDMI + power)"
    echo "  2. Power cycle the screen"
    echo "  3. Run this check again: ./check-touchscreen-hardware.sh"
fi
echo ""

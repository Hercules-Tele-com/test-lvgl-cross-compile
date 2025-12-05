#!/bin/bash
# Scan ALL I2C buses for the FT5316 touchscreen

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}   Scanning ALL I2C Buses for Touchscreen${NC}"
echo -e "${BLUE}================================================================${NC}"
echo ""

echo -e "${YELLOW}Raspberry Pi 4 has multiple I2C buses:${NC}"
echo "  - i2c-0:  GPIO I2C"
echo "  - i2c-1:  GPIO I2C (pins 3+5)"
echo "  - i2c-20: HDMI 0 DDC (first micro HDMI port)"
echo "  - i2c-21: HDMI 1 DDC (second micro HDMI port)"
echo ""
echo "The touchscreen uses HDMI DDC, so it should be on bus 20 or 21!"
echo ""

FOUND_TOUCHSCREEN=false

for BUS in /dev/i2c-*; do
    BUS_NUM=$(basename $BUS | cut -d'-' -f2)

    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}Scanning I2C Bus $BUS_NUM:${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"

    if sudo i2cdetect -y $BUS_NUM 2>/dev/null; then
        # Check if address 0x38 (FT5316) is present
        if sudo i2cdetect -y $BUS_NUM 2>/dev/null | grep -q " 38 "; then
            echo ""
            echo -e "${GREEN}✓✓✓ FOUND TOUCHSCREEN at address 0x38 on bus $BUS_NUM!${NC}"
            echo ""
            FOUND_TOUCHSCREEN=true
            TOUCHSCREEN_BUS=$BUS_NUM
        fi

        # Check if address 0x50 (EDID) is present
        if sudo i2cdetect -y $BUS_NUM 2>/dev/null | grep -q " 50 "; then
            echo ""
            echo -e "${YELLOW}Found EDID at 0x50 on bus $BUS_NUM (display connected)${NC}"
        fi
    else
        echo -e "${RED}✗ Cannot access bus $BUS_NUM${NC}"
    fi

    echo ""
done

echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}   Scan Complete${NC}"
echo -e "${BLUE}================================================================${NC}"
echo ""

if [ "$FOUND_TOUCHSCREEN" = true ]; then
    echo -e "${GREEN}✓✓✓ SUCCESS!${NC}"
    echo ""
    echo -e "${GREEN}Touchscreen detected on I2C bus $TOUCHSCREEN_BUS${NC}"
    echo ""
    echo "This means:"
    echo "  1. Your HDMI adapter IS working (I2C passes through)"
    echo "  2. The touchscreen IS connected and detected"
    echo "  3. We just need to configure the driver for bus $TOUCHSCREEN_BUS"
    echo ""
    echo -e "${YELLOW}NEXT STEP:${NC}"
    echo "  The Victron touchscreen driver needs to be configured to use bus $TOUCHSCREEN_BUS"
    echo "  instead of the default bus 1."
    echo ""
    echo "  Run this to fix the driver:"
    echo "    cd ~/ft5316-touchscreen"
    echo "    # Edit the Python script to use /dev/i2c-$TOUCHSCREEN_BUS"
    echo ""
else
    echo -e "${RED}✗ Touchscreen NOT found on any I2C bus${NC}"
    echo ""
    echo "Checked buses: 0, 1, 20, 21"
    echo ""
    echo "This suggests:"
    echo "  1. Touchscreen not powered/connected"
    echo "  2. HDMI adapter doesn't support I2C"
    echo "  3. Wrong HDMI port (try the other micro HDMI port)"
    echo ""
    echo "Try:"
    echo "  ./fix-adapter-issue.sh  # For troubleshooting steps"
fi
echo ""

#!/bin/bash
# Quick fix for the touchscreen service user issue

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Quick Fix for Victron Touchscreen${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""

echo -e "${YELLOW}Issue Identified:${NC}"
echo "  - The repository uses a setup script, not a standalone Python file"
echo "  - The setup script is designed for user 'pi', but you're user 'emboo'"
echo "  - No I2C device detected at 0x38 (touchscreen not detected)"
echo ""

echo -e "${YELLOW}Solution:${NC}"
echo "  We'll run the Victron setup script with modifications for user 'emboo'"
echo ""

# Check if touchscreen repo exists
if [ ! -d "$HOME/ft5316-touchscreen" ]; then
    echo -e "${RED}ERROR: ft5316-touchscreen repository not found${NC}"
    echo "Please run ./install-victron-touchscreen.sh first"
    exit 1
fi

cd "$HOME/ft5316-touchscreen"

echo -e "${BLUE}[1/5] Downloading and modifying setup script...${NC}"
# The repo has setup_touchscreen.sh - let's run it but modified for user emboo

# Create a modified version of the setup script
cat setup_touchscreen.sh | \
    sed 's|/home/pi/|/home/emboo/|g' | \
    sed 's|User=pi|User=emboo|g' | \
    sed 's|sudo -u pi |sudo -u emboo |g' | \
    sed 's|chown pi:|chown emboo:|g' > /tmp/setup_touchscreen_emboo.sh

chmod +x /tmp/setup_touchscreen_emboo.sh

echo -e "${GREEN}✓${NC} Script modified for user 'emboo'"
echo ""

echo -e "${BLUE}[2/5] Running modified setup script...${NC}"
echo -e "${YELLOW}NOTE: This will prompt for sudo password${NC}"
echo ""
sudo /tmp/setup_touchscreen_emboo.sh

echo ""
echo -e "${BLUE}[3/5] Checking I2C devices...${NC}"
if sudo i2cdetect -y 1 | grep -q " 38 "; then
    echo -e "${GREEN}✓${NC} Touchscreen detected at I2C address 0x38"
else
    echo -e "${YELLOW}⚠${NC} Touchscreen NOT detected at I2C address 0x38"
    echo ""
    echo "I2C scan results:"
    sudo i2cdetect -y 1
    echo ""
    echo "This could mean:"
    echo "  1. Touchscreen is not physically connected"
    echo "  2. HDMI cable is not connected (touchscreen uses HDMI I2C pins)"
    echo "  3. Touchscreen needs power cycle"
    echo ""
    echo "Please check:"
    echo "  - HDMI connection to Victron Cerbo 50 screen"
    echo "  - Power to the screen"
    echo "  - I2C cables/connections"
fi
echo ""

echo -e "${BLUE}[4/5] Checking service status...${NC}"
systemctl status ft5316-touchscreen.service --no-pager || true
echo ""

echo -e "${BLUE}[5/5] Checking if touchscreen script exists...${NC}"
if [ -f "$HOME/ft5316_touch.py" ]; then
    echo -e "${GREEN}✓${NC} Touchscreen script exists at $HOME/ft5316_touch.py"
else
    echo -e "${RED}✗${NC} Touchscreen script not found at $HOME/ft5316_touch.py"
fi
echo ""

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Setup Complete${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""
echo "Next steps:"
echo "  1. If touchscreen is detected (0x38), test it:"
echo "     systemctl status ft5316-touchscreen.service"
echo ""
echo "  2. If touchscreen is NOT detected:"
echo "     - Check HDMI cable connection"
echo "     - Power cycle the Victron Cerbo 50 screen"
echo "     - Verify screen is powered on"
echo "     - Run: sudo i2cdetect -y 1"
echo ""
echo "  3. View logs:"
echo "     sudo journalctl -u ft5316-touchscreen.service -f"
echo ""

#!/bin/bash
# Fix the existing touchscreen service to use user 'emboo' and run the Victron setup

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERROR: This script must be run as root (use sudo)${NC}"
    exit 1
fi

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Fixing Victron FT5316 Touchscreen Service${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""

echo -e "${YELLOW}Problem: Service configured for user 'pi' but system uses 'emboo'${NC}"
echo ""

echo -e "${BLUE}[1/6] Stopping existing service...${NC}"
systemctl stop ft5316-touchscreen.service 2>/dev/null || true
systemctl disable ft5316-touchscreen.service 2>/dev/null || true
echo -e "${GREEN}✓${NC} Service stopped"
echo ""

echo -e "${BLUE}[2/6] Running Victron's official setup script...${NC}"
echo "  This will install dependencies and create the touchscreen driver"
echo ""

if [ -f /home/emboo/ft5316-touchscreen/setup_touchscreen.sh ]; then
    cd /home/emboo/ft5316-touchscreen

    # Run the setup script - it will create everything we need
    # But first, let's backup the original service file it creates
    bash setup_touchscreen.sh

    echo -e "${GREEN}✓${NC} Setup script completed"
else
    echo -e "${RED}✗${NC} Setup script not found"
    echo "  Expected at: /home/emboo/ft5316-touchscreen/setup_touchscreen.sh"
    echo "  Run ./install-victron-touchscreen.sh first"
    exit 1
fi
echo ""

echo -e "${BLUE}[3/6] Updating service file for user 'emboo'...${NC}"
# The setup script creates a service for user 'pi', we need to fix it

if [ -f /etc/systemd/system/ft5316-touchscreen.service ]; then
    # Backup original
    cp /etc/systemd/system/ft5316-touchscreen.service \
       /etc/systemd/system/ft5316-touchscreen.service.bak

    # Replace pi with emboo
    sed -i 's|User=pi|User=emboo|g' /etc/systemd/system/ft5316-touchscreen.service
    sed -i 's|/home/pi/|/home/emboo/|g' /etc/systemd/system/ft5316-touchscreen.service
    sed -i 's|WorkingDirectory=/home/pi|WorkingDirectory=/home/emboo|g' /etc/systemd/system/ft5316-touchscreen.service

    echo "Updated service file:"
    cat /etc/systemd/system/ft5316-touchscreen.service
    echo -e "${GREEN}✓${NC} Service file updated"
else
    echo -e "${YELLOW}⚠${NC} Service file not created by setup script"
fi
echo ""

echo -e "${BLUE}[4/6] Copying touchscreen script to emboo's home...${NC}"
if [ -f /home/pi/ft5316_touch.py ]; then
    cp /home/pi/ft5316_touch.py /home/emboo/
    chown emboo:emboo /home/emboo/ft5316_touch.py
    chmod +x /home/emboo/ft5316_touch.py
    echo -e "${GREEN}✓${NC} Script copied to /home/emboo/ft5316_touch.py"
elif [ -f /home/emboo/ft5316_touch.py ]; then
    echo -e "${GREEN}✓${NC} Script already exists at /home/emboo/ft5316_touch.py"
else
    echo -e "${RED}✗${NC} Touchscreen script not found"
    echo "  The setup script should have created it"
fi
echo ""

echo -e "${BLUE}[5/6] Reloading and starting service...${NC}"
systemctl daemon-reload
systemctl enable ft5316-touchscreen.service
systemctl start ft5316-touchscreen.service
echo -e "${GREEN}✓${NC} Service reloaded and started"
echo ""

echo -e "${BLUE}[6/6] Checking hardware detection...${NC}"
echo "Scanning I2C bus for touchscreen (address 0x38)..."
i2cdetect -y 1 | tee /tmp/i2c_scan.txt

if grep -q " 38 " /tmp/i2c_scan.txt; then
    echo -e "${GREEN}✓✓✓ TOUCHSCREEN DETECTED at I2C address 0x38!${NC}"
else
    echo -e "${YELLOW}⚠⚠⚠ WARNING: Touchscreen NOT detected${NC}"
    echo ""
    echo "Hardware troubleshooting:"
    echo "  1. Check HDMI cable is connected to Victron Cerbo 50"
    echo "  2. Verify the screen is powered on"
    echo "  3. Power cycle the screen (unplug/replug power)"
    echo "  4. Check for loose cables"
    echo ""
    echo "The touchscreen uses HDMI's I2C pins (DDC) for communication."
    echo "Without a proper HDMI connection, it won't be detected."
fi
echo ""

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Service Fix Complete!${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""
echo "Service status:"
systemctl status ft5316-touchscreen.service --no-pager || true
echo ""
echo "Next steps:"
echo "  1. If touchscreen WAS detected (address 0x38):"
echo "     - Touch the screen and check if it responds"
echo "     - View logs: sudo journalctl -u ft5316-touchscreen.service -f"
echo ""
echo "  2. If touchscreen was NOT detected:"
echo "     - Check physical connections (HDMI + power)"
echo "     - After fixing, rerun: sudo i2cdetect -y 1"
echo "     - Once detected, restart service:"
echo "       sudo systemctl restart ft5316-touchscreen.service"
echo ""

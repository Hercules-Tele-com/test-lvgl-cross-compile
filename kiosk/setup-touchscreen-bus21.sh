#!/bin/bash
# Setup Victron touchscreen for I2C bus 21 (HDMI 1) with user emboo

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERROR: This script must be run with sudo${NC}"
    exit 1
fi

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Setting Up Victron Touchscreen on I2C Bus 21${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""

echo -e "${YELLOW}Detected Configuration:${NC}"
echo "  I2C Bus: 21 (HDMI 1 - second micro HDMI port)"
echo "  Touchscreen Address: 0x38"
echo "  User: emboo"
echo ""

echo -e "${BLUE}[1/7] Stopping existing service...${NC}"
systemctl stop ft5316-touchscreen.service 2>/dev/null || true
systemctl disable ft5316-touchscreen.service 2>/dev/null || true
echo -e "${GREEN}✓${NC} Service stopped"
echo ""

echo -e "${BLUE}[2/7] Running Victron setup script...${NC}"
cd /home/emboo/ft5316-touchscreen

# Run the setup script - it will create everything
bash setup_touchscreen.sh

echo -e "${GREEN}✓${NC} Setup script completed"
echo ""

echo -e "${BLUE}[3/7] Modifying touchscreen driver for I2C bus 21...${NC}"

# The setup script creates ft5316_touch.py in /home/pi
# We need to:
# 1. Copy it to /home/emboo
# 2. Change i2c-1 to i2c-21
# 3. Change ownership

if [ -f /home/pi/ft5316_touch.py ]; then
    # Copy to emboo's home
    cp /home/pi/ft5316_touch.py /home/emboo/ft5316_touch.py

    # Modify to use i2c-21 instead of i2c-1
    sed -i 's|/dev/i2c-1|/dev/i2c-21|g' /home/emboo/ft5316_touch.py

    # Fix ownership
    chown emboo:emboo /home/emboo/ft5316_touch.py
    chmod +x /home/emboo/ft5316_touch.py

    echo -e "${GREEN}✓${NC} Driver modified for I2C bus 21"
    echo "  File: /home/emboo/ft5316_touch.py"
    echo "  Bus: /dev/i2c-21"
elif [ -f /home/emboo/ft5316_touch.py ]; then
    # Already exists, just modify it
    sed -i 's|/dev/i2c-1|/dev/i2c-21|g' /home/emboo/ft5316_touch.py
    echo -e "${GREEN}✓${NC} Existing driver modified for I2C bus 21"
else
    echo -e "${RED}✗${NC} Driver script not found!"
    echo "  Expected at /home/pi/ft5316_touch.py"
    exit 1
fi
echo ""

echo -e "${BLUE}[4/7] Updating systemd service for user emboo...${NC}"

if [ -f /etc/systemd/system/ft5316-touchscreen.service ]; then
    # Update the service file
    sed -i 's|User=pi|User=emboo|g' /etc/systemd/system/ft5316-touchscreen.service
    sed -i 's|/home/pi/|/home/emboo/|g' /etc/systemd/system/ft5316-touchscreen.service
    sed -i 's|WorkingDirectory=/home/pi|WorkingDirectory=/home/emboo|g' /etc/systemd/system/ft5316-touchscreen.service

    echo "Updated service file:"
    grep -E "User=|ExecStart=|WorkingDirectory=" /etc/systemd/system/ft5316-touchscreen.service
    echo -e "${GREEN}✓${NC} Service updated for user emboo"
else
    echo -e "${RED}✗${NC} Service file not found"
    exit 1
fi
echo ""

echo -e "${BLUE}[5/7] Copying startup scripts...${NC}"

# Copy ydotoold startup script if it exists
if [ -f /home/pi/start_ydotoold.sh ]; then
    cp /home/pi/start_ydotoold.sh /home/emboo/
    chown emboo:emboo /home/emboo/start_ydotoold.sh
    chmod +x /home/emboo/start_ydotoold.sh

    # Update paths in the copied script
    sed -i 's|/home/pi/|/home/emboo/|g' /home/emboo/start_ydotoold.sh

    echo -e "${GREEN}✓${NC} Startup scripts copied"
fi
echo ""

echo -e "${BLUE}[6/7] Reloading and starting services...${NC}"

systemctl daemon-reload
systemctl enable ft5316-touchscreen.service
systemctl enable ydotoold.service 2>/dev/null || true

# Start ydotoold first if it exists
systemctl start ydotoold.service 2>/dev/null || true
sleep 1

# Start touchscreen service
systemctl start ft5316-touchscreen.service

echo -e "${GREEN}✓${NC} Services started"
echo ""

echo -e "${BLUE}[7/7] Verifying setup...${NC}"

sleep 3

echo "Service status:"
systemctl status ft5316-touchscreen.service --no-pager -l || true
echo ""

echo "Touchscreen detection:"
if sudo i2cdetect -y 21 | grep -q " 38 "; then
    echo -e "${GREEN}✓ Touchscreen still detected on bus 21${NC}"
else
    echo -e "${YELLOW}⚠ Touchscreen not showing on bus 21 (might be in use by driver)${NC}"
fi
echo ""

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Setup Complete!${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""
echo "Configuration:"
echo "  ✓ Driver using I2C bus 21 (/dev/i2c-21)"
echo "  ✓ Service running as user emboo"
echo "  ✓ Touchscreen at address 0x38"
echo "  ✓ Connected to HDMI 1 (second micro HDMI port)"
echo ""
echo "Testing:"
echo "  1. Touch the screen - cursor should move"
echo "  2. View logs: sudo journalctl -u ft5316-touchscreen.service -f"
echo "  3. Check service: systemctl status ft5316-touchscreen.service"
echo ""
echo "If touchscreen doesn't respond:"
echo "  - Check logs for errors"
echo "  - Verify ydotoold service is running: systemctl status ydotoold.service"
echo "  - Test manually: sudo systemctl stop ft5316-touchscreen.service"
echo "                   python3 /home/emboo/ft5316_touch.py"
echo ""

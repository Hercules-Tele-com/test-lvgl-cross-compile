#!/bin/bash
# Install and configure Victron FT5316 touchscreen for Raspberry Pi 4

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Check if running as correct user
if [ "$USER" != "emboo" ]; then
    echo -e "${RED}ERROR: This script must be run as user 'emboo' (not root)${NC}"
    echo "Usage: ./install-victron-touchscreen.sh"
    exit 1
fi

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Installing Victron FT5316 Touchscreen Driver${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""

echo -e "${BLUE}[1/8] Installing dependencies...${NC}"
sudo apt update
sudo apt install -y python3-pip python3-evdev python3-smbus2 i2c-tools git
echo -e "${GREEN}✓${NC} Dependencies installed"
echo ""

echo -e "${BLUE}[2/8] Enabling I2C...${NC}"
if ! grep -q "^dtparam=i2c_arm=on" /boot/firmware/config.txt 2>/dev/null && \
   ! grep -q "^dtparam=i2c_arm=on" /boot/config.txt 2>/dev/null; then
    echo "Enabling I2C in config.txt..."
    if [ -f /boot/firmware/config.txt ]; then
        echo "dtparam=i2c_arm=on" | sudo tee -a /boot/firmware/config.txt
    elif [ -f /boot/config.txt ]; then
        echo "dtparam=i2c_arm=on" | sudo tee -a /boot/config.txt
    fi
    echo -e "${YELLOW}⚠${NC} I2C enabled - system will need reboot after this script"
else
    echo -e "${GREEN}✓${NC} I2C already enabled"
fi

# Load I2C module now
sudo modprobe i2c-dev 2>/dev/null || true
echo -e "${GREEN}✓${NC} I2C configuration complete"
echo ""

echo -e "${BLUE}[3/8] Cloning FT5316 touchscreen driver...${NC}"
DRIVER_DIR="$HOME/ft5316-touchscreen"
if [ -d "$DRIVER_DIR" ]; then
    echo "Updating existing repository..."
    cd "$DRIVER_DIR"
    git pull
else
    echo "Cloning repository..."
    git clone https://github.com/lpopescu-victron/ft5316-touchscreen.git "$DRIVER_DIR"
    cd "$DRIVER_DIR"
fi
echo -e "${GREEN}✓${NC} Driver repository ready"
echo ""

echo -e "${BLUE}[4/8] Installing Python script...${NC}"
# Copy the driver script to home directory for easy access
if [ -f "$DRIVER_DIR/ft5316_touch.py" ]; then
    cp "$DRIVER_DIR/ft5316_touch.py" "$HOME/"
    chmod +x "$HOME/ft5316_touch.py"
    echo -e "${GREEN}✓${NC} Driver script installed to $HOME/ft5316_touch.py"
else
    echo -e "${RED}✗${NC} Driver script not found in repository"
    echo "Contents of repository:"
    ls -la "$DRIVER_DIR"
    exit 1
fi
echo ""

echo -e "${BLUE}[5/8] Creating systemd service...${NC}"
cat > /tmp/ft5316-touchscreen.service << EOF
[Unit]
Description=FT5316 Touchscreen Driver
After=local-fs.target
Before=display-manager.service

[Service]
Type=simple
User=emboo
Group=emboo
WorkingDirectory=$HOME
ExecStart=/usr/bin/python3 $HOME/ft5316_touch.py
Restart=always
RestartSec=5
StartLimitInterval=300
StartLimitBurst=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

sudo cp /tmp/ft5316-touchscreen.service /etc/systemd/system/
sudo systemctl daemon-reload
echo -e "${GREEN}✓${NC} Service file created"
echo ""

echo -e "${BLUE}[6/8] Checking I2C devices...${NC}"
if [ -e /dev/i2c-1 ]; then
    echo "Scanning I2C bus for FT5316 touchscreen (address 0x38)..."
    sudo i2cdetect -y 1 | tee /tmp/i2c_scan.txt

    if grep -q " 38 " /tmp/i2c_scan.txt; then
        echo -e "${GREEN}✓${NC} FT5316 touchscreen detected at address 0x38"
    else
        echo -e "${YELLOW}⚠${NC} Touchscreen not detected at 0x38"
        echo "  This is normal if the screen is not connected yet"
    fi
else
    echo -e "${YELLOW}⚠${NC} /dev/i2c-1 not available yet (may need reboot)"
fi
echo ""

echo -e "${BLUE}[7/8] Enabling touchscreen service...${NC}"
sudo systemctl enable ft5316-touchscreen.service
echo -e "${GREEN}✓${NC} Service enabled for autostart"
echo ""

echo -e "${BLUE}[8/8] Starting touchscreen service...${NC}"
# Try to start the service (may fail if hardware not connected)
if sudo systemctl start ft5316-touchscreen.service 2>/dev/null; then
    echo -e "${GREEN}✓${NC} Service started successfully"
    sleep 2
    systemctl status ft5316-touchscreen.service --no-pager
else
    echo -e "${YELLOW}⚠${NC} Service failed to start (may need reboot or hardware check)"
    echo "Checking logs:"
    sudo journalctl -u ft5316-touchscreen.service -n 20 --no-pager
fi
echo ""

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Installation Complete!${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""
echo "Summary:"
echo "  ✓ Python dependencies installed"
echo "  ✓ I2C enabled"
echo "  ✓ FT5316 driver installed to $HOME/ft5316_touch.py"
echo "  ✓ Systemd service created and enabled"
echo ""
echo "Next steps:"
echo "  1. If I2C was just enabled, reboot now:"
echo "     sudo reboot"
echo ""
echo "  2. After reboot, check service status:"
echo "     systemctl status ft5316-touchscreen.service"
echo ""
echo "  3. Check I2C devices:"
echo "     sudo i2cdetect -y 1"
echo "     (Should see '38' if touchscreen is connected)"
echo ""
echo "  4. View touchscreen logs:"
echo "     sudo journalctl -u ft5316-touchscreen.service -f"
echo ""
echo "  5. Test manually (stop service first):"
echo "     sudo systemctl stop ft5316-touchscreen.service"
echo "     python3 $HOME/ft5316_touch.py"
echo ""

#!/bin/bash
# Victron Cerbo 50 Touchscreen Setup - Complete Installation
# Sets up FT5316 touchscreen on I2C bus 21 (HDMI 1 DDC)

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERROR: This script must be run with sudo${NC}"
    echo "Usage: sudo ./setup-victron-touchscreen.sh"
    exit 1
fi

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Victron Cerbo 50 Touchscreen Setup${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""

# ============================================================================
# Step 1: Enable I2C
# ============================================================================
echo -e "${BLUE}[1/6] Configuring I2C...${NC}"

# Determine which boot config file exists
if [ -f /boot/firmware/config.txt ]; then
    BOOT_CONFIG="/boot/firmware/config.txt"
elif [ -f /boot/config.txt ]; then
    BOOT_CONFIG="/boot/config.txt"
else
    echo -e "${RED}ERROR: Cannot find boot config file${NC}"
    exit 1
fi

# Enable I2C if not already enabled
if ! grep -q "^dtparam=i2c_arm=on" "$BOOT_CONFIG"; then
    echo "Enabling I2C in $BOOT_CONFIG"
    echo "" >> "$BOOT_CONFIG"
    echo "# Enable I2C for touchscreen" >> "$BOOT_CONFIG"
    echo "dtparam=i2c_arm=on" >> "$BOOT_CONFIG"
    I2C_ENABLED_NOW=true
else
    echo "I2C already enabled"
    I2C_ENABLED_NOW=false
fi

# Load I2C modules
modprobe i2c-dev 2>/dev/null || true

echo -e "${GREEN}✓${NC} I2C configured"
echo ""

# ============================================================================
# Step 2: Install dependencies
# ============================================================================
echo -e "${BLUE}[2/6] Installing dependencies...${NC}"

apt-get update -qq
apt-get install -y python3-pip python3-smbus i2c-tools

# Install python-evdev
pip3 install --break-system-packages evdev 2>/dev/null || pip3 install evdev

echo -e "${GREEN}✓${NC} Dependencies installed"
echo ""

# ============================================================================
# Step 3: Configure uinput permissions
# ============================================================================
echo -e "${BLUE}[3/6] Configuring input device permissions...${NC}"

# Add user to input group
usermod -aG input emboo

# Create udev rule for uinput
cat > /etc/udev/rules.d/10-uinput.rules << 'EOF'
SUBSYSTEM=="misc", KERNEL=="uinput", MODE="0660", GROUP="input"
EOF

# Set current permissions
chmod 660 /dev/uinput 2>/dev/null || true
chown root:input /dev/uinput 2>/dev/null || true

# Reload udev rules
udevadm control --reload-rules
udevadm trigger

echo -e "${GREEN}✓${NC} Permissions configured"
echo ""

# ============================================================================
# Step 4: Create touchscreen driver
# ============================================================================
echo -e "${BLUE}[4/6] Creating touchscreen driver...${NC}"

cat > /home/emboo/ft5316_touch.py << 'EOFPY'
#!/usr/bin/env python3
"""
FT5316 Touchscreen Driver for Victron Cerbo 50
Reads touch data from I2C bus 21 and creates virtual touchscreen device
"""

import smbus2
import time
from evdev import UInput, AbsInfo, ecodes

# Configuration
I2C_BUS = 21  # HDMI 1 DDC bus
FT5316_ADDR = 0x38  # FT5316 I2C address
SCREEN_WIDTH = 1280
SCREEN_HEIGHT = 720

# FT5316 Registers
REG_NUM_TOUCHES = 0x02
REG_TOUCH1_XH = 0x03
REG_TOUCH1_XL = 0x04
REG_TOUCH1_YH = 0x05
REG_TOUCH1_YL = 0x06

# Touch event types (from register bits 6-7 of XH)
EVENT_PRESS_DOWN = 0
EVENT_CONTACT = 2
EVENT_LIFT_UP = 1

class FT5316Touch:
    def __init__(self):
        self.bus = smbus2.SMBus(I2C_BUS)

        # Create virtual touchscreen device
        cap = {
            ecodes.EV_ABS: [
                (ecodes.ABS_X, AbsInfo(0, 0, SCREEN_WIDTH, 0, 0, 0)),
                (ecodes.ABS_Y, AbsInfo(0, 0, SCREEN_HEIGHT, 0, 0, 0)),
                (ecodes.ABS_PRESSURE, AbsInfo(0, 0, 255, 0, 0, 0)),
            ],
            ecodes.EV_KEY: [ecodes.BTN_TOUCH],
        }

        self.ui = UInput(cap, name='FT5316-Touchscreen', version=0x1)
        print("Virtual touchscreen device created")

        self.last_x = 0
        self.last_y = 0
        self.touching = False

    def read_touch_data(self):
        """Read touch data from FT5316 controller"""
        try:
            # Read number of active touch points
            num_touches = self.bus.read_byte_data(FT5316_ADDR, REG_NUM_TOUCHES) & 0x0F

            if num_touches > 0:
                # Read first touch point coordinates
                xh = self.bus.read_byte_data(FT5316_ADDR, REG_TOUCH1_XH)
                xl = self.bus.read_byte_data(FT5316_ADDR, REG_TOUCH1_XL)
                yh = self.bus.read_byte_data(FT5316_ADDR, REG_TOUCH1_YH)
                yl = self.bus.read_byte_data(FT5316_ADDR, REG_TOUCH1_YL)

                # Extract event type from XH bits 6-7
                event_flag = (xh >> 6) & 0x03

                # Combine bytes to get X,Y coordinates
                x = ((xh & 0x0F) << 8) | xl
                y = ((yh & 0x0F) << 8) | yl

                # Clamp to screen boundaries
                x = max(0, min(x, SCREEN_WIDTH - 1))
                y = max(0, min(y, SCREEN_HEIGHT - 1))

                # Check if touch is active (press or contact)
                is_touching = (event_flag == EVENT_PRESS_DOWN or event_flag == EVENT_CONTACT)

                return (x, y, is_touching)
            else:
                return (self.last_x, self.last_y, False)

        except Exception as err:
            print(f"Error reading touch data: {err}")
            return (self.last_x, self.last_y, self.touching)

    def run(self):
        """Main loop - poll touchscreen and send events"""
        print("FT5316 Touchscreen Driver Starting...")
        print(f"I2C Bus: {I2C_BUS}")
        print(f"Device Address: 0x{FT5316_ADDR:02X}")
        print(f"Screen Resolution: {SCREEN_WIDTH}x{SCREEN_HEIGHT}")
        print("Driver ready - touch the screen!")
        print("")

        while True:
            try:
                x, y, touching = self.read_touch_data()

                if touching:
                    # Touch is active
                    if not self.touching:
                        # New touch detected
                        print(f"Touch DOWN at ({x}, {y})")
                        self.ui.write(ecodes.EV_KEY, ecodes.BTN_TOUCH, 1)
                        self.touching = True

                    # Send current position
                    self.ui.write(ecodes.EV_ABS, ecodes.ABS_X, x)
                    self.ui.write(ecodes.EV_ABS, ecodes.ABS_Y, y)
                    self.ui.write(ecodes.EV_ABS, ecodes.ABS_PRESSURE, 128)
                    self.ui.syn()

                    self.last_x = x
                    self.last_y = y

                else:
                    # No touch detected
                    if self.touching:
                        # Touch ended
                        print(f"Touch UP at ({self.last_x}, {self.last_y})")
                        self.ui.write(ecodes.EV_KEY, ecodes.BTN_TOUCH, 0)
                        self.ui.write(ecodes.EV_ABS, ecodes.ABS_PRESSURE, 0)
                        self.ui.syn()
                        self.touching = False

                time.sleep(0.01)  # Poll at 100 Hz

            except KeyboardInterrupt:
                print("\nShutting down...")
                break
            except Exception as err:
                print(f"Error in main loop: {err}")
                time.sleep(0.1)

        # Cleanup
        self.ui.close()

if __name__ == "__main__":
    driver = FT5316Touch()
    driver.run()
EOFPY

chmod +x /home/emboo/ft5316_touch.py
chown emboo:emboo /home/emboo/ft5316_touch.py

echo -e "${GREEN}✓${NC} Driver created at /home/emboo/ft5316_touch.py"
echo ""

# ============================================================================
# Step 5: Create systemd service
# ============================================================================
echo -e "${BLUE}[5/6] Creating systemd service...${NC}"

cat > /etc/systemd/system/victron-touchscreen.service << 'EOFSVC'
[Unit]
Description=Victron Cerbo 50 Touchscreen Driver
Documentation=https://github.com/lpopescu-victron/ft5316-touchscreen
After=graphical.target multi-user.target

[Service]
Type=simple
User=root
Group=input
SupplementaryGroups=i2c
ExecStart=/usr/bin/python3 /home/emboo/ft5316_touch.py
Restart=always
RestartSec=5
WorkingDirectory=/home/emboo
StandardOutput=journal
StandardError=journal

# Restart limits
StartLimitInterval=300
StartLimitBurst=10

[Install]
WantedBy=graphical.target
EOFSVC

# Enable and start service
systemctl daemon-reload
systemctl enable victron-touchscreen.service

echo -e "${GREEN}✓${NC} Service created and enabled"
echo ""

# ============================================================================
# Step 6: Verify and start
# ============================================================================
echo -e "${BLUE}[6/6] Verifying hardware and starting service...${NC}"

# Check if I2C bus 21 exists
if [ ! -e /dev/i2c-21 ]; then
    echo -e "${YELLOW}⚠ I2C bus 21 not found - may need reboot${NC}"
    if [ "$I2C_ENABLED_NOW" = true ]; then
        echo "  I2C was just enabled - reboot is required"
    fi
fi

# Try to detect touchscreen on I2C bus 21
if [ -e /dev/i2c-21 ]; then
    echo "Scanning I2C bus 21 for touchscreen..."
    if i2cdetect -y 21 2>/dev/null | grep -q " 38 "; then
        echo -e "${GREEN}✓ Touchscreen detected at address 0x38${NC}"

        # Start the service
        systemctl start victron-touchscreen.service
        sleep 2

        if systemctl is-active --quiet victron-touchscreen.service; then
            echo -e "${GREEN}✓ Service started successfully${NC}"
        else
            echo -e "${YELLOW}⚠ Service failed to start (check logs)${NC}"
        fi
    else
        echo -e "${YELLOW}⚠ Touchscreen not detected on I2C bus 21${NC}"
        echo "  Make sure HDMI is connected to the correct port"
        echo "  Service will auto-start on next boot"
    fi
else
    echo -e "${YELLOW}⚠ I2C bus 21 not available${NC}"
    echo "  Service configured but waiting for hardware"
fi

echo ""

# ============================================================================
# Summary
# ============================================================================
echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Installation Complete!${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""
echo "Configuration Summary:"
echo "  ✓ I2C enabled in $BOOT_CONFIG"
echo "  ✓ Python driver: /home/emboo/ft5316_touch.py"
echo "  ✓ Service: victron-touchscreen.service"
echo "  ✓ I2C Bus: 21 (HDMI 1 - second micro HDMI port)"
echo "  ✓ Device Address: 0x38"
echo ""

if [ "$I2C_ENABLED_NOW" = true ]; then
    echo -e "${YELLOW}⚠ REBOOT REQUIRED${NC}"
    echo "  I2C was just enabled - please reboot for changes to take effect:"
    echo "  ${BLUE}sudo reboot${NC}"
    echo ""
fi

echo "Testing Commands:"
echo "  • Check service status:"
echo "    ${BLUE}systemctl status victron-touchscreen.service${NC}"
echo ""
echo "  • View live logs:"
echo "    ${BLUE}sudo journalctl -u victron-touchscreen.service -f${NC}"
echo ""
echo "  • Scan for touchscreen:"
echo "    ${BLUE}sudo i2cdetect -y 21${NC}"
echo "    (Should show '38' if touchscreen is connected)"
echo ""
echo "  • List input devices:"
echo "    ${BLUE}ls -l /dev/input/by-id/*FT5316*${NC}"
echo ""
echo "Hardware Requirements:"
echo "  • HDMI cable from Pi to Victron screen (preferably HDMI 1 port)"
echo "  • Screen must be powered on"
echo "  • Touchscreen communicates via HDMI DDC pins"
echo ""
echo "The touchscreen will work automatically with:"
echo "  • Chromium (including kiosk mode)"
echo "  • Any X11 or Wayland application"
echo "  • Desktop environment"
echo ""

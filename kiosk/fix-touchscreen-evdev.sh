#!/bin/bash
# Fix touchscreen to use evdev directly (no ydotool needed)

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
echo -e "${GREEN}   Fixing Touchscreen to Use evdev (No ydotool)${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""

echo -e "${BLUE}[1/4] Stopping existing services...${NC}"
systemctl stop ft5316-touchscreen.service 2>/dev/null || true
systemctl stop ydotoold.service 2>/dev/null || true
systemctl disable ydotoold.service 2>/dev/null || true
echo -e "${GREEN}✓${NC} Services stopped"
echo ""

echo -e "${BLUE}[2/4] Creating improved touchscreen driver using evdev...${NC}"
cat > /home/emboo/ft5316_touch.py << 'EOFPY'
#!/usr/bin/env python3
"""
FT5316 Touchscreen Driver for I2C Bus 21
Creates a virtual touchscreen device using evdev
"""

import smbus2
import time
from evdev import UInput, AbsInfo, ecodes as e

# Configuration
I2C_BUS = 21  # HDMI 1 DDC bus
FT5316_ADDR = 0x38
SCREEN_WIDTH = 1280
SCREEN_HEIGHT = 720

# FT5316 Registers
REG_NUM_TOUCHES = 0x02
REG_TOUCH1_XH = 0x03
REG_TOUCH1_XL = 0x04
REG_TOUCH1_YH = 0x05
REG_TOUCH1_YL = 0x06
REG_TOUCH1_EVENT = 0x03  # Bits 6-7 contain event type

# Touch event types
EVENT_PRESS_DOWN = 0
EVENT_CONTACT = 2
EVENT_LIFT_UP = 1

class FT5316Touch:
    def __init__(self):
        self.bus = smbus2.SMBus(I2C_BUS)

        # Create virtual touchscreen device
        cap = {
            e.EV_ABS: [
                (e.ABS_X, AbsInfo(0, 0, SCREEN_WIDTH, 0, 0, 0)),
                (e.ABS_Y, AbsInfo(0, 0, SCREEN_HEIGHT, 0, 0, 0)),
                (e.ABS_PRESSURE, AbsInfo(0, 0, 255, 0, 0, 0)),
            ],
            e.EV_KEY: [e.BTN_TOUCH],
        }

        self.ui = UInput(cap, name='FT5316-Touchscreen', version=0x1)
        print("Virtual touchscreen device created")

        self.last_x = 0
        self.last_y = 0
        self.touching = False

    def read_touch_data(self):
        """Read touch data from FT5316"""
        try:
            # Read number of touch points
            num_touches = self.bus.read_byte_data(FT5316_ADDR, REG_NUM_TOUCHES) & 0x0F

            if num_touches > 0:
                # Read first touch point
                xh = self.bus.read_byte_data(FT5316_ADDR, REG_TOUCH1_XH)
                xl = self.bus.read_byte_data(FT5316_ADDR, REG_TOUCH1_XL)
                yh = self.bus.read_byte_data(FT5316_ADDR, REG_TOUCH1_YH)
                yl = self.bus.read_byte_data(FT5316_ADDR, REG_TOUCH1_YL)

                # Get event type
                event_flag = (xh >> 6) & 0x03

                # Combine bytes to get coordinates
                x = ((xh & 0x0F) << 8) | xl
                y = ((yh & 0x0F) << 8) | yl

                # Clamp to screen bounds
                x = max(0, min(x, SCREEN_WIDTH - 1))
                y = max(0, min(y, SCREEN_HEIGHT - 1))

                # Determine if touching
                is_touching = (event_flag == EVENT_PRESS_DOWN or event_flag == EVENT_CONTACT)

                return (x, y, is_touching)
            else:
                return (self.last_x, self.last_y, False)

        except Exception as e:
            print(f"Error reading touch data: {e}")
            return (self.last_x, self.last_y, self.touching)

    def run(self):
        """Main loop"""
        print("FT5316 Touchscreen Driver Starting...")
        print(f"I2C Bus: {I2C_BUS}")
        print(f"FT5316 Address: 0x{FT5316_ADDR:02X}")
        print(f"Screen: {SCREEN_WIDTH}x{SCREEN_HEIGHT}")
        print("Driver ready - touch the screen!")
        print("")

        while True:
            try:
                x, y, touching = self.read_touch_data()

                if touching:
                    # Touch detected
                    if not self.touching:
                        # New touch - send press event
                        print(f"Touch DOWN at ({x}, {y})")
                        self.ui.write(e.EV_KEY, e.BTN_TOUCH, 1)
                        self.touching = True

                    # Send position
                    self.ui.write(e.EV_ABS, e.ABS_X, x)
                    self.ui.write(e.EV_ABS, e.ABS_Y, y)
                    self.ui.write(e.EV_ABS, e.ABS_PRESSURE, 128)
                    self.ui.syn()

                    self.last_x = x
                    self.last_y = y

                else:
                    # No touch detected
                    if self.touching:
                        # Touch ended - send release event
                        print(f"Touch UP at ({self.last_x}, {self.last_y})")
                        self.ui.write(e.EV_KEY, e.BTN_TOUCH, 0)
                        self.ui.write(e.EV_ABS, e.ABS_PRESSURE, 0)
                        self.ui.syn()
                        self.touching = False

                time.sleep(0.01)  # 100 Hz polling

            except KeyboardInterrupt:
                print("\nShutting down...")
                break
            except Exception as e:
                print(f"Error in main loop: {e}")
                time.sleep(0.1)

        # Cleanup
        self.ui.close()

if __name__ == "__main__":
    driver = FT5316Touch()
    driver.run()
EOFPY

chmod +x /home/emboo/ft5316_touch.py
chown emboo:emboo /home/emboo/ft5316_touch.py
echo -e "${GREEN}✓${NC} Updated driver created (using evdev)"
echo ""

echo -e "${BLUE}[3/4] Updating touchscreen service...${NC}"
cat > /etc/systemd/system/ft5316-touchscreen.service << 'EOFSVC'
[Unit]
Description=FT5316 Touchscreen Driver (evdev)
After=graphical.target multi-user.target

[Service]
User=root
Group=input
SupplementaryGroups=i2c
ExecStart=/usr/bin/python3 /home/emboo/ft5316_touch.py
Restart=always
RestartSec=5
WorkingDirectory=/home/emboo
KillSignal=SIGTERM
TimeoutStopSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=graphical.target
EOFSVC

echo -e "${GREEN}✓${NC} Service updated (no ydotool dependency)"
echo ""

echo -e "${BLUE}[4/4] Starting touchscreen service...${NC}"
systemctl daemon-reload
systemctl enable ft5316-touchscreen.service
systemctl start ft5316-touchscreen.service
sleep 2

echo -e "${GREEN}✓${NC} Service started"
echo ""

echo "Service status:"
systemctl status ft5316-touchscreen.service --no-pager -l | head -20
echo ""

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Fix Complete!${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""
echo "Changes made:"
echo "  ✓ Removed ydotool dependency"
echo "  ✓ Driver now uses evdev to create virtual touchscreen"
echo "  ✓ Service runs as root (needed for uinput access)"
echo "  ✓ Touch events sent directly to Linux input system"
echo ""
echo "Testing:"
echo "  1. Touch the screen - cursor should move NOW"
echo "  2. Check device: ls -l /dev/input/by-id/*FT5316*"
echo "  3. View events: sudo evtest /dev/input/by-id/*FT5316*"
echo "  4. View logs: sudo journalctl -u ft5316-touchscreen.service -f"
echo ""
echo "The touchscreen should now work immediately!"
echo ""

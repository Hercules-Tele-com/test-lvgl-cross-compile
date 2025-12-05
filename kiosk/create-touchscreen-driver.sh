#!/bin/bash
# Create FT5316 touchscreen driver manually for I2C bus 21 and user emboo

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
echo -e "${GREEN}   Creating FT5316 Touchscreen Driver (Manual Setup)${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""

echo -e "${BLUE}[1/9] Installing dependencies...${NC}"
apt-get update
apt-get install -y python3-pip python3-smbus i2c-tools git cmake libudev-dev scdoc
pip3 install --break-system-packages evdev || pip3 install evdev
echo -e "${GREEN}✓${NC} Dependencies installed"
echo ""

echo -e "${BLUE}[2/9] Installing ydotool...${NC}"
if [ ! -d /opt/ydotool ]; then
    cd /opt
    git clone https://github.com/ReimuNotMoe/ydotool.git
    cd ydotool
    mkdir -p build
    cd build
    cmake ..
    make -j$(nproc)
    make install
    echo -e "${GREEN}✓${NC} ydotool installed"
else
    echo -e "${GREEN}✓${NC} ydotool already installed"
fi
echo ""

echo -e "${BLUE}[3/9] Setting up uinput permissions...${NC}"
usermod -aG input emboo

cat > /etc/udev/rules.d/10-uinput.rules << 'EOF'
SUBSYSTEM=="misc", KERNEL=="uinput", MODE="0660", GROUP="input"
EOF

chmod 660 /dev/uinput 2>/dev/null || true
chown root:input /dev/uinput 2>/dev/null || true
udevadm control --reload-rules
udevadm trigger
echo -e "${GREEN}✓${NC} uinput permissions configured"
echo ""

echo -e "${BLUE}[4/9] Creating touchscreen Python driver...${NC}"
cat > /home/emboo/ft5316_touch.py << 'EOFPY'
#!/usr/bin/env python3
"""
FT5316 Touchscreen Driver for I2C Bus 21
Reads touch data from FT5316 controller and sends mouse events via ydotool
"""

import smbus2
import time
import os
import subprocess
from evdev import UInput, ecodes as e

# Configuration
I2C_BUS = 21  # HDMI 1 DDC bus
FT5316_ADDR = 0x38
SCREEN_WIDTH = 720
SCREEN_HEIGHT = 1280
SCALE_X = 2.0
SCALE_Y = 2.0

# FT5316 Registers
REG_NUM_TOUCHES = 0x02
REG_TOUCH1_XH = 0x03
REG_TOUCH1_XL = 0x04
REG_TOUCH1_YH = 0x05
REG_TOUCH1_YL = 0x06

class FT5316Touch:
    def __init__(self):
        self.bus = smbus2.SMBus(I2C_BUS)
        self.last_x = 0
        self.last_y = 0
        self.touching = False

    def read_touch_data(self):
        """Read touch data from FT5316"""
        try:
            # Read number of touch points
            num_touches = self.bus.read_byte_data(FT5316_ADDR, REG_NUM_TOUCHES)

            if num_touches > 0:
                # Read first touch point
                xh = self.bus.read_byte_data(FT5316_ADDR, REG_TOUCH1_XH)
                xl = self.bus.read_byte_data(FT5316_ADDR, REG_TOUCH1_XL)
                yh = self.bus.read_byte_data(FT5316_ADDR, REG_TOUCH1_YH)
                yl = self.bus.read_byte_data(FT5316_ADDR, REG_TOUCH1_YL)

                # Combine bytes
                x = ((xh & 0x0F) << 8) | xl
                y = ((yh & 0x0F) << 8) | yl

                # Scale coordinates
                x = int(x * SCALE_X)
                y = int(y * SCALE_Y)

                # Clamp to screen bounds
                x = max(0, min(x, SCREEN_WIDTH - 1))
                y = max(0, min(y, SCREEN_HEIGHT - 1))

                return (x, y, True)
            else:
                return (self.last_x, self.last_y, False)

        except Exception as e:
            print(f"Error reading touch data: {e}")
            return (self.last_x, self.last_y, False)

    def move_cursor(self, x, y):
        """Move cursor using ydotool"""
        try:
            subprocess.run(['ydotool', 'mousemove', '-a', str(x), str(y)],
                         check=False, capture_output=True)
        except Exception as e:
            print(f"Error moving cursor: {e}")

    def send_click(self, pressed):
        """Send mouse click via ydotool"""
        try:
            if pressed:
                subprocess.run(['ydotool', 'click', '0xC0'],  # Press
                             check=False, capture_output=True)
            else:
                subprocess.run(['ydotool', 'click', '0x00'],  # Release
                             check=False, capture_output=True)
        except Exception as e:
            print(f"Error sending click: {e}")

    def run(self):
        """Main loop"""
        print("FT5316 Touchscreen Driver Starting...")
        print(f"I2C Bus: {I2C_BUS}")
        print(f"FT5316 Address: 0x{FT5316_ADDR:02X}")
        print(f"Screen: {SCREEN_WIDTH}x{SCREEN_HEIGHT}")
        print(f"Scale: {SCALE_X}x, {SCALE_Y}x")
        print("")

        while True:
            try:
                x, y, touching = self.read_touch_data()

                if touching:
                    # Touch detected
                    if not self.touching:
                        # New touch - send press event
                        print(f"Touch DOWN at ({x}, {y})")
                        self.send_click(True)
                        self.touching = True

                    # Move cursor
                    if x != self.last_x or y != self.last_y:
                        self.move_cursor(x, y)
                        self.last_x = x
                        self.last_y = y

                else:
                    # No touch detected
                    if self.touching:
                        # Touch ended - send release event
                        print(f"Touch UP at ({self.last_x}, {self.last_y})")
                        self.send_click(False)
                        self.touching = False

                time.sleep(0.01)  # 100 Hz polling

            except KeyboardInterrupt:
                print("\nShutting down...")
                break
            except Exception as e:
                print(f"Error in main loop: {e}")
                time.sleep(0.1)

if __name__ == "__main__":
    driver = FT5316Touch()
    driver.run()
EOFPY

chmod +x /home/emboo/ft5316_touch.py
chown emboo:emboo /home/emboo/ft5316_touch.py
echo -e "${GREEN}✓${NC} Touchscreen driver created at /home/emboo/ft5316_touch.py"
echo ""

echo -e "${BLUE}[5/9] Creating ydotoold wrapper script...${NC}"
cat > /home/emboo/start_ydotoold.sh << 'EOFSH'
#!/bin/bash
# Start ydotoold daemon

# Kill any existing ydotoold processes
pkill -f ydotoold

# Start ydotoold
/usr/local/bin/ydotoold &

# Wait for socket to be created
sleep 2

# Set socket permissions
if [ -S /tmp/.ydotool_socket ]; then
    chmod 666 /tmp/.ydotool_socket
    echo "ydotoold started, socket permissions set"
else
    echo "Warning: ydotoold socket not found"
fi
EOFSH

chmod +x /home/emboo/start_ydotoold.sh
chown emboo:emboo /home/emboo/start_ydotoold.sh
echo -e "${GREEN}✓${NC} ydotoold wrapper created"
echo ""

echo -e "${BLUE}[6/9] Creating ydotoold service...${NC}"
cat > /etc/systemd/system/ydotoold.service << 'EOFSVC'
[Unit]
Description=Starts ydotoold Daemon
After=graphical.target

[Service]
User=root
ExecStart=/home/emboo/start_ydotoold.sh
Restart=always
WorkingDirectory=/home/emboo
KillSignal=SIGTERM
TimeoutStopSec=10

[Install]
WantedBy=graphical.target
EOFSVC

echo -e "${GREEN}✓${NC} ydotoold service created"
echo ""

echo -e "${BLUE}[7/9] Creating touchscreen service...${NC}"
cat > /etc/systemd/system/ft5316-touchscreen.service << 'EOFSVC2'
[Unit]
Description=FT5316 Touchscreen Driver
After=graphical.target multi-user.target ydotoold.service
Wants=ydotoold.service

[Service]
User=emboo
Group=emboo
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
EOFSVC2

echo -e "${GREEN}✓${NC} Touchscreen service created"
echo ""

echo -e "${BLUE}[8/9] Enabling and starting services...${NC}"
systemctl daemon-reload
systemctl enable ydotoold.service
systemctl enable ft5316-touchscreen.service

systemctl start ydotoold.service
sleep 2
systemctl start ft5316-touchscreen.service
sleep 2

echo -e "${GREEN}✓${NC} Services started"
echo ""

echo -e "${BLUE}[9/9] Checking service status...${NC}"
echo ""
echo "ydotoold status:"
systemctl status ydotoold.service --no-pager -l | head -20
echo ""
echo "touchscreen status:"
systemctl status ft5316-touchscreen.service --no-pager -l | head -20
echo ""

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Setup Complete!${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""
echo "Configuration:"
echo "  ✓ Driver: /home/emboo/ft5316_touch.py"
echo "  ✓ I2C Bus: 21 (HDMI 1)"
echo "  ✓ Touchscreen Address: 0x38"
echo "  ✓ User: emboo"
echo "  ✓ Services: ydotoold + ft5316-touchscreen"
echo ""
echo "Testing:"
echo "  1. Touch the screen - cursor should move"
echo "  2. View logs: sudo journalctl -u ft5316-touchscreen.service -f"
echo "  3. Check touchscreen detection: sudo i2cdetect -y 21"
echo ""
echo "If touchscreen doesn't work:"
echo "  - Check logs for errors"
echo "  - Verify ydotoold is running: systemctl status ydotoold"
echo "  - Test manually: sudo systemctl stop ft5316-touchscreen"
echo "                   python3 /home/emboo/ft5316_touch.py"
echo ""

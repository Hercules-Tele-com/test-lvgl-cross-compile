#!/bin/bash
# Enable USB GPS autoboot service

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}USB GPS Autoboot Setup${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""

PROJECT_DIR="/home/emboo/Projects/test-lvgl-cross-compile"
SERVICE_FILE="$PROJECT_DIR/telemetry-system/systemd/usb-gps-reader.service"

# Check if service file exists
echo -e "${YELLOW}[1/5] Checking GPS service file...${NC}"
if [ ! -f "$SERVICE_FILE" ]; then
    echo -e "${RED}✗ GPS service file not found: $SERVICE_FILE${NC}"
    exit 1
fi
echo "✓ GPS service file exists"

# Check if GPS device exists
echo ""
echo -e "${YELLOW}[2/5] Checking GPS device...${NC}"
if [ -e "/dev/ttyACM0" ]; then
    echo "✓ GPS device found at /dev/ttyACM0"
elif [ -e "/dev/ttyUSB0" ]; then
    echo "⚠ GPS device found at /dev/ttyUSB0 (not /dev/ttyACM0)"
    echo "  You may need to update the service file GPS_DEVICE environment variable"
else
    echo -e "${YELLOW}⚠ GPS device not found${NC}"
    echo "  This is normal if GPS is not currently connected"
    echo "  Available serial devices:"
    ls -la /dev/tty* | grep -E "ACM|USB" || echo "  (none found)"
fi

# Install service
echo ""
echo -e "${YELLOW}[3/5] Installing GPS service...${NC}"
sudo cp "$SERVICE_FILE" /etc/systemd/system/
sudo systemctl daemon-reload
echo "✓ GPS service installed"

# Enable service
echo ""
echo -e "${YELLOW}[4/5] Enabling GPS service...${NC}"
sudo systemctl enable usb-gps-reader.service
echo "✓ GPS service enabled (will start on boot)"

# Try to start service
echo ""
echo -e "${YELLOW}[5/5] Testing GPS service...${NC}"
if [ -e "/dev/ttyACM0" ] || [ -e "/dev/ttyUSB0" ]; then
    echo "Starting GPS service..."
    sudo systemctl start usb-gps-reader.service
    sleep 3

    if systemctl is-active --quiet usb-gps-reader.service; then
        echo -e "${GREEN}✓ GPS service is running${NC}"

        # Show recent logs
        echo ""
        echo "Recent GPS logs:"
        sudo journalctl -u usb-gps-reader.service -n 10 --no-pager
    else
        echo -e "${YELLOW}⚠ GPS service failed to start${NC}"
        echo ""
        echo "Logs:"
        sudo journalctl -u usb-gps-reader.service -n 20 --no-pager
        echo ""
        echo -e "${YELLOW}This may be normal if GPS device is not connected${NC}"
    fi
else
    echo "⚠ GPS device not connected, skipping service start"
    echo "  Service will start automatically when GPS is connected and system boots"
fi

echo ""
echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}GPS Autoboot Setup Complete!${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""
echo "Status:"
echo "  - Service enabled: ✓"
echo "  - Will start on boot: ✓"
echo "  - GPS device: $([ -e /dev/ttyACM0 ] && echo '/dev/ttyACM0' || echo 'Not connected')"
echo ""
echo "Commands:"
echo "  - Check status: ${YELLOW}systemctl status usb-gps-reader.service${NC}"
echo "  - View logs:    ${YELLOW}sudo journalctl -u usb-gps-reader.service -f${NC}"
echo "  - Restart:      ${YELLOW}sudo systemctl restart usb-gps-reader.service${NC}"
echo "  - Disable:      ${YELLOW}sudo systemctl disable usb-gps-reader.service${NC}"
echo ""

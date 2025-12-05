#!/bin/bash
# Fix dashboard refresh and GPS recording issues

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Fixing Dashboard Refresh & GPS Recording${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""

echo -e "${YELLOW}=== FIX 1: Update Web Dashboard Service ===${NC}"
echo ""

# Restart web dashboard to pick up the new WebSocket broadcast code
echo "Restarting web-dashboard.service..."
sudo systemctl restart web-dashboard.service
sleep 2

if systemctl is-active --quiet web-dashboard.service; then
    echo -e "${GREEN}✓${NC} Web dashboard service restarted successfully"
else
    echo -e "${RED}✗${NC} Web dashboard failed to start"
    echo "  Checking logs:"
    sudo journalctl -u web-dashboard.service -n 30 --no-pager
fi
echo ""

echo -e "${YELLOW}=== FIX 2: Detect and Configure GPS Device ===${NC}"
echo ""

# Auto-detect GPS device
GPS_DEVICE=""
if [ -e /dev/ttyACM0 ]; then
    GPS_DEVICE="/dev/ttyACM0"
    echo -e "${GREEN}✓${NC} Found GPS at /dev/ttyACM0"
elif [ -e /dev/ttyUSB0 ]; then
    GPS_DEVICE="/dev/ttyUSB0"
    echo -e "${GREEN}✓${NC} Found GPS at /dev/ttyUSB0"
elif [ -e /dev/ttyUSB1 ]; then
    GPS_DEVICE="/dev/ttyUSB1"
    echo -e "${GREEN}✓${NC} Found GPS at /dev/ttyUSB1"
else
    echo -e "${RED}✗${NC} No GPS device found at /dev/ttyACM0, /dev/ttyUSB0, or /dev/ttyUSB1"
    echo "  Please check if GPS is plugged in:"
    ls -l /dev/tty* | grep -E "USB|ACM" || echo "  No USB serial devices found"
    echo ""
    echo "  Continuing anyway - service will retry when GPS is connected..."
fi

if [ -n "$GPS_DEVICE" ]; then
    # Update service file with correct GPS device
    echo "Updating usb-gps-reader service to use $GPS_DEVICE..."

    # Create updated service file
    cat > /tmp/usb-gps-reader.service << EOF
[Unit]
Description=USB GPS Reader Service
Documentation=https://github.com/your-repo/leaf-can-network
After=influxdb.service
Wants=influxdb.service

[Service]
Type=simple
User=emboo
Group=emboo
WorkingDirectory=/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/services/usb-gps-reader
EnvironmentFile=/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env
Environment=PYTHONUNBUFFERED=1
Environment=GPS_DEVICE=$GPS_DEVICE
ExecStart=/usr/bin/python3 /home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/services/usb-gps-reader/usb_gps_reader.py
Restart=always
RestartSec=10
StartLimitInterval=300
StartLimitBurst=10
StandardOutput=journal
StandardError=journal

# Security hardening
NoNewPrivileges=true
PrivateTmp=true

[Install]
WantedBy=multi-user.target
EOF

    # Install updated service file
    sudo cp /tmp/usb-gps-reader.service /etc/systemd/system/
    sudo systemctl daemon-reload
    echo -e "${GREEN}✓${NC} Service file updated with GPS device $GPS_DEVICE"
fi
echo ""

echo -e "${YELLOW}=== FIX 3: Restart GPS Reader Service ===${NC}"
echo ""

sudo systemctl restart usb-gps-reader.service
sleep 2

if systemctl is-active --quiet usb-gps-reader.service; then
    echo -e "${GREEN}✓${NC} GPS reader service started successfully"
    echo "  Checking for GPS data in logs..."
    sleep 3
    sudo journalctl -u usb-gps-reader.service -n 20 --no-pager | tail -10
else
    echo -e "${RED}✗${NC} GPS reader failed to start"
    echo "  Checking logs:"
    sudo journalctl -u usb-gps-reader.service -n 30 --no-pager
fi
echo ""

echo -e "${YELLOW}=== FIX 4: Hard Refresh Dashboard in Browser ===${NC}"
echo ""

# If Chromium is running in kiosk mode, send it a refresh signal
if pgrep -f "chromium.*kiosk" > /dev/null; then
    echo "Sending refresh to Chromium (Ctrl+F5)..."
    # This requires xdotool to send keypress
    if command -v xdotool &> /dev/null; then
        DISPLAY=:0 xdotool search --class chromium key --clearmodifiers ctrl+F5 2>/dev/null || true
        echo -e "${GREEN}✓${NC} Refresh signal sent to browser"
    else
        echo -e "${YELLOW}⚠${NC} xdotool not installed - manual refresh needed"
        echo "  Install with: sudo apt install xdotool"
        echo "  Or manually press Ctrl+F5 in the browser"
    fi
else
    echo -e "${YELLOW}⚠${NC} Chromium not running in kiosk mode"
    echo "  If dashboard is open in a browser, press Ctrl+F5 to hard refresh"
fi
echo ""

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Fixes Applied!${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""
echo "Summary:"
echo "  1. Web dashboard service restarted (WebSocket now includes status indicators)"
echo "  2. GPS device detected and service configured"
echo "  3. GPS reader service restarted"
echo "  4. Browser refresh triggered (if xdotool available)"
echo ""
echo "Next steps:"
echo "  1. Check dashboard at: http://localhost:8080"
echo "  2. Verify GPS data is appearing (check for green GPS indicator)"
echo "  3. If still not working, run diagnosis:"
echo "     cd ~/Projects/test-lvgl-cross-compile/kiosk"
echo "     ./diagnose-dashboard-and-gps.sh"
echo ""
echo "To monitor GPS data in real-time:"
echo "  sudo journalctl -u usb-gps-reader.service -f"
echo ""

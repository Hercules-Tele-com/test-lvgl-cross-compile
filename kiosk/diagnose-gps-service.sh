#!/bin/bash
# Diagnose GPS service startup issues

echo "=========================================="
echo "GPS Service Diagnostics"
echo "=========================================="
echo ""

echo "1. Checking GPS device..."
if [ -e /dev/ttyACM0 ]; then
    echo "   ✓ GPS device found: /dev/ttyACM0"
    ls -l /dev/ttyACM0
else
    echo "   ✗ GPS device NOT found: /dev/ttyACM0"
    echo "   Available USB devices:"
    ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "   No USB serial devices found"
fi
echo ""

echo "2. Checking Python script..."
SCRIPT="/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/services/usb-gps-reader/usb_gps_reader.py"
if [ -f "$SCRIPT" ]; then
    echo "   ✓ Python script exists: $SCRIPT"
else
    echo "   ✗ Python script NOT found: $SCRIPT"
fi
echo ""

echo "3. Checking InfluxDB config..."
CONFIG="/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env"
if [ -f "$CONFIG" ]; then
    echo "   ✓ InfluxDB config exists: $CONFIG"
else
    echo "   ✗ InfluxDB config NOT found: $CONFIG"
fi
echo ""

echo "4. Checking InfluxDB service..."
if systemctl is-active --quiet influxdb.service; then
    echo "   ✓ InfluxDB service is running"
else
    echo "   ✗ InfluxDB service is NOT running"
    echo "   Status:"
    systemctl status influxdb.service --no-pager | head -5
fi
echo ""

echo "5. Checking service file..."
SERVICE="/etc/systemd/system/usb-gps-reader.service"
if [ -f "$SERVICE" ]; then
    echo "   ✓ Service file exists: $SERVICE"
else
    echo "   ✗ Service file NOT installed: $SERVICE"
    echo "   Looking for source:"
    find /home/emboo/Projects/test-lvgl-cross-compile -name "usb-gps-reader.service" 2>/dev/null
fi
echo ""

echo "6. Checking GPS service status..."
systemctl status usb-gps-reader.service --no-pager || echo "Service not found"
echo ""

echo "7. Checking recent GPS service logs..."
echo "   Last 20 lines:"
journalctl -u usb-gps-reader.service -n 20 --no-pager
echo ""

echo "=========================================="
echo "Diagnostics Complete"
echo "=========================================="
echo ""
echo "To fix GPS service:"
echo "1. If device not found: Check USB connection, try different port"
echo "2. If service not installed: Run telemetry-system/scripts/update-services.sh"
echo "3. If InfluxDB not running: sudo systemctl start influxdb.service"
echo "4. View live logs: sudo journalctl -u usb-gps-reader.service -f"

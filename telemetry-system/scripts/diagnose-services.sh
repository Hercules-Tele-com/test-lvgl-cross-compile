#!/bin/bash
# Service Diagnostics Script
# Checks configuration and helps troubleshoot telemetry services

echo "=== Telemetry Services Diagnostic ==="
echo ""

# Check config file
CONFIG_FILE="$HOME/Projects/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env"
echo "1. Checking InfluxDB configuration..."
if [ -f "$CONFIG_FILE" ]; then
    echo "✓ Config file exists: $CONFIG_FILE"
    echo "  Permissions: $(ls -l $CONFIG_FILE | awk '{print $1, $3, $4}')"

    # Check if tokens are set
    if grep -q "INFLUX_LOGGER_TOKEN=.\+" "$CONFIG_FILE"; then
        echo "✓ INFLUX_LOGGER_TOKEN is set"
    else
        echo "✗ INFLUX_LOGGER_TOKEN is empty or missing"
    fi

    if grep -q "INFLUX_WEB_TOKEN=.\+" "$CONFIG_FILE"; then
        echo "✓ INFLUX_WEB_TOKEN is set"
    else
        echo "✗ INFLUX_WEB_TOKEN is empty or missing"
    fi

    # Show bucket name
    BUCKET=$(grep INFLUX_BUCKET "$CONFIG_FILE" | cut -d'=' -f2)
    echo "  Bucket: $BUCKET"
else
    echo "✗ Config file not found: $CONFIG_FILE"
fi

echo ""

# Check CAN interfaces
echo "2. Checking CAN interfaces..."
for iface in can0 can1; do
    if ip link show $iface &>/dev/null; then
        STATE=$(ip -br link show $iface | awk '{print $2}')
        echo "✓ $iface: $STATE"
    else
        echo "✗ $iface: not found"
    fi
done

echo ""

# Check USB GPS
echo "3. Checking USB GPS device..."
if [ -e /dev/ttyACM0 ]; then
    echo "✓ /dev/ttyACM0 exists"
    echo "  Permissions: $(ls -l /dev/ttyACM0 | awk '{print $1, $3, $4}')"

    if groups | grep -q dialout; then
        echo "✓ User is in dialout group"
    else
        echo "✗ User not in dialout group (may need: sudo usermod -a -G dialout emboo)"
    fi
else
    echo "✗ /dev/ttyACM0 not found"
fi

echo ""

# Check InfluxDB
echo "4. Checking InfluxDB..."
if systemctl is-active influxdb &>/dev/null; then
    echo "✓ InfluxDB service is running"

    if curl -s http://localhost:8086/health &>/dev/null; then
        echo "✓ InfluxDB is accessible at :8086"
    else
        echo "✗ InfluxDB not accessible at :8086"
    fi
else
    echo "✗ InfluxDB service is not running"
fi

echo ""

# Check service statuses
echo "5. Checking service statuses..."
for service in telemetry-logger usb-gps-reader web-dashboard; do
    if systemctl is-active $service &>/dev/null; then
        echo "✓ $service: running"
    else
        STATE=$(systemctl is-active $service)
        echo "✗ $service: $STATE"

        # Show last error
        ERROR=$(journalctl -u $service -n 1 --no-pager | tail -1)
        echo "   Last log: $ERROR"
    fi
done

echo ""
echo "=== Diagnostic Complete ==="
echo ""
echo "To view service logs:"
echo "  sudo journalctl -u telemetry-logger.service -f"
echo "  sudo journalctl -u usb-gps-reader.service -f"
echo "  sudo journalctl -u web-dashboard.service -f"

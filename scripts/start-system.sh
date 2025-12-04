#!/bin/bash
# Complete system startup script for Nissan Leaf CAN Network

echo "Starting Nissan Leaf CAN Network System..."

# 1. Set up CAN buses (EMBOO Battery - 250 kbps)
# Change to 500000 for Nissan Leaf battery
echo "Setting up CAN buses at 250 kbps (EMBOO Battery)..."

# Setup can0
echo "  - Configuring can0..."
sudo ip link set can0 down 2>/dev/null
sudo ip link set can0 type can bitrate 250000
sudo ip link set can0 up

if [ $? -eq 0 ]; then
    echo "    ✓ can0 is UP"
else
    echo "    ✗ ERROR: Failed to set up can0"
fi

# Setup can1
echo "  - Configuring can1..."
sudo ip link set can1 down 2>/dev/null
sudo ip link set can1 type can bitrate 250000
sudo ip link set can1 up

if [ $? -eq 0 ]; then
    echo "    ✓ can1 is UP"
else
    echo "    ⚠ WARNING: Failed to set up can1 (may not be present)"
fi

# 2. Start InfluxDB
echo "Starting InfluxDB..."
sudo systemctl start influxdb
sleep 2

# 3. Start telemetry services
echo "Starting telemetry services..."
sudo systemctl start telemetry-logger-unified.service
sudo systemctl start cloud-sync.service
sudo systemctl start web-dashboard.service

# 4. Wait and verify
sleep 3
echo ""
echo "================================"
echo "Service Status:"
echo "================================"
systemctl status influxdb --no-pager -l | grep Active
systemctl status telemetry-logger-unified.service --no-pager -l | grep Active
systemctl status cloud-sync.service --no-pager -l | grep Active
systemctl status web-dashboard.service --no-pager -l | grep Active

echo ""
echo "================================"
echo "CAN Interfaces:"
echo "================================"
if ip link show can0 >/dev/null 2>&1; then
    ip -details link show can0 | grep -E "can0|bitrate"
else
    echo "can0: not found"
fi

if ip link show can1 >/dev/null 2>&1; then
    echo ""
    ip -details link show can1 | grep -E "can1|bitrate"
else
    echo "can1: not found"
fi

echo ""
echo "================================"
echo "System is ready!"
echo "================================"
echo "Web dashboard: http://$(hostname -I | awk '{print $1}'):8080"
echo ""
echo "To monitor CAN messages:"
echo "  candump can0"
echo "  candump can1"
echo ""
echo "To view logs:"
echo "  sudo journalctl -u telemetry-logger-unified.service -f"

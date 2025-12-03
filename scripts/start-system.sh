#!/bin/bash
# Complete system startup script for Nissan Leaf CAN Network

echo "Starting Nissan Leaf CAN Network System..."

# 1. Set up CAN bus (EMBOO Battery - 250 kbps)
# Change to 500000 for Nissan Leaf battery
echo "Setting up CAN bus at 250 kbps (EMBOO Battery)..."
sudo ip link set can0 down 2>/dev/null
sudo ip link set can0 type can bitrate 250000
sudo ip link set can0 up

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to set up CAN interface"
    exit 1
fi

# 2. Start InfluxDB
echo "Starting InfluxDB..."
sudo systemctl start influxdb
sleep 2

# 3. Start telemetry services
echo "Starting telemetry services..."
sudo systemctl start telemetry-logger.service
sudo systemctl start cloud-sync.service
sudo systemctl start web-dashboard.service

# 4. Wait and verify
sleep 3
echo ""
echo "================================"
echo "Service Status:"
echo "================================"
systemctl status influxdb --no-pager -l | grep Active
systemctl status telemetry-logger.service --no-pager -l | grep Active
systemctl status cloud-sync.service --no-pager -l | grep Active
systemctl status web-dashboard.service --no-pager -l | grep Active

echo ""
echo "================================"
echo "CAN Interface:"
echo "================================"
ip -details link show can0 | grep -E "can0|bitrate"

echo ""
echo "================================"
echo "System is ready!"
echo "================================"
echo "Web dashboard: http://$(hostname -I | awk '{print $1}'):8080"
echo ""
echo "To monitor CAN messages:"
echo "  candump can0"
echo ""
echo "To view logs:"
echo "  sudo journalctl -u telemetry-logger.service -f"

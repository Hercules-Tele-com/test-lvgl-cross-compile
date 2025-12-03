#!/bin/bash
# Complete system shutdown script for Nissan Leaf CAN Network

echo "Shutting down Nissan Leaf CAN Network System..."

# 1. Stop telemetry services
echo "Stopping telemetry services..."
sudo systemctl stop telemetry-logger.service
sudo systemctl stop cloud-sync.service
sudo systemctl stop web-dashboard.service

# 2. Stop InfluxDB
echo "Stopping InfluxDB..."
sudo systemctl stop influxdb

# 3. Bring down CAN interface
echo "Bringing down CAN interface..."
sudo ip link set can0 down

echo ""
echo "System shutdown complete."

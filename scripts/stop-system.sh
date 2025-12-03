#!/bin/bash
# Complete system shutdown script for Nissan Leaf CAN Network

echo "Shutting down Nissan Leaf CAN Network System..."

# 1. Stop telemetry services
echo "Stopping telemetry services..."
sudo systemctl stop telemetry-logger.service
sudo systemctl stop telemetry-logger-can1.service
sudo systemctl stop cloud-sync.service
sudo systemctl stop web-dashboard.service

# 2. Stop InfluxDB
echo "Stopping InfluxDB..."
sudo systemctl stop influxdb

# 3. Bring down CAN interfaces
echo "Bringing down CAN interfaces..."
sudo ip link set can0 down 2>/dev/null
sudo ip link set can1 down 2>/dev/null

echo ""
echo "System shutdown complete."

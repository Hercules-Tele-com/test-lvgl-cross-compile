#!/bin/bash
# Quick start all services manually (for testing)

echo "Starting all services..."
echo ""

sudo systemctl start influxdb.service && echo "✓ InfluxDB started" || echo "✗ InfluxDB failed"
sleep 2

sudo systemctl start telemetry-logger-unified.service && echo "✓ Telemetry logger started" || echo "✗ Telemetry logger failed"
sleep 1

sudo systemctl start usb-gps-reader.service && echo "✓ GPS reader started" || echo "✗ GPS reader failed"
sleep 1

sudo systemctl start web-dashboard.service && echo "✓ Web dashboard started" || echo "✗ Web dashboard failed"
sleep 2

sudo systemctl start dashboard-kiosk.service && echo "✓ Kiosk started" || echo "✗ Kiosk failed"

echo ""
echo "Status:"
sudo systemctl is-active --quiet influxdb.service && echo "  InfluxDB:         RUNNING" || echo "  InfluxDB:         STOPPED"
sudo systemctl is-active --quiet telemetry-logger-unified.service && echo "  Telemetry Logger: RUNNING" || echo "  Telemetry Logger: STOPPED"
sudo systemctl is-active --quiet usb-gps-reader.service && echo "  GPS Reader:       RUNNING" || echo "  GPS Reader:       STOPPED"
sudo systemctl is-active --quiet web-dashboard.service && echo "  Web Dashboard:    RUNNING" || echo "  Web Dashboard:    STOPPED"
sudo systemctl is-active --quiet dashboard-kiosk.service && echo "  Kiosk:            RUNNING" || echo "  Kiosk:            STOPPED"

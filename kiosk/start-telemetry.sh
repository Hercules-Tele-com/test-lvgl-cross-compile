#!/bin/bash
# Start all telemetry services in the correct order

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}Starting All Telemetry Services${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""

# Start InfluxDB
echo -e "${YELLOW}[1/4] Starting InfluxDB...${NC}"
sudo systemctl start influxdb.service
sleep 3
if systemctl is-active --quiet influxdb.service; then
    echo -e "${GREEN}✓ InfluxDB is running${NC}"
else
    echo -e "${RED}✗ InfluxDB failed to start${NC}"
    exit 1
fi

# Start telemetry logger (unified version preferred)
echo ""
echo -e "${YELLOW}[2/4] Starting telemetry logger...${NC}"
if systemctl list-unit-files | grep -q "telemetry-logger-unified.service"; then
    echo "Using unified telemetry logger (can0 + can1)..."
    sudo systemctl start telemetry-logger-unified.service
    sleep 2
    if systemctl is-active --quiet telemetry-logger-unified.service; then
        echo -e "${GREEN}✓ Telemetry logger (unified) is running${NC}"
    else
        echo -e "${YELLOW}⚠ Unified telemetry logger failed, trying single interface...${NC}"
        sudo systemctl start telemetry-logger.service
        sleep 2
        if systemctl is-active --quiet telemetry-logger.service; then
            echo -e "${GREEN}✓ Telemetry logger (can0) is running${NC}"
        else
            echo -e "${RED}✗ Telemetry logger failed to start${NC}"
            echo "Check logs: sudo journalctl -u telemetry-logger.service -n 20"
        fi
    fi
else
    echo "Using single-interface telemetry logger (can0)..."
    sudo systemctl start telemetry-logger.service
    sleep 2
    if systemctl is-active --quiet telemetry-logger.service; then
        echo -e "${GREEN}✓ Telemetry logger (can0) is running${NC}"
    else
        echo -e "${RED}✗ Telemetry logger failed to start${NC}"
        echo "Check logs: sudo journalctl -u telemetry-logger.service -n 20"
    fi
fi

# Start USB GPS reader
echo ""
echo -e "${YELLOW}[3/4] Starting USB GPS reader...${NC}"
if [ -e "/dev/ttyACM0" ] || [ -e "/dev/ttyUSB0" ]; then
    sudo systemctl start usb-gps-reader.service
    sleep 2
    if systemctl is-active --quiet usb-gps-reader.service; then
        echo -e "${GREEN}✓ GPS reader is running${NC}"
    else
        echo -e "${YELLOW}⚠ GPS reader failed to start (check logs)${NC}"
    fi
else
    echo -e "${YELLOW}⚠ GPS device not found, skipping${NC}"
fi

# Start web dashboard
echo ""
echo -e "${YELLOW}[4/4] Starting web dashboard...${NC}"
sudo systemctl start web-dashboard.service
sleep 3
if systemctl is-active --quiet web-dashboard.service; then
    echo -e "${GREEN}✓ Web dashboard is running${NC}"
    if curl -s http://localhost:8080 > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Web dashboard is accessible at http://localhost:8080${NC}"
    fi
else
    echo -e "${RED}✗ Web dashboard failed to start${NC}"
fi

echo ""
echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}Service Status Summary${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""

# Show status of all services
systemctl is-active --quiet influxdb.service && echo "✓ InfluxDB:           RUNNING" || echo "✗ InfluxDB:           STOPPED"
systemctl is-active --quiet telemetry-logger-unified.service && echo "✓ Telemetry Logger:   RUNNING (unified)" || \
    (systemctl is-active --quiet telemetry-logger.service && echo "✓ Telemetry Logger:   RUNNING (can0)" || echo "✗ Telemetry Logger:   STOPPED")
systemctl is-active --quiet usb-gps-reader.service && echo "✓ GPS Reader:         RUNNING" || echo "✗ GPS Reader:         STOPPED"
systemctl is-active --quiet web-dashboard.service && echo "✓ Web Dashboard:      RUNNING" || echo "✗ Web Dashboard:      STOPPED"
systemctl is-active --quiet dashboard-kiosk.service && echo "✓ Kiosk:              RUNNING" || echo "✗ Kiosk:              STOPPED"

echo ""
echo "Commands:"
echo "  - View CAN logs:  ${YELLOW}sudo journalctl -u telemetry-logger-unified.service -f${NC}"
echo "  - View GPS logs:  ${YELLOW}sudo journalctl -u usb-gps-reader.service -f${NC}"
echo "  - View dashboard: ${YELLOW}curl http://localhost:8080/api/status | python3 -m json.tool${NC}"
echo ""

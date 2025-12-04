#!/bin/bash
# Diagnose why services aren't auto-starting

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}   Autostart Diagnostics${NC}"
echo -e "${BLUE}================================================================${NC}"
echo ""

echo -e "${YELLOW}1. Checking if services are enabled...${NC}"
for service in influxdb telemetry-logger-unified usb-gps-reader web-dashboard dashboard-kiosk; do
    if systemctl is-enabled --quiet $service.service; then
        echo -e "  ${GREEN}✓${NC} $service.service is enabled"
    else
        echo -e "  ${RED}✗${NC} $service.service is NOT enabled"
    fi
done
echo ""

echo -e "${YELLOW}2. Checking InfluxDB installation and status...${NC}"
if command -v influxd &> /dev/null; then
    echo -e "  ${GREEN}✓${NC} InfluxDB is installed"
else
    echo -e "  ${RED}✗${NC} InfluxDB is NOT installed"
fi

if [ -f /usr/lib/systemd/system/influxdb.service ]; then
    echo -e "  ${GREEN}✓${NC} InfluxDB service file exists"
else
    echo -e "  ${RED}✗${NC} InfluxDB service file NOT found"
fi
echo ""

echo -e "${YELLOW}3. Checking recent boot logs for InfluxDB...${NC}"
echo "  Last 20 lines from InfluxDB:"
sudo journalctl -u influxdb.service -b --no-pager -n 20 | tail -10
echo ""

echo -e "${YELLOW}4. Checking recent boot logs for telemetry-logger...${NC}"
echo "  Last 10 lines from telemetry-logger:"
sudo journalctl -u telemetry-logger-unified.service -b --no-pager -n 10
echo ""

echo -e "${YELLOW}5. Checking if InfluxDB is set to start automatically...${NC}"
systemctl show influxdb.service -p WantedBy,RequiredBy
echo ""

echo -e "${YELLOW}6. Checking for failed units...${NC}"
systemctl --failed
echo ""

echo -e "${YELLOW}7. Checking system boot target...${NC}"
systemctl get-default
echo ""

echo -e "${YELLOW}8. Checking if InfluxDB can start manually...${NC}"
echo "  Attempting to start InfluxDB..."
sudo systemctl start influxdb.service
sleep 3
if systemctl is-active --quiet influxdb.service; then
    echo -e "  ${GREEN}✓${NC} InfluxDB started successfully!"
else
    echo -e "  ${RED}✗${NC} InfluxDB failed to start"
    echo "  Error details:"
    sudo journalctl -u influxdb.service --no-pager -n 30 | tail -15
fi
echo ""

echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}   Diagnosis Complete${NC}"
echo -e "${BLUE}================================================================${NC}"

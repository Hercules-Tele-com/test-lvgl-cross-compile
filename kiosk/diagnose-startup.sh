#!/bin/bash
# Comprehensive startup diagnostics

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "====================================="
echo "Kiosk Startup Diagnostics"
echo "====================================="
echo ""

# Check if services are enabled
echo -e "${YELLOW}Service Status:${NC}"
echo "InfluxDB:       $(systemctl is-enabled influxdb.service 2>&1) / $(systemctl is-active influxdb.service 2>&1)"
echo "Web Dashboard:  $(systemctl is-enabled web-dashboard.service 2>&1) / $(systemctl is-active web-dashboard.service 2>&1)"
echo "Kiosk:          $(systemctl is-enabled dashboard-kiosk.service 2>&1) / $(systemctl is-active dashboard-kiosk.service 2>&1)"
echo ""

# Check boot logs
echo -e "${YELLOW}InfluxDB Boot Logs:${NC}"
sudo journalctl -u influxdb.service -b --no-pager -n 20
echo ""

echo -e "${YELLOW}Web Dashboard Boot Logs:${NC}"
sudo journalctl -u web-dashboard.service -b --no-pager -n 20
echo ""

echo -e "${YELLOW}Kiosk Boot Logs:${NC}"
sudo journalctl -u dashboard-kiosk.service -b --no-pager -n 20
echo ""

# Check targets
echo -e "${YELLOW}Active Targets:${NC}"
systemctl list-units --type=target --state=active | grep -E "multi-user|graphical|network"
echo ""

# Check if display is available
echo -e "${YELLOW}Display Status:${NC}"
echo "DISPLAY variable: ${DISPLAY:-not set}"
echo "X11 processes:"
ps aux | grep -E "X|xinit|startx" | grep -v grep || echo "No X11 processes found"
echo ""

# Check dependencies
echo -e "${YELLOW}Service Dependencies:${NC}"
echo "Web dashboard dependencies:"
systemctl list-dependencies web-dashboard.service --plain | head -20
echo ""

# Try starting services manually
echo -e "${YELLOW}Attempting Manual Start:${NC}"
echo "Starting InfluxDB..."
sudo systemctl start influxdb.service
sleep 3

if systemctl is-active --quiet influxdb.service; then
    echo -e "${GREEN}✓ InfluxDB started${NC}"
else
    echo -e "${RED}✗ InfluxDB failed${NC}"
    sudo systemctl status influxdb.service --no-pager
fi
echo ""

echo "Starting web dashboard..."
sudo systemctl start web-dashboard.service
sleep 3

if systemctl is-active --quiet web-dashboard.service; then
    echo -e "${GREEN}✓ Web dashboard started${NC}"
    if curl -s http://localhost:8080 > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Web dashboard is accessible${NC}"
    else
        echo -e "${RED}✗ Web dashboard not accessible${NC}"
    fi
else
    echo -e "${RED}✗ Web dashboard failed${NC}"
    sudo systemctl status web-dashboard.service --no-pager
fi
echo ""

# Check environment file
echo -e "${YELLOW}Environment Configuration:${NC}"
ENV_FILE="/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env"
if [ -f "$ENV_FILE" ]; then
    echo "✓ Config file exists: $ENV_FILE"
    echo "  Contents (without secrets):"
    cat "$ENV_FILE" | grep -v TOKEN | head -5
else
    echo "✗ Config file missing: $ENV_FILE"
fi
echo ""

echo "====================================="
echo "Diagnostic Complete"
echo "====================================="

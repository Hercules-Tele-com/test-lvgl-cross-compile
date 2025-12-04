#!/bin/bash
# Start all services immediately (for testing)

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}Starting All Services${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""

# Start InfluxDB
echo -e "${YELLOW}[1/3] Starting InfluxDB...${NC}"
sudo systemctl start influxdb.service
sleep 3

if systemctl is-active --quiet influxdb.service; then
    echo -e "${GREEN}✓ InfluxDB is running${NC}"
else
    echo -e "${RED}✗ InfluxDB failed to start${NC}"
    sudo journalctl -u influxdb.service -n 20
    exit 1
fi

# Start web dashboard
echo ""
echo -e "${YELLOW}[2/3] Starting web dashboard...${NC}"
sudo systemctl start web-dashboard.service
sleep 5

if systemctl is-active --quiet web-dashboard.service; then
    echo -e "${GREEN}✓ Web dashboard is running${NC}"

    if curl -s http://localhost:8080 > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Web dashboard is accessible at http://localhost:8080${NC}"
    else
        echo -e "${YELLOW}⚠ Web dashboard is running but not responding yet${NC}"
    fi
else
    echo -e "${RED}✗ Web dashboard failed to start${NC}"
    sudo journalctl -u web-dashboard.service -n 20
    exit 1
fi

# Start kiosk
echo ""
echo -e "${YELLOW}[3/3] Starting kiosk mode...${NC}"
sudo systemctl start dashboard-kiosk.service
sleep 3

if systemctl is-active --quiet dashboard-kiosk.service; then
    echo -e "${GREEN}✓ Kiosk mode is running${NC}"
else
    echo -e "${YELLOW}⚠ Kiosk mode may still be starting (check display)${NC}"
    echo "Check status: systemctl status dashboard-kiosk.service"
fi

echo ""
echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}All Services Started${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""
echo "Status:"
systemctl status influxdb.service web-dashboard.service dashboard-kiosk.service --no-pager | grep -E "Loaded|Active" || true
echo ""
echo "The kiosk should now be visible on the display."
echo "Access web dashboard from any device: http://$(hostname -I | awk '{print $1}'):8080"
echo ""

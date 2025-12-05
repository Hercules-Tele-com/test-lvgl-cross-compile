#!/bin/bash
# Fix autostart issues - Ensures all services start on boot

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERROR: This script must be run as root (use sudo)${NC}"
    exit 1
fi

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Fixing Autostart Configuration${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""

echo -e "${BLUE}[1/6] Checking InfluxDB service...${NC}"
if [ -f /usr/lib/systemd/system/influxdb.service ] || [ -f /lib/systemd/system/influxdb.service ]; then
    echo "  ✓ InfluxDB service file found"
else
    echo -e "  ${RED}✗ InfluxDB service file not found!${NC}"
    echo "  Please install InfluxDB first:"
    echo "    cd ../telemetry-system && ./scripts/setup-influxdb.sh"
    exit 1
fi
echo ""

echo -e "${BLUE}[2/6] Unmasking and resetting InfluxDB...${NC}"
systemctl unmask influxdb.service 2>/dev/null || true
systemctl daemon-reload
echo "  ✓ InfluxDB service unmasked and reloaded"
echo ""

echo -e "${BLUE}[3/6] Ensuring InfluxDB starts before our services...${NC}"
# Check if InfluxDB has proper After= ordering
if ! grep -q "After=.*network.target" /usr/lib/systemd/system/influxdb.service 2>/dev/null && \
   ! grep -q "After=.*network.target" /lib/systemd/system/influxdb.service 2>/dev/null; then
    echo "  ⚠ InfluxDB may need network.target dependency"
fi
echo "  ✓ InfluxDB dependencies checked"
echo ""

echo -e "${BLUE}[4/6] Re-enabling all services...${NC}"
systemctl disable influxdb.service 2>/dev/null || true
systemctl disable telemetry-logger-unified.service 2>/dev/null || true
systemctl disable usb-gps-reader.service 2>/dev/null || true
systemctl disable web-dashboard.service 2>/dev/null || true
systemctl disable dashboard-kiosk.service 2>/dev/null || true
systemctl disable gps-time-sync.service 2>/dev/null || true

systemctl daemon-reload

systemctl enable influxdb.service
systemctl enable telemetry-logger-unified.service
systemctl enable usb-gps-reader.service
systemctl enable web-dashboard.service
systemctl enable dashboard-kiosk.service
[ -f /etc/systemd/system/gps-time-sync.service ] && systemctl enable gps-time-sync.service || true

echo "  ✓ All services re-enabled"
echo ""

echo -e "${BLUE}[5/6] Verifying service links were created...${NC}"
if [ -L /etc/systemd/system/multi-user.target.wants/influxdb.service ]; then
    echo "  ✓ InfluxDB enabled for multi-user.target"
else
    echo -e "  ${RED}✗ InfluxDB link NOT found${NC}"
    echo "  Creating manually..."
    ln -sf /usr/lib/systemd/system/influxdb.service /etc/systemd/system/multi-user.target.wants/influxdb.service 2>/dev/null || \
    ln -sf /lib/systemd/system/influxdb.service /etc/systemd/system/multi-user.target.wants/influxdb.service
fi

for service in telemetry-logger-unified usb-gps-reader web-dashboard; do
    if [ -L /etc/systemd/system/multi-user.target.wants/$service.service ]; then
        echo "  ✓ $service enabled for multi-user.target"
    else
        echo -e "  ${RED}✗ $service link NOT found${NC}"
    fi
done

if [ -L /etc/systemd/system/graphical.target.wants/dashboard-kiosk.service ]; then
    echo "  ✓ dashboard-kiosk enabled for graphical.target"
else
    echo -e "  ${RED}✗ dashboard-kiosk link NOT found${NC}"
fi
echo ""

echo -e "${BLUE}[6/6] Testing if services can start...${NC}"
echo "  Starting InfluxDB..."
systemctl start influxdb.service
sleep 3

if systemctl is-active --quiet influxdb.service; then
    echo -e "  ${GREEN}✓ InfluxDB is running${NC}"

    echo "  Starting telemetry services..."
    systemctl start telemetry-logger-unified.service
    systemctl start usb-gps-reader.service
    sleep 2

    echo "  Starting web dashboard..."
    systemctl start web-dashboard.service
    sleep 3

    if systemctl is-active --quiet web-dashboard.service; then
        echo -e "  ${GREEN}✓ Web dashboard is running${NC}"

        if [ -n "$DISPLAY" ]; then
            echo "  Starting kiosk..."
            systemctl start dashboard-kiosk.service
            sleep 2

            if systemctl is-active --quiet dashboard-kiosk.service; then
                echo -e "  ${GREEN}✓ Kiosk is running${NC}"
            fi
        else
            echo "  ⚠ No DISPLAY set, skipping kiosk start (will work after graphical login)"
        fi
    else
        echo -e "  ${RED}✗ Web dashboard failed to start${NC}"
        journalctl -u web-dashboard.service -n 20 --no-pager
    fi
else
    echo -e "  ${RED}✗ InfluxDB failed to start${NC}"
    echo "  Checking logs:"
    journalctl -u influxdb.service -n 30 --no-pager
fi
echo ""

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Fix Complete!${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""
echo "Current service status:"
systemctl status influxdb telemetry-logger-unified usb-gps-reader web-dashboard dashboard-kiosk --no-pager || true
echo ""
echo -e "${YELLOW}Please reboot to verify autostart works:${NC}"
echo -e "  ${BLUE}sudo reboot${NC}"
echo ""

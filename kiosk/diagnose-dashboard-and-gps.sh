#!/bin/bash
# Diagnose dashboard refresh and GPS issues

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}   Dashboard Refresh & GPS Diagnostics${NC}"
echo -e "${BLUE}================================================================${NC}"
echo ""

echo -e "${YELLOW}=== PART 1: Dashboard Refresh Issues ===${NC}"
echo ""

echo -e "${YELLOW}1. Checking if web dashboard is running...${NC}"
if systemctl is-active --quiet web-dashboard.service; then
    echo -e "  ${GREEN}✓${NC} Web dashboard service is running"
    echo "  Listening on:"
    sudo netstat -tlnp 2>/dev/null | grep :8080 || ss -tlnp 2>/dev/null | grep :8080
else
    echo -e "  ${RED}✗${NC} Web dashboard service is NOT running"
    echo "  Recent logs:"
    sudo journalctl -u web-dashboard.service -n 20 --no-pager
fi
echo ""

echo -e "${YELLOW}2. Testing web dashboard HTTP endpoint...${NC}"
if curl -s http://localhost:8080/api/status > /tmp/dashboard_status.json; then
    echo -e "  ${GREEN}✓${NC} Dashboard is responding to HTTP requests"
    echo "  Sample response:"
    cat /tmp/dashboard_status.json | head -c 500
    echo ""
else
    echo -e "  ${RED}✗${NC} Dashboard is NOT responding"
fi
echo ""

echo -e "${YELLOW}3. Checking WebSocket support...${NC}"
if grep -q "socketio" /home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/services/web-dashboard/app.py; then
    echo -e "  ${GREEN}✓${NC} WebSocket (socket.io) is configured in app.py"
else
    echo -e "  ${RED}✗${NC} WebSocket support not found in app.py"
fi
echo ""

echo -e "${YELLOW}4. Checking if Chromium is running...${NC}"
if pgrep -f chromium > /dev/null; then
    echo -e "  ${GREEN}✓${NC} Chromium is running"
    echo "  Processes:"
    pgrep -fa chromium | head -5
else
    echo -e "  ${RED}✗${NC} Chromium is NOT running"
fi
echo ""

echo -e "${YELLOW}=== PART 2: GPS Recording Issues ===${NC}"
echo ""

echo -e "${YELLOW}5. Checking USB GPS device...${NC}"
if ls /dev/ttyUSB* 2>/dev/null || ls /dev/ttyACM* 2>/dev/null; then
    echo -e "  ${GREEN}✓${NC} USB GPS device found:"
    ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
else
    echo -e "  ${RED}✗${NC} No USB GPS device found (/dev/ttyUSB* or /dev/ttyACM*)"
fi
echo ""

echo -e "${YELLOW}6. Checking usb-gps-reader service...${NC}"
if systemctl is-active --quiet usb-gps-reader.service; then
    echo -e "  ${GREEN}✓${NC} usb-gps-reader service is running"
    echo "  Recent logs:"
    sudo journalctl -u usb-gps-reader.service -n 15 --no-pager
else
    echo -e "  ${RED}✗${NC} usb-gps-reader service is NOT running"
    echo "  Status:"
    systemctl status usb-gps-reader.service --no-pager -n 10
fi
echo ""

echo -e "${YELLOW}7. Checking CAN bus for GPS messages...${NC}"
if ip link show can0 > /dev/null 2>&1; then
    echo -e "  ${GREEN}✓${NC} CAN interface can0 exists"
    echo "  Monitoring CAN bus for GPS messages (0x710, 0x711, 0x712) for 5 seconds..."
    timeout 5 candump can0 | grep -E "710|711|712" &
    wait $!
    if [ $? -eq 124 ]; then
        echo -e "  ${YELLOW}⚠${NC} Timeout - no GPS messages seen in 5 seconds"
    fi
else
    echo -e "  ${RED}✗${NC} CAN interface can0 not found"
fi
echo ""

echo -e "${YELLOW}8. Checking telemetry logger for GPS parsing...${NC}"
if systemctl is-active --quiet telemetry-logger-unified.service; then
    echo -e "  ${GREEN}✓${NC} Telemetry logger service is running"
    echo "  Recent GPS-related logs:"
    sudo journalctl -u telemetry-logger-unified.service -n 50 --no-pager | grep -i "gps\|0x710\|0x711\|0x712" || echo "  No GPS messages in recent logs"
else
    echo -e "  ${RED}✗${NC} Telemetry logger service is NOT running"
fi
echo ""

echo -e "${YELLOW}9. Checking InfluxDB for GPS data...${NC}"
if [ -f /home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env ]; then
    source /home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env
    echo "  Querying GPS measurement from InfluxDB..."
    influx query 'from(bucket:"'$INFLUX_BUCKET'") |> range(start: -1h) |> filter(fn: (r) => r._measurement == "GPS") |> limit(n:5)' \
        --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN" 2>&1 | head -20
else
    echo -e "  ${RED}✗${NC} InfluxDB config not found"
fi
echo ""

echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}   Diagnosis Complete${NC}"
echo -e "${BLUE}================================================================${NC}"
echo ""
echo "Common issues:"
echo "  1. Dashboard not refreshing: WebSocket connection failed or service stopped"
echo "  2. GPS not recording: Device not found, service not running, or CAN bus down"
echo ""

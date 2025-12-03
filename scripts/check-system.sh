#!/bin/bash
# Quick system health check for Nissan Leaf CAN Network

echo "==================================================================="
echo "  Nissan Leaf CAN Network System Status"
echo "==================================================================="
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "CAN Interfaces:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check can0
if ip link show can0 >/dev/null 2>&1; then
    echo "can0:"
    ip -details link show can0 | grep -E "can0|bitrate|state"
    echo ""
    echo "  Statistics:"
    ip -details -statistics link show can0 | grep -E "RX|TX|errors" | sed 's/^/  /'
else
    echo "✗ can0: not found"
fi
echo ""

# Check can1
if ip link show can1 >/dev/null 2>&1; then
    echo "can1:"
    ip -details link show can1 | grep -E "can1|bitrate|state"
    echo ""
    echo "  Statistics:"
    ip -details -statistics link show can1 | grep -E "RX|TX|errors" | sed 's/^/  /'
else
    echo "⚠ can1: not found (may not be present)"
fi
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Services:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
printf "%-25s %s\n" "InfluxDB:" "$(systemctl is-active influxdb 2>/dev/null || echo 'inactive')"
printf "%-25s %s\n" "Telemetry Logger (can0):" "$(systemctl is-active telemetry-logger.service 2>/dev/null || echo 'inactive')"
printf "%-25s %s\n" "Telemetry Logger (can1):" "$(systemctl is-active telemetry-logger-can1.service 2>/dev/null || echo 'inactive')"
printf "%-25s %s\n" "Cloud Sync:" "$(systemctl is-active cloud-sync.service 2>/dev/null || echo 'inactive')"
printf "%-25s %s\n" "Web Dashboard:" "$(systemctl is-active web-dashboard.service 2>/dev/null || echo 'inactive')"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "CAN Message Activity (5 second sample):"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
# Check can0
if ip link show can0 >/dev/null 2>&1 && [ "$(ip link show can0 | grep -c 'state UP')" -eq 1 ]; then
    MSG_COUNT=$(timeout 5 candump can0 2>/dev/null | wc -l)
    if [ "$MSG_COUNT" -gt 0 ]; then
        echo "can0: ✓ Received $MSG_COUNT messages in 5 seconds (~$((MSG_COUNT/5)) msg/sec)"
    else
        echo "can0: ⚠ No messages received in 5 seconds"
    fi
else
    echo "can0: ✗ Interface is down"
fi

# Check can1
if ip link show can1 >/dev/null 2>&1 && [ "$(ip link show can1 | grep -c 'state UP')" -eq 1 ]; then
    MSG_COUNT=$(timeout 5 candump can1 2>/dev/null | wc -l)
    if [ "$MSG_COUNT" -gt 0 ]; then
        echo "can1: ✓ Received $MSG_COUNT messages in 5 seconds (~$((MSG_COUNT/5)) msg/sec)"
    else
        echo "can1: ⚠ No messages received in 5 seconds"
    fi
else
    echo "can1: ✗ Interface is down or not present"
fi
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Web Dashboard:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if curl -s http://localhost:8080/api/status >/dev/null 2>&1; then
    IP_ADDR=$(hostname -I | awk '{print $1}')
    echo "✓ Web dashboard accessible at http://$IP_ADDR:8080"
else
    echo "✗ Web dashboard not responding"
fi
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "System Resources:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "CPU Temperature: $(vcgencmd measure_temp 2>/dev/null || echo 'N/A')"
echo "Memory Usage:"
free -h | grep -E "Mem:|Swap:"
echo ""
echo "Disk Usage:"
df -h / | tail -1 | awk '{printf "  Root: %s used of %s (%s)\n", $3, $2, $5}'
echo ""

echo "==================================================================="
echo "  End of Status Report"
echo "==================================================================="

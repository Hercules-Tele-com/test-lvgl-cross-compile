#!/bin/bash
# Telemetry System Test Script

set -e

echo "=== Nissan Leaf Telemetry System Test ==="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test functions
test_pass() {
    echo -e "${GREEN}✓${NC} $1"
}

test_fail() {
    echo -e "${RED}✗${NC} $1"
    exit 1
}

test_warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# 1. Check CAN interface
echo "1. Checking CAN interface..."
if ip link show can0 &>/dev/null; then
    if ip link show can0 | grep -q "UP"; then
        test_pass "CAN interface can0 is UP"
    else
        test_fail "CAN interface can0 is DOWN"
    fi
else
    test_fail "CAN interface can0 not found"
fi

# 2. Check InfluxDB
echo ""
echo "2. Checking InfluxDB..."
if systemctl is-active --quiet influxdb; then
    test_pass "InfluxDB service is running"

    if curl -s http://localhost:8086/health | grep -q "pass"; then
        test_pass "InfluxDB health check passed"
    else
        test_fail "InfluxDB health check failed"
    fi
else
    test_fail "InfluxDB service is not running"
fi

# 3. Check configuration files
echo ""
echo "3. Checking configuration files..."
CONFIG_DIR="$(dirname "$0")/../config"

if [ -f "$CONFIG_DIR/influxdb-local.env" ]; then
    test_pass "Local InfluxDB config exists"
else
    test_fail "Local InfluxDB config missing"
fi

if [ -f "$CONFIG_DIR/influxdb-cloud.env" ]; then
    test_pass "Cloud InfluxDB config exists"
else
    test_warn "Cloud InfluxDB config missing (optional)"
fi

# 4. Check Python dependencies
echo ""
echo "4. Checking Python dependencies..."

check_python_package() {
    if python3 -c "import $1" 2>/dev/null; then
        test_pass "Python package '$1' installed"
    else
        test_fail "Python package '$1' not installed"
    fi
}

check_python_package "can"
check_python_package "influxdb_client"
check_python_package "flask"
check_python_package "flask_socketio"

# 5. Check services
echo ""
echo "5. Checking systemd services..."

check_service() {
    local service=$1
    if systemctl is-enabled --quiet "$service" 2>/dev/null; then
        if systemctl is-active --quiet "$service"; then
            test_pass "Service '$service' is enabled and running"
        else
            test_warn "Service '$service' is enabled but not running"
        fi
    else
        test_warn "Service '$service' is not enabled"
    fi
}

check_service "telemetry-logger.service"
check_service "cloud-sync.service"
check_service "web-dashboard.service"

# 6. Test CAN communication (if mock generator is available)
echo ""
echo "6. Testing CAN communication..."

# Start mock generator in background
MOCK_PID=""
if [ -f "$(dirname "$0")/mock_can_generator.py" ]; then
    python3 "$(dirname "$0")/mock_can_generator.py" can0 &
    MOCK_PID=$!
    sleep 2

    # Check for CAN traffic
    if timeout 5 candump can0 -n 10 &>/dev/null; then
        test_pass "CAN messages detected"
    else
        test_warn "No CAN messages detected"
    fi

    # Kill mock generator
    if [ -n "$MOCK_PID" ]; then
        kill $MOCK_PID 2>/dev/null || true
    fi
else
    test_warn "Mock CAN generator not found, skipping CAN test"
fi

# 7. Test InfluxDB data
echo ""
echo "7. Testing InfluxDB data..."

# Source config
if [ -f "$CONFIG_DIR/influxdb-local.env" ]; then
    source "$CONFIG_DIR/influxdb-local.env"

    # Query recent data
    QUERY='from(bucket:"'$INFLUX_BUCKET'") |> range(start: -5m) |> limit(n:1)'

    if influx query "$QUERY" --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN" &>/dev/null; then
        test_pass "InfluxDB query successful"
    else
        test_warn "No recent data in InfluxDB (might be normal if just started)"
    fi
fi

# 8. Test web dashboard
echo ""
echo "8. Testing web dashboard..."

WEB_PORT=${WEB_PORT:-8080}

if curl -s http://localhost:$WEB_PORT/api/status &>/dev/null; then
    test_pass "Web dashboard API is responding"
else
    test_warn "Web dashboard API is not responding"
fi

# Summary
echo ""
echo "=== Test Summary ==="
echo "Tests completed. Check output above for any failures or warnings."
echo ""
echo "To view logs:"
echo "  sudo journalctl -u telemetry-logger.service -f"
echo "  sudo journalctl -u cloud-sync.service -f"
echo "  sudo journalctl -u web-dashboard.service -f"
echo ""
echo "Web Dashboard: http://$(hostname -I | awk '{print $1}'):$WEB_PORT"
echo ""

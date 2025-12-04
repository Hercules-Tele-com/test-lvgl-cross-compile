#!/bin/bash
# Clear InfluxDB data to fix field type conflicts
# This script stops services, clears InfluxDB data, and restarts services

echo "================================"
echo "InfluxDB Data Clear Tool"
echo "================================"
echo ""
echo "⚠️  WARNING: This will delete ALL telemetry data!"
echo ""
echo "This is useful when:"
echo "  - Fixing InfluxDB field type conflicts (422 errors)"
echo "  - Changing CAN message data formats"
echo "  - Starting fresh with clean data"
echo ""
read -p "Are you sure you want to continue? (yes/no): " confirm

if [ "$confirm" != "yes" ]; then
    echo "Aborted."
    exit 0
fi

# Load InfluxDB configuration
CONFIG_DIR="$(dirname "$0")/../config"
if [ ! -f "$CONFIG_DIR/influxdb-local.env" ]; then
    echo "Error: Configuration file not found: $CONFIG_DIR/influxdb-local.env"
    echo "Please run setup-influxdb.sh first"
    exit 1
fi

source "$CONFIG_DIR/influxdb-local.env"

echo ""
echo "Step 1: Stopping telemetry services..."
sudo systemctl stop telemetry-logger-unified.service 2>/dev/null
sudo systemctl stop telemetry-logger.service 2>/dev/null
sudo systemctl stop telemetry-logger-can1.service 2>/dev/null
sudo systemctl stop cloud-sync.service 2>/dev/null

echo "✓ Services stopped"
sleep 2

echo ""
echo "Step 2: Deleting all data from bucket: $INFLUX_BUCKET"
echo ""

# Delete all data using the delete API (requires write permission)
# Delete data from 1970 to 2100 (covers all possible timestamps)
START_TIME="1970-01-01T00:00:00Z"
END_TIME="2100-01-01T00:00:00Z"

# Delete measurements one by one (InfluxDB 2.x doesn't support bulk delete easily)
# New schema uses device families as measurements
MEASUREMENTS=("Battery" "Motor" "Inverter" "GPS" "Vehicle")

for measurement in "${MEASUREMENTS[@]}"; do
    echo "  Deleting $measurement..."
    curl -s -X POST "$INFLUX_URL/api/v2/delete?org=$INFLUX_ORG&bucket=$INFLUX_BUCKET" \
        -H "Authorization: Token $INFLUX_LOGGER_TOKEN" \
        -H "Content-Type: application/json" \
        -d "{
            \"start\": \"$START_TIME\",
            \"stop\": \"$END_TIME\",
            \"predicate\": \"_measurement=\\\"$measurement\\\"\"
        }" >/dev/null 2>&1
done

echo "✓ Data deleted"

echo ""
echo "Step 3: Restarting telemetry services..."
sudo systemctl start telemetry-logger-unified.service
sudo systemctl start cloud-sync.service

sleep 3

echo "✓ Services restarted"

echo ""
echo "================================"
echo "Done! InfluxDB data cleared"
echo "================================"
echo ""
echo "Check if it's working:"
echo "  sudo journalctl -u telemetry-logger-unified.service -f"
echo ""
echo "You should see:"
echo "  - No more 422 errors"
echo "  - [can0] Stats and [can1] Stats"
echo "  - Fresh data being written"
echo ""

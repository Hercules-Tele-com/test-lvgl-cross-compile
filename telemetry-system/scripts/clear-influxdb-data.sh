#!/bin/bash
# Clear InfluxDB data to fix field type conflicts
# This is useful when changing field types in the telemetry logger

echo "⚠️  WARNING: This will delete ALL telemetry data in InfluxDB!"
echo ""
read -p "Are you sure you want to continue? (yes/no): " confirm

if [ "$confirm" != "yes" ]; then
    echo "Aborted."
    exit 0
fi

# Load InfluxDB configuration
source "$(dirname "$0")/../config/influxdb-local.env"

echo ""
echo "Stopping telemetry logger..."
sudo systemctl stop telemetry-logger-unified.service 2>/dev/null
sudo systemctl stop telemetry-logger.service 2>/dev/null
sudo systemctl stop telemetry-logger-can1.service 2>/dev/null

sleep 2

echo "Deleting bucket: $INFLUX_BUCKET"
influx bucket delete \
    --name "$INFLUX_BUCKET" \
    --org "$INFLUX_ORG" \
    --token "$INFLUX_ADMIN_TOKEN" \
    --skip-verify

echo "Recreating bucket: $INFLUX_BUCKET"
influx bucket create \
    --name "$INFLUX_BUCKET" \
    --org "$INFLUX_ORG" \
    --retention 30d \
    --token "$INFLUX_ADMIN_TOKEN" \
    --skip-verify

echo ""
echo "✓ InfluxDB data cleared successfully!"
echo ""
echo "Restarting telemetry logger..."
sudo systemctl start telemetry-logger-unified.service

echo ""
echo "Done! Field type conflicts should be resolved."
echo "Check logs: sudo journalctl -u telemetry-logger-unified.service -f"

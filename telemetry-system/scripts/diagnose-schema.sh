#!/bin/bash
# Diagnostic script to check InfluxDB schema and data

set -e

SCRIPT_DIR="$(dirname "$0")"
CONFIG_FILE="$SCRIPT_DIR/../config/influxdb-local.env"

if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: Config file not found: $CONFIG_FILE"
    exit 1
fi

source "$CONFIG_FILE"

echo "================================"
echo "InfluxDB Schema Diagnostics"
echo "================================"
echo ""

echo "1. Checking measurements..."
influx query 'import "influxdata/influxdb/schema"
schema.measurements(bucket: "'$INFLUX_BUCKET'")' \
  --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN" 2>/dev/null | grep -v "^$" | head -20

echo ""
echo "2. Checking Battery measurement (last 5 records)..."
influx query 'from(bucket:"'$INFLUX_BUCKET'")
  |> range(start: -10m)
  |> filter(fn: (r) => r._measurement == "Battery")
  |> limit(n: 5)' \
  --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN" 2>/dev/null | head -30

echo ""
echo "3. Checking Battery fields available..."
influx query 'import "influxdata/influxdb/schema"
schema.measurementFieldKeys(
  bucket: "'$INFLUX_BUCKET'",
  measurement: "Battery"
)' \
  --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN" 2>/dev/null | head -30

echo ""
echo "4. Checking Battery tags..."
influx query 'import "influxdata/influxdb/schema"
schema.measurementTagKeys(
  bucket: "'$INFLUX_BUCKET'",
  measurement: "Battery"
)' \
  --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN" 2>/dev/null | head -20

echo ""
echo "5. Testing query with pivot (like web dashboard)..."
influx query 'from(bucket: "'$INFLUX_BUCKET'")
  |> range(start: -1h)
  |> filter(fn: (r) => r["_measurement"] == "Battery")
  |> last()
  |> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value")' \
  --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN" 2>/dev/null | head -40

echo ""
echo "6. Checking Motor measurement..."
influx query 'from(bucket:"'$INFLUX_BUCKET'")
  |> range(start: -10m)
  |> filter(fn: (r) => r._measurement == "Motor")
  |> limit(n: 3)' \
  --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN" 2>/dev/null | head -20

echo ""
echo "7. Checking Inverter measurement..."
influx query 'from(bucket:"'$INFLUX_BUCKET'")
  |> range(start: -10m)
  |> filter(fn: (r) => r._measurement == "Inverter")
  |> limit(n: 3)' \
  --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN" 2>/dev/null | head -20

echo ""
echo "================================"
echo "Diagnostics Complete"
echo "================================"

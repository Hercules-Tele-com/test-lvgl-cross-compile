#!/bin/bash
set -e

echo "=== InfluxDB 2.x Setup for Nissan Leaf Telemetry ==="

# Install InfluxDB 2.x
if ! command -v influx &> /dev/null; then
    echo "Installing InfluxDB 2.x..."

    # Add InfluxData repository
    wget -q https://repos.influxdata.com/influxdata-archive_compat.key
    echo '393e8779c89ac8d958f81f942f9ad7fb82a25e133faddaf92e15b16e6ac9ce4c influxdata-archive_compat.key' | sha256sum -c && cat influxdata-archive_compat.key | gpg --dearmor | sudo tee /etc/apt/trusted.gpg.d/influxdata-archive_compat.gpg > /dev/null
    echo 'deb [signed-by=/etc/apt/trusted.gpg.d/influxdata-archive_compat.gpg] https://repos.influxdata.com/debian stable main' | sudo tee /etc/apt/sources.list.d/influxdata.list

    sudo apt-get update
    sudo apt-get install -y influxdb2 influxdb2-cli

    # Enable and start InfluxDB
    sudo systemctl enable influxdb
    sudo systemctl start influxdb

    echo "Waiting for InfluxDB to start..."
    sleep 5
else
    echo "InfluxDB already installed"
fi

# Setup InfluxDB (if not already configured)
if [ ! -f ~/.influxdbv2/configs ]; then
    echo ""
    echo "=== InfluxDB Initial Setup ==="
    echo "Please provide the following information:"

    read -p "Username (default: admin): " INFLUX_USER
    INFLUX_USER=${INFLUX_USER:-admin}

    read -sp "Password (min 8 chars): " INFLUX_PASSWORD
    echo ""

    read -p "Organization (default: leaf-telemetry): " INFLUX_ORG
    INFLUX_ORG=${INFLUX_ORG:-leaf-telemetry}

    read -p "Bucket name (default: leaf-data): " INFLUX_BUCKET
    INFLUX_BUCKET=${INFLUX_BUCKET:-leaf-data}

    read -p "Retention period in days (default: 30): " RETENTION_DAYS
    RETENTION_DAYS=${RETENTION_DAYS:-30}

    # Run setup
    influx setup \
        --username "$INFLUX_USER" \
        --password "$INFLUX_PASSWORD" \
        --org "$INFLUX_ORG" \
        --bucket "$INFLUX_BUCKET" \
        --retention "${RETENTION_DAYS}d" \
        --force

    echo ""
    echo "=== Creating API Tokens ==="

    # Create token for telemetry logger (write-only)
    LOGGER_TOKEN=$(influx auth create \
        --org "$INFLUX_ORG" \
        --write-bucket "$INFLUX_BUCKET" \
        --description "Telemetry Logger Write Token" \
        --json | jq -r '.token')

    # Create token for cloud sync (read-only)
    SYNC_TOKEN=$(influx auth create \
        --org "$INFLUX_ORG" \
        --read-bucket "$INFLUX_BUCKET" \
        --description "Cloud Sync Read Token" \
        --json | jq -r '.token')

    # Create token for web dashboard (read-only)
    WEB_TOKEN=$(influx auth create \
        --org "$INFLUX_ORG" \
        --read-bucket "$INFLUX_BUCKET" \
        --description "Web Dashboard Read Token" \
        --json | jq -r '.token')

    # Save configuration
    CONFIG_DIR="$(dirname "$0")/../config"
    mkdir -p "$CONFIG_DIR"

    cat > "$CONFIG_DIR/influxdb-local.env" <<EOF
# Local InfluxDB Configuration
INFLUX_URL=http://localhost:8086
INFLUX_ORG=$INFLUX_ORG
INFLUX_BUCKET=$INFLUX_BUCKET
INFLUX_LOGGER_TOKEN=$LOGGER_TOKEN
INFLUX_SYNC_TOKEN=$SYNC_TOKEN
INFLUX_WEB_TOKEN=$WEB_TOKEN
EOF

    chmod 600 "$CONFIG_DIR/influxdb-local.env"

    echo ""
    echo "=== Configuration saved to: $CONFIG_DIR/influxdb-local.env ==="
    echo ""
    echo "IMPORTANT: Create influxdb-cloud.env with your cloud credentials:"
    echo "  INFLUX_CLOUD_URL=https://your-cloud-instance.influxdata.com"
    echo "  INFLUX_CLOUD_ORG=your-cloud-org"
    echo "  INFLUX_CLOUD_BUCKET=your-cloud-bucket"
    echo "  INFLUX_CLOUD_TOKEN=your-cloud-write-token"
    echo ""
else
    echo "InfluxDB already configured"
    echo "Configuration location: ~/.influxdbv2/configs"
fi

echo ""
echo "=== Setup Complete ==="
echo "InfluxDB UI available at: http://localhost:8086"
echo ""

#!/bin/bash
# Fix InfluxDB autostart issue

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}InfluxDB Autostart Fix${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""

# Check if InfluxDB is installed
echo -e "${YELLOW}[1/5] Checking InfluxDB installation...${NC}"
if ! command -v influxd &> /dev/null; then
    echo -e "${RED}✗ InfluxDB is not installed${NC}"
    echo "Please install InfluxDB first:"
    echo "  cd ~/Projects/test-lvgl-cross-compile/telemetry-system"
    echo "  ./scripts/setup-influxdb.sh"
    exit 1
fi

echo "✓ InfluxDB is installed: $(influxd version | head -1)"

# Check current service status
echo ""
echo -e "${YELLOW}[2/5] Checking current service status...${NC}"
if systemctl is-enabled --quiet influxdb.service; then
    echo "✓ InfluxDB service is enabled"
else
    echo "⚠ InfluxDB service is not enabled, enabling now..."
    sudo systemctl enable influxdb.service
fi

# Check for any systemd overrides
echo ""
echo -e "${YELLOW}[3/5] Checking for systemd overrides...${NC}"
OVERRIDE_DIR="/etc/systemd/system/influxdb.service.d"
if [ -d "$OVERRIDE_DIR" ]; then
    echo "Found override directory: $OVERRIDE_DIR"
    ls -la "$OVERRIDE_DIR"
    cat "$OVERRIDE_DIR"/*.conf 2>/dev/null || true
else
    echo "No overrides found (this is normal)"
fi

# Create systemd override to ensure InfluxDB starts
echo ""
echo -e "${YELLOW}[4/5] Creating systemd override for reliable startup...${NC}"

sudo mkdir -p "$OVERRIDE_DIR"

cat > /tmp/influxdb-override.conf << 'EOF'
[Unit]
# Ensure InfluxDB starts after network is online
After=network-online.target
Wants=network-online.target

[Service]
# Add a small delay to ensure filesystem is ready
ExecStartPre=/bin/sleep 2

# Increase restart attempts
Restart=on-failure
RestartSec=5
StartLimitInterval=200
StartLimitBurst=10

# Ensure data directory exists and has correct permissions
ExecStartPre=/bin/mkdir -p /var/lib/influxdb
ExecStartPre=/bin/chown influxdb:influxdb /var/lib/influxdb
EOF

sudo cp /tmp/influxdb-override.conf "$OVERRIDE_DIR/override.conf"
sudo systemctl daemon-reload

echo "✓ Created systemd override at $OVERRIDE_DIR/override.conf"

# Test starting InfluxDB
echo ""
echo -e "${YELLOW}[5/5] Testing InfluxDB startup...${NC}"

# Stop if running
sudo systemctl stop influxdb.service 2>/dev/null || true
sleep 2

# Start fresh
echo "Starting InfluxDB..."
sudo systemctl start influxdb.service

# Wait and check
sleep 5

if systemctl is-active --quiet influxdb.service; then
    echo -e "${GREEN}✓ InfluxDB started successfully${NC}"

    # Test connectivity
    if curl -s http://localhost:8086/health > /dev/null 2>&1; then
        echo -e "${GREEN}✓ InfluxDB is responding on port 8086${NC}"
    else
        echo -e "${YELLOW}⚠ InfluxDB is running but not responding yet (may need more time)${NC}"
    fi
else
    echo -e "${RED}✗ InfluxDB failed to start${NC}"
    echo ""
    echo "Checking logs:"
    sudo journalctl -u influxdb.service -n 30 --no-pager
    exit 1
fi

echo ""
echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}Fix Complete!${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""
echo "Changes made:"
echo "  1. Enabled InfluxDB service"
echo "  2. Created systemd override for better startup reliability"
echo "  3. Added 2-second delay before starting"
echo "  4. Added automatic restart on failure"
echo "  5. Ensured data directory exists with correct permissions"
echo ""
echo "InfluxDB is now running and will start on boot."
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "  1. Start web dashboard: ${YELLOW}sudo systemctl start web-dashboard.service${NC}"
echo "  2. Start kiosk: ${YELLOW}sudo systemctl start dashboard-kiosk.service${NC}"
echo "  3. Reboot to test full autostart: ${YELLOW}sudo reboot${NC}"
echo ""

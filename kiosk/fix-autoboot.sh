#!/bin/bash
# Fix web dashboard autoboot issues

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}Web Dashboard Autoboot Fix${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""

PROJECT_DIR="/home/emboo/Projects/test-lvgl-cross-compile"

# Step 1: Check if InfluxDB is installed and running
echo -e "${YELLOW}[1/5] Checking InfluxDB...${NC}"
if systemctl is-active --quiet influxdb; then
    echo "✓ InfluxDB is running"
elif systemctl is-enabled --quiet influxdb; then
    echo "⚠ InfluxDB is installed but not running"
    echo "  Starting InfluxDB..."
    sudo systemctl start influxdb
else
    echo -e "${RED}✗ InfluxDB is not installed or not enabled${NC}"
    echo "  You need to set up InfluxDB first:"
    echo "    cd $PROJECT_DIR/telemetry-system"
    echo "    ./scripts/setup-influxdb.sh"
    exit 1
fi

# Step 2: Check if InfluxDB config exists
echo ""
echo -e "${YELLOW}[2/5] Checking InfluxDB configuration...${NC}"
CONFIG_FILE="$PROJECT_DIR/telemetry-system/config/influxdb-local.env"
if [ -f "$CONFIG_FILE" ]; then
    echo "✓ InfluxDB config exists: $CONFIG_FILE"
else
    echo -e "${RED}✗ InfluxDB config missing: $CONFIG_FILE${NC}"
    echo "  You need to set up the telemetry system first:"
    echo "    cd $PROJECT_DIR/telemetry-system"
    echo "    ./scripts/setup-influxdb.sh"
    exit 1
fi

# Step 3: Update web-dashboard.service with better dependencies
echo ""
echo -e "${YELLOW}[3/5] Updating web-dashboard.service...${NC}"

cat > /tmp/web-dashboard.service << 'EOF'
[Unit]
Description=Nissan Leaf Web Dashboard
Documentation=https://github.com/Hercules-Tele-com/test-lvgl-cross-compile
After=network-online.target influxdb.service
Wants=network-online.target
Requires=influxdb.service

[Service]
Type=simple
User=emboo
Group=emboo
WorkingDirectory=/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/services/web-dashboard
EnvironmentFile=-/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env
Environment=PYTHONUNBUFFERED=1
Environment=WEB_PORT=8080
Environment=SECRET_KEY=change-this-in-production
ExecStartPre=/bin/sleep 3
ExecStart=/usr/bin/python3 /home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/services/web-dashboard/app.py
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=read-only
ReadWritePaths=/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system

[Install]
WantedBy=multi-user.target
EOF

sudo cp /tmp/web-dashboard.service /etc/systemd/system/web-dashboard.service
sudo systemctl daemon-reload
echo "✓ Service file updated"

# Step 4: Update dashboard-kiosk.service with longer delay
echo ""
echo -e "${YELLOW}[4/5] Updating dashboard-kiosk.service...${NC}"

cat > /tmp/dashboard-kiosk.service << 'EOF'
[Unit]
Description=Leaf Dashboard Kiosk Mode
Documentation=https://github.com/Hercules-Tele-com/test-lvgl-cross-compile
After=web-dashboard.service graphical.target
Wants=web-dashboard.service graphical.target
Requires=web-dashboard.service

[Service]
Type=simple
User=emboo
Group=emboo
Environment=DISPLAY=:0
Environment=XAUTHORITY=/home/emboo/.Xauthority
ExecStartPre=/bin/sleep 15
ExecStart=/usr/bin/chromium \
    --kiosk \
    --noerrdialogs \
    --disable-infobars \
    --disable-session-crashed-bubble \
    --disable-features=TranslateUI \
    --disable-component-update \
    --check-for-update-interval=31536000 \
    --disable-backgrounding-occluded-windows \
    --disable-breakpad \
    --disable-component-extensions-with-background-pages \
    --disable-extensions \
    --disable-features=Translate \
    --disable-ipc-flooding-protection \
    --disable-renderer-backgrounding \
    --force-device-scale-factor=1 \
    --app=http://localhost:8080
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

# Security
NoNewPrivileges=true
PrivateTmp=true

[Install]
WantedBy=graphical.target
EOF

sudo cp /tmp/dashboard-kiosk.service /etc/systemd/system/dashboard-kiosk.service
sudo systemctl daemon-reload
echo "✓ Kiosk service file updated"

# Step 5: Enable services
echo ""
echo -e "${YELLOW}[5/5] Enabling services...${NC}"
sudo systemctl enable influxdb.service
sudo systemctl enable web-dashboard.service
sudo systemctl enable dashboard-kiosk.service
echo "✓ All services enabled"

# Test web dashboard manually
echo ""
echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}Testing Services${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""

# Start services in order
echo "Starting InfluxDB..."
sudo systemctl start influxdb.service
sleep 2

echo "Starting web dashboard..."
sudo systemctl start web-dashboard.service
sleep 5

# Check if web dashboard is accessible
echo ""
echo "Testing web dashboard..."
if curl -s http://localhost:8080 > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Web dashboard is accessible at http://localhost:8080${NC}"
else
    echo -e "${RED}✗ Web dashboard is not accessible${NC}"
    echo "Checking logs..."
    sudo journalctl -u web-dashboard.service -n 20
    exit 1
fi

echo ""
echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}Fix Complete!${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""
echo "What was fixed:"
echo "  1. Added InfluxDB as a required dependency for web dashboard"
echo "  2. Changed to network-online.target (more reliable)"
echo "  3. Added 3-second delay before starting web dashboard"
echo "  4. Added 15-second delay before starting kiosk (was 10)"
echo "  5. Made EnvironmentFile optional (- prefix)"
echo "  6. Added ReadWritePaths for telemetry-system directory"
echo ""
echo "Next steps:"
echo "  1. Reboot to test autoboot: ${YELLOW}sudo reboot${NC}"
echo "  2. After reboot, check services: ${YELLOW}systemctl status web-dashboard dashboard-kiosk${NC}"
echo ""
echo "If still having issues after reboot:"
echo "  - Check logs: ${YELLOW}sudo journalctl -u web-dashboard.service -b${NC}"
echo "  - Check logs: ${YELLOW}sudo journalctl -u dashboard-kiosk.service -b${NC}"

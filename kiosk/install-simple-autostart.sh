#!/bin/bash
# Install simple, reliable autostart configuration
# This removes strict dependencies and uses a delayed startup approach

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}Installing Simple Autostart${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""

# Step 1: Create simple web-dashboard service (no strict dependencies)
echo -e "${YELLOW}[1/4] Creating simplified web-dashboard service...${NC}"

cat > /tmp/web-dashboard.service << 'EOF'
[Unit]
Description=Nissan Leaf Web Dashboard
Documentation=https://github.com/Hercules-Tele-com/test-lvgl-cross-compile
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=emboo
Group=emboo
WorkingDirectory=/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/services/web-dashboard
EnvironmentFile=-/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env
Environment=PYTHONUNBUFFERED=1
Environment=WEB_PORT=8080
Environment=SECRET_KEY=change-this-in-production

# Wait for InfluxDB to be ready (simple retry loop)
ExecStartPre=/bin/bash -c 'for i in {1..30}; do systemctl is-active --quiet influxdb.service && break || sleep 2; done'

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

sudo cp /tmp/web-dashboard.service /etc/systemd/system/
echo "✓ Web dashboard service created"

# Step 2: Create simple kiosk service (no strict dependencies)
echo ""
echo -e "${YELLOW}[2/4] Creating simplified kiosk service...${NC}"

cat > /tmp/dashboard-kiosk.service << 'EOF'
[Unit]
Description=Leaf Dashboard Kiosk Mode
Documentation=https://github.com/Hercules-Tele-com/test-lvgl-cross-compile
After=graphical.target web-dashboard.service
Wants=graphical.target

[Service]
Type=simple
User=emboo
Group=emboo
Environment=DISPLAY=:0
Environment=XAUTHORITY=/home/emboo/.Xauthority

# Wait for web dashboard to be ready (simple retry loop)
ExecStartPre=/bin/bash -c 'for i in {1..30}; do curl -s http://localhost:8080 > /dev/null && break || sleep 2; done'

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

sudo cp /tmp/dashboard-kiosk.service /etc/systemd/system/
echo "✓ Kiosk service created"

# Step 3: Reload and enable services
echo ""
echo -e "${YELLOW}[3/4] Reloading systemd and enabling services...${NC}"
sudo systemctl daemon-reload

# Make absolutely sure InfluxDB is enabled
sudo systemctl enable influxdb.service

# Enable our services
sudo systemctl enable web-dashboard.service
sudo systemctl enable dashboard-kiosk.service

echo "✓ All services enabled"

# Step 4: Test the configuration
echo ""
echo -e "${YELLOW}[4/4] Testing configuration...${NC}"

# Stop any running instances
sudo systemctl stop dashboard-kiosk.service 2>/dev/null || true
sudo systemctl stop web-dashboard.service 2>/dev/null || true

# Start InfluxDB if not running
if ! systemctl is-active --quiet influxdb.service; then
    echo "Starting InfluxDB..."
    sudo systemctl start influxdb.service
    sleep 3
fi

# Start web dashboard
echo "Starting web dashboard..."
sudo systemctl start web-dashboard.service

# Wait and check
sleep 8

if systemctl is-active --quiet web-dashboard.service; then
    echo -e "${GREEN}✓ Web dashboard is running${NC}"

    if curl -s http://localhost:8080 > /dev/null 2>&1; then
        echo -e "${GREEN}✓ Web dashboard is accessible${NC}"
    else
        echo -e "${RED}✗ Web dashboard is not accessible${NC}"
    fi
else
    echo -e "${RED}✗ Web dashboard failed to start${NC}"
    sudo journalctl -u web-dashboard.service -n 20
fi

echo ""
echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}Installation Complete!${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""
echo "Changes made:"
echo "  1. Removed strict Requires= dependencies"
echo "  2. Added retry loops in ExecStartPre (waits up to 60 seconds)"
echo "  3. Simplified dependency chain"
echo "  4. Web dashboard waits for InfluxDB to be active"
echo "  5. Kiosk waits for web dashboard to respond on port 8080"
echo ""
echo "Services are now enabled and will start on boot."
echo ""
echo -e "${YELLOW}Next step: Reboot to test autostart${NC}"
echo "  sudo reboot"
echo ""
echo "After reboot, verify:"
echo "  systemctl status influxdb web-dashboard dashboard-kiosk"
echo ""

#!/bin/bash
# Complete System Setup - Configure ALL services for reliable autostart
# This is the ONE script to rule them all

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Nissan Leaf Telemetry System - Complete Setup${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""
echo "This script will configure ALL services for automatic startup"
echo "with proper dependencies, restart policies, and monitoring."
echo ""

PROJECT_DIR="/home/emboo/Projects/test-lvgl-cross-compile"
USER="emboo"

if [ "$EUID" -eq 0 ]; then
    echo -e "${RED}ERROR: Do not run as root. Run as user 'emboo'.${NC}"
    exit 1
fi

echo -e "${BLUE}[Step 1/8] Creating optimized service files with restart policies...${NC}"
echo ""

# ============================================================================
# 1. InfluxDB - Database (no changes needed, already has restart policy)
# ============================================================================
echo "✓ InfluxDB - using system service (already has restart policy)"

# ============================================================================
# 2. Telemetry Logger - Reads CAN bus, writes to InfluxDB
# ============================================================================
echo "Creating: telemetry-logger-unified.service"
cat > /tmp/telemetry-logger-unified.service << 'EOF'
[Unit]
Description=Unified CAN Telemetry Logger (can0 + can1)
Documentation=https://github.com/Hercules-Tele-com/test-lvgl-cross-compile
After=influxdb.service
Wants=influxdb.service

[Service]
Type=simple
User=emboo
Group=emboo
WorkingDirectory=/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/services/telemetry-logger
EnvironmentFile=-/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env
Environment=PYTHONUNBUFFERED=1
Environment=CAN_INTERFACE=can0,can1

# Wait for InfluxDB to be ready (local service, no network needed)
ExecStartPre=/bin/bash -c 'for i in {1..30}; do systemctl is-active --quiet influxdb.service && break || sleep 2; done'

ExecStart=/usr/bin/python3 /home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/services/telemetry-logger/telemetry_logger.py

# Aggressive restart policy
Restart=always
RestartSec=5
StartLimitInterval=300
StartLimitBurst=10

StandardOutput=journal
StandardError=journal

# Security
NoNewPrivileges=true
PrivateTmp=true

[Install]
WantedBy=multi-user.target
EOF

sudo cp /tmp/telemetry-logger-unified.service /etc/systemd/system/

# ============================================================================
# 3. USB GPS Reader - Reads GPS, writes to InfluxDB
# ============================================================================
echo "Creating: usb-gps-reader.service"
cat > /tmp/usb-gps-reader.service << 'EOF'
[Unit]
Description=USB GPS Reader Service
Documentation=https://github.com/Hercules-Tele-com/test-lvgl-cross-compile
After=influxdb.service
Wants=influxdb.service

[Service]
Type=simple
User=emboo
Group=emboo
WorkingDirectory=/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/services/usb-gps-reader
EnvironmentFile=-/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env
Environment=PYTHONUNBUFFERED=1
Environment=GPS_DEVICE=/dev/ttyACM0

# Wait for InfluxDB to be ready (local service, no network needed)
ExecStartPre=/bin/bash -c 'for i in {1..30}; do systemctl is-active --quiet influxdb.service && break || sleep 2; done'

ExecStart=/usr/bin/python3 /home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/services/usb-gps-reader/usb_gps_reader.py

# Restart always (even if GPS not connected, will retry)
Restart=always
RestartSec=10
StartLimitInterval=300
StartLimitBurst=10

StandardOutput=journal
StandardError=journal

# Security
NoNewPrivileges=true
PrivateTmp=true

[Install]
WantedBy=multi-user.target
EOF

sudo cp /tmp/usb-gps-reader.service /etc/systemd/system/

# ============================================================================
# 4. Web Dashboard - Flask web server
# ============================================================================
echo "Creating: web-dashboard.service"
cat > /tmp/web-dashboard.service << 'EOF'
[Unit]
Description=Nissan Leaf Web Dashboard
Documentation=https://github.com/Hercules-Tele-com/test-lvgl-cross-compile
After=influxdb.service
Wants=influxdb.service

[Service]
Type=simple
User=emboo
Group=emboo
WorkingDirectory=/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/services/web-dashboard
EnvironmentFile=-/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env
Environment=PYTHONUNBUFFERED=1
Environment=WEB_PORT=8080
Environment=WEB_HOST=0.0.0.0
Environment=SECRET_KEY=change-this-in-production

# Wait for InfluxDB to be ready (local service, no network needed)
ExecStartPre=/bin/bash -c 'for i in {1..30}; do systemctl is-active --quiet influxdb.service && break || sleep 2; done'

ExecStart=/usr/bin/python3 /home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/services/web-dashboard/app.py

# Aggressive restart policy
Restart=always
RestartSec=5
StartLimitInterval=300
StartLimitBurst=10

StandardOutput=journal
StandardError=journal

# Security
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=read-only
ReadWritePaths=/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system

[Install]
WantedBy=multi-user.target
EOF

sudo cp /tmp/web-dashboard.service /etc/systemd/system/

# ============================================================================
# 5. Dashboard Kiosk - Chromium in kiosk mode
# ============================================================================
echo "Creating: dashboard-kiosk.service"
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

# Wait for web dashboard to be ready
ExecStartPre=/bin/bash -c 'for i in {1..60}; do curl -s http://localhost:8080 > /dev/null && break || sleep 2; done'

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

# Aggressive restart policy
Restart=always
RestartSec=10
StartLimitInterval=300
StartLimitBurst=10

StandardOutput=journal
StandardError=journal

# Security
NoNewPrivileges=true
PrivateTmp=true

[Install]
WantedBy=graphical.target
EOF

sudo cp /tmp/dashboard-kiosk.service /etc/systemd/system/

echo "✓ All service files created with restart policies"
echo ""

# ============================================================================
# Step 2: Remove problematic InfluxDB overrides
# ============================================================================
echo -e "${BLUE}[Step 2/8] Cleaning up InfluxDB configuration...${NC}"

# Remove any override.conf that might be blocking InfluxDB from starting
if [ -d "/etc/systemd/system/influxdb.service.d" ]; then
    echo "Removing InfluxDB override (let service use defaults)"
    sudo rm -rf /etc/systemd/system/influxdb.service.d
fi

sudo systemctl daemon-reload
echo "✓ InfluxDB configuration cleaned"
echo ""

# ============================================================================
# Step 3: Set up GPS time sync (works offline)
# ============================================================================
echo -e "${BLUE}[Step 3/8] Setting up GPS time sync...${NC}"

GPS_TIME_SYNC_SERVICE="$PROJECT_DIR/telemetry-system/systemd/gps-time-sync.service"
if [ -f "$GPS_TIME_SYNC_SERVICE" ]; then
    sudo cp "$GPS_TIME_SYNC_SERVICE" /etc/systemd/system/
    sudo systemctl daemon-reload
    sudo systemctl enable gps-time-sync.service
    echo "✓ GPS time sync configured (uses GPS for accurate time when offline)"
else
    echo "⚠ GPS time sync service not found, skipping"
fi
echo ""

# ============================================================================
# Step 4: Enable all services
# ============================================================================
echo -e "${BLUE}[Step 4/8] Enabling all services for autostart...${NC}"
sudo systemctl enable influxdb.service
sudo systemctl enable telemetry-logger-unified.service
sudo systemctl enable usb-gps-reader.service
sudo systemctl enable web-dashboard.service
sudo systemctl enable dashboard-kiosk.service
echo "✓ All services enabled for autostart"
echo ""

# ============================================================================
# Step 5: Configure boot to desktop
# ============================================================================
echo -e "${BLUE}[Step 5/8] Configuring boot to desktop...${NC}"
sudo raspi-config nonint do_boot_behaviour B4
echo "✓ Configured to boot to desktop with autologin"
echo ""

# ============================================================================
# Step 5: Configure X11 autostart
# ============================================================================
echo -e "${BLUE}[Step 6/8] Configuring X11 autostart...${NC}"
AUTOSTART_DIR="$HOME/.config/lxsession/LXDE-pi"
mkdir -p "$AUTOSTART_DIR"

cat > "$AUTOSTART_DIR/autostart" << 'EOF'
@xset s off
@xset -dpms
@xset s noblank
@unclutter -idle 0.1 -root
EOF

chmod +x "$AUTOSTART_DIR/autostart"
echo "✓ X11 autostart configured"
echo ""

# ============================================================================
# Step 6: Configure sudo for health monitoring (passwordless)
# ============================================================================
echo -e "${BLUE}[Step 7/8] Configuring sudo for health monitoring...${NC}"

# Create sudoers file for passwordless service control
cat > /tmp/emboo-services << 'EOFSUDO'
# Allow emboo user to manage telemetry services without password
emboo ALL=(ALL) NOPASSWD: /bin/systemctl start influxdb.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl start telemetry-logger-unified.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl start usb-gps-reader.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl start web-dashboard.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl start dashboard-kiosk.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl restart influxdb.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl restart telemetry-logger-unified.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl restart usb-gps-reader.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl restart web-dashboard.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl restart dashboard-kiosk.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl stop influxdb.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl stop telemetry-logger-unified.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl stop usb-gps-reader.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl stop web-dashboard.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl stop dashboard-kiosk.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl status influxdb.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl status telemetry-logger-unified.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl status usb-gps-reader.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl status web-dashboard.service
emboo ALL=(ALL) NOPASSWD: /bin/systemctl status dashboard-kiosk.service
EOFSUDO

sudo cp /tmp/emboo-services /etc/sudoers.d/emboo-services
sudo chmod 0440 /etc/sudoers.d/emboo-services

# Create health monitor script
cat > "$PROJECT_DIR/kiosk/health-check.sh" << 'EOFHEALTH'
#!/bin/bash
# System Health Check - Ensures all services are running
# Run this via cron every 5 minutes

SERVICES=(
    "influxdb.service"
    "telemetry-logger-unified.service"
    "usb-gps-reader.service"
    "web-dashboard.service"
    "dashboard-kiosk.service"
)

for service in "${SERVICES[@]}"; do
    if ! systemctl is-active --quiet "$service"; then
        echo "$(date): $service is not running, restarting..."
        sudo systemctl start "$service"
    fi
done
EOFHEALTH

chmod +x "$PROJECT_DIR/kiosk/health-check.sh"

# Add cron job for health monitoring
(crontab -l 2>/dev/null | grep -v "health-check.sh"; echo "*/5 * * * * $PROJECT_DIR/kiosk/health-check.sh >> /tmp/health-check.log 2>&1") | crontab -

echo "✓ Health monitor configured with passwordless sudo"
echo ""

# ============================================================================
# Step 7: Start all services now
# ============================================================================
echo -e "${BLUE}[Step 8/8] Starting all services...${NC}"
echo ""

echo "Starting InfluxDB..."
sudo systemctl start influxdb.service
sleep 3

echo "Starting telemetry logger..."
sudo systemctl start telemetry-logger-unified.service
sleep 2

echo "Starting GPS reader..."
sudo systemctl start usb-gps-reader.service
sleep 2

echo "Starting web dashboard..."
sudo systemctl start web-dashboard.service
sleep 5

echo "Starting kiosk..."
sudo systemctl start dashboard-kiosk.service
sleep 2

echo ""
echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}   Setup Complete!${NC}"
echo -e "${GREEN}================================================================${NC}"
echo ""

# Show status
echo "Service Status:"
echo ""
systemctl is-active --quiet influxdb.service && echo -e "  ${GREEN}✓${NC} InfluxDB:         RUNNING" || echo -e "  ${RED}✗${NC} InfluxDB:         STOPPED"
systemctl is-active --quiet telemetry-logger-unified.service && echo -e "  ${GREEN}✓${NC} Telemetry Logger: RUNNING" || echo -e "  ${RED}✗${NC} Telemetry Logger: STOPPED"
systemctl is-active --quiet usb-gps-reader.service && echo -e "  ${GREEN}✓${NC} GPS Reader:       RUNNING" || echo -e "  ${RED}✗${NC} GPS Reader:       STOPPED"
systemctl is-active --quiet web-dashboard.service && echo -e "  ${GREEN}✓${NC} Web Dashboard:    RUNNING" || echo -e "  ${RED}✗${NC} Web Dashboard:    STOPPED"
systemctl is-active --quiet dashboard-kiosk.service && echo -e "  ${GREEN}✓${NC} Kiosk:            RUNNING" || echo -e "  ${RED}✗${NC} Kiosk:            STOPPED"

echo ""
echo "What was configured:"
echo "  • All services have Restart=always (auto-restart on failure)"
echo "  • All services have StartLimitBurst=10 (up to 10 restart attempts)"
echo "  • Proper startup order with retry loops (wait up to 60s for dependencies)"
echo "  • Boot to desktop with autologin"
echo "  • Screen blanking disabled"
echo "  • Mouse cursor auto-hide"
echo "  • Health monitor (checks every 5 minutes, restarts failed services)"
echo ""
echo "Monitoring:"
echo "  • Health check log: tail -f /tmp/health-check.log"
echo "  • View all logs:    sudo journalctl -f"
echo "  • Check status:     systemctl status influxdb telemetry-logger-unified usb-gps-reader web-dashboard dashboard-kiosk"
echo ""
echo -e "${YELLOW}Reboot to verify everything starts automatically:${NC}"
echo -e "  ${BLUE}sudo reboot${NC}"
echo ""

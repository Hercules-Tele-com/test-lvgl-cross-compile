#!/bin/bash
# EMBOO Kiosk Mode Setup Script
# Sets up Raspberry Pi for full-screen kiosk mode with web dashboard

set -e

echo "================================================"
echo "EMBOO Kiosk Mode Setup"
echo "================================================"
echo ""

# Check if running on Raspberry Pi
if ! grep -q "Raspberry Pi" /proc/cpuinfo 2>/dev/null; then
    echo "⚠️  Warning: This doesn't appear to be a Raspberry Pi"
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Function to check if service exists and is active
check_service() {
    local service=$1
    if systemctl is-active --quiet "$service"; then
        echo "✓ $service is running"
        return 0
    else
        echo "✗ $service is not running"
        return 1
    fi
}

echo "Step 1: Checking system requirements..."
echo "----------------------------------------"

# Check for required packages
REQUIRED_PACKAGES="chromium-browser x11-xserver-utils unclutter python3-flask"
MISSING_PACKAGES=""

for pkg in $REQUIRED_PACKAGES; do
    if ! dpkg -l | grep -q "^ii  $pkg"; then
        MISSING_PACKAGES="$MISSING_PACKAGES $pkg"
    fi
done

if [ ! -z "$MISSING_PACKAGES" ]; then
    echo "⚠️  Missing packages:$MISSING_PACKAGES"
    echo ""
    read -p "Install missing packages? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        sudo apt update
        sudo apt install -y $MISSING_PACKAGES
        echo "✓ Packages installed"
    else
        echo "Skipping package installation"
    fi
else
    echo "✓ All required packages installed"
fi

echo ""
echo "Step 2: Checking services..."
echo "----------------------------------------"

check_service "web-dashboard.service" || echo "  (Run telemetry-system setup first)"
check_service "telemetry-logger-unified.service" || echo "  (Optional: CAN data logging)"

echo ""
echo "Step 3: Configuring autostart..."
echo "----------------------------------------"

AUTOSTART_DIR="$HOME/.config/lxsession/LXDE-pi"
AUTOSTART_FILE="$AUTOSTART_DIR/autostart"

# Create directory if it doesn't exist
mkdir -p "$AUTOSTART_DIR"

# Backup existing autostart
if [ -f "$AUTOSTART_FILE" ]; then
    cp "$AUTOSTART_FILE" "$AUTOSTART_FILE.backup.$(date +%Y%m%d_%H%M%S)"
    echo "✓ Backed up existing autostart file"
fi

# Detect browser
if command -v chromium &> /dev/null; then
    BROWSER="chromium"
elif command -v chromium-browser &> /dev/null; then
    BROWSER="chromium-browser"
else
    echo "✗ Error: Neither 'chromium' nor 'chromium-browser' found!"
    exit 1
fi

# Create autostart configuration
cat > "$AUTOSTART_FILE" << EOF
@xset s off
@xset -dpms
@xset s noblank
@unclutter -idle 0.1 -root
@$BROWSER --kiosk --app=http://localhost:8080
EOF

echo "✓ Created autostart configuration"
echo "  Browser: $BROWSER"
echo "  URL: http://localhost:8080"

echo ""
echo "Step 4: Configuring X11 settings..."
echo "----------------------------------------"

# Disable screen blanking in lightdm
LIGHTDM_CONF="/etc/lightdm/lightdm.conf"
if [ -f "$LIGHTDM_CONF" ]; then
    if sudo grep -q "^xserver-command=X -s 0 -dpms" "$LIGHTDM_CONF"; then
        echo "✓ Screen blanking already disabled in lightdm"
    else
        echo "⚠️  Adding screen blanking disable to lightdm.conf"
        sudo sed -i '/^\[Seat:\*\]/a xserver-command=X -s 0 -dpms' "$LIGHTDM_CONF"
        echo "✓ Updated lightdm configuration"
    fi
fi

echo ""
echo "Step 5: Testing web dashboard..."
echo "----------------------------------------"

if curl -s http://localhost:8080/api/status > /dev/null; then
    echo "✓ Web dashboard is accessible"
else
    echo "✗ Web dashboard is not responding"
    echo "  Start it with: sudo systemctl start web-dashboard"
fi

echo ""
echo "================================================"
echo "Setup Complete!"
echo "================================================"
echo ""
echo "Next steps:"
echo "1. Reboot to start kiosk mode automatically:"
echo "   sudo reboot"
echo ""
echo "2. Or test kiosk mode now (without reboot):"
echo "   ./launch-kiosk.sh"
echo ""
echo "3. To disable autostart later:"
echo "   rm ~/.config/lxsession/LXDE-pi/autostart"
echo ""
echo "4. View setup documentation:"
echo "   cat KIOSK-SETUP.md"
echo ""

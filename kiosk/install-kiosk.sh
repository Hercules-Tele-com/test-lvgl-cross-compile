#!/bin/bash
# Install and configure kiosk mode for Leaf Dashboard
# Designed for Victron Cerbo 50 display (or any HDMI touchscreen)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}Leaf Dashboard Kiosk Setup${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    echo -e "${RED}ERROR: Do not run this script as root (sudo)${NC}"
    echo "This script will use sudo when needed"
    exit 1
fi

# Get the project directory
PROJECT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
KIOSK_DIR="$PROJECT_DIR/kiosk"
USER=$(whoami)

echo -e "${YELLOW}Project directory: $PROJECT_DIR${NC}"
echo -e "${YELLOW}Kiosk directory: $KIOSK_DIR${NC}"
echo -e "${YELLOW}User: $USER${NC}"
echo ""

# Step 1: Install required packages
echo -e "${GREEN}[1/6] Installing required packages...${NC}"
if ! command -v chromium &> /dev/null; then
    echo "Installing chromium..."
    sudo apt update
    sudo apt install -y chromium unclutter x11-xserver-utils
else
    echo "✓ Chromium already installed"
    # Make sure unclutter is installed
    sudo apt install -y unclutter x11-xserver-utils
fi

# Step 2: Install systemd service
echo ""
echo -e "${GREEN}[2/6] Installing systemd service...${NC}"
if [ -f "$KIOSK_DIR/dashboard-kiosk.service" ]; then
    sudo cp "$KIOSK_DIR/dashboard-kiosk.service" /etc/systemd/system/
    sudo systemctl daemon-reload
    echo "✓ Systemd service installed"
else
    echo -e "${RED}ERROR: dashboard-kiosk.service not found in $KIOSK_DIR${NC}"
    exit 1
fi

# Step 3: Configure X11 autostart
echo ""
echo -e "${GREEN}[3/6] Configuring X11 autostart...${NC}"
AUTOSTART_DIR="$HOME/.config/lxsession/LXDE-pi"
mkdir -p "$AUTOSTART_DIR"

if [ -f "$KIOSK_DIR/autostart" ]; then
    cp "$KIOSK_DIR/autostart" "$AUTOSTART_DIR/autostart"
    chmod +x "$AUTOSTART_DIR/autostart"
    echo "✓ X11 autostart configured at: $AUTOSTART_DIR/autostart"
else
    echo -e "${YELLOW}WARNING: autostart file not found, creating minimal version${NC}"
    cat > "$AUTOSTART_DIR/autostart" << 'EOF'
@xset s off
@xset -dpms
@xset s noblank
@unclutter -idle 0.1 -root
EOF
    chmod +x "$AUTOSTART_DIR/autostart"
fi

# Step 4: Configure boot to desktop
echo ""
echo -e "${GREEN}[4/6] Configuring boot to desktop...${NC}"
echo "Ensuring Pi boots to desktop (not console)..."
sudo raspi-config nonint do_boot_behaviour B4  # B4 = Desktop autologin

# Step 5: Enable kiosk service
echo ""
echo -e "${GREEN}[5/6] Enabling kiosk service...${NC}"
sudo systemctl enable dashboard-kiosk.service
echo "✓ Kiosk service enabled (will start on boot)"

# Step 6: Check web dashboard service
echo ""
echo -e "${GREEN}[6/6] Checking web dashboard service...${NC}"
if systemctl is-active --quiet web-dashboard.service; then
    echo "✓ Web dashboard service is running"
elif systemctl is-enabled --quiet web-dashboard.service; then
    echo -e "${YELLOW}⚠ Web dashboard service is enabled but not running${NC}"
    echo "  Starting service..."
    sudo systemctl start web-dashboard.service
    sleep 2
    if systemctl is-active --quiet web-dashboard.service; then
        echo "✓ Web dashboard service started"
    else
        echo -e "${RED}✗ Failed to start web dashboard service${NC}"
        echo "  Check status with: sudo systemctl status web-dashboard.service"
    fi
else
    echo -e "${YELLOW}⚠ Web dashboard service not found or not enabled${NC}"
    echo "  Please install and enable the web dashboard service first:"
    echo "    cd $PROJECT_DIR/telemetry-system"
    echo "    ./scripts/setup-web-dashboard.sh"
fi

# Test web dashboard accessibility
echo ""
echo -e "${GREEN}Testing web dashboard accessibility...${NC}"
if curl -s http://localhost:8080 > /dev/null 2>&1; then
    echo "✓ Web dashboard is accessible at http://localhost:8080"
else
    echo -e "${YELLOW}⚠ Web dashboard not accessible yet${NC}"
    echo "  The kiosk will wait for it to be available on boot"
fi

echo ""
echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}Installation Complete!${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""
echo "Next steps:"
echo ""
echo "1. Reboot the Raspberry Pi:"
echo "   ${YELLOW}sudo reboot${NC}"
echo ""
echo "2. After reboot, the dashboard should appear in kiosk mode"
echo ""
echo "3. To configure touchscreen (run from desktop, not SSH):"
echo "   ${YELLOW}./kiosk/configure-touchscreen.sh${NC}"
echo ""
echo "Optional commands:"
echo "  - Start kiosk now:  ${YELLOW}sudo systemctl start dashboard-kiosk.service${NC}"
echo "  - Stop kiosk:       ${YELLOW}sudo systemctl stop dashboard-kiosk.service${NC}"
echo "  - Disable kiosk:    ${YELLOW}sudo systemctl disable dashboard-kiosk.service${NC}"
echo "  - View logs:        ${YELLOW}journalctl -u dashboard-kiosk.service -f${NC}"
echo ""
echo "Troubleshooting:"
echo "  - If screen is blank: Check HDMI connection and reboot"
echo "  - If touchscreen doesn't work: Run configure-touchscreen.sh from desktop"
echo "  - If browser shows errors: Check web dashboard is running"
echo "  - Exit kiosk mode: Press Alt+F4 or Ctrl+W"
echo ""

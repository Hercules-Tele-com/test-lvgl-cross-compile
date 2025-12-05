#!/bin/bash
# Update autostart to use correct chromium command and kiosk mode URL

echo "Updating autostart configuration for kiosk mode..."

AUTOSTART_FILE="$HOME/.config/lxsession/LXDE-pi/autostart"

# Detect which chromium command is available
if command -v chromium &> /dev/null; then
    BROWSER="chromium"
elif command -v chromium-browser &> /dev/null; then
    BROWSER="chromium-browser"
else
    echo "Error: Neither 'chromium' nor 'chromium-browser' found!"
    echo "Please install chromium: sudo apt install chromium-browser"
    exit 1
fi

echo "Using browser command: $BROWSER"

# Create autostart directory if it doesn't exist
mkdir -p "$(dirname "$AUTOSTART_FILE")"

# Backup existing autostart file
if [ -f "$AUTOSTART_FILE" ]; then
    cp "$AUTOSTART_FILE" "${AUTOSTART_FILE}.backup.$(date +%Y%m%d_%H%M%S)"
    echo "✓ Backed up existing autostart file"
fi

# Create new autostart file
cat > "$AUTOSTART_FILE" << EOF
@xset s off
@xset -dpms
@xset s noblank
@unclutter -idle 0.1 -root
@$BROWSER --kiosk --app=http://localhost:8080
EOF

echo "✓ Updated autostart file: $AUTOSTART_FILE"
echo ""
echo "Configuration:"
echo "  Browser: $BROWSER"
echo "  URL: http://localhost:8080 (kiosk mode is default)"
echo ""
echo "The kiosk mode will launch automatically on next boot."
echo "To test now without rebooting, run:"
echo "  cd ~/Projects/test-lvgl-cross-compile/kiosk && ./launch-kiosk.sh"

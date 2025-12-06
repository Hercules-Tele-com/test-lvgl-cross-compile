#!/bin/bash
# Launch Kiosk Mode Dashboard
# Quick script to test kiosk mode without rebooting

echo "Launching Emboo Telemetry Kiosk Mode..."
echo "Press Ctrl+C to exit"

# Kill any existing chromium instances
pkill -f chromium

# Wait a moment
sleep 1

# Try chromium (Debian/Ubuntu) or chromium-browser (Raspberry Pi OS)
if command -v chromium &> /dev/null; then
    BROWSER="chromium"
elif command -v chromium-browser &> /dev/null; then
    BROWSER="chromium-browser"
else
    echo "Error: Neither 'chromium' nor 'chromium-browser' found!"
    echo "Please install chromium: sudo apt install chromium-browser"
    exit 1
fi

echo "Using browser: $BROWSER"

# Launch chromium in kiosk mode
DISPLAY=:0 $BROWSER --kiosk --app=http://localhost:8080 &

echo "Kiosk mode launched!"
echo "URL: http://localhost:8080 (default is now kiosk mode)"
echo "To exit kiosk mode, press Alt+F4 or run: pkill -f chromium"

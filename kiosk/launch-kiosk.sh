#!/bin/bash
# Launch Kiosk Mode Dashboard
# Quick script to test kiosk mode without rebooting

echo "Launching Emboo Telemetry Kiosk Mode..."
echo "Press Ctrl+C to exit"

# Kill any existing chromium instances
pkill -f chromium-browser

# Wait a moment
sleep 1

# Launch chromium in kiosk mode
DISPLAY=:0 chromium-browser --kiosk --app=http://localhost:8080/kiosk &

echo "Kiosk mode launched!"
echo "To exit kiosk mode, press Alt+F4 or run: pkill -f chromium-browser"

#!/bin/bash
# Configure touchscreen for Victron Cerbo 50 (HDMI Touch)
# This script detects and configures the touchscreen input device

set -e

echo "==================================="
echo "Touchscreen Configuration Script"
echo "==================================="
echo ""

# Check if running in X11
if [ -z "$DISPLAY" ]; then
    echo "ERROR: X11 display not detected. Please run this script from within X11 session."
    echo "Hint: SSH with X forwarding won't work. Run from the Pi desktop or via VNC."
    exit 1
fi

# List all input devices
echo "Detected input devices:"
xinput list

echo ""
echo "Detected touchscreen devices:"
xinput list | grep -i "touch"

echo ""
echo "==================================="
echo "Touchscreen Detection"
echo "==================================="

# Try to find touchscreen device
TOUCH_DEVICE=$(xinput list | grep -i "touch" | head -n 1 | sed 's/.*id=\([0-9]*\).*/\1/')

if [ -z "$TOUCH_DEVICE" ]; then
    echo "WARNING: No touchscreen device detected."
    echo "This may be normal if:"
    echo "  1. You're connected via SSH (touchscreen only available in local X11)"
    echo "  2. Touchscreen drivers not loaded yet"
    echo "  3. Display not connected"
    echo ""
    echo "To manually configure touchscreen later:"
    echo "  1. Boot the Pi with display connected"
    echo "  2. Run: xinput list"
    echo "  3. Find your touchscreen device name"
    echo "  4. Edit kiosk/autostart and uncomment the xinput line"
    exit 0
fi

TOUCH_NAME=$(xinput list | grep "id=$TOUCH_DEVICE" | sed 's/.*\t\(.*\)\s*id=.*/\1/' | xargs)

echo "Found touchscreen:"
echo "  Device ID: $TOUCH_DEVICE"
echo "  Device Name: $TOUCH_NAME"
echo ""

# Get current touchscreen properties
echo "Current touchscreen properties:"
xinput list-props "$TOUCH_DEVICE" | grep -i "matrix\|calibration"

echo ""
echo "==================================="
echo "Touchscreen Configuration"
echo "==================================="

# Default transformation matrix (no rotation)
# Format: [a b c d e f g h i]
# For normal orientation: [1 0 0 0 1 0 0 0 1]
TRANSFORM_MATRIX="1 0 0 0 1 0 0 0 1"

echo "Applying transformation matrix: $TRANSFORM_MATRIX"
xinput set-prop "$TOUCH_DEVICE" --type=float "Coordinate Transformation Matrix" $TRANSFORM_MATRIX

echo ""
echo "✓ Touchscreen configured successfully"
echo ""
echo "To test touchscreen:"
echo "  - Touch the screen and verify cursor movement"
echo "  - If rotation is needed, edit the transformation matrix"
echo ""
echo "Rotation examples:"
echo "  90° clockwise:  0 1 0 -1 0 1 0 0 1"
echo "  180°:          -1 0 1 0 -1 1 0 0 1"
echo "  270° (90° CCW): 0 -1 1 1 0 0 0 0 1"
echo ""
echo "To make this permanent, add to kiosk/autostart:"
echo "  @xinput set-prop \"$TOUCH_NAME\" --type=float \"Coordinate Transformation Matrix\" $TRANSFORM_MATRIX"

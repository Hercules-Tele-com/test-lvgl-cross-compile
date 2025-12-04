#!/bin/bash
# USB GPS Module Testing Script

echo "==================================================================="
echo "  USB GPS Module Test"
echo "==================================================================="
echo ""

# 1. Find USB serial devices
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 1: Detecting USB Serial Devices"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

USB_DEVICES=$(ls /dev/ttyUSB* 2>/dev/null || ls /dev/ttyACM* 2>/dev/null)

if [ -z "$USB_DEVICES" ]; then
    echo "✗ No USB serial devices found!"
    echo ""
    echo "Troubleshooting:"
    echo "  1. Check if GPS module is plugged in"
    echo "  2. Run: lsusb"
    echo "  3. Check dmesg: dmesg | tail -20"
    exit 1
fi

echo "Found USB serial devices:"
for dev in $USB_DEVICES; do
    echo "  - $dev"
done

# Try to identify GPS device
echo ""
echo "USB device information:"
for dev in $USB_DEVICES; do
    if [ -e "$dev" ]; then
        echo ""
        echo "Device: $dev"
        udevadm info --query=all --name=$dev 2>/dev/null | grep -E "ID_VENDOR|ID_MODEL|ID_SERIAL" | sed 's/^/  /'
    fi
done

# Select GPS device (use first device by default)
GPS_DEVICE=$(echo $USB_DEVICES | awk '{print $1}')

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 2: GPS Device Selection"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Using GPS device: $GPS_DEVICE"
echo ""

# Ask user to confirm or select different device
if [ $(echo "$USB_DEVICES" | wc -w) -gt 1 ]; then
    echo "Multiple devices found. To use a different device, press Ctrl+C"
    echo "and run: $0 <device>"
    echo "Example: $0 /dev/ttyUSB1"
    sleep 3
fi

# Allow override from command line
if [ ! -z "$1" ]; then
    GPS_DEVICE=$1
    echo "Using device from command line: $GPS_DEVICE"
fi

# Check device exists
if [ ! -e "$GPS_DEVICE" ]; then
    echo "✗ Device $GPS_DEVICE does not exist!"
    exit 1
fi

# Check device permissions
if [ ! -r "$GPS_DEVICE" ]; then
    echo "⚠ Warning: No read permission on $GPS_DEVICE"
    echo "  Run: sudo chmod 666 $GPS_DEVICE"
    echo "  Or add user to dialout group: sudo usermod -a -G dialout $USER"
    echo ""
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 3: Reading GPS NMEA Sentences (10 seconds)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Reading from $GPS_DEVICE at 9600 baud..."
echo ""

# Try to read NMEA sentences
# Common GPS baud rates: 9600, 4800, 115200
BAUD_RATES="9600 4800 115200"
FOUND=0

for BAUD in $BAUD_RATES; do
    echo "Trying baud rate: $BAUD"

    # Read for 2 seconds
    NMEA_DATA=$(timeout 2 stty -F $GPS_DEVICE $BAUD raw -echo 2>/dev/null && timeout 2 cat $GPS_DEVICE 2>/dev/null | head -5)

    if echo "$NMEA_DATA" | grep -q "^\$GP"; then
        echo "✓ Found GPS data at $BAUD baud!"
        echo ""
        echo "Sample NMEA sentences:"
        echo "$NMEA_DATA" | head -10
        FOUND=1
        CORRECT_BAUD=$BAUD
        break
    fi
done

if [ $FOUND -eq 0 ]; then
    echo "✗ No GPS NMEA data detected!"
    echo ""
    echo "Troubleshooting:"
    echo "  1. Check GPS module power (LED should be blinking)"
    echo "  2. GPS may need clear sky view to get fix"
    echo "  3. Try manual test: cat $GPS_DEVICE"
    echo "  4. Check if device is correct: ls -l /dev/ttyUSB*"
    exit 1
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 4: Parsing GPS Data"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Read more data for parsing
echo "Reading GPS data for 10 seconds..."
stty -F $GPS_DEVICE $CORRECT_BAUD raw -echo 2>/dev/null
GPS_DATA=$(timeout 10 cat $GPS_DEVICE 2>/dev/null)

# Parse GPGGA sentence (position fix)
GPGGA=$(echo "$GPS_DATA" | grep '^\$GPGGA' | tail -1)
if [ ! -z "$GPGGA" ]; then
    echo ""
    echo "Position Data (GPGGA):"
    echo "  $GPGGA"

    # Parse fix quality
    FIX_QUALITY=$(echo "$GPGGA" | awk -F',' '{print $7}')
    case $FIX_QUALITY in
        0) echo "  Fix Quality: No fix" ;;
        1) echo "  Fix Quality: GPS fix (SPS)" ;;
        2) echo "  Fix Quality: DGPS fix" ;;
        *) echo "  Fix Quality: $FIX_QUALITY" ;;
    esac

    # Parse satellites
    SATS=$(echo "$GPGGA" | awk -F',' '{print $8}')
    echo "  Satellites: $SATS"
fi

# Parse GPRMC sentence (recommended minimum data)
GPRMC=$(echo "$GPS_DATA" | grep '^\$GPRMC' | tail -1)
if [ ! -z "$GPRMC" ]; then
    echo ""
    echo "Navigation Data (GPRMC):"
    echo "  $GPRMC"

    # Parse status
    STATUS=$(echo "$GPRMC" | awk -F',' '{print $3}')
    if [ "$STATUS" = "A" ]; then
        echo "  Status: Active (valid position)"
    else
        echo "  Status: Void (no fix)"
    fi
fi

# Parse GPGSV sentence (satellites in view)
GPGSV=$(echo "$GPS_DATA" | grep '^\$GPGSV' | tail -1)
if [ ! -z "$GPGSV" ]; then
    echo ""
    echo "Satellites in View (GPGSV):"
    echo "  $GPGSV"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 5: Summary"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "GPS Device: $GPS_DEVICE"
echo "Baud Rate: $CORRECT_BAUD"

if [ ! -z "$GPGGA" ]; then
    echo "✓ GPS module is working and receiving data"

    if [ "$FIX_QUALITY" != "0" ]; then
        echo "✓ GPS has position fix ($SATS satellites)"
    else
        echo "⚠ GPS is working but no position fix yet"
        echo "  (May need clear sky view)"
    fi
else
    echo "⚠ GPS is sending data but position not available yet"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Next Steps:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "1. Monitor GPS data continuously:"
echo "   cat $GPS_DEVICE"
echo ""
echo "2. Check GPS on CAN bus (if ESP32 GPS module is programmed):"
echo "   candump can0,710:7F0"
echo ""
echo "3. View GPS CAN messages in detail:"
echo "   candump can0 | grep '710\|711\|712'"
echo ""
echo "   CAN IDs:"
echo "   - 0x710: GPS position (lat, lon, altitude)"
echo "   - 0x711: GPS velocity (speed, heading)"
echo "   - 0x712: GPS time (date, time)"
echo ""
echo "4. If using ESP32 GPS module, check serial debug:"
echo "   screen $GPS_DEVICE 115200"
echo ""

echo "==================================================================="
echo "  GPS Test Complete"
echo "==================================================================="

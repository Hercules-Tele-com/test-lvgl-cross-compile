#!/bin/bash
# CAN Bus Testing Script for both can0 and can1

echo "==================================================================="
echo "  CAN Bus Test - can0 and can1"
echo "==================================================================="
echo ""

# Determine battery type from command line or default to EMBOO
BATTERY_TYPE=${1:-EMBOO}

if [ "$BATTERY_TYPE" = "EMBOO" ]; then
    BITRATE=250000
    echo "Battery Type: EMBOO (Orion BMS)"
    echo "CAN Bitrate: 250 kbps"
elif [ "$BATTERY_TYPE" = "NISSAN_LEAF" ] || [ "$BATTERY_TYPE" = "LEAF" ]; then
    BITRATE=500000
    echo "Battery Type: Nissan Leaf"
    echo "CAN Bitrate: 500 kbps"
else
    echo "Unknown battery type: $BATTERY_TYPE"
    echo "Usage: $0 [EMBOO|NISSAN_LEAF]"
    exit 1
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 1: Setting up CAN interfaces"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Setup can0
echo "Setting up can0..."
sudo ip link set can0 down 2>/dev/null
sudo ip link set can0 type can bitrate $BITRATE
sudo ip link set can0 up

if [ $? -eq 0 ]; then
    echo "✓ can0 configured at $BITRATE bps"
else
    echo "✗ Failed to configure can0"
    CAN0_FAILED=1
fi

# Setup can1
echo "Setting up can1..."
sudo ip link set can1 down 2>/dev/null
sudo ip link set can1 type can bitrate $BITRATE
sudo ip link set can1 up

if [ $? -eq 0 ]; then
    echo "✓ can1 configured at $BITRATE bps"
else
    echo "⚠ Failed to configure can1 (may not be present)"
    CAN1_FAILED=1
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 2: CAN Interface Status"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Check can0
if [ -z "$CAN0_FAILED" ]; then
    echo ""
    echo "can0 Status:"
    ip -details link show can0 | grep -E "can0|bitrate|state"
else
    echo "can0: Not available"
fi

# Check can1
if [ -z "$CAN1_FAILED" ]; then
    echo ""
    echo "can1 Status:"
    ip -details link show can1 | grep -E "can1|bitrate|state"
else
    echo "can1: Not available"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 3: Loopback Test"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Test can0 loopback
if [ -z "$CAN0_FAILED" ]; then
    echo ""
    echo "Testing can0 loopback..."
    sudo ip link set can0 down
    sudo ip link set can0 type can bitrate $BITRATE loopback on
    sudo ip link set can0 up

    # Send test message
    cansend can0 123#DEADBEEF 2>/dev/null &
    sleep 0.5

    # Check if we received it
    LOOPBACK_TEST=$(timeout 1 candump can0 2>/dev/null | grep "123")

    if [ ! -z "$LOOPBACK_TEST" ]; then
        echo "✓ can0 loopback test PASSED"
        echo "  Received: $LOOPBACK_TEST"
    else
        echo "✗ can0 loopback test FAILED"
    fi

    # Disable loopback
    sudo ip link set can0 down
    sudo ip link set can0 type can bitrate $BITRATE loopback off
    sudo ip link set can0 up
fi

# Test can1 loopback
if [ -z "$CAN1_FAILED" ]; then
    echo ""
    echo "Testing can1 loopback..."
    sudo ip link set can1 down
    sudo ip link set can1 type can bitrate $BITRATE loopback on
    sudo ip link set can1 up

    # Send test message
    cansend can1 456#CAFEBABE 2>/dev/null &
    sleep 0.5

    # Check if we received it
    LOOPBACK_TEST=$(timeout 1 candump can1 2>/dev/null | grep "456")

    if [ ! -z "$LOOPBACK_TEST" ]; then
        echo "✓ can1 loopback test PASSED"
        echo "  Received: $LOOPBACK_TEST"
    else
        echo "✗ can1 loopback test FAILED"
    fi

    # Disable loopback
    sudo ip link set can1 down
    sudo ip link set can1 type can bitrate $BITRATE loopback off
    sudo ip link set can1 up
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 4: Live Traffic Test (10 seconds)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

echo ""
echo "Monitoring CAN traffic for 10 seconds..."
echo "(Connect your CAN devices to see live data)"
echo ""

# Monitor can0
if [ -z "$CAN0_FAILED" ]; then
    echo "can0 traffic:"
    timeout 10 candump can0 2>/dev/null &
    CAN0_PID=$!
fi

# Monitor can1
if [ -z "$CAN1_FAILED" ]; then
    echo "can1 traffic:"
    timeout 10 candump can1 2>/dev/null &
    CAN1_PID=$!
fi

# Wait for monitoring to complete
wait $CAN0_PID 2>/dev/null
wait $CAN1_PID 2>/dev/null

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 5: Message Count"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Count messages on can0
if [ -z "$CAN0_FAILED" ]; then
    echo ""
    echo "can0 message count (5 second sample):"
    MSG_COUNT=$(timeout 5 candump can0 2>/dev/null | wc -l)
    if [ "$MSG_COUNT" -gt 0 ]; then
        echo "  ✓ Received $MSG_COUNT messages (~$((MSG_COUNT/5)) msg/sec)"
    else
        echo "  ⚠ No messages received"
        echo "  - Check CAN bus connections"
        echo "  - Verify devices are powered and transmitting"
        echo "  - Check termination resistors (120Ω at each end)"
    fi
fi

# Count messages on can1
if [ -z "$CAN1_FAILED" ]; then
    echo ""
    echo "can1 message count (5 second sample):"
    MSG_COUNT=$(timeout 5 candump can1 2>/dev/null | wc -l)
    if [ "$MSG_COUNT" -gt 0 ]; then
        echo "  ✓ Received $MSG_COUNT messages (~$((MSG_COUNT/5)) msg/sec)"
    else
        echo "  ⚠ No messages received"
        echo "  - Check CAN bus connections"
        echo "  - Verify devices are powered and transmitting"
        echo "  - Check termination resistors (120Ω at each end)"
    fi
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 6: Battery-Specific Messages"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if [ "$BATTERY_TYPE" = "EMBOO" ]; then
    echo ""
    echo "EMBOO Battery CAN IDs to monitor:"
    echo "  0x6B0 - Pack status (voltage, current, SOC)"
    echo "  0x6B1 - Pack statistics"
    echo "  0x6B2 - Status flags"
    echo "  0x6B3 - Cell voltages"
    echo "  0x6B4 - Temperature data"
    echo "  0x351 - Pack summary"
    echo "  0x355 - Pack data 1"
    echo "  0x356 - Pack data 2"
    echo ""
    echo "Monitor EMBOO messages on can0:"
    echo "  candump can0,6B0:7F0,351:7FF,355:7FF"
elif [ "$BATTERY_TYPE" = "NISSAN_LEAF" ]; then
    echo ""
    echo "Nissan Leaf CAN IDs to monitor:"
    echo "  0x1DB - Battery SOC"
    echo "  0x1DC - Battery temperature"
    echo "  0x1F2 - Inverter telemetry"
    echo "  0x1DA - Motor RPM"
    echo "  0x390 - Charger status"
    echo ""
    echo "Monitor Nissan Leaf messages on can0:"
    echo "  candump can0,1DB:7FF,1DC:7FF,1F2:7FF,1DA:7FF,390:7FF"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Useful Commands:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Monitor all traffic on both buses:"
echo "  candump can0 &"
echo "  candump can1 &"
echo ""
echo "Send test message:"
echo "  cansend can0 123#DEADBEEF"
echo ""
echo "Check interface statistics:"
echo "  ip -details -statistics link show can0"
echo "  ip -details -statistics link show can1"
echo ""
echo "Reset interface (if errors occur):"
echo "  sudo ip link set can0 down && sudo ip link set can0 up"
echo "  sudo ip link set can1 down && sudo ip link set can1 up"
echo ""

echo "==================================================================="
echo "  CAN Bus Test Complete"
echo "==================================================================="

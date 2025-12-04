#!/bin/bash
# Simple startup script as alternative to systemd dependencies
# This runs after boot and ensures services start in the correct order

LOGFILE="/home/emboo/kiosk-startup.log"

{
    echo "====================================="
    echo "Kiosk Startup Script"
    echo "$(date)"
    echo "====================================="

    # Wait for system to be fully booted
    echo "Waiting 10 seconds for system to stabilize..."
    sleep 10

    # Start InfluxDB
    echo "Starting InfluxDB..."
    sudo systemctl start influxdb.service
    sleep 5

    if systemctl is-active --quiet influxdb.service; then
        echo "✓ InfluxDB started"
    else
        echo "✗ InfluxDB failed to start"
        sudo journalctl -u influxdb.service -n 20
        exit 1
    fi

    # Start web dashboard
    echo "Starting web dashboard..."
    sudo systemctl start web-dashboard.service
    sleep 5

    if systemctl is-active --quiet web-dashboard.service; then
        echo "✓ Web dashboard started"
    else
        echo "✗ Web dashboard failed to start"
        sudo journalctl -u web-dashboard.service -n 20
        exit 1
    fi

    # Test if web dashboard is accessible
    if curl -s http://localhost:8080 > /dev/null 2>&1; then
        echo "✓ Web dashboard is accessible"
    else
        echo "✗ Web dashboard is not accessible"
        exit 1
    fi

    # Start kiosk if we're in graphical mode
    if systemctl is-active --quiet graphical.target; then
        echo "Starting kiosk mode..."
        sudo systemctl start dashboard-kiosk.service
        sleep 3

        if systemctl is-active --quiet dashboard-kiosk.service; then
            echo "✓ Kiosk mode started"
        else
            echo "✗ Kiosk mode failed to start"
            sudo journalctl -u dashboard-kiosk.service -n 20
        fi
    else
        echo "⚠ Graphical target not active, skipping kiosk mode"
    fi

    echo "====================================="
    echo "Startup complete"
    echo "====================================="

} >> "$LOGFILE" 2>&1

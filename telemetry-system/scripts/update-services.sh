#!/bin/bash
# Update systemd services with latest configuration
# Run this after pulling updates to service files

set -e  # Exit on error

SCRIPT_DIR="$(dirname "$0")"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SYSTEMD_DIR="$PROJECT_ROOT/telemetry-system/systemd"

echo "================================"
echo "Update Systemd Services"
echo "================================"
echo ""
echo "Project root: $PROJECT_ROOT"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root (use sudo)"
    exit 1
fi

# List of services to update
SERVICES=(
    "telemetry-logger.service"
    "telemetry-logger-can1.service"
    "telemetry-logger-unified.service"
    "cloud-sync.service"
    "web-dashboard.service"
)

echo "Step 1: Copying service files..."
for service in "${SERVICES[@]}"; do
    if [ -f "$SYSTEMD_DIR/$service" ]; then
        echo "  - $service"
        cp "$SYSTEMD_DIR/$service" /etc/systemd/system/
    else
        echo "  ⚠ $service not found, skipping"
    fi
done
echo ""

echo "Step 2: Reloading systemd daemon..."
systemctl daemon-reload
echo "✓ Daemon reloaded"
echo ""

echo "Step 3: Checking which services are enabled..."
ENABLED_SERVICES=()
for service in "${SERVICES[@]}"; do
    if systemctl is-enabled "$service" >/dev/null 2>&1; then
        ENABLED_SERVICES+=("$service")
        echo "  ✓ $service (enabled)"
    else
        echo "    $service (not enabled)"
    fi
done
echo ""

if [ ${#ENABLED_SERVICES[@]} -eq 0 ]; then
    echo "No services are currently enabled."
    echo ""
    echo "To enable services, run:"
    echo "  sudo systemctl enable telemetry-logger-unified.service"
    echo "  sudo systemctl enable cloud-sync.service"
    echo "  sudo systemctl enable web-dashboard.service"
    exit 0
fi

echo "Step 4: Restarting enabled services..."
for service in "${ENABLED_SERVICES[@]}"; do
    echo "  Restarting $service..."
    systemctl restart "$service"
done
echo "✓ Services restarted"
echo ""

echo "================================"
echo "Update Complete"
echo "================================"
echo ""
echo "Check service status:"
for service in "${ENABLED_SERVICES[@]}"; do
    STATUS=$(systemctl is-active "$service" 2>/dev/null || echo "inactive")
    printf "  %-30s %s\n" "$service:" "$STATUS"
done
echo ""
echo "View logs:"
echo "  sudo journalctl -u telemetry-logger-unified.service -f"
echo "  sudo journalctl -u cloud-sync.service -f"
echo "  sudo journalctl -u web-dashboard.service -f"
echo ""

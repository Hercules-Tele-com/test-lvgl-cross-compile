#!/bin/bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VENV_DIR="$PROJECT_ROOT/telemetry-system/.venv"

echo "=== Nissan Leaf Telemetry System Setup ==="
echo "Project root: $PROJECT_ROOT"
echo ""

# Step 1: Install system dependencies
echo "=== Installing system dependencies ==="
sudo apt-get update
sudo apt-get install -y python3-full python3-venv python3-pip jq wget curl

# Step 2: Create virtual environment
echo ""
echo "=== Creating Python virtual environment ==="
if [ ! -d "$VENV_DIR" ]; then
    python3 -m venv "$VENV_DIR"
    echo "Virtual environment created at: $VENV_DIR"
else
    echo "Virtual environment already exists at: $VENV_DIR"
fi

# Step 3: Activate virtual environment and install dependencies
echo ""
echo "=== Installing Python dependencies ==="
source "$VENV_DIR/bin/activate"

pip install --upgrade pip
pip install influxdb-client python-can flask flask-socketio

echo ""
echo "Installed packages:"
pip list

# Step 4: Update systemd service files to use venv
echo ""
echo "=== Updating systemd service files ==="

# Update telemetry-logger.service
sed -i "s|ExecStart=/usr/bin/python3|ExecStart=$VENV_DIR/bin/python3|g" \
    "$PROJECT_ROOT/telemetry-system/systemd/telemetry-logger.service"

# Update web-dashboard.service
sed -i "s|ExecStart=/usr/bin/python3|ExecStart=$VENV_DIR/bin/python3|g" \
    "$PROJECT_ROOT/telemetry-system/systemd/web-dashboard.service"

# Update cloud-sync.service
sed -i "s|ExecStart=/usr/bin/python3|ExecStart=$VENV_DIR/bin/python3|g" \
    "$PROJECT_ROOT/telemetry-system/systemd/cloud-sync.service"

echo "Updated service files to use virtual environment Python"

# Step 5: Prompt for InfluxDB setup
echo ""
read -p "Do you want to set up InfluxDB? (y/n): " SETUP_INFLUX

if [ "$SETUP_INFLUX" = "y" ] || [ "$SETUP_INFLUX" = "Y" ]; then
    echo ""
    echo "=== Running InfluxDB setup ==="
    bash "$PROJECT_ROOT/telemetry-system/scripts/setup-influxdb.sh"
else
    echo "Skipping InfluxDB setup"
    echo "Note: Services will run without InfluxDB but won't log data"
fi

# Step 6: Install systemd services
echo ""
read -p "Do you want to install and enable systemd services? (y/n): " INSTALL_SERVICES

if [ "$INSTALL_SERVICES" = "y" ] || [ "$INSTALL_SERVICES" = "Y" ]; then
    echo ""
    echo "=== Installing systemd services ==="

    sudo cp "$PROJECT_ROOT/telemetry-system/systemd"/*.service /etc/systemd/system/
    sudo cp "$PROJECT_ROOT/ui-dashboard/systemd/leaf-can-dashboard.service" /etc/systemd/system/ 2>/dev/null || true

    sudo systemctl daemon-reload

    # Enable services
    sudo systemctl enable telemetry-logger.service
    sudo systemctl enable web-dashboard.service
    sudo systemctl enable cloud-sync.service

    echo "Services installed and enabled"
    echo ""
    echo "To start services manually:"
    echo "  sudo systemctl start web-dashboard.service"
    echo "  sudo systemctl start telemetry-logger.service"
    echo "  sudo systemctl start cloud-sync.service"
else
    echo "Skipping service installation"
    echo ""
    echo "To install services later, run:"
    echo "  sudo cp telemetry-system/systemd/*.service /etc/systemd/system/"
    echo "  sudo systemctl daemon-reload"
fi

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Virtual environment: $VENV_DIR"
echo "To activate manually: source $VENV_DIR/bin/activate"
echo ""
echo "Next steps:"
echo "1. Ensure CAN interface is configured:"
echo "   sudo ip link set can0 up type can bitrate 500000"
echo ""
echo "2. Start the web dashboard:"
echo "   sudo systemctl start web-dashboard.service"
echo ""
echo "3. Access dashboard at: http://$(hostname -I | awk '{print $1}'):8080"
echo ""

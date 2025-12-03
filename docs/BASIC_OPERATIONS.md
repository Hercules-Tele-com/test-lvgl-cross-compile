# Basic Operations Guide

This guide provides quick reference commands for starting, stopping, and monitoring the Nissan Leaf CAN Network system on Raspberry Pi.

## Quick Start - Get Everything Online

Run these commands to start all services:

```bash
# 1. Set up CAN bus (choose based on your battery type)
# For EMBOO Battery (250 kbps):
sudo ip link set can0 type can bitrate 250000
sudo ip link set can0 up

# For Nissan Leaf Battery (500 kbps):
# sudo ip link set can0 type can bitrate 500000
# sudo ip link set can0 up

# 2. Start InfluxDB
sudo systemctl start influxdb

# 3. Start telemetry services
sudo systemctl start telemetry-logger.service
sudo systemctl start cloud-sync.service
sudo systemctl start web-dashboard.service

# 4. Verify services are running
sudo systemctl status influxdb
sudo systemctl status telemetry-logger.service
sudo systemctl status cloud-sync.service
sudo systemctl status web-dashboard.service

# 5. Check CAN interface
ip -details link show can0

# 6. Monitor CAN messages
candump can0
```

**Access the web dashboard**: Open browser to `http://<raspberry-pi-ip>:8080`

---

## Helper Scripts

Automated scripts are available in the `scripts/` directory to simplify common operations:

### Start Everything

```bash
cd ~/test-lvgl-cross-compile/scripts
./start-system.sh
```

This script will:
- Configure both can0 and can1 at 250 kbps (EMBOO battery)
- Start InfluxDB
- Start all telemetry services
- Display status of all services and CAN interfaces

### Check System Status

```bash
cd ~/test-lvgl-cross-compile/scripts
./check-system.sh
```

Shows comprehensive status including:
- CAN interface status for both can0 and can1
- Service status (InfluxDB, telemetry, web dashboard)
- CAN message activity (5-second sample)
- Web dashboard accessibility
- System resources

### Stop Everything

```bash
cd ~/test-lvgl-cross-compile/scripts
./stop-system.sh
```

Cleanly stops all services and brings down CAN interfaces.

### Test CAN Interfaces

```bash
cd ~/test-lvgl-cross-compile/scripts

# For EMBOO battery (250 kbps)
./test-can.sh EMBOO

# For Nissan Leaf battery (500 kbps)
./test-can.sh NISSAN_LEAF
```

Performs comprehensive CAN bus testing:
- Sets up both can0 and can1
- Runs loopback tests
- Monitors live traffic
- Shows battery-specific CAN IDs to monitor

### Test GPS Module

```bash
cd ~/test-lvgl-cross-compile/scripts
./test-gps.sh
```

Automatically tests USB GPS module:
- Detects USB serial devices
- Tests multiple baud rates (9600, 4800, 115200)
- Reads and parses NMEA sentences
- Shows GPS fix status and satellite count
- Provides next steps for CAN integration

---

## CAN Bus Setup

### Configure CAN Interfaces

**For EMBOO Battery (Orion BMS, 250 kbps):**
```bash
# Configure can0
sudo ip link set can0 down  # If already up
sudo ip link set can0 type can bitrate 250000
sudo ip link set can0 up

# Configure can1
sudo ip link set can1 down  # If already up
sudo ip link set can1 type can bitrate 250000
sudo ip link set can1 up
```

**For Nissan Leaf Battery (500 kbps):**
```bash
# Configure can0
sudo ip link set can0 down  # If already up
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up

# Configure can1
sudo ip link set can1 down  # If already up
sudo ip link set can1 type can bitrate 500000
sudo ip link set can1 up
```

### Verify CAN Interface

```bash
# Check interface status
ip -details link show can0

# Expected output shows:
# - mtu 16 (CAN MTU)
# - state UP
# - bitrate 250000 (or 500000 for Nissan Leaf)
```

### Monitor CAN Traffic

```bash
# View all CAN messages
candump can0

# View specific CAN ID (e.g., 0x6B0 for EMBOO pack status)
candump can0,6B0:7FF

# View with timestamp
candump can0 -t a

# Save to file
candump can0 -l
```

### Send Test CAN Messages

**EMBOO Battery (0x6B0 - Pack Status):**
```bash
# Example: 350V, 50A charging, 80% SOC
# Voltage: 350V / 0.1 = 3500 = 0x0DAC (big-endian: 0D AC)
# Current: 50A / 0.1 = 500 = 0x01F4 (big-endian: 01 F4)
# SOC: 80% / 0.5 = 160 = 0xA0
cansend can0 6B0#01F40DAC0000A000
```

**Nissan Leaf Battery (0x1DB - Battery SOC):**
```bash
# Example: 75% SOC, 350V, 25A
cansend can0 1DB#9600640AE001900
```

---

## Telemetry System

### Start All Telemetry Services

```bash
# Start InfluxDB
sudo systemctl start influxdb

# Start telemetry services
sudo systemctl start telemetry-logger.service
sudo systemctl start cloud-sync.service
sudo systemctl start web-dashboard.service
```

### Stop All Telemetry Services

```bash
sudo systemctl stop telemetry-logger.service
sudo systemctl stop cloud-sync.service
sudo systemctl stop web-dashboard.service
sudo systemctl stop influxdb
```

### Restart All Telemetry Services

```bash
sudo systemctl restart telemetry-logger.service
sudo systemctl restart cloud-sync.service
sudo systemctl restart web-dashboard.service
```

### Check Service Status

```bash
# Check all services at once
systemctl status influxdb telemetry-logger.service cloud-sync.service web-dashboard.service

# Or check individually
sudo systemctl status telemetry-logger.service
sudo systemctl status cloud-sync.service
sudo systemctl status web-dashboard.service
```

### View Service Logs

```bash
# View telemetry logger logs (live, last 50 lines)
sudo journalctl -u telemetry-logger.service -n 50 -f

# View cloud sync logs
sudo journalctl -u cloud-sync.service -n 50 -f

# View web dashboard logs
sudo journalctl -u web-dashboard.service -n 50 -f

# View all telemetry logs together
sudo journalctl -u telemetry-logger.service -u cloud-sync.service -u web-dashboard.service -f
```

### Enable Services on Boot

```bash
sudo systemctl enable influxdb
sudo systemctl enable telemetry-logger.service
sudo systemctl enable cloud-sync.service
sudo systemctl enable web-dashboard.service
```

### Disable Services on Boot

```bash
sudo systemctl disable telemetry-logger.service
sudo systemctl disable cloud-sync.service
sudo systemctl disable web-dashboard.service
```

---

## LVGL Dashboard (UI Dashboard)

### Run Dashboard

```bash
cd ~/test-lvgl-cross-compile/ui-dashboard

# Run with sudo (required for framebuffer and CAN access)
sudo ./build/leaf-can-dashboard
```

### Build Dashboard

```bash
cd ~/test-lvgl-cross-compile/ui-dashboard
mkdir -p build && cd build

# For EMBOO battery
cmake .. -DBATTERY_TYPE=EMBOO
make -j4

# For Nissan Leaf battery
cmake .. -DBATTERY_TYPE=NISSAN_LEAF
make -j4
```

### Run Dashboard in Background

```bash
# Start in background
sudo ./build/leaf-can-dashboard &

# Find process
ps aux | grep leaf-can-dashboard

# Kill process
sudo pkill leaf-can-dashboard
```

---

## GPS Module

### Check GPS Module Status

If using ESP32 GPS module connected via CAN:

```bash
# Monitor GPS CAN messages (0x710, 0x711, 0x712)
candump can0,710:7F0
```

Expected GPS CAN IDs:
- `0x710`: GPS position (latitude, longitude, altitude)
- `0x711`: GPS velocity (speed, heading)
- `0x712`: GPS time (date/time)

### GPS Module Serial Debug

If GPS module is connected via USB for debugging:

```bash
# Find GPS module serial port
ls /dev/ttyUSB*

# Monitor serial output (115200 baud)
screen /dev/ttyUSB0 115200
# Or use minicom
minicom -D /dev/ttyUSB0 -b 115200
```

---

## Web Dashboard Access

### Access Dashboard

Open browser to:
```
http://<raspberry-pi-ip>:8080
```

Find your Raspberry Pi IP:
```bash
hostname -I
```

### Check Web Dashboard Port

```bash
# Check if port 8080 is listening
sudo netstat -tlnp | grep 8080

# Or with ss
sudo ss -tlnp | grep 8080
```

### Allow Port Through Firewall

```bash
# If using ufw firewall
sudo ufw allow 8080/tcp

# Check firewall status
sudo ufw status
```

---

## System Monitoring

### Check CAN Message Rate

```bash
# Show CAN statistics
ip -details -statistics link show can0

# Monitor message rate
candump can0 | pee cat 'xargs -n1 echo' | uniq -c
```

### Check System Resources

```bash
# CPU and memory usage
top

# Or with htop (more user-friendly)
htop

# Disk usage
df -h

# Check temperature (Raspberry Pi)
vcgencmd measure_temp
```

### Check InfluxDB Data

```bash
# Source InfluxDB credentials
source ~/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env

# Query recent data (last 10 records)
influx query 'from(bucket:"'$INFLUX_BUCKET'") |> range(start: -1h) |> limit(n:10)' \
  --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN"

# Query specific measurement (e.g., battery voltage)
influx query 'from(bucket:"'$INFLUX_BUCKET'") |> range(start: -1h) |> filter(fn: (r) => r._measurement == "battery_voltage")' \
  --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN"
```

---

## Troubleshooting

### CAN Interface Not Working

```bash
# Check if CAN kernel modules are loaded
lsmod | grep can

# Load CAN modules if missing
sudo modprobe can
sudo modprobe can_raw
sudo modprobe mcp251x

# Check dmesg for CAN errors
dmesg | grep -i can

# Reset CAN interface
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 250000  # or 500000
sudo ip link set can0 up
```

### No CAN Messages Received

```bash
# 1. Verify CAN interface is up
ip link show can0

# 2. Check for bus errors
ip -details -statistics link show can0

# 3. Check CAN termination resistors (should be 120Ω at each end)

# 4. Try loopback test
sudo ip link set can0 type can bitrate 250000 loopback on
sudo ip link set can0 up
cansend can0 123#DEADBEEF
candump can0  # Should see the message
```

### Telemetry Services Not Starting

```bash
# Check InfluxDB is running
sudo systemctl status influxdb

# Check for errors in logs
sudo journalctl -u telemetry-logger.service -n 100 --no-pager

# Verify InfluxDB config exists
ls -l ~/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env

# Restart services
sudo systemctl restart telemetry-logger.service
```

### Web Dashboard Not Accessible

```bash
# Check if service is running
sudo systemctl status web-dashboard.service

# Check if port is listening
sudo netstat -tlnp | grep 8080

# Check firewall
sudo ufw status

# View recent logs
sudo journalctl -u web-dashboard.service -n 50 -f

# Test locally on Pi
curl http://localhost:8080/api/status
```

### GPS Not Working

```bash
# Check GPS CAN messages
candump can0,710:7F0

# If using ESP32 GPS module via serial:
ls /dev/ttyUSB*
screen /dev/ttyUSB0 115200

# Check for GPS module in process list
ps aux | grep gps
```

---

## Common Command Sequences

### Complete System Startup

```bash
#!/bin/bash
# Complete system startup script

echo "Starting Nissan Leaf CAN Network System..."

# 1. Set up CAN bus (EMBOO Battery - 250 kbps)
echo "Setting up CAN bus at 250 kbps..."
sudo ip link set can0 down 2>/dev/null
sudo ip link set can0 type can bitrate 250000
sudo ip link set can0 up

# 2. Start InfluxDB
echo "Starting InfluxDB..."
sudo systemctl start influxdb
sleep 2

# 3. Start telemetry services
echo "Starting telemetry services..."
sudo systemctl start telemetry-logger.service
sudo systemctl start cloud-sync.service
sudo systemctl start web-dashboard.service

# 4. Wait and verify
sleep 3
echo ""
echo "Service Status:"
systemctl status influxdb --no-pager -l | grep Active
systemctl status telemetry-logger.service --no-pager -l | grep Active
systemctl status cloud-sync.service --no-pager -l | grep Active
systemctl status web-dashboard.service --no-pager -l | grep Active

echo ""
echo "CAN Interface:"
ip -details link show can0 | grep -E "can0|bitrate"

echo ""
echo "System is ready!"
echo "Web dashboard: http://$(hostname -I | awk '{print $1}'):8080"
```

### Complete System Shutdown

```bash
#!/bin/bash
# Complete system shutdown script

echo "Shutting down Nissan Leaf CAN Network System..."

# 1. Stop telemetry services
echo "Stopping telemetry services..."
sudo systemctl stop telemetry-logger.service
sudo systemctl stop cloud-sync.service
sudo systemctl stop web-dashboard.service

# 2. Stop InfluxDB
echo "Stopping InfluxDB..."
sudo systemctl stop influxdb

# 3. Bring down CAN interface
echo "Bringing down CAN interface..."
sudo ip link set can0 down

echo "System shutdown complete."
```

### Quick System Check

```bash
#!/bin/bash
# Quick system health check

echo "=== Nissan Leaf CAN Network System Status ==="
echo ""

echo "CAN Interface:"
ip -details link show can0 | grep -E "can0|bitrate|state"
echo ""

echo "Services:"
systemctl is-active influxdb telemetry-logger.service cloud-sync.service web-dashboard.service | \
  paste <(echo -e "InfluxDB\nTelemetry Logger\nCloud Sync\nWeb Dashboard") -
echo ""

echo "CAN Message Count (last 5 seconds):"
timeout 5 candump can0 2>/dev/null | wc -l
echo ""

echo "Web Dashboard:"
if curl -s http://localhost:8080/api/status >/dev/null 2>&1; then
  echo "✓ Web dashboard accessible at http://$(hostname -I | awk '{print $1}'):8080"
else
  echo "✗ Web dashboard not responding"
fi
```

Save these scripts to `/home/user/scripts/` and make them executable:

```bash
mkdir -p ~/scripts
chmod +x ~/scripts/*.sh
```

---

## Configuration Files

### Important Configuration Files

- **Battery Type**: `ui-dashboard/CMakeLists.txt` (BATTERY_TYPE option)
- **ESP32 Battery Config**: `modules/*/platformio.ini` (build_flags)
- **InfluxDB Local**: `telemetry-system/config/influxdb-local.env`
- **InfluxDB Cloud**: `telemetry-system/config/influxdb-cloud.env`
- **Systemd Services**: `/etc/systemd/system/telemetry-logger.service`

### View Configuration

```bash
# View battery type in CMake build
cd ~/test-lvgl-cross-compile/ui-dashboard/build
grep "Battery type" CMakeCache.txt

# View CAN bitrate
grep CAN_BITRATE ~/test-lvgl-cross-compile/lib/LeafCANBus/src/LeafCANMessages.h
```

---

## Battery Type Quick Reference

### EMBOO Battery (Orion BMS)
- **CAN Speed**: 250 kbps
- **Key CAN IDs**: 0x6B0, 0x6B1, 0x6B2, 0x6B3, 0x6B4, 0x351, 0x355, 0x356, 0x35A
- **Setup**: `sudo ip link set can0 type can bitrate 250000`
- **Build**: `cmake .. -DBATTERY_TYPE=EMBOO`

### Nissan Leaf Battery
- **CAN Speed**: 500 kbps
- **Key CAN IDs**: 0x1DB, 0x1DC, 0x1F2, 0x1DA, 0x390
- **Setup**: `sudo ip link set can0 type can bitrate 500000`
- **Build**: `cmake .. -DBATTERY_TYPE=NISSAN_LEAF`

---

## Support

For detailed information, see:
- Battery Configuration: `docs/BATTERY_CONFIGURATION.md`
- Telemetry System: `telemetry-system/README.md`
- Quick Start: `telemetry-system/QUICKSTART.md`
- Main Documentation: `README.md`
- Project Guidance: `CLAUDE.md`

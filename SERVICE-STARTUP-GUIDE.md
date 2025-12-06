# Service Startup Troubleshooting & Configuration

## Problem: Services Not Starting on Boot

The issue you're experiencing is a **timing problem**: the web-dashboard service tries to start before InfluxDB is fully ready.

### Root Cause
- InfluxDB takes time to initialize on boot
- web-dashboard was starting immediately after network.target
- No explicit wait for InfluxDB to be ready
- Eventually starts because InfluxDB becomes ready

### Solution Applied

**1. Web Dashboard Service (`web-dashboard.service`)**
- Added dependency: `After=influxdb.service`
- Added startup wait script: `ExecStartPre` waits up to 60 seconds for InfluxDB
- Added 5-second grace period after InfluxDB is active

**2. Telemetry Logger Service (`telemetry-logger-unified.service`)**
- Already has proper InfluxDB wait logic
- Waits up to 60 seconds for InfluxDB to be active

## USB GPS Integration

USB GPS support has been integrated into the unified telemetry logger.

### Configuration

Edit `/etc/systemd/system/telemetry-logger-unified.service`:

```ini
# Enable USB GPS
Environment=USB_GPS_ENABLED=true
Environment=USB_GPS_DEVICE=/dev/ttyACM0
```

To **disable** USB GPS (use only CAN GPS):
```ini
Environment=USB_GPS_ENABLED=false
```

### GPS Device Detection

Find your GPS device:
```bash
# List USB serial devices
ls -l /dev/ttyACM* /dev/ttyUSB*

# Check which device is the GPS
dmesg | grep -i gps
dmesg | grep -i tty
```

Common GPS devices:
- `/dev/ttyACM0` - U-Blox USB GPS
- `/dev/ttyUSB0` - USB-to-serial GPS
- `/dev/serial/by-id/usb-u-blox_*` - U-Blox by ID

### Required Packages

USB GPS requires additional Python packages:
```bash
pip3 install pyserial pynmea2
```

These are already in `telemetry-system/services/telemetry-logger/requirements.txt`.

## Service Restart Procedure

After making changes, reload and restart services:

```bash
# Copy updated service files
cd ~/Projects/test-lvgl-cross-compile/telemetry-system
sudo cp systemd/web-dashboard.service /etc/systemd/system/
sudo cp systemd/telemetry-logger-unified.service /etc/systemd/system/

# Reload systemd configuration
sudo systemctl daemon-reload

# Restart services
sudo systemctl restart influxdb
sudo systemctl restart telemetry-logger-unified
sudo systemctl restart web-dashboard

# Check status
systemctl status influxdb
systemctl status telemetry-logger-unified
systemctl status web-dashboard
```

## Verify Boot Startup

Reboot and check all services start correctly:

```bash
sudo reboot

# After reboot, check services
systemctl status influxdb
systemctl status telemetry-logger-unified
systemctl status web-dashboard

# Check logs
sudo journalctl -u web-dashboard -f
sudo journalctl -u telemetry-logger-unified -f
```

## GPS Status Verification

Check if GPS is working:

```bash
# View telemetry logger logs
sudo journalctl -u telemetry-logger-unified -f

# Should see:
# "Initializing USB GPS: /dev/ttyACM0"
# "USB GPS initialized successfully"
# "GPS reader thread started"
# "[usb_gps] Stats: X msgs (X/s), X writes (X/s), X fixes"
```

GPS data will appear in InfluxDB under the "GPS" measurement with:
- `source=usb_gps` tag
- `device_type=U-Blox` tag
- Fields: latitude, longitude, altitude, satellites, speed_kmh, heading

## Troubleshooting

### Web Dashboard Won't Start

```bash
# Check InfluxDB is running
systemctl status influxdb

# If InfluxDB is down, start it
sudo systemctl start influxdb

# Wait a few seconds, then start dashboard
sleep 5
sudo systemctl start web-dashboard
```

### USB GPS Not Working

```bash
# Check device exists
ls -l /dev/ttyACM0

# Check permissions
sudo usermod -a -G dialout emboo
sudo reboot  # Required for group membership

# Test GPS manually
sudo cat /dev/ttyACM0
# Should see NMEA sentences like: $GPGGA,... $GPRMC,...

# Check service logs
sudo journalctl -u telemetry-logger-unified | grep -i gps
```

### Service Fails to Start

```bash
# Check detailed error
sudo systemctl status web-dashboard -l
sudo journalctl -xe -u web-dashboard

# Common issues:
# 1. InfluxDB not running → sudo systemctl start influxdb
# 2. Permission denied → Check file paths and user permissions
# 3. Python module missing → pip3 install -r requirements.txt
```

## Service Dependencies Summary

```
Boot Sequence:
1. network.target
2. influxdb.service (waits for network)
3. telemetry-logger-unified.service (waits for InfluxDB)
4. web-dashboard.service (waits for InfluxDB)
5. desktop-dashboard.service (independent, port 80)
6. cloud-sync.service (optional, can start anytime)
```

All services should now start reliably on boot! 🎉

# Telemetry System Quick Start Guide

Get your Nissan Leaf telemetry system up and running in minutes!

## Prerequisites Checklist

- [ ] Raspberry Pi 4 with Raspberry Pi OS installed
- [ ] Waveshare 2-CH CAN HAT properly installed and configured
- [ ] CAN interface `can0` is up and running (`ip link show can0`)
- [ ] Internet connection (for initial setup)

## Installation (5 minutes)

### 1. Install System Dependencies

```bash
sudo apt update && sudo apt install -y \
    python3-pip \
    can-utils \
    influxdb2 \
    influxdb2-cli \
    jq
```

### 2. Install Python Dependencies

```bash
cd ~/test-lvgl-cross-compile/telemetry-system

# Install all required Python packages
pip3 install --break-system-packages \
    python-can \
    influxdb-client \
    Flask \
    Flask-CORS \
    Flask-SocketIO \
    python-socketio \
    eventlet \
    requests
```

### 3. Setup InfluxDB

```bash
chmod +x scripts/setup-influxdb.sh
./scripts/setup-influxdb.sh
```

**Follow the prompts:**
- Username: `admin` (or your choice)
- Password: (min 8 characters)
- Organization: `leaf-telemetry` (recommended)
- Bucket: `leaf-data` (recommended)
- Retention: `30` days (recommended)

This will automatically:
- Install and configure InfluxDB 2.x
- Create API tokens for each service
- Save configuration to `config/influxdb-local.env`

### 4. Configure Cloud Sync (Optional)

If you have InfluxDB Cloud:

```bash
cp config/influxdb-cloud.env.template config/influxdb-cloud.env
nano config/influxdb-cloud.env
```

Fill in your cloud credentials:
```bash
INFLUX_CLOUD_URL=https://us-west-2-1.aws.cloud2.influxdata.com
INFLUX_CLOUD_ORG=your-org-name
INFLUX_CLOUD_BUCKET=leaf-telemetry
INFLUX_CLOUD_TOKEN=your-write-token
```

Save and secure:
```bash
chmod 600 config/influxdb-cloud.env
```

### 5. Install and Start Services

```bash
# Update username in service files (if not using 'emboo')
sed -i "s/emboo/$USER/g" systemd/*.service

# Install systemd services
sudo cp systemd/*.service /etc/systemd/system/

# Reload systemd
sudo systemctl daemon-reload

# Enable and start all services
sudo systemctl enable --now telemetry-logger.service
sudo systemctl enable --now cloud-sync.service
sudo systemctl enable --now web-dashboard.service
```

### 6. Verify Installation

```bash
# Check service status
systemctl status telemetry-logger.service
systemctl status cloud-sync.service
systemctl status web-dashboard.service

# Run automated tests
chmod +x tests/test_system.sh
./tests/test_system.sh
```

## Accessing the Web Dashboard

Open a browser on any device on the same network:

```
http://<your-pi-ip>:8080
```

Find your Pi's IP:
```bash
hostname -I
```

Example: `http://192.168.1.100:8080`

## Testing Without a Vehicle

Use the mock CAN data generator to test the system:

```bash
# Make sure can0 is up
sudo ip link set can0 up type can bitrate 500000

# Run mock generator
python3 tests/mock_can_generator.py can0
```

This simulates realistic driving scenarios. Watch the web dashboard update in real-time!

## Viewing Logs

```bash
# View all logs in real-time
sudo journalctl -u telemetry-logger.service -f
sudo journalctl -u cloud-sync.service -f
sudo journalctl -u web-dashboard.service -f

# View last 50 lines
sudo journalctl -u telemetry-logger.service -n 50
```

## Common First-Time Issues

### Issue: Services won't start

**Solution:**
```bash
# Check if can0 is up
ip link show can0

# If down, bring it up
sudo ip link set can0 up type can bitrate 500000

# Restart services
sudo systemctl restart telemetry-logger.service
```

### Issue: Web dashboard shows no data

**Solution:**
```bash
# Check if telemetry logger is receiving messages
sudo journalctl -u telemetry-logger.service -f

# Send a test CAN message
cansend can0 1F2#0E1000640A1E1C01

# Or run the mock generator
python3 tests/mock_can_generator.py can0
```

### Issue: Cloud sync not working

**Solution:**
```bash
# Verify cloud config exists
cat config/influxdb-cloud.env

# Check cloud sync logs
sudo journalctl -u cloud-sync.service -f

# Test internet connectivity
ping -c 3 1.1.1.1
```

## Next Steps

### Customize Sync Interval

Edit `/etc/systemd/system/cloud-sync.service`:

```ini
Environment=SYNC_INTERVAL_SECONDS=600  # 10 minutes instead of 5
```

Restart:
```bash
sudo systemctl daemon-reload
sudo systemctl restart cloud-sync.service
```

### Change Web Port

Edit `/etc/systemd/system/web-dashboard.service`:

```ini
Environment=WEB_PORT=8888  # Use port 8888 instead of 8080
```

Restart:
```bash
sudo systemctl daemon-reload
sudo systemctl restart web-dashboard.service
```

### Add Firewall Rule (Optional)

If using UFW:
```bash
sudo ufw allow 8080/tcp
```

### Enable on Boot (Already Done)

Services are already configured to start automatically on boot. Test by rebooting:

```bash
sudo reboot
```

After reboot, check that all services started:
```bash
systemctl status telemetry-logger.service
systemctl status cloud-sync.service
systemctl status web-dashboard.service
```

## Architecture Overview

```
┌─────────────────────────────────────────┐
│         Nissan Leaf Vehicle             │
│  ┌──────────┐  ┌───────────┐           │
│  │  ESP32   │  │ Leaf ECU  │           │
│  │  Modules │  │ (Inverter)│           │
│  └────┬─────┘  └─────┬─────┘           │
│       └──────────────┘                  │
│        CAN Bus (500kbps)                │
│               │                         │
└───────────────┼─────────────────────────┘
                ▼
        ┌───────────────┐
        │ Waveshare HAT │
        │    (can0)     │
        └───────┬───────┘
                ▼
        ┌───────────────────────────┐
        │ Telemetry Logger Service  │
        │ (Reads CAN → InfluxDB)    │
        └───────────┬───────────────┘
                    ▼
        ┌───────────────────────────┐
        │  InfluxDB 2.x (Local)     │
        │  • 30 day retention       │
        │  • Time-series database   │
        └─────┬──────────────┬──────┘
              │              │
              ▼              ▼
    ┌──────────────┐  ┌────────────────┐
    │  Cloud Sync  │  │ Web Dashboard  │
    │  (When online)│  │   (Port 8080)  │
    └──────┬───────┘  └────────────────┘
           ▼
    ┌──────────────┐
    │ InfluxDB     │
    │ Cloud        │
    └──────────────┘
```

## Features You Can Use Now

✅ **Real-Time Monitoring**: Speed, battery, motor, temps, charging
✅ **Historical Charts**: View trends over 1h, 6h, 24h, or 7 days
✅ **Offline Operation**: Everything works without internet
✅ **Cloud Backup**: Data syncs to cloud when connected
✅ **Auto-Start**: Services start automatically on boot
✅ **Mock Testing**: Test without driving

## Support

- **Full Documentation**: See `README.md` in this directory
- **Architecture**: See `CLAUDE.md` in root directory
- **Pi Setup**: See `Pi Setup.md` in root directory
- **Logs**: `sudo journalctl -u <service-name> -f`

## Quick Command Reference

```bash
# View all services status
systemctl status telemetry-logger cloud-sync web-dashboard

# Restart all services
sudo systemctl restart telemetry-logger cloud-sync web-dashboard

# View CAN traffic
candump can0

# Send test CAN message
cansend can0 1DB#5A0118016800FA0000

# Run mock generator
python3 tests/mock_can_generator.py can0

# Query InfluxDB
source config/influxdb-local.env
influx query 'from(bucket:"'$INFLUX_BUCKET'") |> range(start: -5m)' \
  --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN"

# Access InfluxDB UI
# http://<pi-ip>:8086

# Access Web Dashboard
# http://<pi-ip>:8080
```

---

**Congratulations! Your telemetry system is now running! 🎉**

Drive your Leaf and watch the data flow into the dashboard!

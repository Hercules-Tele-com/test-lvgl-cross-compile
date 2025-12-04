# Complete Kiosk System - One Command Setup

This is the **comprehensive, unified solution** for the Nissan Leaf telemetry kiosk system. Run ONE command and everything is configured with automatic startup, restart policies, and monitoring.

## 🚀 Quick Start (ONE COMMAND)

```bash
cd ~/Projects/test-lvgl-cross-compile
./kiosk/setup-complete-system.sh
```

That's it! The script will:
1. ✅ Configure all 5 services with aggressive restart policies
2. ✅ Set proper startup order with dependency waiting
3. ✅ Enable all services for autostart on boot
4. ✅ Configure boot to desktop
5. ✅ Set up X11 (disable screen blanking, hide cursor)
6. ✅ Create health monitor (checks every 5 minutes)
7. ✅ Start everything immediately

## 🏗️ System Architecture

```
Boot Sequence (Automatic):
├─ 1. InfluxDB starts
│    ├─ Restart: always
│    └─ Max failures: 10 in 5 minutes
│
├─ 2. Telemetry Logger starts (waits for InfluxDB)
│    ├─ Reads: CAN bus (can0, can1)
│    ├─ Writes: InfluxDB
│    ├─ Restart: always (5s delay)
│    └─ Max failures: 10 in 5 minutes
│
├─ 3. GPS Reader starts (waits for InfluxDB)
│    ├─ Reads: /dev/ttyACM0
│    ├─ Writes: InfluxDB
│    ├─ Restart: always (10s delay)
│    └─ Max failures: 10 in 5 minutes
│
├─ 4. Web Dashboard starts (waits for InfluxDB)
│    ├─ Reads: InfluxDB
│    ├─ Serves: http://localhost:8080
│    ├─ Restart: always (5s delay)
│    └─ Max failures: 10 in 5 minutes
│
└─ 5. Kiosk starts (waits for web dashboard)
     ├─ Displays: Chromium fullscreen
     ├─ URL: http://localhost:8080
     ├─ Restart: always (10s delay)
     └─ Max failures: 10 in 5 minutes

Health Monitor (every 5 minutes):
└─ Checks all services, restarts if stopped
```

## 📊 What Gets Configured

### Services Created/Updated

All services get these features:

| Service | Purpose | Restart Policy | Dependencies |
|---------|---------|----------------|--------------|
| **influxdb** | Time-series database | `Restart=on-failure` | None |
| **telemetry-logger-unified** | CAN bus reader | `Restart=always`, 5s delay | InfluxDB |
| **usb-gps-reader** | GPS data reader | `Restart=always`, 10s delay | InfluxDB |
| **web-dashboard** | Flask web UI | `Restart=always`, 5s delay | InfluxDB |
| **dashboard-kiosk** | Chromium kiosk | `Restart=always`, 10s delay | Web Dashboard |

### Restart Policies

Every service has:
- `Restart=always` - Service restarts on any failure
- `RestartSec=5-10` - Wait 5-10 seconds before restart
- `StartLimitBurst=10` - Allow up to 10 restart attempts
- `StartLimitInterval=300` - Within 5 minutes

**This means:** If a service crashes, it automatically restarts up to 10 times in 5 minutes.

### Startup Dependencies

Services wait for their dependencies using retry loops:
```bash
ExecStartPre=/bin/bash -c 'for i in {1..30}; do systemctl is-active --quiet influxdb.service && break || sleep 2; done'
```

**This means:** Each service waits up to 60 seconds for its dependencies to be ready.

### Health Monitoring

A cron job runs every 5 minutes:
```bash
*/5 * * * * /home/emboo/Projects/test-lvgl-cross-compile/kiosk/health-check.sh
```

**This means:** Every 5 minutes, the system checks if all services are running. If any stopped, it restarts them.

## 🔍 Monitoring & Troubleshooting

### Check Status

```bash
# Check all services at once
systemctl status influxdb telemetry-logger-unified usb-gps-reader web-dashboard dashboard-kiosk

# Quick status check
systemctl is-active influxdb telemetry-logger-unified usb-gps-reader web-dashboard dashboard-kiosk
```

### View Logs

```bash
# Follow all logs in real-time
sudo journalctl -f

# View specific service
sudo journalctl -u telemetry-logger-unified.service -f

# View recent logs from all services
sudo journalctl -u influxdb -u telemetry-logger-unified -u usb-gps-reader -u web-dashboard -u dashboard-kiosk -b
```

### Health Monitor Logs

```bash
# View health check activity
tail -f /tmp/health-check.log

# See if health monitor is running
crontab -l | grep health-check
```

### Manual Control

```bash
# Restart everything
sudo systemctl restart influxdb telemetry-logger-unified usb-gps-reader web-dashboard dashboard-kiosk

# Stop everything
sudo systemctl stop influxdb telemetry-logger-unified usb-gps-reader web-dashboard dashboard-kiosk

# Disable autostart (keeps current session running)
sudo systemctl disable telemetry-logger-unified usb-gps-reader web-dashboard dashboard-kiosk
```

## 🛡️ Reliability Features

### Automatic Restart

If any service crashes:
1. Systemd detects the failure immediately
2. Waits 5-10 seconds (RestartSec)
3. Attempts to restart
4. Repeats up to 10 times in 5 minutes
5. If still failing after 10 attempts, stops trying (prevents restart loops)

### Dependency Handling

If a dependency fails to start:
- Service waits up to 60 seconds with retry loop
- If dependency becomes available within 60s, service starts
- If not, service fails but systemd will retry (Restart=always)

### Health Monitoring

Every 5 minutes:
- Health monitor checks if all services are active
- If any service stopped, automatically restarts it
- Logs all restart attempts to `/tmp/health-check.log`

### Graceful Degradation

Services are designed to handle missing dependencies:
- Web dashboard works without data (shows "unknown" status)
- GPS reader restarts if GPS unplugged/replugged
- Telemetry logger handles CAN bus disconnects

## 🔧 Customization

### Change Restart Delay

Edit service files in `/etc/systemd/system/` and modify:
```ini
RestartSec=10  # Change to desired delay in seconds
```

Then reload:
```bash
sudo systemctl daemon-reload
sudo systemctl restart <service-name>
```

### Change Health Check Interval

Edit crontab:
```bash
crontab -e

# Change from every 5 minutes to every 1 minute:
*/1 * * * * /home/emboo/Projects/test-lvgl-cross-compile/kiosk/health-check.sh >> /tmp/health-check.log 2>&1
```

### Disable Health Monitor

```bash
crontab -e
# Comment out or delete the health-check.sh line
```

## 🧪 Testing Reliability

### Test Crash Recovery

```bash
# Kill a service and watch it restart
sudo pkill -f telemetry_logger
sleep 5
systemctl status telemetry-logger-unified

# Should show: restarted automatically
```

### Test Boot Reliability

```bash
# Reboot and verify all services start
sudo reboot

# After reboot, check status
systemctl status influxdb telemetry-logger-unified usb-gps-reader web-dashboard dashboard-kiosk
```

### Test Health Monitor

```bash
# Stop a service
sudo systemctl stop telemetry-logger-unified

# Wait 5 minutes, check if it restarted
sleep 300
systemctl status telemetry-logger-unified

# Check health monitor log
cat /tmp/health-check.log
```

## 📋 Verification Checklist

After running `setup-complete-system.sh`:

- [ ] All 5 services show as `enabled` (will start on boot)
- [ ] All 5 services show as `active (running)`
- [ ] Web dashboard accessible at http://localhost:8080
- [ ] Kiosk visible on display
- [ ] Health monitor added to crontab
- [ ] Boot configured for desktop autologin
- [ ] Screen blanking disabled

Verify with:
```bash
systemctl list-unit-files | grep -E "influxdb|telemetry-logger|gps-reader|web-dashboard|dashboard-kiosk"
systemctl status influxdb telemetry-logger-unified usb-gps-reader web-dashboard dashboard-kiosk
crontab -l | grep health-check
curl http://localhost:8080/api/status
```

## 🚨 Emergency Recovery

If everything breaks:

```bash
# Stop all services
sudo systemctl stop influxdb telemetry-logger-unified usb-gps-reader web-dashboard dashboard-kiosk

# Run setup again
cd ~/Projects/test-lvgl-cross-compile
./kiosk/setup-complete-system.sh

# Or reboot (everything will auto-start)
sudo reboot
```

## 📞 Support

If services still won't start after setup:

1. Check logs: `sudo journalctl -xe`
2. Check InfluxDB config: `/home/emboo/Projects/test-lvgl-cross-compile/telemetry-system/config/influxdb-local.env`
3. Verify CAN interfaces: `ip link show can0 can1`
4. Verify GPS device: `ls -la /dev/ttyACM0`

## 🎯 Summary

**Before this system:**
- Services had to be started manually
- No automatic restart on failure
- No health monitoring
- Piecemeal configuration

**After this system:**
- ONE command configures everything
- All services auto-start on boot
- Automatic restart on failure (up to 10 attempts)
- Health monitor checks every 5 minutes
- Comprehensive logging
- Reliable, production-ready

**Just run:** `./kiosk/setup-complete-system.sh` and reboot. Everything works.

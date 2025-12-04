# Quick Fix for Web Dashboard and Cloud Sync Services

## Issue
The web-dashboard and cloud-sync services were failing to start due to incorrect paths in the service files. The services expected `/home/emboo/test-lvgl-cross-compile` but the actual path is `/home/emboo/Projects/test-lvgl-cross-compile`.

## What Was Fixed
- ✅ Updated `web-dashboard.service` with correct paths
- ✅ Updated `cloud-sync.service` with correct paths
- ✅ Made cloud config file optional (won't fail if not configured)
- ✅ Created `update-services.sh` script to simplify updates

## How to Apply the Fix on Your Raspberry Pi

### Quick Method (Recommended)
```bash
# Pull the latest changes
cd ~/Projects/test-lvgl-cross-compile
git pull

# Update the services (requires sudo)
sudo ./telemetry-system/scripts/update-services.sh

# Check if services are running
./scripts/check-system.sh
```

### Manual Method
If the script doesn't work, do it manually:

```bash
# 1. Pull latest changes
cd ~/Projects/test-lvgl-cross-compile
git pull

# 2. Copy service files
sudo cp telemetry-system/systemd/web-dashboard.service /etc/systemd/system/
sudo cp telemetry-system/systemd/cloud-sync.service /etc/systemd/system/

# 3. Reload systemd
sudo systemctl daemon-reload

# 4. Restart services
sudo systemctl restart web-dashboard.service
sudo systemctl restart cloud-sync.service

# 5. Check status
systemctl status web-dashboard.service
systemctl status cloud-sync.service
```

## Verify It's Working

### Check Service Status
```bash
./scripts/check-system.sh
```

You should see:
```
Web Dashboard:            active
Cloud Sync:               active
```

### Access Web Dashboard
Open your browser and go to:
```
http://<your-pi-ip>:8080
```

To find your Pi's IP address:
```bash
hostname -I
```

### Check Logs
If services are still failing, check the logs:

```bash
# Web dashboard logs
sudo journalctl -u web-dashboard.service -n 50 --no-pager

# Cloud sync logs
sudo journalctl -u cloud-sync.service -n 50 --no-pager
```

## Common Issues After Fix

### InfluxDB Not Running
If InfluxDB is not running, the web dashboard will fail:
```bash
sudo systemctl status influxdb
sudo systemctl start influxdb
```

### Cloud Config File Missing (Normal)
If you haven't configured cloud sync yet, the cloud-sync service will log warnings but continue running. This is normal. To configure cloud sync:

```bash
cd ~/Projects/test-lvgl-cross-compile/telemetry-system
cp config/influxdb-cloud.env.template config/influxdb-cloud.env
nano config/influxdb-cloud.env
# Fill in your InfluxDB Cloud credentials
sudo systemctl restart cloud-sync.service
```

### Port 8080 Already in Use
If another service is using port 8080:
```bash
# Check what's using port 8080
sudo lsof -i :8080

# Or change the port in the service file
sudo nano /etc/systemd/system/web-dashboard.service
# Change Environment=WEB_PORT=8080 to another port
sudo systemctl daemon-reload
sudo systemctl restart web-dashboard.service
```

## Summary of Service File Changes

### web-dashboard.service
- ❌ Old: `/home/emboo/test-lvgl-cross-compile/...`
- ✅ New: `/home/emboo/Projects/test-lvgl-cross-compile/...`

### cloud-sync.service
- ❌ Old: `/home/emboo/test-lvgl-cross-compile/...`
- ✅ New: `/home/emboo/Projects/test-lvgl-cross-compile/...`
- ✅ New: `EnvironmentFile=-/home/emboo/Projects/...` (optional)

## Next Steps

Once the services are running:

1. **Verify telemetry data** - Check that the web dashboard shows live data
2. **Test cell voltages** - Confirm 144 cells are being tracked
3. **Check fault detection** - Verify faults appear in the UI
4. **Monitor performance** - Ensure no errors in logs

For ongoing monitoring:
```bash
# Follow logs in real-time
sudo journalctl -u telemetry-logger-unified.service -f

# Check system health
./scripts/check-system.sh
```

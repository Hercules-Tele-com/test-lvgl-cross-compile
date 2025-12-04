# Kiosk Mode Quick Start Guide

Get your Leaf Dashboard running in kiosk mode in under 10 minutes!

## Prerequisites

- Raspberry Pi 4 (4GB recommended)
- Victron Cerbo 50 display (or any HDMI display with touchscreen)
- Raspberry Pi OS installed
- Web dashboard service already set up and running

## Installation (One Command)

```bash
cd ~/Projects/test-lvgl-cross-compile
./kiosk/install-kiosk.sh
```

This will:
- ✓ Install Chromium browser
- ✓ Install unclutter (hides cursor)
- ✓ Configure systemd service
- ✓ Set up X11 autostart
- ✓ Enable boot to desktop
- ✓ Verify web dashboard is running

## Activation

Reboot your Raspberry Pi:
```bash
sudo reboot
```

**That's it!** The dashboard will appear in fullscreen kiosk mode after reboot.

## Touchscreen Configuration

If touchscreen doesn't work after reboot:

1. Connect to the Pi desktop (locally, not via SSH)
2. Open terminal
3. Run:
   ```bash
   cd ~/Projects/test-lvgl-cross-compile
   ./kiosk/configure-touchscreen.sh
   ```

## Verification

After reboot, you should see:
- Fullscreen web dashboard (no browser toolbars)
- No mouse cursor (hidden automatically)
- Touch input working
- Real-time metrics updating

## Troubleshooting

### Black Screen
```bash
# Check HDMI settings
sudo nano /boot/config.txt

# Add these lines:
hdmi_force_hotplug=1
hdmi_drive=2
gpu_mem=128

# Save and reboot
sudo reboot
```

### Browser Not Loading
```bash
# Check web dashboard is running
systemctl status web-dashboard.service

# Start if not running
sudo systemctl start web-dashboard.service

# Restart kiosk
sudo systemctl restart dashboard-kiosk.service
```

### Touchscreen Not Working
```bash
# From Pi desktop (not SSH), run:
./kiosk/configure-touchscreen.sh
```

## Service Management

```bash
# View logs
sudo journalctl -u dashboard-kiosk.service -f

# Stop kiosk
sudo systemctl stop dashboard-kiosk.service

# Start kiosk
sudo systemctl start dashboard-kiosk.service

# Disable kiosk (won't start on boot)
sudo systemctl disable dashboard-kiosk.service
```

## Exit Kiosk Mode

- **Keyboard:** Press `Alt+F4` or `Ctrl+W`
- **SSH:** `sudo systemctl stop dashboard-kiosk.service`

## What's Running?

```
┌─────────────────────────────────────┐
│  Chromium (Kiosk Mode)              │
│  → http://localhost:8080             │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  web-dashboard.service              │
│  Flask (Python) on port 8080        │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  InfluxDB (Local)                   │
└─────────────────────────────────────┘
              ↑
┌─────────────────────────────────────┐
│  telemetry-logger-unified.service   │
│  CAN data (can0, can1)              │
└─────────────────────────────────────┘
```

## For More Details

See [kiosk/README.md](README.md) for:
- Advanced configuration
- Display rotation
- Performance tuning
- Security settings
- Multiple displays
- Troubleshooting guide

## Next Steps

Once kiosk mode is running:

1. **Test touchscreen:** Touch the screen and verify it works
2. **Configure display:** Adjust brightness/rotation if needed
3. **Verify data:** Check that CAN data is flowing (battery, motor, GPS)
4. **Remote access:** Access dashboard from phone/laptop at `http://<pi-ip>:8080`

## Support

Issues? Check:
1. Logs: `sudo journalctl -u dashboard-kiosk.service -f`
2. Full README: `kiosk/README.md`
3. GitHub Issues: https://github.com/Hercules-Tele-com/test-lvgl-cross-compile/issues

# Kiosk Mode Configuration

This directory contains configuration files and scripts for running the Leaf Dashboard in kiosk mode on a Raspberry Pi with the Victron Cerbo 50 display (or any HDMI touchscreen).

## Quick Start

```bash
# Install kiosk mode
./kiosk/install-kiosk.sh

# Reboot to activate
sudo reboot

# After reboot, configure touchscreen (from desktop, not SSH)
./kiosk/configure-touchscreen.sh
```

## Files

### `dashboard-kiosk.service`
Systemd service that launches Chromium in kiosk mode pointing to `http://localhost:8080`.

**Features:**
- Auto-starts on boot (after graphical.target)
- Runs as user `emboo`
- Waits 10 seconds for web dashboard to be ready
- Auto-restarts on crash
- Fullscreen kiosk mode with no toolbars/dialogs

**Installed to:** `/etc/systemd/system/dashboard-kiosk.service`

### `autostart`
X11 autostart script for LXDE desktop environment.

**Features:**
- Disables screen blanking and power management
- Hides mouse cursor after 0.1s inactivity
- Optional display rotation configuration
- Optional touchscreen calibration

**Installed to:** `~/.config/lxsession/LXDE-pi/autostart`

### `configure-touchscreen.sh`
Script to detect and configure touchscreen input device.

**Usage:**
```bash
# MUST be run from desktop (not SSH)
./kiosk/configure-touchscreen.sh
```

**Features:**
- Auto-detects touchscreen device
- Lists all input devices
- Applies coordinate transformation matrix
- Provides rotation examples (90°, 180°, 270°)

### `install-kiosk.sh`
Main installation script that sets up everything.

**What it does:**
1. Installs chromium, unclutter, x11-xserver-utils
2. Copies systemd service to `/etc/systemd/system/`
3. Creates X11 autostart configuration
4. Configures Pi to boot to desktop with autologin
5. Enables kiosk service
6. Verifies web dashboard is running

## Architecture

```
Boot Sequence:
1. Raspberry Pi boots
2. X11 desktop starts (auto-login as emboo)
3. autostart script runs:
   - Disable screen blanking
   - Hide mouse cursor
4. dashboard-kiosk.service starts:
   - Wait 10 seconds for web dashboard
   - Launch Chromium in kiosk mode
   - Display http://localhost:8080

Runtime:
┌─────────────────────────────────────────┐
│  Chromium Kiosk (DISPLAY=:0)            │
│  http://localhost:8080                  │
│                                          │
│  ┌────────────────────────────────────┐ │
│  │  Web Dashboard (HTML/JS)           │ │
│  │  - Real-time metrics via WebSocket │ │
│  │  - GPS map (Leaflet.js)            │ │
│  │  - Battery/Motor telemetry         │ │
│  └────────────────────────────────────┘ │
└─────────────────────────────────────────┘
              ↓ HTTP/WebSocket
┌─────────────────────────────────────────┐
│  web-dashboard.service                  │
│  Flask (Python) on port 8080            │
└─────────────────────────────────────────┘
              ↓ InfluxDB Query
┌─────────────────────────────────────────┐
│  InfluxDB (Local)                       │
│  Telemetry data storage                 │
└─────────────────────────────────────────┘
              ↑ CAN data
┌─────────────────────────────────────────┐
│  telemetry-logger-unified.service       │
│  Reads CAN (can0, can1)                 │
└─────────────────────────────────────────┘
```

## Display Configuration

### Victron Cerbo 50 Specifications
- **Resolution:** 800x480 pixels
- **Interface:** HDMI
- **Touchscreen:** HDMI-CEC touch (USB not required)
- **Brightness:** Auto-adjustable

### Recommended Settings
For optimal display on Victron Cerbo 50, edit `/boot/config.txt`:

```ini
# HDMI settings for Victron Cerbo 50
hdmi_force_hotplug=1
hdmi_drive=2
hdmi_group=2
hdmi_mode=87
hdmi_cvt=800 480 60 6 0 0 0

# Disable overscan
disable_overscan=1

# GPU memory (for Chromium)
gpu_mem=128
```

After editing, reboot:
```bash
sudo reboot
```

## Touchscreen Configuration

### Auto-Detection
The `configure-touchscreen.sh` script attempts to auto-detect the touchscreen device.

### Manual Configuration
If auto-detection fails:

1. List input devices:
   ```bash
   xinput list
   ```

2. Find your touchscreen (e.g., "FT5406 memory based driver")

3. Get device ID (e.g., `id=6`)

4. Set transformation matrix:
   ```bash
   xinput set-prop 6 --type=float "Coordinate Transformation Matrix" 1 0 0 0 1 0 0 0 1
   ```

5. Add to `autostart` to make permanent

### Rotation Examples
If the touchscreen orientation is wrong:

**90° clockwise:**
```bash
xinput set-prop <DEVICE_ID> --type=float "Coordinate Transformation Matrix" 0 1 0 -1 0 1 0 0 1
```

**180° (upside down):**
```bash
xinput set-prop <DEVICE_ID> --type=float "Coordinate Transformation Matrix" -1 0 1 0 -1 1 0 0 1
```

**270° clockwise (90° counter-clockwise):**
```bash
xinput set-prop <DEVICE_ID> --type=float "Coordinate Transformation Matrix" 0 -1 1 1 0 0 0 0 1
```

## Service Management

### Start/Stop Kiosk
```bash
# Start kiosk
sudo systemctl start dashboard-kiosk.service

# Stop kiosk
sudo systemctl stop dashboard-kiosk.service

# Restart kiosk
sudo systemctl restart dashboard-kiosk.service
```

### Enable/Disable Auto-Start
```bash
# Enable (start on boot)
sudo systemctl enable dashboard-kiosk.service

# Disable (don't start on boot)
sudo systemctl disable dashboard-kiosk.service
```

### View Logs
```bash
# Follow logs in real-time
sudo journalctl -u dashboard-kiosk.service -f

# View recent logs
sudo journalctl -u dashboard-kiosk.service -n 100

# View logs since boot
sudo journalctl -u dashboard-kiosk.service -b
```

### Check Status
```bash
# Check if service is running
sudo systemctl status dashboard-kiosk.service

# Check all related services
systemctl status dashboard-kiosk.service web-dashboard.service telemetry-logger-unified.service
```

## Troubleshooting

### Black Screen on Boot
**Symptoms:** Display shows black screen or "No signal"

**Solutions:**
1. Check HDMI cable connection
2. Verify `/boot/config.txt` HDMI settings (see Display Configuration above)
3. Check logs: `sudo journalctl -u dashboard-kiosk.service`
4. Try different HDMI mode: `hdmi_mode=16` (1024x768) or `hdmi_mode=4` (720p)

### Touchscreen Not Working
**Symptoms:** Touch input doesn't move cursor or register clicks

**Solutions:**
1. Verify touchscreen is detected:
   ```bash
   xinput list | grep -i touch
   ```
2. Run configuration script from desktop (not SSH):
   ```bash
   ./kiosk/configure-touchscreen.sh
   ```
3. Check kernel logs:
   ```bash
   dmesg | grep -i touch
   ```
4. Ensure touchscreen driver is loaded:
   ```bash
   lsmod | grep hid
   ```

### Browser Not Loading
**Symptoms:** Chromium starts but shows error page

**Solutions:**
1. Verify web dashboard is running:
   ```bash
   systemctl status web-dashboard.service
   curl http://localhost:8080
   ```
2. Check web dashboard logs:
   ```bash
   sudo journalctl -u web-dashboard.service -f
   ```
3. Start web dashboard manually:
   ```bash
   sudo systemctl start web-dashboard.service
   ```

### Cursor Still Visible
**Symptoms:** Mouse cursor is visible on screen

**Solutions:**
1. Verify unclutter is running:
   ```bash
   ps aux | grep unclutter
   ```
2. Check autostart configuration:
   ```bash
   cat ~/.config/lxsession/LXDE-pi/autostart
   ```
3. Start unclutter manually:
   ```bash
   unclutter -idle 0.1 -root &
   ```

### Screen Blanks After Inactivity
**Symptoms:** Display turns off after a few minutes

**Solutions:**
1. Verify xset commands in autostart:
   ```bash
   cat ~/.config/lxsession/LXDE-pi/autostart
   ```
2. Manually disable blanking:
   ```bash
   xset s off
   xset -dpms
   xset s noblank
   ```
3. Check for conflicting power management:
   ```bash
   # Disable console blanking
   sudo sh -c 'echo -e "\nconsoleblank=0" >> /boot/cmdline.txt'
   sudo reboot
   ```

### Kiosk Service Fails to Start
**Symptoms:** `systemctl status dashboard-kiosk.service` shows "failed"

**Solutions:**
1. Check detailed logs:
   ```bash
   sudo journalctl -u dashboard-kiosk.service -xe
   ```
2. Verify X11 is running:
   ```bash
   echo $DISPLAY  # Should show :0 or :0.0
   ```
3. Test Chromium manually:
   ```bash
   chromium --version
   chromium --kiosk http://localhost:8080
   ```
4. Increase startup delay in service file (edit `ExecStartPre=/bin/sleep 10` to higher value)

### Performance Issues
**Symptoms:** Dashboard is slow or choppy

**Solutions:**
1. Check CPU/memory usage:
   ```bash
   top
   ```
2. Increase GPU memory in `/boot/config.txt`:
   ```ini
   gpu_mem=256  # Increase from 128
   ```
3. Close unnecessary services:
   ```bash
   sudo systemctl disable bluetooth.service
   sudo systemctl disable hciuart.service
   ```
4. Reduce Chromium flags (edit `dashboard-kiosk.service`):
   - Remove `--disable-features=...` lines for testing

## Exiting Kiosk Mode

To temporarily exit kiosk mode without stopping the service:

1. **Keyboard shortcuts:**
   - `Alt + F4` - Close Chromium
   - `Ctrl + W` - Close current window
   - `Alt + Tab` - Switch to other windows

2. **Via SSH:**
   ```bash
   sudo systemctl stop dashboard-kiosk.service
   ```

3. **Permanently disable:**
   ```bash
   sudo systemctl disable dashboard-kiosk.service
   sudo reboot
   ```

## Security Considerations

### Network Exposure
- Web dashboard listens on `localhost:8080` only (not exposed to network by default)
- To access from other devices on LAN, edit `web-dashboard.service`:
  ```ini
  Environment=WEB_HOST=0.0.0.0  # Listen on all interfaces
  ```

### User Permissions
- Kiosk runs as user `emboo` (not root)
- NoNewPrivileges=true in service file (prevents privilege escalation)
- PrivateTmp=true (isolated /tmp directory)

### Browser Sandbox
- Chromium runs in sandboxed mode
- Extensions disabled for security
- Auto-updates disabled to prevent unexpected changes

## Advanced Configuration

### Custom Chromium Flags
Edit `dashboard-kiosk.service` to add more flags:

```ini
ExecStart=/usr/bin/chromium \
    --kiosk \
    --window-size=800,480 \         # Match display resolution
    --window-position=0,0 \         # Top-left corner
    --force-device-scale-factor=1.25 \  # Scale UI (for readability)
    --app=http://localhost:8080
```

### Multiple Displays
To support multiple displays, create separate service files:

```bash
# Primary display (HDMI-0)
cp dashboard-kiosk.service dashboard-kiosk-display0.service
# Edit: Environment=DISPLAY=:0.0

# Secondary display (HDMI-1)
cp dashboard-kiosk.service dashboard-kiosk-display1.service
# Edit: Environment=DISPLAY=:0.1
```

### Watchdog Timer
To automatically restart if dashboard becomes unresponsive:

Edit `dashboard-kiosk.service`:
```ini
[Service]
WatchdogSec=60
Restart=on-watchdog
```

## Performance Tuning

### Raspberry Pi 4 Optimization
```bash
# /boot/config.txt
gpu_mem=256
over_voltage=2
arm_freq=1750  # Mild overclock (safe for Pi 4)
```

### Chromium Optimization
For better performance on low-end hardware:
```ini
# In dashboard-kiosk.service
ExecStart=/usr/bin/chromium \
    --kiosk \
    --disable-gpu \                # Use software rendering (if GPU causes issues)
    --disable-software-rasterizer \
    --enable-low-end-device-mode \
    --app=http://localhost:8080
```

## Comparison: Kiosk Mode vs LVGL Dashboard

| Feature | LVGL Dashboard | Web Kiosk |
|---------|----------------|-----------|
| **Boot Time** | 2-3 seconds | 5-8 seconds |
| **Memory Usage** | 30 MB | 150-200 MB |
| **CPU Usage (Idle)** | 3-5% | 8-12% |
| **Development Speed** | Slow (C++ recompile) | Fast (F5 refresh) |
| **Remote Access** | No | Yes (any device on LAN) |
| **Maps** | Custom implementation | Leaflet.js (built-in) |
| **Charts** | Limited widgets | Chart.js (full-featured) |
| **Maintenance** | Complex (C++) | Simple (HTML/JS) |

**Recommendation:** Use web kiosk for easier development and remote access. Use LVGL for minimal resource usage or offline-only deployments.

## Related Documentation

- [KIOSK_PROPOSAL.md](../docs/KIOSK_PROPOSAL.md) - Detailed architecture comparison
- [Web Dashboard Service](../telemetry-system/services/web-dashboard/README.md)
- [Telemetry System](../telemetry-system/README.md)
- [Raspberry Pi Setup](../Pi%20Setup.md)

## Support

For issues or questions:
1. Check logs: `sudo journalctl -u dashboard-kiosk.service -f`
2. Review troubleshooting section above
3. Open issue on GitHub: https://github.com/Hercules-Tele-com/test-lvgl-cross-compile/issues

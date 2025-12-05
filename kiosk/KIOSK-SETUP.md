# Kiosk Mode Setup Guide

Complete guide for setting up the EMBOO Telemetry Kiosk Mode on Raspberry Pi with Victron Cerbo 50 touchscreen (800x480).

## 📋 Summary of Completed Tasks

### ✅ Recently Completed (Latest Session)
1. **DNR Selector Redesign** - Changed from full words to letter-only format (D N R) with active highlighting
2. **EMBOO Logo Integration** - Added sunset orange logo above DNR selector
3. **Albert Sans Typography** - Integrated custom font family throughout UI
4. **DC Bus Voltage Fix** - Corrected scaling (now shows 337V instead of 3370V)
5. **Battery Cell Chart** - Added toggle view with individual cell monitoring and highest/lowest highlighting
6. **Charger Status Updates** - Changed "Offline" to "Disconnected" with auto-page switching

### ✅ Previously Completed
1. **Screen Resolution Optimization** - Adjusted for 800x480 Victron Cerbo 50
2. **Page Navigation System** - Bottom navigation buttons with 5 pages
3. **Branding Updates** - Renamed "Leaf Telemetry" to "Emboo Telemetry"
4. **Live Clock** - Top-left HH:MM:SS display
5. **Temperature Color Coding** - Green/orange/red status indicators
6. **Real-time Data Flow** - WebSocket integration with fallback polling
7. **GPS Integration** - Live map with vehicle position
8. **Service Configuration** - Systemd services for telemetry, cloud sync, and web dashboard

---

## 🖥️ Hardware Requirements

- **Raspberry Pi 4** (4GB recommended)
- **Victron Cerbo 50 Touchscreen** (800x480, 5-inch)
- **Waveshare CAN HAT** (for can0 interface at 250 kbps)
- **EMBOO Battery** with Orion BMS
- **ROAM Motor** controller
- **GPS Module** (optional, via serial)
- **Power Supply** (12V from vehicle battery)

---

## 🚀 Quick Setup (New Installation)

### 1. Install Base System

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install required packages
sudo apt install -y \
    python3-pip \
    python3-flask \
    python3-flask-socketio \
    python3-influxdb-client \
    chromium-browser \
    x11-xserver-utils \
    unclutter \
    matchbox-window-manager \
    xinit \
    can-utils \
    git

# Install Python packages
pip3 install python-can pyserial
```

### 2. Configure CAN Interface

Create CAN interface configuration:

```bash
sudo nano /etc/network/interfaces.d/can0
```

Add the following:

```
auto can0
iface can0 inet manual
    pre-up /sbin/ip link set can0 type can bitrate 250000
    up /sbin/ifconfig can0 up
    down /sbin/ifconfig can0 down
```

**Important:** EMBOO Battery uses **250 kbps** (not 500 kbps like Nissan Leaf)

Enable CAN interface:

```bash
sudo ifconfig can0 up
```

Verify CAN is working:

```bash
ip -details link show can0
candump can0
```

### 3. Clone Repository

```bash
cd ~
git clone <your-repo-url> test-lvgl-cross-compile
cd test-lvgl-cross-compile
```

### 4. Setup Telemetry System

```bash
cd telemetry-system

# Setup InfluxDB
./scripts/setup-influxdb.sh

# Configure cloud sync (optional)
cp config/influxdb-cloud.env.template config/influxdb-cloud.env
nano config/influxdb-cloud.env  # Add your cloud credentials

# Install systemd services
sudo cp systemd/telemetry-logger-unified.service /etc/systemd/system/
sudo cp systemd/cloud-sync.service /etc/systemd/system/
sudo cp systemd/web-dashboard.service /etc/systemd/system/
sudo cp systemd/desktop-dashboard.service /etc/systemd/system/

# Enable and start services
sudo systemctl daemon-reload
sudo systemctl enable telemetry-logger-unified cloud-sync web-dashboard desktop-dashboard
sudo systemctl start telemetry-logger-unified cloud-sync web-dashboard desktop-dashboard
```

Verify services are running:

```bash
systemctl status telemetry-logger-unified
systemctl status web-dashboard          # Kiosk mode on port 8080
systemctl status desktop-dashboard      # Desktop view on port 80
```

**Port Configuration:**
- **Port 8080**: Kiosk mode (for Victron touchscreen) - `http://localhost:8080`
- **Port 80**: Original desktop dashboard (for remote viewing) - `http://localhost:80`

### 5. Configure Kiosk Mode

Run the setup script:

```bash
cd ~/test-lvgl-cross-compile/kiosk
./setup-kiosk.sh
```

This script will:
- Configure X11 for kiosk mode
- Set up autostart to launch Chromium in kiosk mode
- Configure touchscreen (if needed)
- Disable screen blanking and screensaver

### 6. Configure Autostart

```bash
cd ~/test-lvgl-cross-compile/kiosk
./update-autostart-kiosk.sh
```

This creates/updates `~/.config/lxsession/LXDE-pi/autostart` with:
- Screen blanking disabled
- Chromium kiosk mode at http://localhost:8080
- Unclutter to hide cursor

### 7. Reboot

```bash
sudo reboot
```

After reboot, kiosk mode should auto-start and display the EMBOO telemetry dashboard.

---

## 🔧 Manual Testing

### Test Web Dashboard

```bash
# Check if service is running
systemctl status web-dashboard.service

# Test from command line
curl http://localhost:8080/api/status

# View in browser (if connected via SSH with X forwarding)
chromium-browser http://localhost:8080
```

### Launch Kiosk Mode Manually

```bash
cd ~/test-lvgl-cross-compile/kiosk
./launch-kiosk.sh
```

### Check CAN Data

```bash
# Monitor CAN traffic
candump can0

# Send test message
cansend can0 1F2#0E1000640A1E1C01

# Check CAN interface status
ip -details link show can0
```

### View Service Logs

```bash
# Telemetry logger
sudo journalctl -u telemetry-logger-unified.service -f

# Web dashboard
sudo journalctl -u web-dashboard.service -f

# Cloud sync
sudo journalctl -u cloud-sync.service -f
```

---

## 📱 Touchscreen Configuration

### Victron Cerbo 50 Touchscreen

The Victron Cerbo 50 is an 800x480 5-inch touchscreen. If touch input is not working correctly:

```bash
cd ~/test-lvgl-cross-compile/kiosk
sudo ./configure-touchscreen.sh
```

This script will:
1. Detect connected touchscreen devices
2. Allow you to select the correct device
3. Configure X11 to use the touchscreen
4. Test touch calibration

### Manual Touchscreen Configuration

```bash
# List input devices
xinput list

# Set touchscreen device (replace <device-id> with actual ID)
xinput set-prop <device-id> "Coordinate Transformation Matrix" 1 0 0 0 1 0 0 0 1
```

---

## 🔄 Service Management

### Start/Stop/Restart Services

```bash
# Start all services
cd ~/test-lvgl-cross-compile/kiosk
./start-all.sh

# Individual services
sudo systemctl restart telemetry-logger-unified
sudo systemctl restart web-dashboard
sudo systemctl restart cloud-sync

# Stop services
sudo systemctl stop telemetry-logger-unified
sudo systemctl stop web-dashboard
sudo systemctl stop cloud-sync

# Check status
systemctl status telemetry-logger-unified
systemctl status web-dashboard
systemctl status cloud-sync
```

### Enable/Disable Auto-Start

```bash
# Enable services to start on boot
sudo systemctl enable telemetry-logger-unified
sudo systemctl enable web-dashboard
sudo systemctl enable cloud-sync

# Disable auto-start
sudo systemctl disable telemetry-logger-unified
sudo systemctl disable web-dashboard
sudo systemctl disable cloud-sync
```

---

## 🎨 Kiosk Mode Features

### Page Navigation

The kiosk mode includes 5 pages accessible via bottom navigation buttons:

1. **Overview** - SOC, speed, power, DNR selector, temperatures
2. **Battery** - Detailed battery metrics with cell chart toggle
3. **Motor** - RPM, torque, DC bus voltage, temperatures
4. **GPS** - Live map with vehicle position and heading
5. **Charger** - Charging status, voltage, current, power

### DNR Selector

- **D** (Drive) - Green with glow effect when active
- **N** (Neutral) - Gray with glow effect when active (default)
- **R** (Reverse) - Orange with glow effect when active
- EMBOO logo displayed above selector

### Battery Cell Chart

Toggle between Summary and Cell Chart views:
- **Summary View** - SOC, voltage, current, power, temperature, cell range
- **Cell Chart View** - Individual cell voltages with highest (red) and lowest (blue) highlighting
- Supports up to 144 cells
- Shows average, delta, and cell numbers

### Temperature Color Coding

- **Green** - Normal operating range
- **Orange** - Warm (approaching limits)
- **Red** - Hot (exceeds safe limits)

Thresholds:
- Battery: <50°C (normal), 50-60°C (warm), >60°C (hot)
- Motor: <80°C (normal), 80-100°C (warm), >100°C (hot)
- Inverter: <80°C (normal), 80-100°C (warm), >100°C (hot)

### Auto-Switch to Charger

When the charger is connected, the dashboard automatically switches to the Charger page.

---

## 🐛 Troubleshooting

### Kiosk Mode Not Starting

```bash
# Check X11 display
echo $DISPLAY

# Check autostart file
cat ~/.config/lxsession/LXDE-pi/autostart

# Check if Chromium is installed
which chromium-browser
which chromium

# Manually test kiosk launch
cd ~/test-lvgl-cross-compile/kiosk
./launch-kiosk.sh
```

### Web Dashboard Not Loading

```bash
# Check service status
systemctl status web-dashboard.service

# Check logs
sudo journalctl -u web-dashboard.service -n 50

# Test API endpoint
curl http://localhost:8080/api/status

# Restart service
sudo systemctl restart web-dashboard.service
```

### No CAN Data

```bash
# Check CAN interface is up
ip link show can0

# Bring up CAN interface
sudo ifconfig can0 down
sudo ip link set can0 type can bitrate 250000
sudo ifconfig can0 up

# Monitor CAN traffic
candump can0

# Check if telemetry logger is running
systemctl status telemetry-logger-unified.service

# View telemetry logs
sudo journalctl -u telemetry-logger-unified.service -f
```

### Touchscreen Not Responding

```bash
# Check X11 input devices
xinput list

# Reconfigure touchscreen
cd ~/test-lvgl-cross-compile/kiosk
sudo ./configure-touchscreen.sh

# Restart X11 session
sudo systemctl restart lightdm
```

### Screen Blanking

If the screen still blanks despite configuration:

```bash
# Edit autostart
nano ~/.config/lxsession/LXDE-pi/autostart

# Ensure these lines are present:
@xset s off
@xset -dpms
@xset s noblank
```

### Fonts Not Loading

If Albert Sans fonts don't load:

```bash
# Check font files
ls -lh ~/test-lvgl-cross-compile/telemetry-system/services/web-dashboard/static/fonts/

# Should see:
# AlbertSans-Regular.ttf
# AlbertSans-Medium.ttf
# AlbertSans-Bold.ttf

# If missing, copy from assets
cp ~/test-lvgl-cross-compile/lvgl-demo/assets/AlbertSans-*.ttf \
   ~/test-lvgl-cross-compile/telemetry-system/services/web-dashboard/static/fonts/

# Clear browser cache
rm -rf ~/.cache/chromium/
```

### DC Bus Voltage Incorrect

The DC bus voltage should be displayed as ~337V (not 3370V). If it shows the wrong value:

```bash
# Check kiosk.js has the division by 10
grep -A 2 "DC Bus Voltage" ~/test-lvgl-cross-compile/telemetry-system/services/web-dashboard/static/js/kiosk.js

# Should show:
# const dcBusVoltage = m.voltage_dc_bus || 0;
# updateElement('dcBus', dcBusVoltage ? (dcBusVoltage / 10).toFixed(1) : '--');
```

---

## 📁 File Structure

```
test-lvgl-cross-compile/
├── kiosk/
│   ├── KIOSK-SETUP.md          # This file
│   ├── setup-kiosk.sh          # Main setup script
│   ├── launch-kiosk.sh         # Test kiosk mode
│   ├── update-autostart-kiosk.sh  # Configure autostart
│   ├── configure-touchscreen.sh   # Touchscreen setup
│   └── start-all.sh            # Start all services
├── telemetry-system/
│   ├── services/
│   │   ├── telemetry-logger/   # CAN data logger
│   │   ├── cloud-sync/         # Cloud synchronization
│   │   └── web-dashboard/      # Flask web apps
│   │       ├── static/
│   │       │   ├── kiosk.html  # Kiosk mode UI (800x480)
│   │       │   ├── index.html  # Desktop dashboard
│   │       │   ├── js/kiosk.js # Kiosk JavaScript
│   │       │   ├── fonts/      # Albert Sans fonts
│   │       │   └── images/     # EMBOO logo
│   │       ├── app.py          # Kiosk server (port 8080)
│   │       └── app_desktop.py  # Desktop server (port 80)
│   ├── systemd/                # Service files
│   │   ├── telemetry-logger-unified.service
│   │   ├── cloud-sync.service
│   │   ├── web-dashboard.service        # Port 8080
│   │   └── desktop-dashboard.service    # Port 80
│   ├── scripts/                # Utility scripts
│   └── config/                 # Configuration files
└── CLAUDE.md                   # Main project documentation
```

---

## 🔐 Security Notes

- The web dashboard runs without authentication (designed for local network only)
- CAN bus is directly accessible - ensure physical security
- Cloud sync credentials stored in plaintext in `config/influxdb-cloud.env`
- Kiosk mode prevents user from accessing system - use SSH for maintenance

---

## 🔄 Updating the System

### Pull Latest Changes

```bash
cd ~/test-lvgl-cross-compile
git pull origin main  # or your branch name

# Restart services to apply changes
sudo systemctl restart telemetry-logger-unified
sudo systemctl restart web-dashboard
sudo systemctl restart cloud-sync

# Refresh kiosk (if running)
pkill chromium-browser
cd ~/test-lvgl-cross-compile/kiosk
./launch-kiosk.sh
```

### Update Kiosk Mode Only

```bash
cd ~/test-lvgl-cross-compile
git pull

# Clear browser cache
rm -rf ~/.cache/chromium/

# Restart web dashboard
sudo systemctl restart web-dashboard

# Relaunch kiosk
pkill chromium-browser
cd kiosk && ./launch-kiosk.sh
```

---

## 📊 Data Flow

```
CAN Bus (can0, 250 kbps)
    ↓
Telemetry Logger (telemetry-logger-unified.service)
    ↓
InfluxDB (Local)
    ↓
├─→ Cloud Sync (cloud-sync.service) → InfluxDB Cloud
└─→ Web Dashboard (web-dashboard.service, Port 8080)
        ↓
    Kiosk Mode (Chromium, http://localhost:8080)
        ↓
    WebSocket Updates (Real-time) + Polling (Fallback)
```

---

## 💡 Tips

1. **Development Testing:** Use `./launch-kiosk.sh` to quickly test changes without rebooting
2. **Remote Access:** Access dashboard from another device: `http://<pi-ip>:8080`
3. **Log Monitoring:** Keep `journalctl -f` open in SSH to monitor service logs
4. **CAN Monitoring:** Use `candump can0` to verify CAN messages are being received
5. **Cell Chart:** Toggle between battery summary and cell chart using the button in top-right
6. **Auto-Switch:** Charger page will automatically open when charger is connected
7. **Font Customization:** Replace fonts in `static/fonts/` to customize typography

---

## 📞 Support

For issues or questions:
1. Check service logs: `sudo journalctl -u <service-name> -f`
2. Verify CAN interface: `candump can0`
3. Test API: `curl http://localhost:8080/api/status`
4. Check this documentation's Troubleshooting section
5. Review project documentation in `CLAUDE.md`

---

## 📝 Changelog

### 2025-12-05 - Latest Updates

**Data Validation & Timeouts:**
- Added 60-second timeout for all sensor data
- Stale data (>60 seconds old) now shows as "--"
- Prevents displaying outdated/incorrect values
- Improves data reliability and user confidence

**GPS Status Improvements:**
- Distinguishes between "GPS Service Offline" and "Searching for fix"
- Shows satellite count even when no position fix
- Map marker popup indicates GPS status:
  - "GPS Fix Active" - Valid position data
  - "GPS Service Active - Searching for fix" - Service running, no position yet
  - "GPS Service Offline" - No recent data from GPS service

**Dual-Port Configuration:**
- Port 8080: Kiosk mode (optimized for Victron touchscreen)
- Port 80: Original desktop dashboard (for remote viewing)
- Separate services for independent operation
- Remote access from laptops/phones via port 80

**Kiosk Mode Enhancements:**
- DNR selector with letter-only display (D N R)
- Integrated Albert Sans font family
- EMBOO logo above DNR selector
- Fixed DC bus voltage scaling (divide by 10)
- Battery cell chart with highest/lowest highlighting
- Changed "Charger Offline" to "Charger Disconnected"
- Auto-switch to charger page when charger connects

**Previous Updates:**
- Optimized for 800x480 Victron Cerbo 50 touchscreen
- Added bottom navigation with 5 pages
- Implemented real-time WebSocket updates
- Added temperature color coding
- Integrated GPS with live map
- Created systemd services for all components
- Added cloud sync capability
- Implemented offline-resilient data logging

# Kiosk Mode Dashboard - Emboo Telemetry

## Overview

The kiosk mode dashboard is optimized for the **Victron Cerbo 50 touchscreen** (1280x720 resolution). It provides a non-scrolling, touch-friendly interface with bottom navigation buttons for easy one-handed operation.

**Branding**: Emboo Telemetry (EMBOO Battery Management System)

## Features

- **Fixed 1280x720 layout** - No scrolling required
- **5 dedicated pages** with bottom navigation
- **Large touch-friendly buttons** (60px height)
- **Real-time WebSocket updates** (2-second fallback polling)
- **High-contrast dark theme** optimized for visibility
- **Touch-optimized controls** with no hover states
- **Live clock** in top-left corner (HH:MM:SS format)
- **Centered title** with status indicators on the right

## Pages

### 1. Overview (📊)
- **Battery SOC** - Large display with color coding (red <20%, orange <40%, green ≥40%)
- **Vehicle Speed** - Current speed in km/h
- **Battery Power** - Current power draw/charge in kW
- **Battery Temperature** - Average battery temp in °C

### 2. Battery (🔋)
- **SOC Display** - Extra large state of charge percentage
- **Voltage** - Pack voltage in V
- **Current** - Pack current in A (positive = discharge, negative = charge)
- **Power** - Calculated or reported power in kW
- **Temperature** - Average battery temperature
- **Cell Range** - Min and max cell voltages in mV
- **Cell Delta** - Voltage difference between highest and lowest cells

### 3. Motor (⚙️)
- **Motor RPM** - Current motor speed
- **Torque** - Motor torque in Nm
- **Motor Temp** - Stator temperature in °C
- **Inverter Temp** - Inverter temperature in °C
- **DC Bus Voltage** - High voltage bus voltage
- **Direction** - Forward/Reverse/Neutral status

### 4. GPS (🗺️)
- **Live Map** - OpenStreetMap with vehicle marker
- **Altitude** - Current altitude in meters
- **Satellites** - Number of GPS satellites in view
- **Heading** - Direction of travel in degrees
- **GPS Speed** - Speed from GPS (independent verification)

### 5. Charger (🔌)
- **Charging Status** - Active/Idle indicator with visual feedback
- **Charging Power** - Current charge power in kW
- **Output Voltage** - Charger output voltage
- **Output Current** - Charger output current
- **Status Flags** - Hardware/temperature/input fault indicators
- **Time Remaining** - Estimated time to full charge (if available)

## Accessing Kiosk Mode

### Local Access
```
http://localhost:8080/kiosk
```

### Remote Access
```
http://<raspberry-pi-ip>:8080/kiosk
```

### Quick Launch Script
For quick testing without rebooting, use the launch script:
```bash
cd ~/Projects/test-lvgl-cross-compile/kiosk
./launch-kiosk.sh
```

This will:
- Kill any existing Chromium instances
- Launch kiosk mode in full-screen
- Exit with Alt+F4 or `pkill -f chromium-browser`

## Setup for Auto-Launch

### 1. Configure Chromium to Launch Kiosk Mode

Edit the autostart file:
```bash
nano ~/.config/lxsession/LXDE-pi/autostart
```

Add:
```
@chromium-browser --kiosk --noerrdialogs --disable-infobars --no-first-run http://localhost:8080/kiosk
```

### 2. Alternative: Using Victron Venus OS

If running on Venus OS, add to `/data/rcS.local`:
```bash
#!/bin/bash
# Launch web dashboard in kiosk mode
export DISPLAY=:0
chromium-browser --kiosk --noerrdialogs --disable-infobars http://localhost:8080/kiosk &
```

Make it executable:
```bash
chmod +x /data/rcS.local
```

### 3. Full-Screen Without Browser Chrome

For completely chromeless display:
```bash
@chromium-browser --kiosk --app=http://localhost:8080/kiosk
```

## Status Indicators

The top bar shows real-time status for all major systems:

- **Battery** (EMBOO/Orion BMS)
- **Motor** (ROAM motor controller)
- **GPS** (USB GPS reader)
- **Charger** (Elcon charger)

Status colors:
- 🟢 **Green** - Online and receiving data (< 30 seconds old)
- 🔴 **Red** - Offline or stale data (> 30 seconds old)
- ⚫ **Gray** - Unknown/not initialized

## Navigation

Touch any of the 5 bottom navigation buttons to switch pages:
- Overview, Battery, Motor, GPS, Charger

The active page is highlighted with:
- Green accent color (#00ff88)
- Top border indicator
- Background highlight

## Optional Features

### Auto-Rotate Pages

To enable automatic page rotation every 10 seconds, open the browser console (F12) and run:
```javascript
enableAutoRotate()
```

To disable:
```javascript
disableAutoRotate()
```

**Note**: Auto-rotate is **disabled by default** to prevent unwanted page changes during manual interaction.

## Technical Details

### Layout Specifications
- **Top Bar**: 80px height
  - Left: Live clock (HH:MM:SS)
  - Center: Emboo Telemetry title
  - Right: Status indicators (Battery, Motor, GPS, Charger)
- **Content Area**: 580px height (no scrolling)
- **Bottom Navigation**: 60px height
- **Total**: 720px (exactly fits screen)

### Color Palette
- Background: `#0a0a0a` (dark black)
- Primary Accent: `#00ff88` (green)
- Secondary Accent: `#00b4d8` (blue)
- Warning: `#ff8800` (orange)
- Error: `#ff3333` (red)
- Text: `#e0e0e0` (light gray)

### Fonts
- System fonts: `-apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto`
- Sizes: 5rem (huge metrics), 4rem (large metrics), 1.8rem (standard values)

### Performance
- WebSocket primary updates (real-time)
- 2-second fallback polling if WebSocket disconnects
- Map lazy initialization (only when GPS page is active)
- Minimal DOM updates for smooth performance

## Troubleshooting

### Dashboard Not Updating
1. Check WebSocket connection in browser console
2. Verify web-dashboard.service is running:
   ```bash
   systemctl status web-dashboard.service
   ```
3. Check InfluxDB connection:
   ```bash
   systemctl status influxdb.service
   ```

### Map Not Displaying
1. Check internet connection (required for map tiles)
2. Verify GPS data is being received:
   ```bash
   journalctl -u usb-gps-reader.service -f
   ```
3. Check browser console for errors

### Touch Not Responding
1. Verify touchscreen driver is running:
   ```bash
   systemctl status victron-touchscreen.service
   ```
2. Check touchscreen detection:
   ```bash
   sudo i2cdetect -y 21
   ```
3. See VICTRON-TOUCHSCREEN.md for detailed troubleshooting

### Screen Size Issues
The kiosk mode is optimized for **exactly 1280x720**. If using a different resolution:

1. Edit `/home/user/test-lvgl-cross-compile/telemetry-system/services/web-dashboard/static/kiosk.html`
2. Adjust dimensions in the `<style>` section:
   - `body { width: 1280px; height: 720px; }`
   - `.top-bar { height: 80px; }`
   - `.content-area { height: 580px; }`
   - `.bottom-nav { height: 60px; }`

## Comparison: Kiosk vs. Standard Dashboard

| Feature | Kiosk Mode | Standard Mode |
|---------|------------|---------------|
| Resolution | 1280x720 fixed | Responsive |
| Navigation | Bottom buttons | Top tabs |
| Scrolling | None | Vertical scroll |
| Target Device | Victron Cerbo 50 | Desktop/tablet |
| Touch Optimized | Yes (60px buttons) | Partial |
| Historical Charts | No | Yes |
| Trip Statistics | No | Yes |
| Data Export | No | Yes |
| Map Display | Full page | Embedded |

## URLs

- **Standard Dashboard**: `http://<ip>:8080/`
- **Kiosk Dashboard**: `http://<ip>:8080/kiosk`
- **API Status**: `http://<ip>:8080/api/status`

## Files

- HTML: `telemetry-system/services/web-dashboard/static/kiosk.html`
- JavaScript: `telemetry-system/services/web-dashboard/static/js/kiosk.js`
- Flask Route: `telemetry-system/services/web-dashboard/app.py` (line 316)

## Future Enhancements

Potential features for future versions:
- Configurable page layout (drag-and-drop metrics)
- Custom alert thresholds with visual/audio notifications
- Dark/light theme toggle
- Brightness control for day/night use
- Multi-language support
- Historical data mini-charts on overview page
- Trip statistics summary page

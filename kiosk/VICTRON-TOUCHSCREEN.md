# Victron Cerbo 50 Touchscreen Setup

Complete setup guide for the FT5316 capacitive touchscreen on Raspberry Pi 4.

## Quick Setup

Run the automated setup script:

```bash
cd ~/Projects/test-lvgl-cross-compile/kiosk
sudo ./setup-victron-touchscreen.sh
```

If I2C was just enabled, reboot:
```bash
sudo reboot
```

## What the Script Does

1. **Enables I2C** in boot config (`/boot/firmware/config.txt` or `/boot/config.txt`)
2. **Installs dependencies**: python3-pip, python3-smbus, i2c-tools, evdev
3. **Configures permissions**: Adds user to input group, sets uinput permissions
4. **Creates driver**: `/home/emboo/ft5316_touch.py` (Python touchscreen driver)
5. **Creates service**: `victron-touchscreen.service` (auto-starts on boot)
6. **Starts touchscreen**: Service starts immediately if hardware detected

## Hardware Configuration

### I2C Bus Configuration
- **Bus**: I2C bus 21 (HDMI 1 DDC)
- **Address**: 0x38 (FT5316 touchscreen controller)
- **Connection**: Via HDMI's I2C/DDC pins

### Physical Setup
- **HDMI Port**: Connect to HDMI 1 (second micro HDMI port on Pi 4)
- **Screen**: Victron Cerbo 50 touchscreen
- **Power**: Screen must be powered on
- **Cable**: Micro HDMI to HDMI cable or adapter

### Why HDMI 1 (bus 21)?
Raspberry Pi 4 has two micro HDMI ports:
- **HDMI 0** → I2C bus 20
- **HDMI 1** → I2C bus 21 (where touchscreen was detected)

The touchscreen uses HDMI's DDC (Display Data Channel) pins for I2C communication.

## Boot Configuration Changes

The setup script adds this to your boot config:

```ini
# Enable I2C for touchscreen
dtparam=i2c_arm=on
```

**Location**: `/boot/firmware/config.txt` (Raspberry Pi OS Bookworm) or `/boot/config.txt` (older versions)

## Verification

### Check Service Status
```bash
systemctl status victron-touchscreen.service
```
Should show: `Active: active (running)`

### Check Hardware Detection
```bash
sudo i2cdetect -y 21
```
Should show `38` in the grid (FT5316 touchscreen)

### View Live Logs
```bash
sudo journalctl -u victron-touchscreen.service -f
```
Touch the screen - you should see "Touch DOWN" and "Touch UP" messages

### Check Input Device
```bash
ls -l /dev/input/by-id/*FT5316*
```
Should show the virtual touchscreen device

## How It Works

### Driver Architecture
1. **Python driver** (`ft5316_touch.py`) reads touch data from I2C bus 21
2. **evdev UInput** creates virtual touchscreen device (`/dev/input/eventX`)
3. **Linux input system** sends events to X11/Wayland
4. **Applications** receive touch as standard input events

### Touch Event Flow
```
FT5316 (0x38) → I2C Bus 21 → Python Driver → evdev UInput →
    /dev/input/eventX → X11/Wayland → Applications
```

### Service Configuration
- **Name**: `victron-touchscreen.service`
- **User**: root (required for uinput access)
- **Auto-start**: Enabled for graphical.target
- **Restart**: Always (auto-recovers from errors)
- **Poll Rate**: 100 Hz (10ms delay)

## Troubleshooting

### Touchscreen Not Detected

**Check I2C bus 21:**
```bash
ls -l /dev/i2c-21
```
If not found, I2C may not be enabled. Reboot after running setup script.

**Scan for device:**
```bash
sudo i2cdetect -y 21
```

If `38` is NOT shown:
1. Check HDMI cable is connected (Pi → Victron screen)
2. Verify screen is powered on
3. Try the other micro HDMI port on the Pi
4. Power cycle the screen (unplug/replug power)
5. Try a different HDMI cable

### Service Won't Start

**Check logs:**
```bash
sudo journalctl -u victron-touchscreen.service -n 50
```

**Common issues:**
- I2C not enabled (reboot required)
- Wrong I2C bus number (check with `i2cdetect -y 21`)
- Permissions issue (driver runs as root to access uinput)
- Missing evdev module (`pip3 install evdev`)

### Touchscreen Not Responding

**Check if driver sees touches:**
```bash
sudo journalctl -u victron-touchscreen.service -f
```
Touch the screen - you should see log messages.

**If logs show touches but cursor doesn't move:**
- Check that `/dev/input/by-id/*FT5316*` exists
- Verify X11 or Wayland is running (`echo $DISPLAY`)
- Restart desktop environment

**Manual test:**
```bash
sudo systemctl stop victron-touchscreen.service
sudo python3 /home/emboo/ft5316_touch.py
# Touch screen, watch for output
# Ctrl+C to stop
sudo systemctl start victron-touchscreen.service
```

### Wrong HDMI Port

If touchscreen detected on bus 20 instead of 21:

1. Edit the driver:
```bash
sudo nano /home/emboo/ft5316_touch.py
```

2. Change line:
```python
I2C_BUS = 21  # Change to 20
```

3. Restart service:
```bash
sudo systemctl restart victron-touchscreen.service
```

## Integration with Kiosk Mode

The touchscreen works automatically with Chromium kiosk mode. No additional configuration needed.

The driver creates a standard Linux input device that Chromium recognizes as a touchscreen.

## Uninstallation

To remove the touchscreen driver:

```bash
# Stop and disable service
sudo systemctl stop victron-touchscreen.service
sudo systemctl disable victron-touchscreen.service

# Remove files
sudo rm /etc/systemd/system/victron-touchscreen.service
sudo rm /home/emboo/ft5316_touch.py
sudo rm /etc/udev/rules.d/10-uinput.rules

# Remove I2C config (optional)
# Edit boot config and remove "dtparam=i2c_arm=on"
sudo nano /boot/firmware/config.txt

# Reload systemd
sudo systemctl daemon-reload
```

## Technical Specifications

### FT5316 Touchscreen Controller
- **Manufacturer**: FocalTech
- **Type**: Capacitive multi-touch controller
- **Max Touch Points**: 5 (driver uses first point only)
- **I2C Address**: 0x38 (7-bit)
- **I2C Speed**: 400 kHz (fast mode)
- **Resolution**: Up to 1280x720

### Communication Protocol
- **Transport**: I2C via HDMI DDC pins
- **Registers**:
  - 0x02: Number of touch points
  - 0x03-0x06: First touch point data (X,Y coordinates)
- **Event Types**: Press Down (0), Contact (2), Lift Up (1)

### Virtual Device Specifications
- **Name**: FT5316-Touchscreen
- **Type**: Touchscreen (absolute coordinates)
- **Events**: BTN_TOUCH, ABS_X, ABS_Y, ABS_PRESSURE
- **Resolution**: 1280x720
- **Protocol**: Linux evdev (UInput)

## References

- [FT5316 Driver GitHub](https://github.com/lpopescu-victron/ft5316-touchscreen)
- [FT5316 Datasheet](https://www.buydisplay.com/download/ic/FT5316.pdf)
- [Victron Cerbo GX 50](https://www.victronenergy.com/accessories/cerbo-gx-50)
- [Linux evdev Documentation](https://www.kernel.org/doc/html/latest/input/input.html)

## License

The touchscreen driver is based on work by lpopescu-victron.
This documentation and setup script are provided as-is for the Nissan Leaf CAN Network project.

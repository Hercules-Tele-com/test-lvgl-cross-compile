# Victron Cerbo 50 Touchscreen Setup (FT5316)

This guide covers setting up the Victron Cerbo 50's FT5316 touchscreen on Raspberry Pi 4.

## Problem You're Experiencing

The service is failing with:
```
Process: 1730 ExecStart=/usr/bin/python3 /home/pi/ft5316_touch.py (code=exited, status=217/USER)
```

**Exit code 217/USER** means the service file specifies a user (`pi`) that doesn't exist on your system. Your system uses user `emboo`, not `pi`.

**Additionally**: The I2C scan shows **no device at address 0x38**, which means the touchscreen hardware is not being detected. This is likely a physical connection issue.

## Two-Step Fix Process

### Step 1: Check Hardware Connection (CRITICAL!)

Before fixing the service, we need to ensure the touchscreen hardware is connected:

```bash
cd ~/Projects/test-lvgl-cross-compile/kiosk
./check-touchscreen-hardware.sh
```

This will:
- ✅ Check if I2C is enabled
- ✅ Scan for the FT5316 touchscreen at address 0x38
- ✅ Check HDMI connection status
- ✅ Provide troubleshooting steps if not detected

**IMPORTANT**: The touchscreen uses HDMI's I2C pins (DDC) for communication. You MUST have:
- ✅ HDMI cable connected from Raspberry Pi to Victron Cerbo 50 screen
- ✅ Screen powered ON
- ✅ Both ends of HDMI cable firmly seated

If the touchscreen is NOT detected at address 0x38:
1. Check HDMI cable connection
2. Power cycle the Victron screen (unplug/replug power)
3. Try a different HDMI cable
4. Re-run the hardware check script

### Step 2: Fix the Service (Once Hardware is Detected)

After confirming the touchscreen shows at address 0x38:

```bash
cd ~/Projects/test-lvgl-cross-compile/kiosk
sudo ./fix-touchscreen-service.sh
```

This will:
1. ✅ Run Victron's official setup script (installs dependencies, creates driver)
2. ✅ Update service file to use user `emboo` instead of `pi`
3. ✅ Copy touchscreen script to `/home/emboo/ft5316_touch.py`
4. ✅ Enable and start the touchscreen service
5. ✅ Verify hardware detection one more time

## If You Want to Diagnose First

```bash
cd ~/Projects/test-lvgl-cross-compile/kiosk
./diagnose-touchscreen.sh
```

This will show you:
- Whether the service file exists and what user it's configured for
- Whether the touchscreen script exists
- Whether user 'pi' exists (it doesn't on your system)
- I2C device detection
- Python dependencies status

## Manual Setup (Alternative)

If you prefer to set it up manually:

### 1. Install Dependencies
```bash
sudo apt update
sudo apt install -y python3-pip python3-evdev python3-smbus2 i2c-tools git
```

### 2. Enable I2C
```bash
# For Raspberry Pi OS Bookworm (newer):
sudo nano /boot/firmware/config.txt

# For older Pi OS:
sudo nano /boot/config.txt

# Add this line:
dtparam=i2c_arm=on

# Save and exit, then load the module:
sudo modprobe i2c-dev
```

### 3. Clone the Driver
```bash
cd ~
git clone https://github.com/lpopescu-victron/ft5316-touchscreen.git
cp ft5316-touchscreen/ft5316_touch.py ~/
chmod +x ~/ft5316_touch.py
```

### 4. Create Service File
```bash
sudo nano /etc/systemd/system/ft5316-touchscreen.service
```

Paste this content (note `User=emboo`):
```ini
[Unit]
Description=FT5316 Touchscreen Driver
After=local-fs.target
Before=display-manager.service

[Service]
Type=simple
User=emboo
Group=emboo
WorkingDirectory=/home/emboo
ExecStart=/usr/bin/python3 /home/emboo/ft5316_touch.py
Restart=always
RestartSec=5
StartLimitInterval=300
StartLimitBurst=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

### 5. Enable and Start
```bash
sudo systemctl daemon-reload
sudo systemctl enable ft5316-touchscreen.service
sudo systemctl start ft5316-touchscreen.service
```

## Verification

### Check Service Status
```bash
systemctl status ft5316-touchscreen.service
```

Should show `Active: active (running)`.

### Check I2C Devices
```bash
sudo i2cdetect -y 1
```

You should see `38` in the output - this is the FT5316 touchscreen's I2C address.

### View Logs
```bash
# Live logs:
sudo journalctl -u ft5316-touchscreen.service -f

# Recent logs:
sudo journalctl -u ft5316-touchscreen.service -n 50
```

### Test Manually
```bash
# Stop the service first:
sudo systemctl stop ft5316-touchscreen.service

# Run manually to see output:
python3 ~/ft5316_touch.py

# Press Ctrl+C to stop, then restart service:
sudo systemctl start ft5316-touchscreen.service
```

## Troubleshooting

### "No module named 'evdev'"
```bash
sudo apt install -y python3-evdev
```

### "No module named 'smbus2'"
```bash
sudo apt install -y python3-smbus2
```

### "/dev/i2c-1 not found"
I2C is not enabled. Enable it in `/boot/firmware/config.txt` (or `/boot/config.txt`) and reboot.

### "No device detected at 0x38"
- Check physical connections to the Victron Cerbo 50 screen
- Verify the screen is powered on
- Check HDMI connection is secure
- Some touchscreens need the HDMI connection active to enable I2C

### Service keeps restarting (exit code 217)
The service file specifies a user that doesn't exist. Use the installation script which configures it for user `emboo`.

## Hardware Notes

**Victron Cerbo 50 Touchscreen:**
- Uses FT5316 capacitive touch controller
- I2C address: 0x38
- Requires I2C enabled on Raspberry Pi
- Works over HDMI for video + I2C for touch
- Touch input appears as `/dev/input/eventX` device

## Reference

- Driver GitHub: https://github.com/lpopescu-victron/ft5316-touchscreen
- FT5316 Datasheet: [FocalTech FT5316](https://www.buydisplay.com/download/ic/FT5316.pdf)
- Victron Cerbo 50: https://www.victronenergy.com/accessories/cerbo-gx-50

## Integration with Kiosk Mode

Once the touchscreen is working, it will automatically work with the Chromium kiosk mode. The touchscreen driver creates a standard Linux input device that Chromium recognizes automatically.

No additional configuration needed in the kiosk setup!

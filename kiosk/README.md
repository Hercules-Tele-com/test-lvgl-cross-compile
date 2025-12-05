# Kiosk Mode Scripts

Essential scripts for EMBOO Telemetry Kiosk Mode setup and operation.

## 📁 Files

### Setup & Configuration
- **`KIOSK-SETUP.md`** - Complete setup guide with troubleshooting
- **`setup-kiosk.sh`** - Main setup script (run once)
- **`update-autostart-kiosk.sh`** - Configure/update autostart settings
- **`configure-touchscreen.sh`** - Configure Victron Cerbo 50 touchscreen

### Operation
- **`launch-kiosk.sh`** - Launch kiosk mode for testing (manual)
- **`start-all.sh`** - Start all telemetry services

### Configuration Files
- **`autostart`** - Example autostart configuration

## 🚀 Quick Start

```bash
# 1. Run main setup
./setup-kiosk.sh

# 2. Reboot to start kiosk mode
sudo reboot

# Or test without rebooting:
./launch-kiosk.sh
```

## 📖 Documentation

For complete setup instructions, troubleshooting, and configuration details, see:
- **[KIOSK-SETUP.md](KIOSK-SETUP.md)** - Full documentation

## 🔧 Common Tasks

### Update Autostart Configuration
```bash
./update-autostart-kiosk.sh
```

### Test Kiosk Mode
```bash
./launch-kiosk.sh
```

### Start All Services
```bash
./start-all.sh
```

### Configure Touchscreen
```bash
sudo ./configure-touchscreen.sh
```

## 📋 Script Descriptions

| Script | Purpose | When to Use |
|--------|---------|-------------|
| `setup-kiosk.sh` | Initial kiosk setup | First-time setup only |
| `launch-kiosk.sh` | Launch kiosk manually | Testing changes |
| `update-autostart-kiosk.sh` | Update autostart config | Change startup behavior |
| `configure-touchscreen.sh` | Touchscreen calibration | Touch not working |
| `start-all.sh` | Start all services | Restart services |

## 🗑️ Removed Scripts

The following scripts were removed in cleanup (2025-12-05):
- `diagnose-autostart.sh` - No longer needed
- `diagnose-dashboard-and-gps.sh` - No longer needed
- `diagnose-gps-service.sh` - No longer needed
- `fix-autostart.sh` - Replaced by `update-autostart-kiosk.sh`
- `fix-dashboard-and-gps.sh` - No longer needed
- `setup-complete-system.sh` - Replaced by `setup-kiosk.sh`
- `setup-victron-touchscreen.sh` - Merged into `setup-kiosk.sh`
- `COMPLETE-SYSTEM-README.md` - Replaced by `KIOSK-SETUP.md`
- `VICTRON-TOUCHSCREEN.md` - Merged into `KIOSK-SETUP.md`

## 💡 Tips

1. **Read the docs first:** Check `KIOSK-SETUP.md` for detailed instructions
2. **Test before autostart:** Use `./launch-kiosk.sh` to test changes
3. **Check service logs:** `sudo journalctl -u web-dashboard -f`
4. **Monitor CAN:** `candump can0`
5. **Remote access:** Access dashboard from other devices at `http://<pi-ip>:8080`

# CLAUDE.md

This file provides guidance to Claude Code when working with the Nissan Leaf CAN Network monorepo project.

## Project Overview

This is a comprehensive monorepo for a Nissan Leaf CAN network system featuring:
- Multiple ESP32 modules with CAN bus connectivity (TJA1050 transceivers)
- Shared LeafCANBus library for CAN communication
- Raspberry Pi LVGL dashboard with cross-platform development support
- Support for multiple battery types:
  - **Nissan Leaf Battery** (500 kbps, default)
  - **EMBOO Battery** (Orion BMS, 250 kbps)
- Integration with 2012 Nissan Leaf inverter, charger, and EM57 motor

## Project Structure

```
leaf-can-network/
├── lib/
│   └── LeafCANBus/              # Shared CAN library (ESP32)
│       ├── library.json
│       └── src/
│           ├── LeafCANBus.h     # CAN bus class (subscribe + publish)
│           ├── LeafCANBus.cpp
│           ├── LeafCANMessages.h # CAN ID definitions & state structures
│           └── LeafCANMessages.cpp # Pack/unpack functions
├── modules/                     # ESP32 PlatformIO projects
│   ├── gps-module/             # GPS receiver (publishes to CAN)
│   ├── temp-gauge/
│   ├── fuel-gauge/
│   ├── speedometer/
│   ├── ui-dash/
│   └── body-sensors/
└── ui-dashboard/               # Raspberry Pi LVGL application
    ├── CMakeLists.txt
    ├── lv_conf.h
    └── src/
        ├── main.cpp
        ├── ui/                 # LVGL UI components
        ├── platform/
        │   ├── windows/        # SDL2 simulator
        │   └── linux/          # SocketCAN + framebuffer
        └── shared/             # CAN receiver logic
```

## Battery Configuration

The system supports multiple battery types with compile-time configuration:

### Supported Battery Types

1. **Nissan Leaf Battery** (default)
   - CAN bus speed: 500 kbps
   - Configuration: `-DNISSAN_LEAF_BATTERY`

2. **EMBOO Battery** (Orion BMS / ENNOID-style)
   - CAN bus speed: 250 kbps
   - Configuration: `-DEMBOO_BATTERY`

### Configuring Battery Type

**For UI Dashboard (CMake):**
```bash
cd ui-dashboard
mkdir build && cd build
cmake .. -DBATTERY_TYPE=EMBOO  # or NISSAN_LEAF (default)
make
```

**For ESP32 Modules (PlatformIO):**
Edit `platformio.ini` and uncomment the desired battery type:
```ini
build_flags =
    -DCORE_DEBUG_LEVEL=3
    ; -DNISSAN_LEAF_BATTERY    ; Nissan Leaf (500 kbps, default)
    -DEMBOO_BATTERY            ; EMBOO/Orion BMS (250 kbps)
```

**See [docs/BATTERY_CONFIGURATION.md](docs/BATTERY_CONFIGURATION.md) for detailed configuration instructions.**

## Build Commands

### ESP32 Modules (PlatformIO)

Navigate to a specific module directory before running commands:

```bash
cd modules/gps-module

# Build the module
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200

# Clean build
pio run --target clean

# Build all modules (from root)
cd modules && for dir in */; do (cd "$dir" && pio run); done
```

### Raspberry Pi Dashboard

**Building on Windows (SDL2 simulator):**
```bash
cd ui-dashboard
mkdir build && cd build

# Configure with vcpkg or system SDL2
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build . --config Release

# Run
./Release/leaf-can-dashboard.exe
```

**Building on Raspberry Pi (production):**
```bash
cd ui-dashboard
mkdir build && cd build

# Configure
cmake ..

# Build
make -j4

# Run (requires root for framebuffer and CAN access)
sudo ./leaf-can-dashboard
```

## Architecture Notes

### ESP32 CAN Communication

The `LeafCANBus` library provides a unified interface for both receiving and transmitting CAN messages:

**Subscribing to CAN IDs (receive):**
```cpp
LeafCANBus canBus;
InverterState inverterState;

canBus.begin();
canBus.subscribe(CAN_ID_INVERTER_TELEMETRY, unpack_inverter_telemetry, &inverterState);
```

**Publishing CAN messages (transmit):**
```cpp
GPSPositionState gpsPosition;

// Publish every 1000ms
canBus.publish(CAN_ID_GPS_POSITION, 1000, pack_gps_position, &gpsPosition);
```

**Processing in loop:**
```cpp
void loop() {
    canBus.process();  // Handles both RX and periodic TX
    delay(10);
}
```

### CAN ID Allocation

- **0x1F2, 0x1DB, 0x1DC, 0x1D4, 0x1DA, 0x390**: Existing Nissan Leaf CAN IDs
- **0x710-0x71F**: GPS module CAN IDs
- **0x720-0x72F**: Body sensor CAN IDs
- **0x730-0x73F**: UI dashboard CAN IDs
- **0x740+**: Custom gauge commands

### LVGL Dashboard Cross-Platform Design

The UI dashboard is designed to run on both Windows (for development) and Raspberry Pi (for deployment):

- **Windows**: Uses SDL2 for display and mock CAN data generator
- **Linux/Pi**: Uses Linux framebuffer for display and SocketCAN for real CAN data

The same UI code (`dashboard_ui.cpp`) works on both platforms. Platform-specific code is isolated in `platform/windows/` and `platform/linux/`.

## Hardware Configuration

### ESP32 CAN Pins
- Default TX: GPIO 5
- Default RX: GPIO 4
- Transceiver: TJA1050 or compatible
- Bus speed: 500 kbps

### Raspberry Pi
- CAN interface: Waveshare CAN HAT (can0)
- Display: Connected via HDMI or DSI (framebuffer at /dev/fb0)
- Recommended: Raspberry Pi 4 (4GB)

## Development Workflow

### Adding a New ESP32 Module

1. Create module directory: `mkdir -p modules/new-module/src`
2. Copy `platformio.ini` from existing module
3. Update `lib_extra_dirs = ../../lib` to access LeafCANBus
4. Write `main.cpp` using LeafCANBus for CAN communication
5. Build and test: `cd modules/new-module && pio run`

### Adding New CAN Messages

1. Add CAN ID to `lib/LeafCANBus/src/LeafCANMessages.h`
2. Define state structure in same file
3. Implement pack/unpack functions in `LeafCANMessages.cpp`
4. Update ESP32 modules to subscribe/publish as needed
5. Update UI dashboard to display new data

### Debugging CAN Issues

**ESP32:**
- Monitor serial output at 115200 baud
- Check CAN stats: `canBus.getRxCount()`, `getTxCount()`, `getErrorCount()`
- Verify TWAI driver initialization messages
- Check for bus-off recovery attempts

**Raspberry Pi:**
- Monitor CAN traffic: `candump can0`
- Send test messages: `cansend can0 710#0102030405060708`
- Check interface status: `ip -details link show can0`

## Testing

### ESP32 Module Testing
No automated test framework configured. Requires hardware-in-the-loop validation:
- Verify serial debug output
- Use CAN analyzer (e.g., PCAN-USB) to verify transmitted messages
- Monitor CAN bus with `candump` on Raspberry Pi

### UI Dashboard Testing
- **Windows simulator**: Mock CAN data automatically generated for UI testing
- **Raspberry Pi**: Requires real CAN bus or virtual CAN (`sudo modprobe vcan`)

## Common Issues

### ESP32 Build Errors
- Ensure PlatformIO is up to date: `pio upgrade`
- Clean build directory: `pio run --target clean`
- Check `lib_extra_dirs` points to correct shared library path

### SDL2 Not Found (Windows)
- Install SDL2 via vcpkg: `vcpkg install sdl2:x64-windows`
- Set CMAKE_TOOLCHAIN_FILE to vcpkg toolchain in CMake command

### CAN Bus Errors on Raspberry Pi
- Check CAN interface is up: `sudo ip link set can0 up type can bitrate 500000`
- Verify SocketCAN kernel module: `lsmod | grep can`
- Check physical CAN HAT connection and termination resistors

### LVGL Display Issues
- Verify lv_conf.h color depth matches display (32-bit for framebuffer)
- Check framebuffer device permissions: `ls -l /dev/fb0`
- For Pi, run as root or add user to `video` group

## Dependencies

### ESP32 Modules
- Platform: espressif32
- Framework: arduino
- Libraries: TinyGPSPlus (for gps-module)

### Raspberry Pi Dashboard
- CMake 3.16+
- LVGL 8.3.11 (fetched automatically)
- SDL2 (Windows only)
- Linux kernel with SocketCAN support (Pi only)

## VSCode Integration

Recommended extensions:
- PlatformIO IDE (for ESP32 development)
- CMake Tools (for Raspberry Pi dashboard)
- C/C++ (Microsoft)

## Telemetry System

### Overview

The telemetry system provides comprehensive data logging, cloud synchronization, and web-based monitoring:

- **Unified Telemetry Logger**: Python service that monitors multiple CAN interfaces (can0, can1) simultaneously
- **Cell Voltage Tracking**: Extracts individual cell voltages (up to 144 cells) from EMBOO/Orion BMS
- **Fault Management**: Real-time fault detection and logging for battery/motor systems
- **Cloud Sync**: Automatically syncs data to InfluxDB Cloud when network is available
- **Web Dashboard**: Flask-based web UI with real-time updates and historical charts
- **Offline Capability**: Works completely offline with local storage and syncs when connection returns

### Architecture

```
CAN Bus (can0, can1) → Unified Telemetry Logger → InfluxDB (Local)
                                                        ↓
                                                  Cloud Sync → InfluxDB Cloud
                                                        ↓
                                                  Web Dashboard (Port 8080)

Telemetry Logger Features:
- Dual CAN interface support (configurable bitrates)
- EMBOO battery: cell voltages (144 cells), SOC, temps, faults
- ROAM motor: torque, RPM, voltages, currents, temps
- Per-interface statistics tracking
```

### Quick Start

```bash
# Setup InfluxDB
cd telemetry-system
./scripts/setup-influxdb.sh

# Configure cloud credentials (optional)
cp config/influxdb-cloud.env.template config/influxdb-cloud.env
nano config/influxdb-cloud.env

# Install and start services
sudo cp systemd/telemetry-logger-unified.service /etc/systemd/system/
sudo cp systemd/cloud-sync.service /etc/systemd/system/
sudo cp systemd/web-dashboard.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now telemetry-logger-unified cloud-sync web-dashboard

# Access web dashboard
# http://<pi-ip>:8080

# If you encounter InfluxDB field type conflicts, clear the database:
cd telemetry-system
./scripts/clear-influxdb-data.sh
```

### Service Management

```bash
# View logs (unified logger shows per-interface stats)
sudo journalctl -u telemetry-logger-unified.service -f
sudo journalctl -u cloud-sync.service -f
sudo journalctl -u web-dashboard.service -f

# Restart services
sudo systemctl restart telemetry-logger-unified.service
sudo systemctl restart cloud-sync.service
sudo systemctl restart web-dashboard.service

# Check status
systemctl status telemetry-logger-unified.service
systemctl status cloud-sync.service
systemctl status web-dashboard.service

# Use convenience scripts
cd scripts
./start-system.sh    # Start all services
./stop-system.sh     # Stop all services
./check-system.sh    # System health check
```

### Testing

```bash
# Run automated tests
cd telemetry-system
./tests/test_system.sh

# Generate mock CAN data for testing
python3 tests/mock_can_generator.py can0
```

### Adding New Telemetry Data

1. Define CAN ID and state structure in `lib/LeafCANBus/src/LeafCANMessages.h`
2. Add parser function in `telemetry-system/services/telemetry-logger/telemetry_logger.py`
3. Update web dashboard to display new data in `telemetry-system/services/web-dashboard/`
4. Update mock generator to include new message in `telemetry-system/tests/mock_can_generator.py`

### Configuration Files

- `telemetry-system/config/influxdb-local.env` - Local InfluxDB connection (auto-generated)
- `telemetry-system/config/influxdb-cloud.env` - Cloud InfluxDB connection (manual)
- `telemetry-system/systemd/*.service` - Systemd service definitions

### Data Retention

- **Local InfluxDB**: 30 days (configurable during setup)
- **Cloud InfluxDB**: Based on your cloud plan
- **Sync Frequency**: 5 minutes (configurable via `SYNC_INTERVAL_SECONDS`)

### Web Dashboard Features

- **Real-time Dashboard**: Mimics LVGL dashboard with live updates via WebSocket
- **Historical Trends**: Victron-style charts with configurable time ranges (1h, 6h, 24h, 7d)
- **Metrics**: Speed, battery SOC, power, temperatures, charging status, GPS, voltages
- **No Authentication**: Designed for local network use only

### Troubleshooting Telemetry Issues

**No data in InfluxDB:**
```bash
# Check if logger is receiving messages
sudo journalctl -u telemetry-logger.service -f

# Send test CAN message
cansend can0 1F2#0E1000640A1E1C01

# Query InfluxDB directly
source telemetry-system/config/influxdb-local.env
influx query 'from(bucket:"'$INFLUX_BUCKET'") |> range(start: -1h) |> limit(n:10)' \
  --org "$INFLUX_ORG" --token "$INFLUX_LOGGER_TOKEN"
```

**Cloud sync not working:**
```bash
# Check logs
sudo journalctl -u cloud-sync.service -f

# Verify cloud credentials
source telemetry-system/config/influxdb-cloud.env
curl -I "$INFLUX_CLOUD_URL/health"

# Test connectivity
ping -c 3 1.1.1.1
```

**Web dashboard not accessible:**
```bash
# Check if service is running
sudo systemctl status web-dashboard.service

# Test locally on Pi
curl http://localhost:8080/api/status

# Check firewall
sudo ufw allow 8080/tcp
```

### Performance Impact

On Raspberry Pi 4:
- **CPU**: <5% per service
- **Memory**: ~100 MB total for all three services
- **Disk**: ~1-2 MB/hour in InfluxDB
- **Network**: ~10-50 KB/s during cloud sync

## Important Reminders

- Do what has been asked; nothing more, nothing less.
- NEVER create files unless absolutely necessary.
- ALWAYS prefer editing existing files over creating new ones.
- NEVER proactively create documentation files unless explicitly requested.
- All ESP32 modules MUST include `lib_extra_dirs = ../../lib` in platformio.ini
- CAN IDs in custom range (0x700+) to avoid conflicts with Leaf OEM messages

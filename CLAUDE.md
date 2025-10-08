# CLAUDE.md

This file provides guidance to Claude Code when working with the Nissan Leaf CAN Network monorepo project.

## Project Overview

This is a comprehensive monorepo for a Nissan Leaf CAN network system featuring:
- Multiple ESP32 modules with CAN bus connectivity (TJA1050 transceivers at 500 kbps)
- Shared LeafCANBus library for CAN communication
- Raspberry Pi LVGL dashboard with cross-platform development support
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

## Important Reminders

- Do what has been asked; nothing more, nothing less.
- NEVER create files unless absolutely necessary.
- ALWAYS prefer editing existing files over creating new ones.
- NEVER proactively create documentation files unless explicitly requested.
- All ESP32 modules MUST include `lib_extra_dirs = ../../lib` in platformio.ini
- CAN IDs in custom range (0x700+) to avoid conflicts with Leaf OEM messages

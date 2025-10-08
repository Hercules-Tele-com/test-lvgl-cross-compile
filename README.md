# Nissan Leaf CAN Network - ESP32 + Raspberry Pi Dashboard

A comprehensive monorepo for a Nissan Leaf CAN bus network featuring multiple ESP32 modules and a cross-platform LVGL dashboard for Raspberry Pi.

## Project Overview

This project implements a distributed CAN network for a 2012 Nissan Leaf, integrating:
- **ESP32 modules** with CAN transceivers (TJA1050) for sensor reading and custom gauge control
- **Shared LeafCANBus library** providing unified subscribe/publish CAN interface
- **Raspberry Pi dashboard** with LVGL UI, supporting both Windows development (SDL2) and Pi deployment (framebuffer + SocketCAN)
- **Integration** with existing Leaf components: inverter, battery, charger, EM57 motor

### Key Features

- 🚗 Monitor real Nissan Leaf CAN data (battery SOC, motor RPM, temperatures, etc.)
- 📡 Add custom sensors (GPS, temperatures, voltages) to CAN network
- 📊 Beautiful LVGL dashboard with real-time vehicle telemetry
- 🖥️ Cross-platform UI: develop on Windows, deploy to Raspberry Pi
- 🔌 Modular ESP32 architecture with shared CAN library
- ⚡ 500 kbps CAN bus with custom ID allocation (0x700+ range)

## Repository Structure

```
test-lvgl-cross-compile/
├── lib/
│   └── LeafCANBus/              # Shared ESP32 CAN library
│       ├── library.json
│       └── src/
│           ├── LeafCANBus.h     # Subscribe/publish CAN interface
│           ├── LeafCANBus.cpp
│           ├── LeafCANMessages.h # CAN message definitions
│           └── LeafCANMessages.cpp
├── modules/                     # ESP32 PlatformIO projects
│   ├── gps-module/             # GPS with CAN publishing (example)
│   ├── temp-gauge/
│   ├── fuel-gauge/
│   ├── speedometer/
│   ├── ui-dash/
│   └── body-sensors/
└── ui-dashboard/               # Raspberry Pi LVGL dashboard
    ├── CMakeLists.txt
    ├── lv_conf.h
    └── src/
        ├── main.cpp
        ├── ui/                 # Dashboard UI components
        ├── platform/
        │   ├── windows/        # SDL2 simulator + mock CAN
        │   └── linux/          # Framebuffer + SocketCAN
        └── shared/             # CAN receiver logic
```

## Hardware Requirements

### ESP32 Modules
- ESP32 development boards (ESP32-DevKitC or similar)
- TJA1050 CAN transceivers (or SN65HVD230)
- 500 kbps CAN bus with 120Ω termination
- 5V power supply
- Sensors: GPS module (UART), temperature sensors, etc.

**Wiring:**
- ESP32 GPIO 5 → CAN TX
- ESP32 GPIO 4 → CAN RX
- TJA1050 CANH/CANL → CAN bus

### Raspberry Pi Dashboard
- Raspberry Pi 4 (4GB recommended) or Pi 3B+
- Waveshare CAN HAT (MCP2515 + SN65HVD230)
- Display (HDMI or DSI touchscreen)
- SD card (16GB+)
- 5V/3A power supply

## Building and Deployment

### ESP32 Modules

#### Prerequisites
- [PlatformIO](https://platformio.org/) installed
- USB cable for ESP32 programming
- ESP32 connected to CAN bus via TJA1050 transceiver

#### Build and Upload

Navigate to a specific module:
```bash
cd modules/gps-module
```

Build the firmware:
```bash
pio run
```

Upload to ESP32:
```bash
pio run --target upload
```

Monitor serial output (115200 baud):
```bash
pio device monitor --baud 115200
```

Clean build:
```bash
pio run --target clean
```

#### Build All Modules
```bash
# From repository root
cd modules
for dir in gps-module temp-gauge fuel-gauge speedometer ui-dash body-sensors; do
    echo "Building $dir..."
    (cd "$dir" && pio run)
done
```

---

### Raspberry Pi Dashboard

The dashboard can be built on **Windows** (for development) or **Raspberry Pi** (for deployment).

---

## Building on Windows (SDL2 Simulator)

The Windows build uses SDL2 to simulate the display and generates mock CAN data for testing.

### Prerequisites

1. **Install vcpkg** (for SDL2):
   ```powershell
   cd C:\
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   .\vcpkg integrate install
   ```

2. **Install SDL2**:
   ```powershell
   .\vcpkg install sdl2:x64-windows
   ```

3. **Install CMake**:
   - Download from [cmake.org](https://cmake.org/download/)
   - Or via Chocolatey: `choco install cmake`

4. **Install Visual Studio** (2019 or later):
   - Include "Desktop development with C++" workload
   - Or use MinGW/GCC if preferred

### Build Steps

1. **Navigate to dashboard directory**:
   ```powershell
   cd ui-dashboard
   mkdir build
   cd build
   ```

2. **Configure CMake** (using vcpkg):
   ```powershell
   cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -A x64
   ```

3. **Build**:
   ```powershell
   cmake --build . --config Release
   ```

4. **Run the simulator**:
   ```powershell
   .\Release\leaf-can-dashboard.exe
   ```

### Expected Output (Windows)
- SDL2 window opens showing the dashboard UI
- Mock CAN data displays simulated driving (speed, battery SOC, RPM, GPS)
- Console shows CAN data generation logs

---

## Building on Raspberry Pi (Production)

The Pi build uses Linux framebuffer for display and SocketCAN for real CAN communication.

### Prerequisites

1. **Raspberry Pi OS** (32-bit or 64-bit, Bullseye or later)
2. **Update system**:
   ```bash
   sudo apt update
   sudo apt upgrade -y
   ```

3. **Install build tools**:
   ```bash
   sudo apt install -y build-essential cmake git
   ```

4. **Enable SPI** (for Waveshare CAN HAT):
   ```bash
   sudo raspi-config
   # Navigate to: Interface Options → SPI → Enable
   ```

5. **Configure CAN interface**:

   Add to `/boot/config.txt`:
   ```
   dtparam=spi=on
   dtoverlay=mcp2515-can0,oscillator=12000000,interrupt=25
   dtoverlay=spi-bcm2835-overlay
   ```

   Add to `/etc/network/interfaces`:
   ```
   auto can0
   iface can0 inet manual
       pre-up /sbin/ip link set can0 type can bitrate 500000
       up /sbin/ifconfig can0 up
       down /sbin/ifconfig can0 down
   ```

6. **Reboot**:
   ```bash
   sudo reboot
   ```

7. **Verify CAN interface**:
   ```bash
   ip link show can0
   # Should show can0 interface with state UP
   ```

### Build Steps

1. **Clone repository** (if not already on Pi):
   ```bash
   git clone https://github.com/yourusername/test-lvgl-cross-compile.git
   cd test-lvgl-cross-compile/ui-dashboard
   ```

2. **Create build directory**:
   ```bash
   mkdir build
   cd build
   ```

3. **Configure CMake**:
   ```bash
   cmake ..
   ```

4. **Build**:
   ```bash
   make -j4
   ```

5. **Run** (requires root for framebuffer and CAN access):
   ```bash
   sudo ./leaf-can-dashboard
   ```

### Expected Output (Raspberry Pi)
- Dashboard UI renders directly to framebuffer (HDMI/DSI display)
- Receives real CAN data from `can0` interface
- Displays live vehicle telemetry from Leaf CAN bus

---

## Testing CAN Communication

### Monitor CAN Traffic (Raspberry Pi)
```bash
# Install can-utils
sudo apt install can-utils

# Dump all CAN messages
candump can0

# Send test message
cansend can0 710#0102030405060708
```

### Monitor CAN Traffic (ESP32)
View serial output at 115200 baud. The LeafCANBus library prints:
- Subscription confirmations
- Publisher configurations
- RX/TX statistics
- Bus-off recovery attempts

Example output:
```
[CAN] Initialized on TX=5, RX=4
[CAN] Subscribed to 0x1F2
[CAN] Publisher added for 0x710 (interval: 1000 ms)
[GPS] Position: 37.774900, -122.419400
CAN Stats - RX: 1234, TX: 567, Errors: 0
```

---

## Development Workflow

### Adding a New ESP32 Module

1. **Create module directory**:
   ```bash
   cd modules
   mkdir my-new-module
   cd my-new-module
   mkdir src
   ```

2. **Create platformio.ini**:
   ```ini
   [env:esp32dev]
   platform = espressif32
   board = esp32dev
   framework = arduino
   monitor_speed = 115200
   lib_extra_dirs = ../../lib
   lib_deps =
       ; Add your dependencies here
   ```

3. **Write src/main.cpp**:
   ```cpp
   #include <Arduino.h>
   #include "LeafCANBus.h"

   LeafCANBus canBus;

   void setup() {
       Serial.begin(115200);
       canBus.begin();
       // Subscribe/publish as needed
   }

   void loop() {
       canBus.process();
       delay(10);
   }
   ```

4. **Build and upload**:
   ```bash
   pio run --target upload
   ```

### Adding New CAN Messages

1. Edit `lib/LeafCANBus/src/LeafCANMessages.h`:
   - Add CAN ID definition
   - Define state structure

2. Edit `lib/LeafCANBus/src/LeafCANMessages.cpp`:
   - Implement pack/unpack functions

3. Update ESP32 modules to subscribe/publish
4. Update UI dashboard to display new data

---

## Troubleshooting

### ESP32 Build Errors
- **Solution**: Update PlatformIO: `pio upgrade`
- Verify `lib_extra_dirs = ../../lib` in platformio.ini
- Clean build: `pio run --target clean`

### SDL2 Not Found (Windows)
- Install via vcpkg: `vcpkg install sdl2:x64-windows`
- Verify CMAKE_TOOLCHAIN_FILE points to vcpkg

### CAN Interface Not Found (Pi)
- Check SPI enabled: `lsmod | grep spi`
- Verify CAN HAT wiring and oscillator frequency (12 MHz)
- Reload device tree: `sudo dtoverlay mcp2515-can0`

### Framebuffer Permission Denied (Pi)
- Run as root: `sudo ./leaf-can-dashboard`
- Or add user to video group: `sudo usermod -a -G video $USER`

### No CAN Messages Received
- Verify CAN bus termination (120Ω resistors at both ends)
- Check bitrate matches (500 kbps)
- Use `candump can0` to verify messages on bus
- Check ESP32 CAN TX/RX pin connections

---

## VSCode Development Setup

### Recommended Extensions
- **PlatformIO IDE** - ESP32 development
- **CMake Tools** - Raspberry Pi dashboard builds
- **C/C++** (Microsoft) - IntelliSense

### Workspace Settings

Create `.vscode/settings.json`:
```json
{
    "platformio-ide.autoRebuildAutocompleteIndex": true,
    "cmake.configureOnOpen": false,
    "files.associations": {
        "*.h": "c",
        "*.cpp": "cpp"
    }
}
```

---

## License

MIT License - See LICENSE file for details

## Contributing

Pull requests welcome! Please ensure:
- ESP32 code compiles with PlatformIO
- Dashboard builds on both Windows (SDL2) and Linux (framebuffer)
- CAN message definitions follow existing pack/unpack pattern

## Support

For issues, questions, or feature requests, please open a GitHub issue.

---

**Project Status**: Active Development

**Last Updated**: 2025

**Tested On**:
- ESP32-DevKitC V4
- Raspberry Pi 4 (4GB) with Waveshare CAN HAT
- 2012 Nissan Leaf (24 kWh battery)

# Windows Build Guide

This guide explains how to build and run the Leaf CAN Dashboard on Windows.

## Prerequisites

1. **Visual Studio 2019 or newer** (with C++ tools)
2. **CMake 3.16+** (https://cmake.org/download/)
3. **vcpkg** (for SDL2 dependency)
4. **Git** (for cloning repositories)

## Setup SDL2 with vcpkg

```powershell
# Install vcpkg if you haven't already
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Install SDL2
.\vcpkg install sdl2:x64-windows
```

## Build Steps

### 1. Configure with CMake

```powershell
cd ui-dashboard
mkdir build
cd build

# For EMBOO battery (250 kbps)
cmake .. -DBATTERY_TYPE=EMBOO -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -A x64

# For Nissan Leaf battery (500 kbps)
cmake .. -DBATTERY_TYPE=NISSAN_LEAF -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -A x64
```

### 2. Build

```powershell
# Build Release version
cmake --build . --config Release

# Or build Debug version
cmake --build . --config Debug
```

### 3. Run

```powershell
# Run Release build
.\Release\leaf-can-dashboard.exe

# Run Debug build
.\Debug\leaf-can-dashboard.exe
```

## CAN Data Source

The Windows version automatically attempts to use one of two CAN data sources:

### 1. PEAK CAN Hardware (CANable Pro) - **Primary**

If you have a PEAK CAN device (like CANable Pro) connected:

1. Install PCAN-Basic drivers from: https://www.peak-system.com/PCAN-Basic.239.0.html
2. Connect your CANable Pro or PEAK CAN device
3. The application will automatically detect and use it

**Console output when PCAN is detected:**
```
=== CAN Initialization ===
[CAN] Attempting to connect to CANable Pro via PCAN...
[PCAN] ✓ Successfully connected to CANable Pro (PCAN_USBBUS1 @ 500kbps)
[PCAN] Reading live CAN messages...
```

### 2. Demo Playback Mode - **Fallback**

If no PCAN hardware is detected, the application automatically falls back to demo playback mode:

1. Reads CAN messages from `can_log_demo.txt`
2. Plays back messages in real-time
3. Loops continuously

**Console output when using demo playback:**
```
=== CAN Initialization ===
[CAN] Attempting to connect to CANable Pro via PCAN...
[PCAN] PCANBasic.dll not found (install PEAK drivers from peak-system.com)
[CAN] Falling back to demo playback mode...
[DemoPlayback] Searching for can_log_demo.txt...
[DemoPlayback] ✓ Found CAN log file: can_log_demo.txt
[DemoPlayback] ✓ Loaded 1234 CAN messages from 1250 lines
[DemoPlayback] Time range: 0.000000s to 120.500000s (120.5s duration)
[DemoPlayback] Playback will loop continuously
```

## Demo CAN Log File

The `can_log_demo.txt` file is automatically copied to the build output directory by CMake.

**Location after build:**
- Debug: `build/Debug/can_log_demo.txt`
- Release: `build/Release/can_log_demo.txt`

If you get an error about missing `can_log_demo.txt`, you can manually copy it:

```powershell
# From the ui-dashboard directory
copy src\platform\windows\can_log_demo.txt build\Release\
copy src\platform\windows\can_log_demo.txt build\Debug\
```

## CAN Log File Format

The demo log file uses a simple text format:

```
# Timestamp(s)  CAN_ID  DLC  Data_Bytes
0.000000        351     8    0B B8 00 00 00 00 00 00
0.000000        356     6    01 95 01 E5 01 00
0.010000        1F2     8    00 0E 03 01 2A 00 00 3C
```

- **Timestamp**: Time in seconds (floating point)
- **CAN_ID**: CAN identifier in hexadecimal
- **DLC**: Data length code (0-8)
- **Data**: Up to 8 bytes in hexadecimal (space-separated)

You can create your own log files by recording CAN traffic on Linux:

```bash
candump can0 -t a -L > my_can_log.txt
```

Then convert to the format above, or modify `parse_can_log_line()` in `mock_can.cpp` to support your format.

## Troubleshooting

### Issue: CMake can't find SDL2

**Error:**
```
Could not find a package configuration file provided by "SDL2"
```

**Solution:**
```powershell
# Ensure vcpkg is set up correctly
C:\vcpkg\vcpkg install sdl2:x64-windows

# Add toolchain file to cmake command
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Issue: can_log_demo.txt not found

**Error:**
```
[DemoPlayback] ERROR: Could not find can_log_demo.txt
```

**Solution 1 - Rebuild:**
```powershell
# Clean and rebuild to trigger the file copy
cmake --build . --config Release --target clean
cmake --build . --config Release
```

**Solution 2 - Manual copy:**
```powershell
# From ui-dashboard/build directory
copy ..\src\platform\windows\can_log_demo.txt Release\
copy ..\src\platform\windows\can_log_demo.txt Debug\
```

**Solution 3 - Run from source directory:**
```powershell
# Copy exe to source directory (where can_log_demo.txt can be found easier)
cd ui-dashboard
copy build\Release\leaf-can-dashboard.exe .
.\leaf-can-dashboard.exe
```

### Issue: PCAN driver not loading

**Error:**
```
[PCAN] PCANBasic.dll not found
```

**Solution:**
1. Download and install PCAN-Basic from: https://www.peak-system.com/PCAN-Basic.239.0.html
2. Make sure PCANBasic.dll is in your PATH or in the same directory as the executable
3. Restart the application

If you don't have PCAN hardware, this is expected. The application will automatically fall back to demo playback mode.

### Issue: No CAN messages displayed

**Problem:** Application runs but shows no CAN data on the dashboard.

**Debug steps:**

1. **Check console output** - Look for errors or warnings
2. **Verify data source:**
   - PCAN mode: Check "[PCAN] Received X messages" heartbeat
   - Demo mode: Should see messages processing automatically
3. **Check battery type configuration:**
   ```powershell
   # In build directory, check CMakeCache.txt
   findstr BATTERY CMakeCache.txt
   ```
4. **Try with verbose debug output** - Edit `mock_can.cpp` and set CAN_DEBUG to 1 at the top

## Battery Type Configuration

The battery type must match your CAN bus speed and message format:

### EMBOO Battery (Orion BMS)
- **CAN Speed:** 250 kbps
- **CAN IDs:** 0x6B0-0x6B4, 0x351, 0x355, 0x356, 0x35A
- **Build command:**
  ```powershell
  cmake .. -DBATTERY_TYPE=EMBOO -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -A x64
  ```

### Nissan Leaf Battery
- **CAN Speed:** 500 kbps
- **CAN IDs:** 0x1DB, 0x1DC, 0x1F2, 0x1DA, 0x390
- **Build command:**
  ```powershell
  cmake .. -DBATTERY_TYPE=NISSAN_LEAF -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -A x64
  ```

## Performance Notes

- **SDL2 Window:** Runs at 60 FPS with LVGL rendering
- **CAN Message Rate:** Supports up to 10,000 messages/second
- **Demo Playback:** Maintains original message timing for realistic simulation

## Development

### Modifying CAN Message Handling

CAN messages are processed in:
- `src/platform/windows/mock_can.cpp` - Hardware/demo data source
- `src/shared/can_receiver.cpp` - Message parsing and state updates
- `src/ui/dashboard_ui.cpp` - UI rendering

### Adding New CAN IDs

1. Add CAN ID definition to `lib/LeafCANBus/src/LeafCANMessages.h`
2. Add pack/unpack functions to `LeafCANMessages.cpp`
3. Add message handler in `can_receiver.cpp`
4. Update UI to display new data

## See Also

- Main project documentation: `../README.md`
- Battery configuration guide: `../docs/BATTERY_CONFIGURATION.md`
- Basic operations (Raspberry Pi): `../docs/BASIC_OPERATIONS.md`
- Project guidance: `../CLAUDE.md`

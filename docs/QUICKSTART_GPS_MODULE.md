# Quick Start: GPS Module Implementation

## Overview

This guide provides step-by-step instructions for building and testing the GPS position tracker module using the LilyGO T-CAN485 and NEO-M8N GPS receiver.

**Estimated time**: 4-6 hours
**Difficulty**: Intermediate
**Prerequisites**: PlatformIO, basic soldering skills

---

## Parts Required

| Item | Quantity | Purchase Link | Cost |
|------|----------|---------------|------|
| LilyGO T-CAN485 | 1 | [AliExpress](https://www.aliexpress.com/item/1005006076540945.html) | ~$15 |
| NEO-M8N GPS module with antenna | 1 | [Amazon](https://www.amazon.com/s?k=neo-m8n+gps) | ~$12 |
| Dupont jumper wires (F-F) | 4 | Local electronics store | ~$2 |
| 120Ω resistor (1/4W) | 1 | Local electronics store | $0.10 |
| USB-C cable | 1 | Existing | — |
| 12V power supply (testing) | 1 | Existing | — |

**Total**: ~$30

---

## Step 1: Hardware Assembly (30 minutes)

### 1.1 Inspect T-CAN485 Board

When you receive the T-CAN485, verify:
- [ ] No visible damage to components
- [ ] USB-C port is intact
- [ ] Antenna connector (if using Wi-Fi features)
- [ ] CAN terminal blocks present

### 1.2 Wire GPS Module to T-CAN485

**NEO-M8N GPS Module Pinout**:
```
┌─────────────────┐
│  NEO-M8N GPS    │
│                 │
│  VCC  ●─────────┼──► 3.3V (T-CAN485)
│  GND  ●─────────┼──► GND  (T-CAN485)
│  TX   ●─────────┼──► IO18 (T-CAN485)
│  RX   ●─────────┼──► IO19 (T-CAN485)
│                 │
└─────────────────┘
```

**Wiring Table**:
| GPS Pin | Wire Color | T-CAN485 Pin | Notes |
|---------|------------|--------------|-------|
| VCC | Red | 3V3 | 3.3V power |
| GND | Black | GND | Ground |
| TX | Yellow | IO18 | GPS transmit → ESP32 receive |
| RX | Green | IO19 | GPS receive → ESP32 transmit |

**IMPORTANT**:
- Double-check VCC goes to **3.3V**, NOT 5V (will damage GPS module)
- TX on GPS connects to RX on ESP32 (IO18)
- RX on GPS connects to TX on ESP32 (IO19)

### 1.3 Photo Documentation

Take photos before powering on:
- [ ] Overall wiring
- [ ] Close-up of each connection
- [ ] GPS antenna placement

This helps troubleshooting later.

---

## Step 2: Software Setup (1 hour)

### 2.1 Create Module Directory

```bash
cd /home/user/test-lvgl-cross-compile/modules
mkdir -p gps-module-tcan485/src
cd gps-module-tcan485
```

### 2.2 Create platformio.ini

```bash
cat > platformio.ini << 'EOF'
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
monitor_filters = esp32_exception_decoder
lib_extra_dirs = ../../lib

lib_deps =
    ../../lib/LeafCANBus
    mikalhart/TinyGPSPlus @ ^1.0.3

build_flags =
    -D CORE_DEBUG_LEVEL=3
EOF
```

### 2.3 Create Main Source File

```bash
cat > src/main.cpp << 'EOF'
#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "LeafCANBus.h"
#include "LeafCANMessages.h"

// GPS UART on Hardware Serial1
#define GPS_RX_PIN 18
#define GPS_TX_PIN 19
#define GPS_BAUD 9600

HardwareSerial gpsSerial(1);
TinyGPSPlus gps;
LeafCANBus canBus;

GPSPositionState gpsPosition = {0};
GPSStatusState gpsStatus = {0};

uint32_t lastGPSUpdate = 0;
uint32_t bootTime = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=================================");
    Serial.println("  GPS Module - T-CAN485");
    Serial.println("  Nissan Leaf CAN Network");
    Serial.println("=================================\n");

    // Initialize GPS UART
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    Serial.printf("[GPS] UART initialized: RX=%d, TX=%d, Baud=%d\n",
                  GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);

    // Initialize CAN bus
    if (canBus.begin()) {
        Serial.println("[CAN] Bus initialized @ 500 kbps");

        // Register periodic publishers
        canBus.publish(CAN_ID_GPS_POSITION, 1000, pack_gps_position, &gpsPosition);
        canBus.publish(CAN_ID_GPS_STATUS, 5000, pack_gps_status, &gpsStatus);

        Serial.println("[CAN] Publishers registered:");
        Serial.printf("  - 0x%03X: GPS Position (1000ms)\n", CAN_ID_GPS_POSITION);
        Serial.printf("  - 0x%03X: GPS Status (5000ms)\n", CAN_ID_GPS_STATUS);
    } else {
        Serial.println("[CAN] ERROR: Failed to initialize!");
    }

    bootTime = millis();
    Serial.println("\n[READY] Waiting for GPS lock...\n");
}

void loop() {
    // Process incoming GPS data
    while (gpsSerial.available() > 0) {
        char c = gpsSerial.read();
        gps.encode(c);
    }

    // Update GPS state when new data arrives
    if (gps.location.isUpdated() || gps.altitude.isUpdated() || gps.speed.isUpdated()) {
        gpsPosition.latitude_deg = gps.location.lat();
        gpsPosition.longitude_deg = gps.location.lng();
        gpsPosition.speed_kmh = gps.speed.kmph();
        gpsPosition.heading_deg = gps.course.deg();
        gpsPosition.altitude_m = gps.altitude.meters();

        gpsStatus.satellites = gps.satellites.value();
        gpsStatus.hdop = gps.hdop.value();
        gpsStatus.fix_quality = gps.location.isValid() ? 1 : 0;

        uint32_t now = millis();
        if (now - lastGPSUpdate > 5000) {  // Print every 5 seconds
            Serial.printf("[GPS] Lat=%.6f, Lon=%.6f, Speed=%.1f km/h, Sats=%d\n",
                          gpsPosition.latitude_deg,
                          gpsPosition.longitude_deg,
                          gpsPosition.speed_kmh,
                          gpsStatus.satellites);
            lastGPSUpdate = now;
        }
    }

    // Print status every 30 seconds
    static uint32_t lastStatus = 0;
    uint32_t now = millis();
    if (now - lastStatus > 30000) {
        uint32_t uptimeSec = (now - bootTime) / 1000;
        Serial.printf("\n[STATUS] Uptime: %lu sec\n", uptimeSec);
        Serial.printf("  GPS chars processed: %lu\n", gps.charsProcessed());
        Serial.printf("  GPS sentences valid: %lu\n", gps.passedChecksum());
        Serial.printf("  GPS sentences failed: %lu\n", gps.failedChecksum());
        Serial.printf("  CAN TX: %lu, RX: %lu, Errors: %lu\n\n",
                      canBus.getTxCount(), canBus.getRxCount(), canBus.getErrorCount());
        lastStatus = now;
    }

    // Process CAN bus (handles periodic publishing)
    canBus.process();

    delay(10);
}
EOF
```

### 2.4 Add CAN Message Definitions to LeafCANMessages.h

Open `lib/LeafCANBus/src/LeafCANMessages.h` and add:

```cpp
// GPS Module CAN IDs
#define CAN_ID_GPS_POSITION  0x710
#define CAN_ID_GPS_STATUS    0x711

// GPS Position State (0x710)
struct GPSPositionState {
    double latitude_deg;      // Decimal degrees
    double longitude_deg;     // Decimal degrees
    float speed_kmh;          // km/h
    float heading_deg;        // Degrees (0-360)
    float altitude_m;         // Meters above sea level
};

// GPS Status State (0x711)
struct GPSStatusState {
    uint8_t satellites;       // Number of satellites
    uint16_t hdop;            // Horizontal dilution of precision * 100
    uint8_t fix_quality;      // 0=none, 1=GPS, 2=DGPS
};

// Function declarations
void pack_gps_position(uint8_t* data, void* state);
void pack_gps_status(uint8_t* data, void* state);
```

### 2.5 Add Pack Functions to LeafCANMessages.cpp

Open `lib/LeafCANBus/src/LeafCANMessages.cpp` and add:

```cpp
void pack_gps_position(uint8_t* data, void* state) {
    GPSPositionState* gps = (GPSPositionState*)state;

    // Latitude: int32_t (degrees * 1e7)
    int32_t lat = (int32_t)(gps->latitude_deg * 1e7);
    data[0] = (lat >> 24) & 0xFF;
    data[1] = (lat >> 16) & 0xFF;
    data[2] = (lat >> 8) & 0xFF;
    data[3] = lat & 0xFF;

    // Longitude: int32_t (degrees * 1e7)
    int32_t lon = (int32_t)(gps->longitude_deg * 1e7);
    data[4] = (lon >> 24) & 0xFF;
    data[5] = (lon >> 16) & 0xFF;
    data[6] = (lon >> 8) & 0xFF;
    data[7] = lon & 0xFF;
}

void pack_gps_status(uint8_t* data, void* state) {
    GPSStatusState* gps = (GPSStatusState*)state;

    // Speed: uint16_t (km/h * 100)
    uint16_t speed = (uint16_t)(((GPSPositionState*)state)->speed_kmh * 100);
    data[0] = (speed >> 8) & 0xFF;
    data[1] = speed & 0xFF;

    // Heading: uint16_t (degrees * 10)
    uint16_t heading = (uint16_t)(((GPSPositionState*)state)->heading_deg * 10);
    data[2] = (heading >> 8) & 0xFF;
    data[3] = heading & 0xFF;

    // Altitude: int16_t (meters)
    int16_t altitude = (int16_t)((GPSPositionState*)state)->altitude_m;
    data[4] = (altitude >> 8) & 0xFF;
    data[5] = altitude & 0xFF;

    // Satellites and fix quality
    data[6] = gps->satellites;
    data[7] = gps->fix_quality;
}
```

---

## Step 3: Initial Testing (30 minutes)

### 3.1 Build and Upload

```bash
cd /home/user/test-lvgl-cross-compile/modules/gps-module-tcan485

# Build
pio run

# Upload to T-CAN485
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

**Expected Output**:
```
=================================
  GPS Module - T-CAN485
  Nissan Leaf CAN Network
=================================

[GPS] UART initialized: RX=18, TX=19, Baud=9600
[CAN] Bus initialized @ 500 kbps
[CAN] Publishers registered:
  - 0x710: GPS Position (1000ms)
  - 0x711: GPS Status (5000ms)

[READY] Waiting for GPS lock...

[GPS] Lat=37.774929, Lon=-122.419418, Speed=0.0 km/h, Sats=8

[STATUS] Uptime: 30 sec
  GPS chars processed: 1024
  GPS sentences valid: 15
  GPS sentences failed: 0
  CAN TX: 35, RX: 0, Errors: 0
```

### 3.2 Troubleshooting No GPS Lock

If you see:
```
GPS chars processed: 0
GPS sentences valid: 0
```

**Checklist**:
- [ ] GPS antenna is connected
- [ ] GPS module has clear view of sky (move away from windows/buildings)
- [ ] Wait 2-5 minutes for cold start
- [ ] Check wiring (TX/RX might be swapped)
- [ ] Test with simple serial passthrough:
```cpp
void loop() {
    while (gpsSerial.available()) {
        Serial.write(gpsSerial.read());  // Should see $GPGGA, $GPRMC, etc.
    }
}
```

---

## Step 4: CAN Bus Integration (1 hour)

### 4.1 Hardware: Add CAN Termination

If this is the **first** module on your CAN bus, add a 120Ω termination resistor:

```
T-CAN485 Terminal Block:
  CAN-H ──┬──► To CAN bus
          │
          ├── 120Ω resistor
          │
  CAN-L ──┴──► To CAN bus
```

Solder the resistor between CAN-H and CAN-L terminals.

### 4.2 Connect to CAN Network

**Option A: Test with Raspberry Pi**

Connect twisted pair wires:
- T-CAN485 CAN-H → Raspberry Pi CAN HAT CAN-H
- T-CAN485 CAN-L → Raspberry Pi CAN HAT CAN-L
- Common GND between devices

On Raspberry Pi:
```bash
# Bring up CAN interface
sudo ip link set can0 up type can bitrate 500000

# Monitor CAN traffic
candump can0
```

**Expected Output**:
```
can0  710   [8]  02 3C 28 B5 F8 6A 14 0A   # GPS Position
can0  711   [8]  00 00 01 2C 00 64 08 01   # GPS Status
```

**Option B: Test with PCAN-USB Adapter**

On Windows with PCAN-View software:
- Set bitrate to 500 kbps
- Monitor for messages 0x710 and 0x711
- Verify 1-second interval

### 4.3 Decode CAN Messages

Create a Python script to decode GPS data:

```python
#!/usr/bin/env python3
import can
import struct

bus = can.interface.Bus(channel='can0', bustype='socketcan')

print("Listening for GPS CAN messages...\n")

while True:
    msg = bus.recv()

    if msg.arbitration_id == 0x710:  # GPS Position
        lat_raw = struct.unpack('>i', msg.data[0:4])[0]
        lon_raw = struct.unpack('>i', msg.data[4:8])[0]
        lat = lat_raw / 1e7
        lon = lon_raw / 1e7
        print(f"GPS Position: {lat:.6f}, {lon:.6f}")

    elif msg.arbitration_id == 0x711:  # GPS Status
        speed_raw = struct.unpack('>H', msg.data[0:2])[0]
        heading_raw = struct.unpack('>H', msg.data[2:4])[0]
        alt_raw = struct.unpack('>h', msg.data[4:6])[0]
        sats = msg.data[6]
        fix = msg.data[7]

        speed = speed_raw / 100.0
        heading = heading_raw / 10.0
        print(f"  Speed: {speed:.1f} km/h, Heading: {heading:.1f}°, Sats: {sats}, Fix: {fix}")
```

---

## Step 5: Dashboard Integration (1 hour)

### 5.1 Update CANReceiver

Edit `ui-dashboard/src/shared/can_receiver.h`:

```cpp
class CANReceiver {
public:
    // GPS data (0x710, 0x711)
    int32_t getLatitude() const { return latitude_.load(); }      // deg * 1e7
    int32_t getLongitude() const { return longitude_.load(); }    // deg * 1e7
    uint16_t getGPSSpeed() const { return gps_speed_.load(); }    // km/h * 100
    uint16_t getHeading() const { return heading_.load(); }       // deg * 10
    int16_t getAltitude() const { return altitude_.load(); }      // meters
    uint8_t getSatellites() const { return satellites_.load(); }

private:
    std::atomic<int32_t> latitude_{0};
    std::atomic<int32_t> longitude_{0};
    std::atomic<uint16_t> gps_speed_{0};
    std::atomic<uint16_t> heading_{0};
    std::atomic<int16_t> altitude_{0};
    std::atomic<uint8_t> satellites_{0};
};
```

Edit `ui-dashboard/src/shared/can_receiver.cpp`:

```cpp
void CANReceiver::processCANMessage(uint32_t can_id, uint8_t len, const uint8_t* data) {
    const uint32_t baseId = (can_id & 0x7FF);

    // 0x710: GPS Position
    if (baseId == 0x710 && len >= 8) {
        int32_t lat = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        int32_t lon = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
        latitude_.store(lat, std::memory_order_relaxed);
        longitude_.store(lon, std::memory_order_relaxed);
    }

    // 0x711: GPS Status/Velocity
    else if (baseId == 0x711 && len >= 8) {
        uint16_t speed = (data[0] << 8) | data[1];
        uint16_t heading = (data[2] << 8) | data[3];
        int16_t altitude = (data[4] << 8) | data[5];
        uint8_t sats = data[6];

        gps_speed_.store(speed, std::memory_order_relaxed);
        heading_.store(heading, std::memory_order_relaxed);
        altitude_.store(altitude, std::memory_order_relaxed);
        satellites_.store(sats, std::memory_order_relaxed);
    }

    // Existing message handlers...
}
```

### 5.2 Display GPS Data on Dashboard

Edit `ui-dashboard/src/ui/dashboard_ui.cpp`:

```cpp
void DashboardUI::update(const CANReceiver& can) {
    // Existing updates...

    // GPS speed (cross-check with vehicle speed)
    uint16_t gps_speed_raw = can.getGPSSpeed();
    float gps_speed_kmh = gps_speed_raw / 100.0f;

    if (gps_speed_label) {
        lv_label_set_text_fmt(gps_speed_label, "GPS: %.1f km/h", gps_speed_kmh);
    }

    // Satellite count
    uint8_t sats = can.getSatellites();
    if (satellite_label) {
        lv_label_set_text_fmt(satellite_label, "Sats: %u", sats);
    }
}
```

### 5.3 Rebuild and Test Dashboard

```bash
cd /home/user/test-lvgl-cross-compile/ui-dashboard/build
cmake ..
make -j4

# On Raspberry Pi
sudo ./leaf-can-dashboard

# On Windows
./Release/leaf-can-dashboard.exe
```

**Verify**:
- GPS speed appears on dashboard
- Satellite count displays correctly
- Speed matches vehicle speed (if moving)

---

## Step 6: Field Testing (1 hour)

### 6.1 Install in Vehicle

**Mounting**:
- Mount T-CAN485 on A-pillar or dashboard
- Route GPS antenna to roof or dash (clear sky view)
- Connect 12V power via inline fuse
- Connect CAN-H/L to vehicle CAN bus

**Safety**:
- [ ] Fuse rating: 5A
- [ ] CAN wires twisted and shielded
- [ ] All connections secured with heat shrink
- [ ] GPS antenna away from metal surfaces

### 6.2 Drive Test

Start vehicle and drive for 10 minutes:

**Check**:
- [ ] GPS acquires lock within 2 minutes
- [ ] Latitude/longitude updates every second
- [ ] Speed matches speedometer (±2 km/h)
- [ ] Dashboard displays GPS data
- [ ] No CAN bus errors (`canBus.getErrorCount()` stays at 0)

**Log Data**:
```bash
# On Raspberry Pi
candump -l can0
# Creates candump-YYYY-MM-DD_HHMMSS.log

# Later analyze:
cat candump-*.log | grep " 710 "
```

---

## Success Criteria

✅ **Module Complete** when:
- [ ] GPS acquires satellite lock outdoors
- [ ] CAN messages 0x710 and 0x711 transmit at 1 Hz
- [ ] Dashboard displays GPS speed and satellite count
- [ ] GPS speed matches vehicle speed within 2 km/h
- [ ] No CAN bus errors after 30 minutes of operation
- [ ] Module survives vehicle power cycling (key on/off)

---

## Next Steps

After completing the GPS module:

1. **Temperature Module** (see `PERIPHERAL_ARCHITECTURE.md`)
   - Use same T-CAN485 platform
   - Add DS18B20 sensors on 1-Wire bus
   - Publish CAN IDs 0x720, 0x721

2. **RS485 Bridge** (see `PERIPHERAL_ARCHITECTURE.md`)
   - Use T-CAN485 built-in RS485 interface
   - Add Modbus RTU sensor polling
   - Publish CAN ID 0x730

3. **System Integration**
   - Install all 3 modules in vehicle
   - Verify CAN bus utilization <60%
   - Update dashboard UI with all sensor data

---

## Support

- **Hardware issues**: Check `docs/HARDWARE_DIAGRAM.txt`
- **CAN issues**: See troubleshooting section in `PERIPHERAL_ARCHITECTURE.md`
- **Software bugs**: Create issue at https://github.com/Hercules-Tele-com/test-lvgl-cross-compile/issues

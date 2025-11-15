# LilyGO T-CAN485 Peripheral Device Architecture

## Overview

This document defines the hardware and software architecture for ESP32-based peripheral modules using the LilyGO T-CAN485 development board. The system consists of distributed sensor modules communicating via CAN bus at 500 kbps.

## Hardware Platform: LilyGO T-CAN485

### Core Specifications
- **MCU**: ESP32-WROOM-32 (dual-core, 240 MHz, 4MB Flash)
- **CAN Interface**: SN65HVD231 transceiver (built-in TWAI protocol)
- **RS485 Interface**: MAX13487EESA+ transceiver
- **Power Input**: 5V-12V via 2-pin terminal (ME2107A50M5G booster)
- **Connectivity**: Wi-Fi 802.11 b/g/n, BLE
- **Storage**: MicroSD card slot (SPI)
- **Indicator**: WS2812B RGB LED

### GPIO Pinout

| Function | GPIO Pin | Notes |
|----------|----------|-------|
| **RS485 TX** | IO22 | Hardware Serial2 |
| **RS485 RX** | IO21 | Hardware Serial2 |
| **RS485 Callback** | IO17 | Flow control |
| **RS485 Enable** | IO9 | Transmit enable |
| **CAN TX** | — | Built-in TWAI |
| **CAN RX** | — | Built-in TWAI |
| **SD MISO** | IO2 | SPI |
| **SD MOSI** | IO15 | SPI |
| **SD SCLK** | IO14 | SPI |
| **SD CS** | IO13 | SPI |
| **RGB LED** | IO4 | WS2812B data |
| **Booster EN** | IO16 | Power enable |

### Available GPIOs for Sensors
- **IO5, IO18, IO19, IO23**: General purpose (UART/I2C/1-Wire)
- **IO25, IO26, IO27, IO32, IO33**: ADC capable
- **IO34, IO35, IO36, IO39**: Input-only (no pull-up)

---

## Module 1: GPS Position Module

### Hardware Configuration

**GPS Receiver**: u-blox NEO-M8N or NEO-6M
- **VCC** → 3.3V
- **GND** → GND
- **TX (GPS)** → IO18 (ESP32 RX)
- **RX (GPS)** → IO19 (ESP32 TX)

**Benefits of NEO-M8N**:
- Multi-GNSS (GPS, Galileo, GLONASS, BeiDou)
- Horizontal accuracy: 2.5-4 meters
- Better satellite acquisition than NEO-6M

### Software Architecture

**PlatformIO Dependencies**:
```ini
lib_deps =
    ../../lib/LeafCANBus
    mikalhart/TinyGPSPlus @ ^1.0.3
```

**Key Components**:
```cpp
#include <LeafCANBus.h>
#include <TinyGPSPlus.h>

HardwareSerial gpsSerial(1);  // UART1
TinyGPSPlus gps;
LeafCANBus canBus;
GPSPositionState gpsPosition;

void setup() {
    gpsSerial.begin(9600, SERIAL_8N1, 18, 19);  // RX=18, TX=19
    canBus.begin();
    canBus.publish(CAN_ID_GPS_POSITION, 1000, pack_gps_position, &gpsPosition);
    canBus.publish(CAN_ID_GPS_STATUS, 5000, pack_gps_status, &gpsStatus);
}

void loop() {
    while (gpsSerial.available() > 0) {
        gps.encode(gpsSerial.read());
    }

    if (gps.location.isUpdated()) {
        gpsPosition.latitude_deg = gps.location.lat();
        gpsPosition.longitude_deg = gps.location.lng();
        gpsPosition.speed_kmh = gps.speed.kmph();
        gpsPosition.heading_deg = gps.course.deg();
        gpsPosition.altitude_m = gps.altitude.meters();
    }

    canBus.process();
    delay(10);
}
```

### CAN Message Definitions

**0x710: GPS Position (1000ms interval)**
| Byte | Description | Format |
|------|-------------|--------|
| 0-3 | Latitude | int32_t (degrees * 1e7) |
| 4-7 | Longitude | int32_t (degrees * 1e7) |

**0x711: GPS Velocity (1000ms interval)**
| Byte | Description | Format |
|------|-------------|--------|
| 0-1 | Speed | uint16_t (km/h * 100) |
| 2-3 | Heading | uint16_t (degrees * 10) |
| 4-5 | Altitude | int16_t (meters) |
| 6 | Satellites | uint8_t |
| 7 | Fix quality | uint8_t (0=none, 1=GPS, 2=DGPS) |

---

## Module 2: Temperature Sensor Array

### Hardware Configuration

**DS18B20 Sensors** (1-Wire, up to 8 sensors on one GPIO)

**Wiring**:
- All **VDD** → 3.3V
- All **GND** → GND
- All **DQ (data)** → IO23 (with 4.7kΩ pull-up to 3.3V)

**Sensor Locations** (example for Leaf inverter/motor):
1. Motor stator winding
2. Motor coolant inlet
3. Motor coolant outlet
4. Inverter IGBT heatsink
5. Inverter coolant inlet
6. Inverter coolant outlet
7. Charger heatsink
8. Battery coolant

### Software Architecture

**PlatformIO Dependencies**:
```ini
lib_deps =
    ../../lib/LeafCANBus
    paulstoffregen/OneWire @ ^2.3.7
    milesburton/DallasTemperature @ ^3.11.0
```

**Key Components**:
```cpp
#include <LeafCANBus.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 23
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LeafCANBus canBus;

// Store 64-bit addresses for each sensor location
DeviceAddress sensorAddrs[8];
TemperatureArrayState tempState;

void setup() {
    sensors.begin();
    uint8_t count = sensors.getDeviceCount();

    // Discover and store addresses
    for (uint8_t i = 0; i < count && i < 8; i++) {
        sensors.getAddress(sensorAddrs[i], i);
        sensors.setResolution(sensorAddrs[i], 12);  // 12-bit = 0.0625°C
    }

    canBus.begin();
    canBus.publish(CAN_ID_TEMP_ARRAY_1, 2000, pack_temp_array_1, &tempState);
    canBus.publish(CAN_ID_TEMP_ARRAY_2, 2000, pack_temp_array_2, &tempState);
}

void loop() {
    sensors.requestTemperatures();

    for (uint8_t i = 0; i < 8; i++) {
        float tempC = sensors.getTempC(sensorAddrs[i]);
        if (tempC != DEVICE_DISCONNECTED_C) {
            tempState.temps_c[i] = (int16_t)(tempC * 10);  // Store as °C * 10
        }
    }

    canBus.process();
    delay(10);
}
```

### CAN Message Definitions

**0x720: Temperature Array 1 (2000ms interval)**
| Byte | Description | Format |
|------|-------------|--------|
| 0-1 | Temp 1 (Motor stator) | int16_t (°C * 10) |
| 2-3 | Temp 2 (Motor coolant in) | int16_t (°C * 10) |
| 4-5 | Temp 3 (Motor coolant out) | int16_t (°C * 10) |
| 6-7 | Temp 4 (Inverter IGBT) | int16_t (°C * 10) |

**0x721: Temperature Array 2 (2000ms interval)**
| Byte | Description | Format |
|------|-------------|--------|
| 0-1 | Temp 5 (Inv coolant in) | int16_t (°C * 10) |
| 2-3 | Temp 6 (Inv coolant out) | int16_t (°C * 10) |
| 4-5 | Temp 7 (Charger heatsink) | int16_t (°C * 10) |
| 6-7 | Temp 8 (Battery coolant) | int16_t (°C * 10) |

---

## Module 3: RS485 Sensor Bridge

### Use Cases

1. **Modbus RTU Sensors**: Industrial temp/pressure/flow sensors
2. **Remote Sensor Clusters**: Daisy-chained RS485 devices
3. **Third-Party Displays**: RS485 gauges or HMI panels

### Hardware Configuration

**RS485 Wiring** (built-in MAX13487):
- **A+** → RS485 bus A+
- **B-** → RS485 bus B-
- **GND** → Common ground
- **IO9** → RS485 enable (TX high, RX low)

**Example: Modbus RTU Temperature Sensor**
- Address: 0x01
- Baud: 9600, 8N1
- Register 0x0000: Temperature (°C * 10)

### Software Architecture

**PlatformIO Dependencies**:
```ini
lib_deps =
    ../../lib/LeafCANBus
    emelianov/modbus-esp8266 @ ^4.1.0
```

**Key Components**:
```cpp
#include <LeafCANBus.h>
#include <ModbusRTU.h>

#define RS485_EN 9
HardwareSerial rs485Serial(2);  // UART2: TX=22, RX=21
ModbusRTU mb;
LeafCANBus canBus;

RS485SensorState rs485State;

void setup() {
    pinMode(RS485_EN, OUTPUT);
    rs485Serial.begin(9600, SERIAL_8N1, 21, 22);

    mb.begin(&rs485Serial, RS485_EN);
    mb.master();

    canBus.begin();
    canBus.publish(CAN_ID_RS485_DATA, 1000, pack_rs485_data, &rs485State);
}

void loop() {
    // Poll Modbus slave 0x01, read holding register 0x0000 (1 word)
    if (!mb.slave()) {
        mb.readHreg(0x01, 0x0000, &rs485State.temp_raw, 1);
    }

    mb.task();
    canBus.process();
    delay(10);
}
```

### CAN Message Definition

**0x730: RS485 Sensor Data (1000ms interval)**
| Byte | Description | Format |
|------|-------------|--------|
| 0 | Sensor address | uint8_t |
| 1 | Data type | uint8_t (0=temp, 1=pressure, etc.) |
| 2-3 | Value | int16_t (scaled) |
| 4-5 | Status flags | uint16_t |
| 6-7 | Reserved | — |

---

## Power Distribution

### Wiring Harness Design

```
12V Vehicle Battery
    │
    ├─── [Fuse 5A] ──> T-CAN485 GPS Module (12V input)
    │                      │
    │                      └─── NEO-M8N GPS (3.3V regulated)
    │
    ├─── [Fuse 5A] ──> T-CAN485 Temp Module (12V input)
    │                      │
    │                      └─── DS18B20 sensors x8 (3.3V parasitic)
    │
    ├─── [Fuse 5A] ──> T-CAN485 RS485 Bridge (12V input)
    │                      │
    │                      └─── RS485 sensors (external power)
    │
    └─── CAN Bus (twisted pair)
             │
             ├─── CAN-H (yellow)
             ├─── CAN-L (green)
             └─── 120Ω termination resistors (both ends)
```

### CAN Bus Termination

- **120Ω resistor** at first module (GPS)
- **120Ω resistor** at last module (Dashboard/Gateway)
- Total impedance: 60Ω

---

## Integration with Existing System

### Updated CAN ID Map

| ID | Source | Description | Interval |
|----|--------|-------------|----------|
| **Existing OEM Messages** |
| 0x1F2 | Leaf PDM | Vehicle speed, gear | 100ms |
| 0x1D4 | Leaf Inverter | Motor temp, torque | 100ms |
| 0x1DB | Leaf VCM | Pedal position | 10ms |
| 0x1DC | Leaf VCM | Brake switch | 10ms |
| 0x1DA | Leaf VCM | Steering angle | 100ms |
| 0x390 | Leaf BMS | SOC low precision | 500ms |
| **Orion BMS Messages** |
| 0x351 | Orion BMS | Pack voltage, current, SOC | 100ms |
| 0x355 | Orion BMS | Cell voltages (pack 1) | 200ms |
| 0x356 | Orion BMS | Cell voltages (pack 2) | 200ms |
| 0x35F | Orion BMS | Pack temperatures | 1000ms |
| **New Peripheral Messages** |
| **0x710** | GPS Module | **Latitude, Longitude** | **1000ms** |
| **0x711** | GPS Module | **Speed, Heading, Altitude** | **1000ms** |
| **0x720** | Temp Module | **Temps 1-4 (motor/inverter)** | **2000ms** |
| **0x721** | Temp Module | **Temps 5-8 (coolant/battery)** | **2000ms** |
| **0x730** | RS485 Bridge | **External sensor data** | **1000ms** |

### CANReceiver Updates

**Add to `can_receiver.h`**:
```cpp
// GPS data (0x710, 0x711)
int32_t getLatitude() const { return latitude_.load(); }      // deg * 1e7
int32_t getLongitude() const { return longitude_.load(); }    // deg * 1e7
uint16_t getGPSSpeed() const { return gps_speed_.load(); }    // km/h * 100
uint16_t getHeading() const { return heading_.load(); }       // deg * 10
int16_t getAltitude() const { return altitude_.load(); }      // meters
uint8_t getSatellites() const { return satellites_.load(); }

// Temperature array (0x720, 0x721)
int16_t getMotorStatorTemp() const { return motor_stator_temp_.load(); }  // °C * 10
int16_t getMotorCoolantInTemp() const { return motor_coolant_in_temp_.load(); }
int16_t getInverterIGBTTemp() const { return inverter_igbt_temp_.load(); }
int16_t getBatteryCoolantTemp() const { return battery_coolant_temp_.load(); }
```

### Dashboard UI Updates

**Add to `dashboard_ui.cpp`**:
```cpp
void DashboardUI::update(const CANReceiver& can) {
    // Existing updates...

    // Update GPS speed (cross-check with vehicle speed 0x1F2)
    uint16_t gps_speed_raw = can.getGPSSpeed();
    float gps_speed_kmh = gps_speed_raw / 100.0f;

    // Update temperature bars (additional sensors)
    int16_t motor_stator = can.getMotorStatorTemp() / 10;  // °C
    int16_t battery_coolant = can.getBatteryCoolantTemp() / 10;

    if (motor_stator_label) {
        lv_label_set_text_fmt(motor_stator_label, "Stator: %d°C", motor_stator);
    }
}
```

---

## Development Roadmap

### Phase 1: GPS Module (Week 1-2)
1. **Hardware**: Acquire T-CAN485 + NEO-M8N module
2. **Software**:
   - Create `modules/gps-module-tcan485/` directory
   - Implement GPS UART parsing with TinyGPSPlus
   - Add CAN message pack functions to `LeafCANMessages.cpp`
   - Test CAN transmission with `candump can0`
3. **Integration**: Update `CANReceiver` to parse 0x710/0x711
4. **Validation**: Cross-check GPS speed vs vehicle speed (0x1F2)

### Phase 2: Temperature Module (Week 3-4)
1. **Hardware**: Install 8x DS18B20 sensors on motor/inverter
2. **Software**:
   - Create `modules/temp-module-tcan485/` directory
   - Implement 1-Wire sensor discovery and reading
   - Add CAN message pack functions for 0x720/0x721
3. **Integration**: Add temperature displays to dashboard UI
4. **Validation**: Verify temps match existing Leaf inverter temp (0x1D4)

### Phase 3: RS485 Bridge (Week 5-6)
1. **Hardware**: Set up RS485 test bench with Modbus sensor
2. **Software**:
   - Create `modules/rs485-bridge-tcan485/` directory
   - Implement Modbus RTU polling
   - Add CAN message pack for 0x730
3. **Integration**: Add RS485 data to dashboard
4. **Validation**: Compare RS485 sensor vs DS18B20 accuracy

### Phase 4: System Integration (Week 7-8)
1. **Wiring Harness**: Build final power and CAN distribution
2. **Physical Installation**: Mount modules in vehicle
3. **Testing**:
   - CAN bus load analysis (verify <60% utilization)
   - Error handling (sensor disconnects, CAN errors)
   - Dashboard UI stress test
4. **Documentation**: Update CLAUDE.md with new modules

---

## Testing Checklist

### GPS Module
- [ ] GPS acquires satellite lock within 2 minutes
- [ ] CAN messages 0x710/0x711 transmit at 1Hz
- [ ] Latitude/longitude match phone GPS (±5 meters)
- [ ] Speed matches vehicle speed sensor (±2 km/h)
- [ ] Dashboard displays GPS data without lag

### Temperature Module
- [ ] All 8 DS18B20 sensors detected on boot
- [ ] Temperature readings stable (±1°C variance)
- [ ] CAN messages 0x720/0x721 transmit at 0.5Hz
- [ ] Dashboard temperature bars update smoothly
- [ ] Sensor disconnect detected and flagged

### RS485 Bridge
- [ ] Modbus communication established at 9600 baud
- [ ] CAN message 0x730 transmits at 1Hz
- [ ] RS485 sensor data appears on dashboard
- [ ] TX enable (IO9) switches correctly
- [ ] No CAN bus errors from RS485 module

### System Integration
- [ ] All modules coexist on CAN bus without errors
- [ ] CAN bus utilization <60% (measure with `canbusload can0`)
- [ ] No message collisions or arbitration losses
- [ ] Dashboard receives all data sources simultaneously
- [ ] Power consumption <500mA per module at 12V

---

## Bill of Materials

| Item | Quantity | Unit Cost | Notes |
|------|----------|-----------|-------|
| LilyGO T-CAN485 | 3 | $15 | AliExpress/official store |
| NEO-M8N GPS module | 1 | $12 | Includes antenna |
| DS18B20 waterproof sensors | 8 | $2 | 1-meter cable |
| 4.7kΩ resistor | 1 | $0.10 | 1-Wire pull-up |
| 120Ω resistor | 2 | $0.10 | CAN termination |
| Twisted pair wire (CAN) | 5m | $0.50/m | 24AWG shielded |
| Power wire (12V) | 5m | $0.30/m | 18AWG |
| Inline fuse holders | 3 | $1 | 5A mini blade fuses |
| Heat shrink tubing | 1 set | $5 | Assorted sizes |
| **Total** | | **~$90** | Plus shipping |

---

## Appendix: Example PlatformIO Structure

```
modules/gps-module-tcan485/
├── platformio.ini
├── src/
│   └── main.cpp
└── README.md

modules/temp-module-tcan485/
├── platformio.ini
├── src/
│   └── main.cpp
└── README.md

modules/rs485-bridge-tcan485/
├── platformio.ini
├── src/
│   └── main.cpp
└── README.md
```

**Common platformio.ini template**:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_extra_dirs = ../../lib

lib_deps =
    ../../lib/LeafCANBus
    # Module-specific dependencies here
```

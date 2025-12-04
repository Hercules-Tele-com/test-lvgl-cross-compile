# Battery Configuration Guide

This document explains how to configure the Nissan Leaf CAN Network system to work with different battery types.

## Supported Battery Types

### 1. Nissan Leaf Battery (Default)
- **CAN Bus Speed**: 500 kbps
- **CAN IDs**:
  - `0x1DB`: Battery state of charge
  - `0x1DC`: Battery temperature
  - `0x1F2`: Inverter telemetry
  - `0x1DA`: Motor RPM
  - `0x390`: Charger status
- **Configuration Flag**: `NISSAN_LEAF_BATTERY`

### 2. EMBOO Battery (Orion BMS / ENNOID-style)
- **CAN Bus Speed**: 250 kbps
- **CAN IDs**:
  - `0x6B0`: Pack voltage, current, SOC
  - `0x6B1`: Pack statistics (min/max cell voltages and temps)
  - `0x6B2`: Status and error flags
  - `0x6B3`: Individual cell voltages (pairs)
  - `0x6B4`: Temperature data
  - `0x351`: Pack summary
  - `0x355`: Pack data 1 (SOC, health)
  - `0x356`: Pack data 2 (voltage, current, temp)
  - `0x35A`: Pack data 3 (additional data)
- **Configuration Flag**: `EMBOO_BATTERY`

## Configuring the UI Dashboard (Raspberry Pi / Windows)

### Using CMake

When building the UI dashboard, you can specify the battery type using the `BATTERY_TYPE` CMake variable:

#### For Nissan Leaf Battery (Default):
```bash
cd ui-dashboard
mkdir build && cd build
cmake .. -DBATTERY_TYPE=NISSAN_LEAF
make
```

#### For EMBOO Battery:
```bash
cd ui-dashboard
mkdir build && cd build
cmake .. -DBATTERY_TYPE=EMBOO
make
```

If you don't specify `BATTERY_TYPE`, the system defaults to `NISSAN_LEAF`.

## Configuring ESP32 Modules

### Using PlatformIO

ESP32 modules use PlatformIO for building. The battery type is configured in the `platformio.ini` file:

#### Example: GPS Module

Edit `modules/gps-module/platformio.ini`:

**For Nissan Leaf Battery (Default):**
```ini
build_flags =
    -DCORE_DEBUG_LEVEL=3
    -DNISSAN_LEAF_BATTERY    ; Nissan Leaf battery (500 kbps, default)
    ; -DEMBOO_BATTERY        ; EMBOO/Orion BMS battery (250 kbps)
```

**For EMBOO Battery:**
```ini
build_flags =
    -DCORE_DEBUG_LEVEL=3
    ; -DNISSAN_LEAF_BATTERY  ; Nissan Leaf battery (500 kbps, default)
    -DEMBOO_BATTERY          ; EMBOO/Orion BMS battery (250 kbps)
```

Then build the module:
```bash
cd modules/gps-module
pio run
```

## CAN Bus Speed Configuration

**Important**: Make sure your CAN bus is configured to match the battery type:

### Nissan Leaf Battery
- Set CAN bus speed to **500 kbps**
- On Raspberry Pi with SocketCAN:
  ```bash
  sudo ip link set can0 type can bitrate 500000
  sudo ip link set can0 up
  ```

### EMBOO Battery
- Set CAN bus speed to **250 kbps**
- On Raspberry Pi with SocketCAN:
  ```bash
  sudo ip link set can0 type can bitrate 250000
  sudo ip link set can0 up
  ```

### ESP32 Modules
The CAN bus speed is configured in the `LeafCANBus` library initialization:

```cpp
#include <LeafCANBus.h>

LeafCANBus canBus;

void setup() {
    // The library automatically uses the correct bitrate based on BATTERY_TYPE
    canBus.begin();  // Uses CAN_BITRATE from LeafCANMessages.h
}
```

The `CAN_BITRATE` constant is automatically set based on the battery type configuration:
- `NISSAN_LEAF_BATTERY`: 500000 bps
- `EMBOO_BATTERY`: 250000 bps

## Data Structures

### EMBOO Battery Specific Structures

When `EMBOO_BATTERY` is defined, the following data structures are available:

#### Pack Status (0x6B0)
```cpp
typedef struct {
    float pack_voltage;     // Pack voltage (V)
    float pack_current;     // Pack current (A, signed: + = charging, - = discharging)
    float pack_soc;         // State of charge (%)
    float pack_amphours;    // Amp hours (Ah)
} EmbooPackStatus;
```

#### Pack Statistics (0x6B1)
```cpp
typedef struct {
    uint16_t relay_state;   // Relay state flags
    float high_temp;        // High temperature (°C)
    float input_voltage;    // Input supply voltage (V)
    float summed_voltage;   // Pack summed voltage (V)
} EmbooPackStats;
```

#### Individual Cell Voltages (0x6B3)
```cpp
typedef struct {
    uint8_t cell_id;        // Cell ID (0-99)
    float cell_voltage;     // Cell voltage (V)
    float cell_resistance;  // Cell resistance (mOhm)
    bool cell_balancing;    // Cell balancing active
    float cell_open_voltage; // Cell open circuit voltage (V)
} EmbooCellVoltage;
```

#### Temperature Data (0x6B4)
```cpp
typedef struct {
    float high_temp;        // High temperature (°C)
    float low_temp;         // Low temperature (°C)
    uint8_t rolling_counter; // Rolling counter
} EmbooTemperatures;
```

## Pack/Unpack Functions

For EMBOO battery, use the following functions to encode/decode CAN messages:

```cpp
// Pack status (0x6B0)
void unpack_emboo_pack_status(const uint8_t* data, uint8_t len, void* state);
void pack_emboo_pack_status(const void* state, uint8_t* data, uint8_t* len);

// Pack statistics (0x6B1)
void unpack_emboo_pack_stats(const uint8_t* data, uint8_t len, void* state);
void pack_emboo_pack_stats(const void* state, uint8_t* data, uint8_t* len);

// Cell voltages (0x6B3)
void unpack_emboo_cell_voltage(const uint8_t* data, uint8_t len, void* state);
void pack_emboo_cell_voltage(const void* state, uint8_t* data, uint8_t* len);

// Temperatures (0x6B4)
void unpack_emboo_temperatures(const uint8_t* data, uint8_t len, void* state);
void pack_emboo_temperatures(const void* state, uint8_t* data, uint8_t* len);
```

## Example Usage

### Receiving EMBOO Battery Data (ESP32)

```cpp
#include <LeafCANBus.h>

#ifdef EMBOO_BATTERY
LeafCANBus canBus;
EmbooPackStatus packStatus;

void setup() {
    Serial.begin(115200);

    // Initialize CAN bus (automatically uses 250 kbps for EMBOO)
    canBus.begin();

    // Subscribe to pack status messages
    canBus.subscribe(CAN_ID_PACK_STATUS, unpack_emboo_pack_status, &packStatus);
}

void loop() {
    canBus.process();

    // Display pack status
    Serial.printf("Voltage: %.1fV, Current: %.1fA, SOC: %.1f%%\n",
                  packStatus.pack_voltage,
                  packStatus.pack_current,
                  packStatus.pack_soc);

    delay(1000);
}
#endif
```

### Monitoring on Raspberry Pi

The UI dashboard automatically decodes and displays EMBOO battery messages when built with `-DBATTERY_TYPE=EMBOO`. The following data is displayed:

- Pack voltage, current, SOC (from 0x6B0)
- Cell voltages (min/max from 0x6B3)
- Temperatures (min/max from 0x6B4)
- Pack summary data (from 0x351, 0x355, 0x356)

## CAN Message Formats

### EMBOO Battery Message Details

#### 0x6B0: Pack Status
| Byte | Description | Scale |
|------|-------------|-------|
| 0-1 | Pack Current (signed, big-endian) | 0.1 A |
| 2-3 | Pack Voltage (big-endian) | 0.1 V |
| 4-5 | Amp Hours (big-endian) | 0.1 Ah |
| 6 | State of Charge | 0.5 % |
| 7 | CRC Checksum | - |

#### 0x6B1: Pack Statistics
| Byte | Description | Scale |
|------|-------------|-------|
| 0-1 | Relay State (big-endian) | - |
| 2 | High Temperature | 1.0 °C |
| 3-4 | Input Supply Voltage (big-endian) | 0.1 V |
| 5-6 | Pack Summed Voltage (big-endian) | 0.01 V |
| 7 | CRC Checksum | - |

#### 0x6B3: Individual Cell Voltages
| Byte | Description | Scale |
|------|-------------|-------|
| 0 | Cell ID (0-99) | - |
| 1-2 | Cell Voltage (big-endian) | 0.0001 V |
| 3-4 | Cell Resistance (15 bits) + Balancing (1 bit) | 0.01 mOhm |
| 5-6 | Cell Open Voltage (big-endian) | 0.0001 V |
| 7 | Checksum | - |

#### 0x6B4: Temperature Data
| Byte | Description | Scale |
|------|-------------|-------|
| 0 | Flags | - |
| 1 | Blank | - |
| 2 | High Temperature | 1.0 °C |
| 3 | Low Temperature | 1.0 °C |
| 4 | Rolling Counter | - |
| 5-7 | Blank | - |

## Troubleshooting

### Issue: CAN messages not being received

**Check CAN bus speed:**
```bash
# On Raspberry Pi
ip -details link show can0
```

Make sure the bitrate matches your battery type:
- Nissan Leaf: 500000
- EMBOO: 250000

### Issue: Wrong data being displayed

**Verify battery type configuration:**

For UI dashboard:
```bash
cd ui-dashboard/build
cmake .. --trace-expand 2>&1 | grep BATTERY_TYPE
```

For ESP32 modules:
```bash
cd modules/gps-module
pio run -v 2>&1 | grep "BATTERY"
```

### Issue: Compilation errors

Make sure you've cleaned your build directory when switching battery types:

```bash
# For UI dashboard
cd ui-dashboard
rm -rf build
mkdir build && cd build
cmake .. -DBATTERY_TYPE=EMBOO
make

# For ESP32 modules
cd modules/gps-module
pio run --target clean
pio run
```

## References

- [Orion BMS DBC File](https://github.com/collin80/SavvyCAN/blob/master/examples/DBC/Orion_BMS.dbc)
- [ENNOID BMS Documentation](https://github.com/EnnoidMe/ENNOID-BMS)
- [Nissan Leaf CAN Bus Information](https://github.com/dalathegreat/Nissan-LEAF-Battery-Upgrade)

## Contributing

If you add support for additional battery types, please update this document with:
1. CAN bus speed
2. CAN ID mappings
3. Data structure definitions
4. Pack/unpack function signatures
5. Example usage code

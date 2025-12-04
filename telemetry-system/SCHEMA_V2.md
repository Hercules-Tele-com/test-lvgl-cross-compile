# InfluxDB Schema V2 - Device Family Organization

## Overview

The telemetry system now uses a device-family-based schema that organizes data by device type (Battery, Motor, Inverter, GPS, Vehicle) rather than by individual metrics. This makes it easier to query, filter by vehicle/device, and maintain consistency.

## Schema Structure

### Measurements (Device Families)

The new schema uses **5 device family measurements** instead of 16+ individual metric measurements:

1. **Battery** - All battery-related data (SOC, voltage, current, temperatures, cells, faults)
2. **Motor** - All motor-related data (RPM, torque, voltage, current, temperatures)
3. **Inverter** - Inverter/controller data (voltage, current, temperatures)
4. **GPS** - GPS position and velocity data
5. **Vehicle** - Vehicle-level data (speed, body sensors, auxiliary voltages)

### Tags (Metadata)

Every measurement includes these tags for filtering and identification:

- **serial_number**: Unique identifier format: `{hostname}-{DeviceFamily}-{DeviceType}`
  - Example: `LeafCruiser-Battery-EMBOO`
  - Example: `LeafCruiser-Motor-ROAM`
  - Example: `LeafCruiser-GPS-ESP32`
- **source**: Data source (emboo_bms, roam_motor, leaf_ecu, esp32_gps, etc.)
- **device_type**: Specific device model (Orion_BMS, RM100, EM57, NEO_6M, etc.)

### Fields (Measurements)

Fields use **consistent naming** across similar devices:

## Battery Measurement

**Tags:**
- serial_number: `{hostname}-Battery-{DeviceType}`
- source: `emboo_bms` | `leaf_ecu`
- device_type: `Orion_BMS` | `Leaf_24kWh`

**Fields:**
```
# State of Charge & Power
soc_percent (int)              # Battery state of charge (%)
gids (int)                     # Nissan Leaf specific: available Wh/80
voltage (float)                # Pack voltage (V)
current (float)                # Pack current (A, positive=discharge)
power_kw (float)               # Pack power (kW)

# Temperatures
temp_max (float)               # Maximum cell temperature (°C)
temp_min (float)               # Minimum cell temperature (°C)
temp_avg (float)               # Average cell temperature (°C)
temp_delta (float)             # Temperature spread (max-min) (°C)
temp_sensor_count (int)        # Number of temperature sensors

# Cell Voltages (EMBOO only)
cell_count (int)               # Number of cells
cell_voltage_min (float)       # Minimum cell voltage (V)
cell_voltage_max (float)       # Maximum cell voltage (V)
cell_voltage_avg (float)       # Average cell voltage (V)
cell_voltage_delta (float)     # Cell voltage spread (V)
cell_000 ... cell_143 (float)  # Individual cell voltages (V)

# Charging (Leaf only)
charging_flag (int)            # 1=charging, 0=not charging
charge_current (float)         # Charging current (A)
charge_voltage (float)         # Charging voltage (V)
charge_time_remaining_min (int)# Time to full charge (minutes)
charge_power_kw (float)        # Charging power (kW)

# Faults (EMBOO only)
fault_count (int)              # Number of active faults
fault_status_raw (int)         # Raw fault bitmask
fault_{fault_name} (int)       # Individual fault flags (1=active)
fault_total_{fault_name} (int) # Lifetime fault occurrence count
```

**Fault Names:**
- discharge_overcurrent
- charge_overcurrent
- cell_overvoltage
- cell_undervoltage
- over_temperature
- under_temperature
- communication_fault
- internal_fault
- cell_imbalance

## Motor Measurement

**Tags:**
- serial_number: `{hostname}-Motor-{DeviceType}`
- source: `roam_motor` | `leaf_ecu`
- device_type: `RM100` | `EM57`
- direction: `forward` | `reverse` | `stopped` (Leaf EM57 only)

**Fields:**
```
# Speed & Position
rpm (int)                      # Motor RPM (negative=reverse)
position_angle (int)           # Motor position angle (degrees) [ROAM]
electrical_freq (int)          # Electrical frequency (Hz) [ROAM]

# Torque
torque_request (int)           # Requested torque (Nm) [ROAM]
torque_actual (int)            # Actual torque (Nm) [ROAM]

# Electrical
voltage_dc_bus (int)           # DC bus voltage (0.1V units) [ROAM]
voltage_output (int)           # Output voltage (0.1V units) [ROAM]
current_phase_a (int)          # Phase A current (A) [ROAM]
current_phase_b (int)          # Phase B current (A) [ROAM]
current_phase_c (int)          # Phase C current (A) [ROAM]
current_dc_bus (int)           # DC bus current (A) [ROAM]

# Temperatures
temp_stator (float)            # Stator temperature (°C) [ROAM]
```

## Inverter Measurement

**Tags:**
- serial_number: `{hostname}-Inverter-{DeviceType}`
- source: `leaf_ecu` | `roam_motor`
- device_type: `EM57` | `RM100`

**Fields:**
```
voltage (float)                # DC bus voltage (V)
current (float)                # DC current (A)
temp_inverter (float)          # Inverter/control board temperature (°C)
temp_motor (float)             # Motor temperature (°C) [Leaf only]
status_flags (int)             # Status flags [Leaf only]
power_kw (float)               # Power (kW) [Leaf only]
```

## GPS Measurement

**Tags:**
- serial_number: `{hostname}-GPS-{DeviceType}`
- source: `esp32_gps`
- device_type: `NEO_6M`

**Fields:**
```
latitude (float)               # Latitude (decimal degrees)
longitude (float)              # Longitude (decimal degrees)
speed_kmh (float)              # Ground speed (km/h)
heading (float)                # Course over ground (degrees)
pdop (float)                   # Position dilution of precision
```

## Vehicle Measurement

**Tags:**
- serial_number: `{hostname}-Vehicle-{DeviceType}`
- source: `leaf_ecu` | `esp32_body`
- device_type: `Leaf_ZE0` | `BodySensors`

**Fields:**
```
# Speed
speed_kmh (float)              # Vehicle speed (km/h)
speed_mph (float)              # Vehicle speed (mph)

# Body Sensors
body_temp1 (float)             # Body temperature sensor 1 (°C)
body_temp2 (float)             # Body temperature sensor 2 (°C)
body_temp3 (float)             # Body temperature sensor 3 (°C)
body_temp4 (float)             # Body temperature sensor 4 (°C)

# Auxiliary Power
aux_voltage_12v (float)        # 12V auxiliary voltage (V)
aux_voltage_5v (float)         # 5V auxiliary voltage (V)
aux_current_12v (float)        # 12V auxiliary current (A)
```

## Migration from V1 Schema

### V1 → V2 Measurement Mapping

| V1 Measurement      | V2 Measurement | V2 Tags                                    |
|---------------------|----------------|---------------------------------------------|
| battery_soc         | Battery        | source=leaf_ecu OR emboo_bms               |
| battery_temp        | Battery        | source=leaf_ecu OR emboo_bms               |
| battery_cells       | Battery        | source=emboo_bms                           |
| battery_faults      | Battery        | source=emboo_bms                           |
| motor_rpm           | Motor          | source=leaf_ecu OR roam_motor              |
| motor_torque        | Motor          | source=roam_motor                          |
| motor_voltage       | Motor          | source=roam_motor                          |
| motor_current       | Motor          | source=roam_motor                          |
| motor_temp          | Motor          | source=roam_motor                          |
| inverter            | Inverter       | source=leaf_ecu OR roam_motor              |
| vehicle_speed       | Vehicle        | source=leaf_ecu                            |
| charger             | Battery        | source=leaf_ecu, charging=yes tag          |
| gps_position        | GPS            | source=esp32_gps                           |
| gps_velocity        | GPS            | source=esp32_gps                           |
| body_temp           | Vehicle        | source=esp32_body                          |
| body_voltage        | Vehicle        | source=esp32_body                          |

### Field Name Changes

**Battery:**
- `pack_voltage` → `voltage`
- `pack_current` → `current`
- `pack_power_kw` → `power_kw`
- `sensor_count` → `temp_sensor_count`
- `voltage_min` → `cell_voltage_min`
- `voltage_max` → `cell_voltage_max`
- `voltage_avg` → `cell_voltage_avg`
- `voltage_delta` → `cell_voltage_delta`
- `current` → `charge_current` (when charging)
- `voltage` → `charge_voltage` (when charging)
- `time_remaining_min` → `charge_time_remaining_min`
- `status_raw` → `fault_status_raw`
- `total_{fault}` → `fault_total_{fault}`

**Motor:**
- `motor_angle` → `position_angle`
- `dc_bus_voltage` → `voltage_dc_bus`
- `output_voltage` → `voltage_output`
- `phase_a_current` → `current_phase_a`
- `phase_b_current` → `current_phase_b`
- `phase_c_current` → `current_phase_c`
- `dc_bus_current` → `current_dc_bus`
- `temp_motor` → `temp_stator` (ROAM only)

**Vehicle:**
- `temp1` → `body_temp1`
- `temp2` → `body_temp2`
- `temp3` → `body_temp3`
- `temp4` → `body_temp4`
- `voltage_12v` → `aux_voltage_12v`
- `voltage_5v` → `aux_voltage_5v`
- `current_12v` → `aux_current_12v`

## Migration Steps

1. **Pull latest code:**
   ```bash
   cd ~/Projects/test-lvgl-cross-compile
   git pull
   ```

2. **Stop services:**
   ```bash
   sudo systemctl stop telemetry-logger-unified.service cloud-sync.service web-dashboard.service
   ```

3. **Clear old data (OPTIONAL but recommended):**
   ```bash
   cd telemetry-system
   ./scripts/clear-influxdb-data.sh
   ```

   **WARNING**: This deletes ALL telemetry data! Skip if you want to keep historical data (both schemas will coexist).

4. **Update services:**
   ```bash
   sudo ./scripts/update-services.sh
   ```

5. **Restart services:**
   ```bash
   sudo systemctl start telemetry-logger-unified.service cloud-sync.service web-dashboard.service
   ```

6. **Verify data:**
   ```bash
   # Check service logs
   sudo journalctl -u telemetry-logger-unified.service -f

   # Query new schema in InfluxDB
   influx query 'from(bucket:"leaf-data")
     |> range(start: -5m)
     |> filter(fn: (r) => r._measurement == "Battery")
     |> limit(n: 5)'
   ```

## Example Queries

### Get current battery SOC for specific vehicle
```flux
from(bucket: "leaf-data")
  |> range(start: -1m)
  |> filter(fn: (r) => r._measurement == "Battery")
  |> filter(fn: (r) => r.serial_number =~ /^LeafCruiser-/)
  |> filter(fn: (r) => r._field == "soc_percent")
  |> last()
```

### Get all data from EMBOO battery
```flux
from(bucket: "leaf-data")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "Battery")
  |> filter(fn: (r) => r.source == "emboo_bms")
```

### Compare motor RPM from different sources
```flux
from(bucket: "leaf-data")
  |> range(start: -10m)
  |> filter(fn: (r) => r._measurement == "Motor")
  |> filter(fn: (r) => r._field == "rpm")
  |> group(columns: ["source"])
```

### Get all active battery faults
```flux
from(bucket: "leaf-data")
  |> range(start: -1m)
  |> filter(fn: (r) => r._measurement == "Battery")
  |> filter(fn: (r) => r._field =~ /^fault_.*/ and r._field !~ /^fault_total_/)
  |> filter(fn: (r) => r._value == 1)
  |> last()
```

### Get hostname from any measurement
```flux
from(bucket: "leaf-data")
  |> range(start: -1m)
  |> limit(n: 1)
  |> map(fn: (r) => ({ r with hostname: strings.split(v: r.serial_number, t: "-")[0] }))
```

## Benefits of New Schema

1. **Hostname Visibility**: Every measurement includes the vehicle hostname in `serial_number` tag
2. **Easier Filtering**: Filter by device family, source, or specific device type
3. **Consistent Naming**: Similar fields have the same names across devices
4. **Scalability**: Easy to add new devices (just new serial_number values)
5. **Simpler Queries**: Fewer measurements to remember (5 vs 16+)
6. **Better Organization**: Related data grouped logically by device type
7. **Web Dashboard**: Can prominently display hostname at top of UI

## Hostname Display in Web UI

The web dashboard can now extract and display the hostname prominently:

```python
# Python example for web dashboard
def get_hostname_from_serial(serial_number):
    """Extract hostname from serial_number tag"""
    return serial_number.split('-')[0]  # Returns "LeafCruiser"
```

Display in UI header:
```
╔═══════════════════════════════════════╗
║        LeafCruiser Dashboard          ║
║    EMBOO Battery | ROAM Motor        ║
╚═══════════════════════════════════════╝
```

# Nissan Leaf BMS to Victron Protocol Converter

ESP32 module that converts Nissan Leaf battery CAN messages to Victron/Pylontech BMS protocol, enabling Leaf batteries to work with Victron solar inverters, chargers, and ESS (Energy Storage System) equipment.

## Purpose

This module acts as a protocol translator, reading native Nissan Leaf battery data from the CAN bus and retransmitting it in the standard Victron BMS format. This allows you to use Nissan Leaf battery packs with Victron equipment for:

- Solar energy storage systems
- Off-grid power systems
- RV/Marine installations
- Home backup power
- DIY Powerwall projects

## Hardware Requirements

- ESP32 DevKit or compatible board
- TJA1050 or MCP2515 CAN transceiver
- Connection to vehicle CAN bus (500 kbps)

### Wiring

```
ESP32          TJA1050
GPIO 5 (TX) -> CTX
GPIO 4 (RX) -> CRX
3.3V        -> VCC
GND         -> GND
            -> CAN H (to bus)
            -> CAN L (to bus)
```

## CAN Protocol

### Input (Nissan Leaf Messages)

| CAN ID | Description | Rate |
|--------|-------------|------|
| 0x1DB  | Battery SOC, voltage, current, GIDS | 100ms |
| 0x1DC  | Battery temperatures (min/max/avg) | 500ms |
| 0x390  | Charger status | 1000ms |

### Output (Victron Protocol)

| CAN ID | Description | Rate |
|--------|-------------|------|
| 0x351  | Pack voltage, current, SOC, SOH | 1000ms |
| 0x355  | Battery temperatures (min/max/avg) | 1000ms |
| 0x356  | Charge/discharge current limits | 1000ms |
| 0x35E  | Alarms and warnings | 1000ms |
| 0x35F  | Manufacturer info | 5000ms |

## Features

- **Automatic Current Limiting**: Dynamically adjusts charge/discharge limits based on:
  - State of Charge (SOC)
  - Battery temperature
  - Battery health (SOH)

- **Temperature Protection**:
  - Reduces charging at high temps (>45°C)
  - Limits power at low temps (<0°C)
  - Monitors min/max/avg temperatures

- **SOC-Based Derating**:
  - Reduces charge current above 90% SOC
  - Reduces discharge current below 20% SOC

- **Safety Alarms**:
  - Low SOC alarm (<10%)
  - High temperature alarm (>50°C)
  - Low temperature alarm (<-10°C)

- **Status Monitoring**:
  - Serial output for debugging
  - Periodic status reports every 5 seconds
  - Watchdog for missing CAN data

## Building and Uploading

```bash
cd modules/bms-victron

# Build
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

## Serial Output

The module provides detailed serial output for monitoring:

```
=== Nissan Leaf BMS to Victron Protocol ===
Version 1.0
CAN Bus: 500 kbps, GPIO 5 (TX), GPIO 4 (RX)

CAN bus initialized successfully
Subscribed to Leaf battery messages:
  0x1DB - Battery SOC
  0x1DC - Battery Temperature
  0x390 - Charger Status

Publishing Victron protocol messages:
  0x351 - Pack voltage, current, SOC, SOH
  0x355 - Battery temperatures
  0x356 - Charge/discharge limits
  0x35E - Alarms and warnings
  0x35F - Manufacturer info

Ready to convert Leaf → Victron protocol...
==========================================

[0x1DB] SOC: 85%, Voltage: 360.5V, Current: 12.3A, GIDS: 239
[0x1DC] Temps: Min 22°C, Max 28°C, Avg 25°C
[0x390] Charging: NO

--- BMS Status ---
Pack: 360.5V, 12.3A
SOC: 85%, SOH: 85%
Temps: 22°C / 25°C / 28°C (min/avg/max)
Charging: NO
Last update: 150 ms ago
------------------
```

## Victron Configuration

### MultiPlus/Quattro/GX Devices

When using this BMS with Victron equipment:

1. **Battery Settings** (in VictronConnect or VEConfig):
   - Battery type: Lithium-ion
   - BMS type: CAN-bus BMS (Pylontech US2000)
   - Follow battery (CAN-bus BMS): Yes

2. **CAN Bus Settings**:
   - Baudrate: 500 kbps
   - Connect CAN H/L to Victron CAN bus

3. **Battery Capacity**:
   - Configure based on your Leaf pack:
     - 24 kWh pack: ~20-22 kWh usable
     - 30 kWh pack: ~25-27 kWh usable
     - 40 kWh pack: ~36-38 kWh usable
     - 62 kWh pack: ~56-58 kWh usable

4. **Voltage Limits**:
   - Max charge voltage: 404V (for 96S pack)
   - Min discharge voltage: 288V (for 96S pack)
   - Adjust based on your pack configuration

## Current Limits

Default limits (automatically adjusted based on conditions):

- **Charge**: 50A (25 kW @ 360V)
- **Discharge**: 150A (54 kW @ 360V)

These are conservative values. Adjust in `main.cpp` based on your:
- Battery capacity and age
- Cooling capability
- Inverter/charger ratings
- Safety margins

## Safety Notes

⚠️ **IMPORTANT SAFETY INFORMATION** ⚠️

- High voltage battery systems can be dangerous
- Ensure proper fusing and circuit protection
- Monitor battery temperatures continuously
- Use appropriate contactor/relay for disconnection
- Follow local electrical codes
- Consider professional installation for grid-tied systems
- Test thoroughly before deploying in production

## Customization

### Adjust Current Limits

Edit `main.cpp` in `packVictron0x356()`:

```cpp
float charge_limit = 50.0f;      // Base charge current (A)
float discharge_limit = 150.0f;  // Base discharge current (A)
```

### Adjust Voltage Limits

Edit `main.cpp` in `packVictron0x356()`:

```cpp
uint16_t vchg = 4040;  // 404.0V max charge
uint16_t vdis = 2880;  // 288.0V min discharge
```

### Change Temperature Thresholds

Edit temperature protection logic in `packVictron0x356()`:

```cpp
if (bmsState.temp_max > 45 || bmsState.temp_min < 0) {
    // Temperature derating logic
}
```

## Troubleshooting

### No Leaf Data Received

- Check CAN wiring and termination
- Verify CAN bus speed (500 kbps)
- Ensure vehicle is powered on
- Monitor serial output for errors

### Victron Not Detecting BMS

- Verify CAN bus connection to Victron
- Check baudrate matches (500 kbps)
- Ensure proper CAN termination
- Check Victron settings for CAN BMS type

### Incorrect Values

- Verify CAN message formats match your Leaf model year
- Check byte order and scaling factors
- Monitor raw CAN data with analyzer
- Adjust parsers in `main.cpp` if needed

## Compatibility

### Tested Nissan Leaf Models

- 2011-2017 (24/30 kWh packs)
- 2018-2020 (40 kWh packs)
- 2019+ (62 kWh packs)

CAN message formats may vary by model year. Adjust parsers as needed.

### Victron Equipment

Compatible with all Victron equipment supporting CAN-bus BMS:
- MultiPlus/MultiPlus-II
- Quattro
- GX devices (Cerbo GX, Venus GX, etc.)
- MPPT Solar Chargers (with GX device)

## Resources

- [Victron CAN-bus BMS Protocol](https://www.victronenergy.com/live/battery_compatibility:can-bus_bms)
- [Nissan Leaf CAN Database](https://github.com/dalathegreat/Nissan-LEAF-Battery-Upgrade)
- [LeafSpy for Android](https://play.google.com/store/apps/details?id=com.Turbo3.Leaf_Spy_Pro) - Monitor Leaf battery data

## License

See main repository LICENSE file.

## Support

For issues and questions:
- Check serial monitor output for errors
- Verify CAN connections and termination
- Consult Victron documentation
- See main repository CLAUDE.md for architecture

---

**Disclaimer**: This project involves high-voltage battery systems. Use at your own risk. The authors are not responsible for any damage, injury, or loss resulting from the use of this software or hardware.

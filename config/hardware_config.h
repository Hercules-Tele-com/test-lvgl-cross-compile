#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

// ============================================================================
// CENTRALIZED HARDWARE CONFIGURATION
// ============================================================================
// This file defines all hardware configurations for the Nissan Leaf CAN Network.
// Configure your vehicle's hardware by uncommenting the appropriate options.
// ============================================================================

// ============================================================================
// BATTERY CONFIGURATION
// ============================================================================
// Choose ONE battery type:

// #define NISSAN_LEAF_BATTERY      // Original Nissan Leaf battery (500 kbps, IDs 0x1DB, 0x1DC, etc.)
#define EMBOO_BATTERY               // EMBOO/Orion BMS battery (250 kbps, IDs 0x6B0-0x6B4, 0x351, etc.)

// ============================================================================
// MOTOR/INVERTER CONFIGURATION
// ============================================================================
// Choose ONE motor controller type:

#define NISSAN_MOTOR                // Nissan Leaf EM57 motor with inverter (CAN IDs 0x1DA, 0x1F2, etc.)
// #define ROAM_MOTOR               // ROAM/RM100 motor controller (CAN IDs 0x0A0-0x0A7, 0x0AC)

// ============================================================================
// GPS CONFIGURATION
// ============================================================================
// Enable GPS features:

#define GPS_ENABLED                 // Enable GPS module support
// #define FAKE_GPS                 // Use motor RPM to calculate speed instead of real GPS

// ============================================================================
// CAN INTERFACE CONFIGURATION
// ============================================================================
// Number of CAN interfaces:

#define DUAL_CAN_INTERFACE          // Enable dual CAN interface support (can0 and can1)

// CAN bitrate is automatically set based on battery type:
// - NISSAN_LEAF_BATTERY: 500 kbps
// - EMBOO_BATTERY: 250 kbps

// ============================================================================
// TELEMETRY CONFIGURATION
// ============================================================================
// Enable telemetry features:

#define TELEMETRY_ENABLED           // Enable InfluxDB telemetry logging
#define CLOUD_SYNC_ENABLED          // Enable cloud synchronization
#define WEB_DASHBOARD_ENABLED       // Enable web dashboard

// ============================================================================
// DEBUG CONFIGURATION
// ============================================================================
// Enable debug output:

// #define DEBUG_CAN                // Enable CAN bus debug output
// #define DEBUG_GPS                // Enable GPS debug output
// #define DEBUG_TELEMETRY          // Enable telemetry debug output

// ============================================================================
// NOTES
// ============================================================================
//
// Battery Types:
// - NISSAN_LEAF_BATTERY: 500 kbps, CAN IDs 0x1DB (SOC), 0x1DC (temp), 0x1F2 (inverter)
// - EMBOO_BATTERY: 250 kbps, CAN IDs 0x6B0-0x6B4 (pack status), 0x351/0x355/0x356 (pack data)
//
// Motor Types:
// - NISSAN_MOTOR: Nissan Leaf EM57 motor with inverter
//   - CAN ID 0x1DA: Motor RPM
//   - CAN ID 0x1F2: Inverter voltage, current, temps
//
// - ROAM_MOTOR: ROAM/RM100 motor controller
//   - CAN ID 0x0A0-0x0A2: Temperature sensors
//   - CAN ID 0x0A5: Motor position/RPM
//   - CAN ID 0x0A6: Current information
//   - CAN ID 0x0A7: Voltage information
//   - CAN ID 0x0AC: Torque information
//
// ============================================================================

#endif // HARDWARE_CONFIG_H

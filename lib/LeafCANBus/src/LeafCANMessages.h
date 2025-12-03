#ifndef LEAF_CAN_MESSAGES_H
#define LEAF_CAN_MESSAGES_H

#include <stdint.h>

// ============================================================================
// BATTERY TYPE CONFIGURATION
// ============================================================================
// Define one of these to select battery type:
// - NISSAN_LEAF_BATTERY: Original Nissan Leaf battery (500 kbps, IDs 0x1DB, 0x1DC, etc.)
// - EMBOO_BATTERY: EMBOO/Orion BMS battery (250 kbps, IDs 0x6B0-0x6B4, 0x351, 0x355, etc.)
//
// Default to Nissan Leaf if nothing is defined
#if !defined(NISSAN_LEAF_BATTERY) && !defined(EMBOO_BATTERY)
  #define NISSAN_LEAF_BATTERY
#endif

// ============================================================================
// CAN ID DEFINITIONS
// ============================================================================

#ifdef NISSAN_LEAF_BATTERY
// Existing Nissan Leaf CAN IDs (500 kbps)
#define CAN_ID_INVERTER_TELEMETRY   0x1F2  // Inverter voltage, current, temps
#define CAN_ID_BATTERY_SOC          0x1DB  // Battery state of charge
#define CAN_ID_BATTERY_TEMP         0x1DC  // Battery temperature
#define CAN_ID_VEHICLE_SPEED        0x1D4  // Vehicle speed
#define CAN_ID_MOTOR_RPM            0x1DA  // Motor RPM
#define CAN_ID_CHARGER_STATUS       0x390  // Charger status and current
#define CAN_BITRATE                 500000 // 500 kbps
#endif

#ifdef EMBOO_BATTERY
// EMBOO/Orion BMS CAN IDs (250 kbps)
#define CAN_ID_PACK_STATUS          0x6B0  // Pack voltage, current, SOC
#define CAN_ID_PACK_STATS           0x6B1  // Min/max cell voltages and temps
#define CAN_ID_STATUS_FLAGS         0x6B2  // Status and error flags
#define CAN_ID_CELL_VOLTAGES        0x6B3  // Individual cell voltages (pairs)
#define CAN_ID_TEMPERATURES         0x6B4  // Temperature data
#define CAN_ID_PACK_SUMMARY         0x351  // Pack summary
#define CAN_ID_PACK_DATA1           0x355  // Additional pack data
#define CAN_ID_PACK_DATA2           0x356  // Additional pack data
#define CAN_ID_PACK_DATA3           0x35A  // Additional pack data
#define CAN_BITRATE                 250000 // 250 kbps
#endif

// Custom ESP32 module CAN IDs (0x700+ range) - common to both battery types
#define CAN_ID_GPS_POSITION         0x710  // GPS latitude, longitude
#define CAN_ID_GPS_VELOCITY         0x711  // GPS speed, heading
#define CAN_ID_GPS_TIME             0x712  // GPS date/time
#define CAN_ID_BODY_TEMP_SENSORS    0x720  // Multiple temperature sensors
#define CAN_ID_BODY_VOLTAGE         0x721  // Voltage monitoring
#define CAN_ID_UI_DASH_STATUS       0x730  // UI dashboard heartbeat
#define CAN_ID_CUSTOM_GAUGE_CMD     0x740  // Commands to custom gauges

// ============================================================================
// STATE STRUCTURES
// ============================================================================

// Inverter telemetry data
typedef struct {
    float voltage;          // DC bus voltage (V)
    float current;          // DC current (A)
    int16_t temp_inverter;  // Inverter temperature (°C)
    int16_t temp_motor;     // Motor temperature (°C)
    uint8_t status_flags;   // Status bits
} InverterState;

// Battery state of charge
typedef struct {
    uint8_t soc_percent;    // State of charge (0-100%)
    uint16_t gids;          // GIDs (capacity units)
    float pack_voltage;     // Pack voltage (V)
    float pack_current;     // Pack current (A)
} BatterySOCState;

// Battery temperature
typedef struct {
    int8_t temp_max;        // Max cell temp (°C)
    int8_t temp_min;        // Min cell temp (°C)
    int8_t temp_avg;        // Average temp (°C)
    uint8_t sensor_count;   // Number of temp sensors
} BatteryTempState;

// Vehicle speed
typedef struct {
    float speed_kmh;        // Speed in km/h
    float speed_mph;        // Speed in mph
} VehicleSpeedState;

// Motor RPM
typedef struct {
    int16_t rpm;            // Motor revolutions per minute
    uint8_t direction;      // 0=stopped, 1=forward, 2=reverse
} MotorRPMState;

// Charger status
typedef struct {
    uint8_t charging;       // 0=not charging, 1=charging
    float charge_current;   // Charge current (A)
    float charge_voltage;   // Charge voltage (V)
    uint16_t charge_time;   // Time remaining (minutes)
} ChargerState;

// GPS position data
typedef struct {
    double latitude;        // Latitude (decimal degrees)
    double longitude;       // Longitude (decimal degrees)
    float altitude;         // Altitude (meters)
    uint8_t satellites;     // Number of satellites
    uint8_t fix_quality;    // GPS fix quality (0-2)
} GPSPositionState;

// GPS velocity data
typedef struct {
    float speed_kmh;        // Ground speed (km/h)
    float heading;          // Course over ground (degrees)
    float pdop;             // Position dilution of precision
} GPSVelocityState;

// GPS time data
typedef struct {
    uint16_t year;          // Year
    uint8_t month;          // Month (1-12)
    uint8_t day;            // Day (1-31)
    uint8_t hour;           // Hour (0-23)
    uint8_t minute;         // Minute (0-59)
    uint8_t second;         // Second (0-59)
} GPSTimeState;

// Body temperature sensors
typedef struct {
    int16_t temp1;          // Temperature sensor 1 (°C * 10)
    int16_t temp2;          // Temperature sensor 2 (°C * 10)
    int16_t temp3;          // Temperature sensor 3 (°C * 10)
    int16_t temp4;          // Temperature sensor 4 (°C * 10)
} BodyTempState;

// Body voltage monitoring
typedef struct {
    float voltage_12v;      // 12V system voltage
    float voltage_5v;       // 5V system voltage
    float current_12v;      // 12V system current (A)
} BodyVoltageState;

// ============================================================================
// EMBOO BATTERY STATE STRUCTURES (Orion BMS / ENNOID-style)
// ============================================================================

#ifdef EMBOO_BATTERY

// Pack status (0x6B0)
typedef struct {
    float pack_voltage;     // Pack voltage (V)
    float pack_current;     // Pack current (A, signed: + = charging, - = discharging)
    float pack_soc;         // State of charge (%)
    float pack_amphours;    // Amp hours (Ah)
} EmbooPackStatus;

// Pack statistics (0x6B1)
typedef struct {
    uint16_t relay_state;   // Relay state flags
    float high_temp;        // High temperature (°C)
    float input_voltage;    // Input supply voltage (V)
    float summed_voltage;   // Pack summed voltage (V)
} EmbooPackStats;

// Status flags (0x6B2)
typedef struct {
    uint8_t status_flags;   // Status flags
    uint8_t error_flags;    // Error flags
} EmbooStatusFlags;

// Individual cell voltages (0x6B3)
typedef struct {
    uint8_t cell_id;        // Cell ID (0-99)
    float cell_voltage;     // Cell voltage (V)
    float cell_resistance;  // Cell resistance (mOhm)
    bool cell_balancing;    // Cell balancing active
    float cell_open_voltage; // Cell open circuit voltage (V)
} EmbooCellVoltage;

// Temperature data (0x6B4)
typedef struct {
    float high_temp;        // High temperature (°C)
    float low_temp;         // Low temperature (°C)
    uint8_t rolling_counter; // Rolling counter
} EmbooTemperatures;

// Pack summary (0x351)
typedef struct {
    float max_pack_voltage; // Maximum pack voltage (V)
    float min_pack_voltage; // Minimum pack voltage (V)
    float pack_ccl;         // Pack charge current limit (A)
    float pack_dcl;         // Pack discharge current limit (A)
} EmbooPackSummary;

// Pack data 1 (0x355)
typedef struct {
    uint16_t pack_soc_int;  // Pack SOC integer (%)
    uint16_t pack_health;   // Pack health (%)
    float pack_soc_decimal; // Pack SOC decimal (%)
} EmbooPackData1;

// Pack data 2 (0x356)
typedef struct {
    float pack_summed_voltage; // Pack summed voltage (V)
    float avg_current;      // Average current (A)
    float high_temp;        // High temperature (°C)
} EmbooPackData2;

#endif // EMBOO_BATTERY

// ============================================================================
// PACK/UNPACK FUNCTIONS
// ============================================================================

// Inverter telemetry
void unpack_inverter_telemetry(const uint8_t* data, uint8_t len, void* state);
void pack_inverter_telemetry(const void* state, uint8_t* data, uint8_t* len);

// Battery SOC
void unpack_battery_soc(const uint8_t* data, uint8_t len, void* state);
void pack_battery_soc(const void* state, uint8_t* data, uint8_t* len);

// Battery temperature
void unpack_battery_temp(const uint8_t* data, uint8_t len, void* state);
void pack_battery_temp(const void* state, uint8_t* data, uint8_t* len);

// Vehicle speed
void unpack_vehicle_speed(const uint8_t* data, uint8_t len, void* state);
void pack_vehicle_speed(const void* state, uint8_t* data, uint8_t* len);

// Motor RPM
void unpack_motor_rpm(const uint8_t* data, uint8_t len, void* state);
void pack_motor_rpm(const void* state, uint8_t* data, uint8_t* len);

// Charger status
void unpack_charger_status(const uint8_t* data, uint8_t len, void* state);
void pack_charger_status(const void* state, uint8_t* data, uint8_t* len);

// GPS position
void unpack_gps_position(const uint8_t* data, uint8_t len, void* state);
void pack_gps_position(const void* state, uint8_t* data, uint8_t* len);

// GPS velocity
void unpack_gps_velocity(const uint8_t* data, uint8_t len, void* state);
void pack_gps_velocity(const void* state, uint8_t* data, uint8_t* len);

// GPS time
void unpack_gps_time(const uint8_t* data, uint8_t len, void* state);
void pack_gps_time(const void* state, uint8_t* data, uint8_t* len);

// Body temperature sensors
void unpack_body_temp(const uint8_t* data, uint8_t len, void* state);
void pack_body_temp(const void* state, uint8_t* data, uint8_t* len);

// Body voltage monitoring
void unpack_body_voltage(const uint8_t* data, uint8_t len, void* state);
void pack_body_voltage(const void* state, uint8_t* data, uint8_t* len);

// ============================================================================
// EMBOO BATTERY PACK/UNPACK FUNCTIONS
// ============================================================================

#ifdef EMBOO_BATTERY

// Pack status (0x6B0)
void unpack_emboo_pack_status(const uint8_t* data, uint8_t len, void* state);
void pack_emboo_pack_status(const void* state, uint8_t* data, uint8_t* len);

// Pack statistics (0x6B1)
void unpack_emboo_pack_stats(const uint8_t* data, uint8_t len, void* state);
void pack_emboo_pack_stats(const void* state, uint8_t* data, uint8_t* len);

// Status flags (0x6B2)
void unpack_emboo_status_flags(const uint8_t* data, uint8_t len, void* state);
void pack_emboo_status_flags(const void* state, uint8_t* data, uint8_t* len);

// Cell voltages (0x6B3)
void unpack_emboo_cell_voltage(const uint8_t* data, uint8_t len, void* state);
void pack_emboo_cell_voltage(const void* state, uint8_t* data, uint8_t* len);

// Temperatures (0x6B4)
void unpack_emboo_temperatures(const uint8_t* data, uint8_t len, void* state);
void pack_emboo_temperatures(const void* state, uint8_t* data, uint8_t* len);

// Pack summary (0x351)
void unpack_emboo_pack_summary(const uint8_t* data, uint8_t len, void* state);
void pack_emboo_pack_summary(const void* state, uint8_t* data, uint8_t* len);

// Pack data 1 (0x355)
void unpack_emboo_pack_data1(const uint8_t* data, uint8_t len, void* state);
void pack_emboo_pack_data1(const void* state, uint8_t* data, uint8_t* len);

// Pack data 2 (0x356)
void unpack_emboo_pack_data2(const uint8_t* data, uint8_t len, void* state);
void pack_emboo_pack_data2(const void* state, uint8_t* data, uint8_t* len);

#endif // EMBOO_BATTERY

#endif // LEAF_CAN_MESSAGES_H

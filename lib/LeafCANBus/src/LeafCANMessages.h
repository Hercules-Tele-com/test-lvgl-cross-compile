#ifndef LEAF_CAN_MESSAGES_H
#define LEAF_CAN_MESSAGES_H

#include <stdint.h>

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================
// Include centralized hardware configuration file if available
// If not using centralized config, individual defines can be set in build system
#if __has_include("../../../config/hardware_config.h")
  #include "../../../config/hardware_config.h"
#endif

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
// MOTOR/INVERTER TYPE CONFIGURATION
// ============================================================================
// Define one of these to select motor controller type:
// - NISSAN_MOTOR: Nissan Leaf EM57 motor with inverter (CAN IDs 0x1DA, 0x1F2, etc.)
// - ROAM_MOTOR: ROAM/RM100 motor controller (CAN IDs 0x0A0-0x0A7, 0x0AC)
//
// Default to Nissan motor if nothing is defined
#if !defined(NISSAN_MOTOR) && !defined(ROAM_MOTOR)
  #define NISSAN_MOTOR
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

// ============================================================================
// MOTOR/INVERTER CAN IDs
// ============================================================================

#ifdef ROAM_MOTOR
// ROAM/RM100 Motor Controller CAN IDs
#define CAN_ID_MOTOR_TEMP_1         0x0A0  // IGBT temperatures (Phase A, B, C, Gate Driver)
#define CAN_ID_MOTOR_TEMP_2         0x0A1  // Control board temp, RTD temps
#define CAN_ID_MOTOR_TEMP_3         0x0A2  // RTD temps, stator temp, torque shudder
#define CAN_ID_MOTOR_ANALOG         0x0A3  // Analog input voltages
#define CAN_ID_MOTOR_DIGITAL        0x0A4  // Digital input status
#define CAN_ID_MOTOR_POSITION       0x0A5  // Motor angle, RPM, frequency
#define CAN_ID_MOTOR_CURRENT        0x0A6  // Phase currents, DC bus current
#define CAN_ID_MOTOR_VOLTAGE        0x0A7  // DC bus voltage, output voltage, phase voltages
#define CAN_ID_MOTOR_TORQUE         0x0AC  // Torque request and actual torque
// Note: For Nissan motor, existing IDs 0x1DA (RPM) and 0x1F2 (inverter) are used
#endif

// Custom ESP32 module CAN IDs (0x700+ range) - common to both battery and motor types
#define CAN_ID_GPS_POSITION         0x710  // GPS latitude, longitude
#define CAN_ID_GPS_VELOCITY         0x711  // GPS speed, heading
#define CAN_ID_GPS_TIME             0x712  // GPS date/time
#define CAN_ID_BODY_TEMP_SENSORS    0x720  // Multiple temperature sensors
#define CAN_ID_BODY_VOLTAGE         0x721  // Voltage monitoring
#define CAN_ID_UI_DASH_STATUS       0x730  // UI dashboard heartbeat
#define CAN_ID_CUSTOM_GAUGE_CMD     0x740  // Commands to custom gauges

// Elcon Charger CAN IDs (extended 29-bit format, 250 kbps)
// Note: Uses extended CAN frame format (0x18FF50E5 = 418054373 decimal)
#define CAN_ID_ELCON_CHARGER_STATUS 0x18FF50E5  // Charger status (output voltage, current, flags)

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

// Charger status (Nissan Leaf charger on 0x390)
typedef struct {
    uint8_t charging;       // 0=not charging, 1=charging
    float charge_current;   // Charge current (A)
    float charge_voltage;   // Charge voltage (V)
    uint16_t charge_time;   // Time remaining (minutes)
} ChargerState;

// Elcon Charger status (on 0x18FF50E5)
// Message format: 8 bytes at 1000ms cycle time
// Bytes 0-1: Output voltage (16-bit, scale 0.1 V)
// Bytes 2-3: Output current (16-bit, scale 0.1 A)
// Byte 4: Status flags (bits 0-4)
typedef struct {
    float output_voltage;   // Output voltage (V)
    float output_current;   // Output current (A)
    uint8_t hw_status;      // Hardware status (0=OK, 1=fault)
    uint8_t temp_status;    // Temperature status (0=OK, 1=over-temp)
    uint8_t input_voltage_status;  // Input voltage status (0=OK, 1=fault)
    uint8_t charging_state; // Charging state (0=idle, 1=charging)
    uint8_t comm_status;    // Communication status (0=OK, 1=fault)
    uint8_t online;         // Derived: 1 if recent message received, 0 otherwise
} ElconChargerState;

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
// ROAM MOTOR STATE STRUCTURES (RM100 Motor Controller)
// ============================================================================

#ifdef ROAM_MOTOR

// Motor torque state (0x0AC)
typedef struct {
    int16_t torque_request;  // Requested torque (Nm)
    int16_t torque_actual;   // Actual torque (Nm)
    int16_t torque_max_pos;  // Maximum positive torque (Nm)
    int16_t torque_max_neg;  // Maximum negative torque (Nm)
} RoamMotorTorque;

// Motor position and speed (0x0A5)
typedef struct {
    uint16_t motor_angle;         // Electrical angle (degrees)
    int16_t motor_rpm;            // Motor RPM
    uint16_t electrical_freq;     // Electrical frequency (Hz)
    int16_t delta_resolver;       // Delta resolver (degrees, ±180°)
} RoamMotorPosition;

// Motor voltage information (0x0A7)
typedef struct {
    uint16_t dc_bus_voltage;      // DC bus voltage (V)
    uint16_t output_voltage;      // Output voltage (V, peak line-neutral)
    uint16_t vab_vd_voltage;      // VAB (Phase A-B) or Vd voltage (V)
    uint16_t vbc_vq_voltage;      // VBC (Phase B-C) or Vq voltage (V)
} RoamMotorVoltage;

// Motor current information (0x0A6)
typedef struct {
    int16_t phase_a_current;      // Phase A current (A)
    int16_t phase_b_current;      // Phase B current (A)
    int16_t phase_c_current;      // Phase C current (A)
    int16_t dc_bus_current;       // DC bus current (A)
} RoamMotorCurrent;

// Motor temperatures #1 (0x0A0)
typedef struct {
    int16_t igbt_a_temp;          // IGBT Phase A temperature (°C * 10)
    int16_t igbt_b_temp;          // IGBT Phase B temperature (°C * 10)
    int16_t igbt_c_temp;          // IGBT Phase C temperature (°C * 10)
    int16_t gate_driver_temp;     // Gate driver board temperature (°C * 10)
} RoamMotorTemp1;

// Motor temperatures #2 (0x0A1)
typedef struct {
    int16_t control_board_temp;   // Control board temperature (°C * 10)
    int16_t rtd1_temp;            // RTD #1 temperature (°C * 10)
    int16_t rtd2_temp;            // RTD #2 temperature (°C * 10)
    int16_t rtd3_temp;            // RTD #3 temperature (°C * 10)
} RoamMotorTemp2;

// Motor temperatures #3 (0x0A2)
typedef struct {
    int16_t rtd4_temp;            // RTD #4 temperature (°C * 10)
    int16_t rtd5_temp;            // RTD #5 temperature (°C * 10)
    int16_t stator_temp;          // Motor stator temperature (°C * 10)
    int16_t torque_shudder;       // Torque shudder compensation value
} RoamMotorTemp3;

// Comprehensive motor state (aggregated)
typedef struct {
    // Torque
    int16_t torque_request;       // Requested torque (Nm)
    int16_t torque_actual;        // Actual torque (Nm)

    // Speed and position
    int16_t motor_rpm;            // Motor RPM
    uint16_t motor_angle;         // Electrical angle (degrees)

    // Electrical
    uint16_t dc_voltage;          // DC bus voltage (V)
    int16_t dc_current;           // DC bus current (A)
    int16_t electrical_power;     // Electrical power (W)
    int16_t mechanical_power;     // Mechanical power (W)

    // Temperatures
    int8_t inverter_temp;         // Inverter/control board temp (°C)
    int8_t stator_temp;           // Motor stator temp (°C)
    int8_t coolant_temp;          // Coolant temp (°C)

    // Status
    uint8_t mode_status;          // Operating mode status
    uint8_t mode_request;         // Operating mode request
    bool contactor_state;         // Contactor state
    uint8_t dtc_flags;            // Diagnostic trouble codes
    uint8_t derating;             // Power derating (%)

    // Odometer
    uint32_t odometer;            // Odometer (meters)
} RoamMotorState;

#endif // ROAM_MOTOR

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

// ============================================================================
// ROAM MOTOR PACK/UNPACK FUNCTIONS
// ============================================================================

#ifdef ROAM_MOTOR

// Motor torque (0x0AC)
void unpack_roam_motor_torque(const uint8_t* data, uint8_t len, void* state);
void pack_roam_motor_torque(const void* state, uint8_t* data, uint8_t* len);

// Motor position (0x0A5)
void unpack_roam_motor_position(const uint8_t* data, uint8_t len, void* state);
void pack_roam_motor_position(const void* state, uint8_t* data, uint8_t* len);

// Motor voltage (0x0A7)
void unpack_roam_motor_voltage(const uint8_t* data, uint8_t len, void* state);
void pack_roam_motor_voltage(const void* state, uint8_t* data, uint8_t* len);

// Motor current (0x0A6)
void unpack_roam_motor_current(const uint8_t* data, uint8_t len, void* state);
void pack_roam_motor_current(const void* state, uint8_t* data, uint8_t* len);

// Motor temperatures #1 (0x0A0)
void unpack_roam_motor_temp1(const uint8_t* data, uint8_t len, void* state);
void pack_roam_motor_temp1(const void* state, uint8_t* data, uint8_t* len);

// Motor temperatures #2 (0x0A1)
void unpack_roam_motor_temp2(const uint8_t* data, uint8_t len, void* state);
void pack_roam_motor_temp2(const void* state, uint8_t* data, uint8_t* len);

// Motor temperatures #3 (0x0A2)
void unpack_roam_motor_temp3(const uint8_t* data, uint8_t len, void* state);
void pack_roam_motor_temp3(const void* state, uint8_t* data, uint8_t* len);

// Comprehensive motor state update
void update_roam_motor_state(RoamMotorState* state, const RoamMotorTorque* torque,
                             const RoamMotorPosition* position, const RoamMotorVoltage* voltage,
                             const RoamMotorCurrent* current, const RoamMotorTemp2* temp2,
                             const RoamMotorTemp3* temp3);

#endif // ROAM_MOTOR

#endif // LEAF_CAN_MESSAGES_H

#include "LeafCANMessages.h"
#include <string.h>

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static uint16_t bytes_to_uint16(const uint8_t* data, uint8_t offset) {
    return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

static int16_t bytes_to_int16(const uint8_t* data, uint8_t offset) {
    return (int16_t)bytes_to_uint16(data, offset);
}

static void uint16_to_bytes(uint16_t value, uint8_t* data, uint8_t offset) {
    data[offset] = value & 0xFF;
    data[offset + 1] = (value >> 8) & 0xFF;
}

static void int16_to_bytes(int16_t value, uint8_t* data, uint8_t offset) {
    uint16_to_bytes((uint16_t)value, data, offset);
}

// ============================================================================
// INVERTER TELEMETRY (0x1F2)
// ============================================================================

void unpack_inverter_telemetry(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    InverterState* s = (InverterState*)state;

    s->voltage = bytes_to_uint16(data, 0) * 0.5f;  // Scale factor 0.5V
    s->current = bytes_to_int16(data, 2) * 0.1f;   // Scale factor 0.1A
    s->temp_inverter = (int16_t)data[4] - 40;      // Offset -40°C
    s->temp_motor = (int16_t)data[5] - 40;         // Offset -40°C
    s->status_flags = data[6];
}

void pack_inverter_telemetry(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const InverterState* s = (const InverterState*)state;

    memset(data, 0, 8);
    uint16_to_bytes((uint16_t)(s->voltage / 0.5f), data, 0);
    int16_to_bytes((int16_t)(s->current / 0.1f), data, 2);
    data[4] = (uint8_t)(s->temp_inverter + 40);
    data[5] = (uint8_t)(s->temp_motor + 40);
    data[6] = s->status_flags;
    *len = 8;
}

// ============================================================================
// BATTERY SOC (0x1DB)
// ============================================================================

void unpack_battery_soc(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    BatterySOCState* s = (BatterySOCState*)state;

    s->soc_percent = (data[0] >> 1) & 0x7F;  // Bits 1-7
    s->gids = bytes_to_uint16(data, 2);
    s->pack_voltage = bytes_to_uint16(data, 4) * 0.5f;
    s->pack_current = bytes_to_int16(data, 6) * 0.1f;
}

void pack_battery_soc(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const BatterySOCState* s = (const BatterySOCState*)state;

    memset(data, 0, 8);
    data[0] = (s->soc_percent & 0x7F) << 1;
    uint16_to_bytes(s->gids, data, 2);
    uint16_to_bytes((uint16_t)(s->pack_voltage / 0.5f), data, 4);
    int16_to_bytes((int16_t)(s->pack_current / 0.1f), data, 6);
    *len = 8;
}

// ============================================================================
// BATTERY TEMPERATURE (0x1DC)
// ============================================================================

void unpack_battery_temp(const uint8_t* data, uint8_t len, void* state) {
    if (len < 4 || !state) return;
    BatteryTempState* s = (BatteryTempState*)state;

    s->temp_max = (int8_t)data[0] - 40;
    s->temp_min = (int8_t)data[1] - 40;
    s->temp_avg = (int8_t)data[2] - 40;
    s->sensor_count = data[3];
}

void pack_battery_temp(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const BatteryTempState* s = (const BatteryTempState*)state;

    memset(data, 0, 8);
    data[0] = (uint8_t)(s->temp_max + 40);
    data[1] = (uint8_t)(s->temp_min + 40);
    data[2] = (uint8_t)(s->temp_avg + 40);
    data[3] = s->sensor_count;
    *len = 4;
}

// ============================================================================
// VEHICLE SPEED (0x1D4)
// ============================================================================

void unpack_vehicle_speed(const uint8_t* data, uint8_t len, void* state) {
    if (len < 2 || !state) return;
    VehicleSpeedState* s = (VehicleSpeedState*)state;

    s->speed_kmh = bytes_to_uint16(data, 0) * 0.01f;
    s->speed_mph = s->speed_kmh * 0.621371f;
}

void pack_vehicle_speed(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const VehicleSpeedState* s = (const VehicleSpeedState*)state;

    memset(data, 0, 8);
    uint16_to_bytes((uint16_t)(s->speed_kmh / 0.01f), data, 0);
    *len = 2;
}

// ============================================================================
// MOTOR RPM (0x1DA)
// ============================================================================

void unpack_motor_rpm(const uint8_t* data, uint8_t len, void* state) {
    if (len < 3 || !state) return;
    MotorRPMState* s = (MotorRPMState*)state;

    s->rpm = bytes_to_int16(data, 0);
    s->direction = data[2];
}

void pack_motor_rpm(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const MotorRPMState* s = (const MotorRPMState*)state;

    memset(data, 0, 8);
    int16_to_bytes(s->rpm, data, 0);
    data[2] = s->direction;
    *len = 3;
}

// ============================================================================
// CHARGER STATUS (0x390)
// ============================================================================

void unpack_charger_status(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    ChargerState* s = (ChargerState*)state;

    s->charging = data[0] & 0x01;
    s->charge_current = data[1] * 0.5f;
    s->charge_voltage = bytes_to_uint16(data, 2) * 0.1f;
    s->charge_time = bytes_to_uint16(data, 4);
}

void pack_charger_status(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const ChargerState* s = (const ChargerState*)state;

    memset(data, 0, 8);
    data[0] = s->charging ? 0x01 : 0x00;
    data[1] = (uint8_t)(s->charge_current / 0.5f);
    uint16_to_bytes((uint16_t)(s->charge_voltage / 0.1f), data, 2);
    uint16_to_bytes(s->charge_time, data, 4);
    *len = 8;
}

// ============================================================================
// GPS POSITION (0x710)
// ============================================================================

void unpack_gps_position(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    GPSPositionState* s = (GPSPositionState*)state;

    // Latitude: 4 bytes, scaled by 1e7
    int32_t lat_raw = (int32_t)data[0] | ((int32_t)data[1] << 8) |
                      ((int32_t)data[2] << 16) | ((int32_t)data[3] << 24);
    s->latitude = (double)lat_raw / 1e7;

    // Altitude/satellites/fix packed in remaining bytes
    s->altitude = (int16_t)bytes_to_int16(data, 4);
    s->satellites = data[6];
    s->fix_quality = data[7];
}

void pack_gps_position(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const GPSPositionState* s = (const GPSPositionState*)state;

    memset(data, 0, 8);

    // Latitude: 4 bytes, scaled by 1e7
    int32_t lat_raw = (int32_t)(s->latitude * 1e7);
    data[0] = lat_raw & 0xFF;
    data[1] = (lat_raw >> 8) & 0xFF;
    data[2] = (lat_raw >> 16) & 0xFF;
    data[3] = (lat_raw >> 24) & 0xFF;

    int16_to_bytes((int16_t)s->altitude, data, 4);
    data[6] = s->satellites;
    data[7] = s->fix_quality;
    *len = 8;
}

// ============================================================================
// GPS VELOCITY (0x711)
// ============================================================================

void unpack_gps_velocity(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    GPSVelocityState* s = (GPSVelocityState*)state;

    // Longitude: 4 bytes, scaled by 1e7 (shared with position logically, but sent here)
    int32_t lon_raw = (int32_t)data[0] | ((int32_t)data[1] << 8) |
                      ((int32_t)data[2] << 16) | ((int32_t)data[3] << 24);
    // Speed and heading
    s->speed_kmh = bytes_to_uint16(data, 4) * 0.01f;
    s->heading = bytes_to_uint16(data, 6) * 0.01f;
}

void pack_gps_velocity(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const GPSVelocityState* s = (const GPSVelocityState*)state;

    memset(data, 0, 8);

    // Note: In a real implementation, you'd pass longitude from GPS state
    // For now, we'll pack speed and heading
    uint16_to_bytes((uint16_t)(s->speed_kmh / 0.01f), data, 4);
    uint16_to_bytes((uint16_t)(s->heading / 0.01f), data, 6);
    *len = 8;
}

// ============================================================================
// GPS TIME (0x712)
// ============================================================================

void unpack_gps_time(const uint8_t* data, uint8_t len, void* state) {
    if (len < 7 || !state) return;
    GPSTimeState* s = (GPSTimeState*)state;

    s->year = bytes_to_uint16(data, 0);
    s->month = data[2];
    s->day = data[3];
    s->hour = data[4];
    s->minute = data[5];
    s->second = data[6];
}

void pack_gps_time(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const GPSTimeState* s = (const GPSTimeState*)state;

    memset(data, 0, 8);
    uint16_to_bytes(s->year, data, 0);
    data[2] = s->month;
    data[3] = s->day;
    data[4] = s->hour;
    data[5] = s->minute;
    data[6] = s->second;
    *len = 7;
}

// ============================================================================
// BODY TEMPERATURE SENSORS (0x720)
// ============================================================================

void unpack_body_temp(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    BodyTempState* s = (BodyTempState*)state;

    s->temp1 = bytes_to_int16(data, 0);
    s->temp2 = bytes_to_int16(data, 2);
    s->temp3 = bytes_to_int16(data, 4);
    s->temp4 = bytes_to_int16(data, 6);
}

void pack_body_temp(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const BodyTempState* s = (const BodyTempState*)state;

    memset(data, 0, 8);
    int16_to_bytes(s->temp1, data, 0);
    int16_to_bytes(s->temp2, data, 2);
    int16_to_bytes(s->temp3, data, 4);
    int16_to_bytes(s->temp4, data, 6);
    *len = 8;
}

// ============================================================================
// BODY VOLTAGE MONITORING (0x721)
// ============================================================================

void unpack_body_voltage(const uint8_t* data, uint8_t len, void* state) {
    if (len < 6 || !state) return;
    BodyVoltageState* s = (BodyVoltageState*)state;

    s->voltage_12v = bytes_to_uint16(data, 0) * 0.01f;
    s->voltage_5v = bytes_to_uint16(data, 2) * 0.01f;
    s->current_12v = bytes_to_uint16(data, 4) * 0.01f;
}

void pack_body_voltage(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const BodyVoltageState* s = (const BodyVoltageState*)state;

    memset(data, 0, 8);
    uint16_to_bytes((uint16_t)(s->voltage_12v / 0.01f), data, 0);
    uint16_to_bytes((uint16_t)(s->voltage_5v / 0.01f), data, 2);
    uint16_to_bytes((uint16_t)(s->current_12v / 0.01f), data, 4);
    *len = 6;
}

// ============================================================================
// EMBOO BATTERY PACK/UNPACK FUNCTIONS (Orion BMS / ENNOID-style)
// ============================================================================

#ifdef EMBOO_BATTERY

// Helper functions for big-endian conversion
static uint16_t bytes_to_uint16_be(const uint8_t* data, uint8_t offset) {
    return ((uint16_t)data[offset] << 8) | (uint16_t)data[offset + 1];
}

static int16_t bytes_to_int16_be(const uint8_t* data, uint8_t offset) {
    return (int16_t)bytes_to_uint16_be(data, offset);
}

static void uint16_to_bytes_be(uint16_t value, uint8_t* data, uint8_t offset) {
    data[offset] = (value >> 8) & 0xFF;
    data[offset + 1] = value & 0xFF;
}

static void int16_to_bytes_be(int16_t value, uint8_t* data, uint8_t offset) {
    uint16_to_bytes_be((uint16_t)value, data, offset);
}

// ============================================================================
// PACK STATUS (0x6B0)
// ============================================================================

void unpack_emboo_pack_status(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    EmbooPackStatus* s = (EmbooPackStatus*)state;

    // Current (big-endian, signed, 0.1A scale)
    s->pack_current = bytes_to_int16_be(data, 0) * 0.1f;

    // Voltage (big-endian, 0.1V scale)
    s->pack_voltage = bytes_to_uint16_be(data, 2) * 0.1f;

    // Amp hours (big-endian, 0.1Ah scale)
    s->pack_amphours = bytes_to_uint16_be(data, 4) * 0.1f;

    // SOC (0.5% scale)
    s->pack_soc = data[6] * 0.5f;
}

void pack_emboo_pack_status(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const EmbooPackStatus* s = (const EmbooPackStatus*)state;

    memset(data, 0, 8);
    int16_to_bytes_be((int16_t)(s->pack_current / 0.1f), data, 0);
    uint16_to_bytes_be((uint16_t)(s->pack_voltage / 0.1f), data, 2);
    uint16_to_bytes_be((uint16_t)(s->pack_amphours / 0.1f), data, 4);
    data[6] = (uint8_t)(s->pack_soc / 0.5f);
    *len = 8;
}

// ============================================================================
// PACK STATISTICS (0x6B1)
// ============================================================================

void unpack_emboo_pack_stats(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    EmbooPackStats* s = (EmbooPackStats*)state;

    // Relay state (big-endian)
    s->relay_state = bytes_to_uint16_be(data, 0);

    // High temperature (1.0°C scale)
    s->high_temp = (float)data[2];

    // Input supply voltage (big-endian, 0.1V scale)
    s->input_voltage = bytes_to_uint16_be(data, 3) * 0.1f;

    // Pack summed voltage (big-endian, 0.01V scale)
    s->summed_voltage = bytes_to_uint16_be(data, 5) * 0.01f;
}

void pack_emboo_pack_stats(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const EmbooPackStats* s = (const EmbooPackStats*)state;

    memset(data, 0, 8);
    uint16_to_bytes_be(s->relay_state, data, 0);
    data[2] = (uint8_t)s->high_temp;
    uint16_to_bytes_be((uint16_t)(s->input_voltage / 0.1f), data, 3);
    uint16_to_bytes_be((uint16_t)(s->summed_voltage / 0.01f), data, 5);
    *len = 8;
}

// ============================================================================
// STATUS FLAGS (0x6B2)
// ============================================================================

void unpack_emboo_status_flags(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    EmbooStatusFlags* s = (EmbooStatusFlags*)state;

    s->status_flags = data[0];
    s->error_flags = data[3];
}

void pack_emboo_status_flags(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const EmbooStatusFlags* s = (const EmbooStatusFlags*)state;

    memset(data, 0, 8);
    data[0] = s->status_flags;
    data[3] = s->error_flags;
    *len = 8;
}

// ============================================================================
// CELL VOLTAGES (0x6B3)
// ============================================================================

void unpack_emboo_cell_voltage(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    EmbooCellVoltage* s = (EmbooCellVoltage*)state;

    // Cell ID
    s->cell_id = data[0];

    // Skip header frames (cell_id > 100 indicates status frames)
    if (s->cell_id > 100) return;

    // Cell voltage (big-endian, 0.0001V scale)
    s->cell_voltage = bytes_to_uint16_be(data, 1) * 0.0001f;

    // Cell resistance (15 bits, 0.01 mOhm) + balancing (1 bit)
    uint16_t resistance_raw = bytes_to_uint16_be(data, 3);
    s->cell_resistance = (resistance_raw & 0x7FFF) * 0.01f;
    s->cell_balancing = (resistance_raw & 0x8000) != 0;

    // Cell open voltage (big-endian, 0.0001V scale)
    s->cell_open_voltage = bytes_to_uint16_be(data, 5) * 0.0001f;
}

void pack_emboo_cell_voltage(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const EmbooCellVoltage* s = (const EmbooCellVoltage*)state;

    memset(data, 0, 8);
    data[0] = s->cell_id;
    uint16_to_bytes_be((uint16_t)(s->cell_voltage / 0.0001f), data, 1);

    uint16_t resistance_raw = (uint16_t)(s->cell_resistance / 0.01f) & 0x7FFF;
    if (s->cell_balancing) resistance_raw |= 0x8000;
    uint16_to_bytes_be(resistance_raw, data, 3);

    uint16_to_bytes_be((uint16_t)(s->cell_open_voltage / 0.0001f), data, 5);
    *len = 8;
}

// ============================================================================
// TEMPERATURES (0x6B4)
// ============================================================================

void unpack_emboo_temperatures(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    EmbooTemperatures* s = (EmbooTemperatures*)state;

    s->high_temp = (float)data[2];
    s->low_temp = (float)data[3];
    s->rolling_counter = data[4];
}

void pack_emboo_temperatures(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const EmbooTemperatures* s = (const EmbooTemperatures*)state;

    memset(data, 0, 8);
    data[2] = (uint8_t)s->high_temp;
    data[3] = (uint8_t)s->low_temp;
    data[4] = s->rolling_counter;
    *len = 8;
}

// ============================================================================
// PACK SUMMARY (0x351)
// ============================================================================

void unpack_emboo_pack_summary(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    EmbooPackSummary* s = (EmbooPackSummary*)state;

    // Little-endian format
    s->max_pack_voltage = bytes_to_uint16(data, 0) * 0.1f;
    s->pack_ccl = bytes_to_uint16(data, 2) * 0.1f;
    s->pack_dcl = bytes_to_uint16(data, 4) * 0.1f;
    s->min_pack_voltage = bytes_to_uint16(data, 6) * 0.1f;
}

void pack_emboo_pack_summary(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const EmbooPackSummary* s = (const EmbooPackSummary*)state;

    memset(data, 0, 8);
    uint16_to_bytes((uint16_t)(s->max_pack_voltage / 0.1f), data, 0);
    uint16_to_bytes((uint16_t)(s->pack_ccl / 0.1f), data, 2);
    uint16_to_bytes((uint16_t)(s->pack_dcl / 0.1f), data, 4);
    uint16_to_bytes((uint16_t)(s->min_pack_voltage / 0.1f), data, 6);
    *len = 8;
}

// ============================================================================
// PACK DATA 1 (0x355)
// ============================================================================

void unpack_emboo_pack_data1(const uint8_t* data, uint8_t len, void* state) {
    if (len < 6 || !state) return;
    EmbooPackData1* s = (EmbooPackData1*)state;

    // Little-endian format
    s->pack_soc_int = bytes_to_uint16(data, 0);     // 1.0% scale
    s->pack_health = bytes_to_uint16(data, 2);      // 1.0% scale
    s->pack_soc_decimal = bytes_to_uint16(data, 4) * 0.1f; // 0.1% scale
}

void pack_emboo_pack_data1(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const EmbooPackData1* s = (const EmbooPackData1*)state;

    memset(data, 0, 8);
    uint16_to_bytes(s->pack_soc_int, data, 0);
    uint16_to_bytes(s->pack_health, data, 2);
    uint16_to_bytes((uint16_t)(s->pack_soc_decimal / 0.1f), data, 4);
    *len = 6;
}

// ============================================================================
// PACK DATA 2 (0x356)
// ============================================================================

void unpack_emboo_pack_data2(const uint8_t* data, uint8_t len, void* state) {
    if (len < 6 || !state) return;
    EmbooPackData2* s = (EmbooPackData2*)state;

    // Little-endian format
    s->pack_summed_voltage = bytes_to_uint16(data, 0) * 0.01f;
    // Note: Average current has unusual scale (1.5259E-6A), skipping for now
    s->avg_current = 0.0f;
    s->high_temp = bytes_to_uint16(data, 4) * 0.1f;
}

void pack_emboo_pack_data2(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const EmbooPackData2* s = (const EmbooPackData2*)state;

    memset(data, 0, 8);
    uint16_to_bytes((uint16_t)(s->pack_summed_voltage / 0.01f), data, 0);
    uint16_to_bytes((uint16_t)(s->high_temp / 0.1f), data, 4);
    *len = 6;
}

#endif // EMBOO_BATTERY

// ============================================================================
// ROAM MOTOR PACK/UNPACK FUNCTIONS
// ============================================================================

#ifdef ROAM_MOTOR

// ============================================================================
// MOTOR TORQUE (0x0AC)
// ============================================================================

void unpack_roam_motor_torque(const uint8_t* data, uint8_t len, void* state) {
    if (len < 4 || !state) return;
    RoamMotorTorque* s = (RoamMotorTorque*)state;

    // Little-endian format, Nm (no scaling)
    s->torque_request = (int16_t)((data[1] << 8) | data[0]);
    s->torque_actual = (int16_t)((data[3] << 8) | data[2]);
}

void pack_roam_motor_torque(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const RoamMotorTorque* s = (const RoamMotorTorque*)state;

    memset(data, 0, 8);
    data[0] = s->torque_request & 0xFF;
    data[1] = (s->torque_request >> 8) & 0xFF;
    data[2] = s->torque_actual & 0xFF;
    data[3] = (s->torque_actual >> 8) & 0xFF;
    *len = 4;
}

// ============================================================================
// MOTOR POSITION (0x0A5)
// ============================================================================

void unpack_roam_motor_position(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    RoamMotorPosition* s = (RoamMotorPosition*)state;

    // Little-endian format
    s->motor_angle = (data[0] << 8) | data[1];
    s->motor_rpm = (int16_t)((data[3] << 8) | data[2]);
    s->electrical_freq = (data[4] << 8) | data[5];
    s->delta_resolver = (int16_t)((data[6] << 8) | data[7]);
}

void pack_roam_motor_position(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const RoamMotorPosition* s = (const RoamMotorPosition*)state;

    memset(data, 0, 8);
    data[0] = (s->motor_angle >> 8) & 0xFF;
    data[1] = s->motor_angle & 0xFF;
    data[2] = s->motor_rpm & 0xFF;
    data[3] = (s->motor_rpm >> 8) & 0xFF;
    data[4] = (s->electrical_freq >> 8) & 0xFF;
    data[5] = s->electrical_freq & 0xFF;
    data[6] = (s->delta_resolver >> 8) & 0xFF;
    data[7] = s->delta_resolver & 0xFF;
    *len = 8;
}

// ============================================================================
// MOTOR VOLTAGE (0x0A7)
// ============================================================================

void unpack_roam_motor_voltage(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    RoamMotorVoltage* s = (RoamMotorVoltage*)state;

    // Little-endian format (big-endian pairs)
    s->dc_bus_voltage = (data[0] << 8) | data[1];
    s->output_voltage = (data[2] << 8) | data[3];
    s->vab_vd_voltage = (data[4] << 8) | data[5];
    s->vbc_vq_voltage = (data[6] << 8) | data[7];
}

void pack_roam_motor_voltage(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const RoamMotorVoltage* s = (const RoamMotorVoltage*)state;

    memset(data, 0, 8);
    data[0] = (s->dc_bus_voltage >> 8) & 0xFF;
    data[1] = s->dc_bus_voltage & 0xFF;
    data[2] = (s->output_voltage >> 8) & 0xFF;
    data[3] = s->output_voltage & 0xFF;
    data[4] = (s->vab_vd_voltage >> 8) & 0xFF;
    data[5] = s->vab_vd_voltage & 0xFF;
    data[6] = (s->vbc_vq_voltage >> 8) & 0xFF;
    data[7] = s->vbc_vq_voltage & 0xFF;
    *len = 8;
}

// ============================================================================
// MOTOR CURRENT (0x0A6)
// ============================================================================

void unpack_roam_motor_current(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    RoamMotorCurrent* s = (RoamMotorCurrent*)state;

    // Little-endian format (big-endian pairs)
    s->phase_a_current = (int16_t)((data[0] << 8) | data[1]);
    s->phase_b_current = (int16_t)((data[2] << 8) | data[3]);
    s->phase_c_current = (int16_t)((data[4] << 8) | data[5]);
    s->dc_bus_current = (int16_t)((data[6] << 8) | data[7]);
}

void pack_roam_motor_current(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const RoamMotorCurrent* s = (const RoamMotorCurrent*)state;

    memset(data, 0, 8);
    data[0] = (s->phase_a_current >> 8) & 0xFF;
    data[1] = s->phase_a_current & 0xFF;
    data[2] = (s->phase_b_current >> 8) & 0xFF;
    data[3] = s->phase_b_current & 0xFF;
    data[4] = (s->phase_c_current >> 8) & 0xFF;
    data[5] = s->phase_c_current & 0xFF;
    data[6] = (s->dc_bus_current >> 8) & 0xFF;
    data[7] = s->dc_bus_current & 0xFF;
    *len = 8;
}

// ============================================================================
// MOTOR TEMPERATURES #1 (0x0A0)
// ============================================================================

void unpack_roam_motor_temp1(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    RoamMotorTemp1* s = (RoamMotorTemp1*)state;

    // Little-endian format (big-endian pairs), °C * 10
    s->igbt_a_temp = (int16_t)((data[1] << 8) | data[0]);
    s->igbt_b_temp = (int16_t)((data[3] << 8) | data[2]);
    s->igbt_c_temp = (int16_t)((data[5] << 8) | data[4]);
    s->gate_driver_temp = (int16_t)((data[7] << 8) | data[6]);
}

void pack_roam_motor_temp1(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const RoamMotorTemp1* s = (const RoamMotorTemp1*)state;

    memset(data, 0, 8);
    data[0] = s->igbt_a_temp & 0xFF;
    data[1] = (s->igbt_a_temp >> 8) & 0xFF;
    data[2] = s->igbt_b_temp & 0xFF;
    data[3] = (s->igbt_b_temp >> 8) & 0xFF;
    data[4] = s->igbt_c_temp & 0xFF;
    data[5] = (s->igbt_c_temp >> 8) & 0xFF;
    data[6] = s->gate_driver_temp & 0xFF;
    data[7] = (s->gate_driver_temp >> 8) & 0xFF;
    *len = 8;
}

// ============================================================================
// MOTOR TEMPERATURES #2 (0x0A1)
// ============================================================================

void unpack_roam_motor_temp2(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    RoamMotorTemp2* s = (RoamMotorTemp2*)state;

    // Little-endian format (big-endian pairs), °C * 10
    s->control_board_temp = (int16_t)((data[1] << 8) | data[0]);
    s->rtd1_temp = (int16_t)((data[3] << 8) | data[2]);
    s->rtd2_temp = (int16_t)((data[5] << 8) | data[4]);
    s->rtd3_temp = (int16_t)((data[7] << 8) | data[6]);
}

void pack_roam_motor_temp2(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const RoamMotorTemp2* s = (const RoamMotorTemp2*)state;

    memset(data, 0, 8);
    data[0] = s->control_board_temp & 0xFF;
    data[1] = (s->control_board_temp >> 8) & 0xFF;
    data[2] = s->rtd1_temp & 0xFF;
    data[3] = (s->rtd1_temp >> 8) & 0xFF;
    data[4] = s->rtd2_temp & 0xFF;
    data[5] = (s->rtd2_temp >> 8) & 0xFF;
    data[6] = s->rtd3_temp & 0xFF;
    data[7] = (s->rtd3_temp >> 8) & 0xFF;
    *len = 8;
}

// ============================================================================
// MOTOR TEMPERATURES #3 (0x0A2)
// ============================================================================

void unpack_roam_motor_temp3(const uint8_t* data, uint8_t len, void* state) {
    if (len < 8 || !state) return;
    RoamMotorTemp3* s = (RoamMotorTemp3*)state;

    // Little-endian format (big-endian pairs), °C * 10
    s->rtd4_temp = (int16_t)((data[1] << 8) | data[0]);
    s->rtd5_temp = (int16_t)((data[3] << 8) | data[2]);
    s->stator_temp = (int16_t)((data[5] << 8) | data[4]);
    s->torque_shudder = (int16_t)((data[7] << 8) | data[6]);
}

void pack_roam_motor_temp3(const void* state, uint8_t* data, uint8_t* len) {
    if (!state || !data || !len) return;
    const RoamMotorTemp3* s = (const RoamMotorTemp3*)state;

    memset(data, 0, 8);
    data[0] = s->rtd4_temp & 0xFF;
    data[1] = (s->rtd4_temp >> 8) & 0xFF;
    data[2] = s->rtd5_temp & 0xFF;
    data[3] = (s->rtd5_temp >> 8) & 0xFF;
    data[4] = s->stator_temp & 0xFF;
    data[5] = (s->stator_temp >> 8) & 0xFF;
    data[6] = s->torque_shudder & 0xFF;
    data[7] = (s->torque_shudder >> 8) & 0xFF;
    *len = 8;
}

// ============================================================================
// COMPREHENSIVE MOTOR STATE UPDATE
// ============================================================================

void update_roam_motor_state(RoamMotorState* state, const RoamMotorTorque* torque,
                             const RoamMotorPosition* position, const RoamMotorVoltage* voltage,
                             const RoamMotorCurrent* current, const RoamMotorTemp2* temp2,
                             const RoamMotorTemp3* temp3) {
    if (!state) return;

    // Update torque
    if (torque) {
        state->torque_request = torque->torque_request;
        state->torque_actual = torque->torque_actual;
    }

    // Update speed and position
    if (position) {
        state->motor_rpm = position->motor_rpm;
        state->motor_angle = position->motor_angle;
    }

    // Update electrical
    if (voltage && current) {
        state->dc_voltage = voltage->dc_bus_voltage;
        state->dc_current = current->dc_bus_current;

        // Calculate electrical power (W)
        state->electrical_power = (int16_t)((int32_t)state->dc_voltage * (int32_t)state->dc_current / 1000);

        // Calculate mechanical power (W) = (Torque * RPM * 2π) / 60
        // Simplified: MPwr ≈ Torque * RPM / 9.5488
        if (torque && position) {
            state->mechanical_power = (int16_t)(((int32_t)torque->torque_actual * (int32_t)position->motor_rpm) / 10);
        }
    }

    // Update temperatures (°C * 10 → °C)
    if (temp2) {
        state->inverter_temp = (int8_t)(temp2->control_board_temp / 10);
    }

    if (temp3) {
        state->stator_temp = (int8_t)(temp3->stator_temp / 10);
    }
}

#endif // ROAM_MOTOR

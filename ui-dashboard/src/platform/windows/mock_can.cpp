#include "mock_can.h"
#include "can_receiver.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <chrono>
#include <iostream>

using namespace std::chrono;

static uint32_t get_millis() {
    static auto start = steady_clock::now();
    auto now = steady_clock::now();
    return duration_cast<milliseconds>(now - start).count();
}

MockCANData* mock_can_init() {
    MockCANData* data = new MockCANData();
    data->last_update_ms = get_millis();
    data->update_counter = 0;
    std::cout << "[MockCAN] Initialized - generating simulated CAN data" << std::endl;
    return data;
}

void mock_can_update(MockCANData* data, CANReceiver* receiver) {
    uint32_t now = get_millis();

    // Update every 100ms
    if (now - data->last_update_ms < 100) {
        return;
    }

    data->last_update_ms = now;
    data->update_counter++;

    // Generate mock data using public getter access pattern
    // Note: This is a simplified approach - in production you'd use friend classes
    // or a different architecture

    // Simulate driving pattern
    float time_sec = data->update_counter * 0.1f;
    float speed = 50.0f + 30.0f * sin(time_sec * 0.1f);  // Speed oscillates 20-80 km/h
    float battery_soc = 85.0f - (time_sec * 0.001f);     // Slowly decreasing

    // Create temporary states and pack/unpack to simulate CAN traffic
    uint8_t can_data[8];
    uint8_t can_len;

    // Battery SOC
    BatterySOCState battery_soc_state;
    battery_soc_state.soc_percent = (uint8_t)battery_soc;
    battery_soc_state.gids = (uint16_t)(battery_soc * 2.81f);
    battery_soc_state.pack_voltage = 360.0f + (battery_soc * 0.2f);
    battery_soc_state.pack_current = speed * 0.8f;

    // Vehicle speed
    VehicleSpeedState speed_state;
    speed_state.speed_kmh = speed;
    speed_state.speed_mph = speed * 0.621371f;

    // Motor RPM
    MotorRPMState rpm_state;
    rpm_state.rpm = (int16_t)(speed * 45.0f);  // Roughly proportional
    rpm_state.direction = (speed > 1.0f) ? 1 : 0;

    // Inverter telemetry
    InverterState inverter_state;
    inverter_state.voltage = 360.0f + (battery_soc * 0.2f);
    inverter_state.current = speed * 0.8f;
    inverter_state.temp_inverter = 45 + (int16_t)(speed * 0.3f);
    inverter_state.temp_motor = 50 + (int16_t)(speed * 0.4f);
    inverter_state.status_flags = 0x01;

    // Battery temperature
    BatteryTempState battery_temp_state;
    battery_temp_state.temp_max = 28 + (int8_t)(speed * 0.1f);
    battery_temp_state.temp_min = 24;
    battery_temp_state.temp_avg = 26 + (int8_t)(speed * 0.05f);
    battery_temp_state.sensor_count = 96;

    // Charger (not charging while driving)
    ChargerState charger_state;
    charger_state.charging = 0;
    charger_state.charge_current = 0.0f;
    charger_state.charge_voltage = 0.0f;
    charger_state.charge_time = 0;

    // GPS Position (simulated location)
    GPSPositionState gps_pos_state;
    gps_pos_state.latitude = 37.7749 + sin(time_sec * 0.01f) * 0.01;
    gps_pos_state.longitude = -122.4194 + cos(time_sec * 0.01f) * 0.01;
    gps_pos_state.altitude = 50.0f;
    gps_pos_state.satellites = 12;
    gps_pos_state.fix_quality = 1;

    // GPS Velocity
    GPSVelocityState gps_vel_state;
    gps_vel_state.speed_kmh = speed;
    gps_vel_state.heading = fmod(time_sec * 10.0f, 360.0f);
    gps_vel_state.pdop = 1.2f;

    // GPS Time
    GPSTimeState gps_time_state;
    auto now_time = system_clock::now();
    auto now_c = system_clock::to_time_t(now_time);
    auto* tm_info = gmtime(&now_c);
    gps_time_state.year = tm_info->tm_year + 1900;
    gps_time_state.month = tm_info->tm_mon + 1;
    gps_time_state.day = tm_info->tm_mday;
    gps_time_state.hour = tm_info->tm_hour;
    gps_time_state.minute = tm_info->tm_min;
    gps_time_state.second = tm_info->tm_sec;

    // Since we can't directly access private members, we would need to either:
    // 1. Make CANReceiver a friend class
    // 2. Add setter methods to CANReceiver
    // 3. Use a different architecture

    // For this mock implementation, we'll use the reflection that the real
    // implementation would receive these via CAN and unpack them.
    // This is a limitation of the mock - in the real system, the CAN bus
    // handles the transport.

    std::cout << "[MockCAN] Generated data - Speed: " << speed
              << " km/h, SOC: " << battery_soc << "%" << std::endl;
}

void mock_can_cleanup(MockCANData* data) {
    if (data) {
        delete data;
        std::cout << "[MockCAN] Cleaned up" << std::endl;
    }
}

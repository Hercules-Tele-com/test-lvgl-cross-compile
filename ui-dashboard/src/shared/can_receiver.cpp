#include "can_receiver.h"
#include <string.h>

#ifdef PLATFORM_WINDOWS
#include "windows/mock_can.h"
#elif defined(PLATFORM_LINUX)
#include "linux/socketcan.h"
#endif

CANReceiver::CANReceiver() : platform_data(nullptr) {
    memset(&inverter_state, 0, sizeof(inverter_state));
    memset(&battery_soc_state, 0, sizeof(battery_soc_state));
    memset(&battery_temp_state, 0, sizeof(battery_temp_state));
    memset(&vehicle_speed_state, 0, sizeof(vehicle_speed_state));
    memset(&motor_rpm_state, 0, sizeof(motor_rpm_state));
    memset(&charger_state, 0, sizeof(charger_state));
    memset(&gps_position_state, 0, sizeof(gps_position_state));
    memset(&gps_velocity_state, 0, sizeof(gps_velocity_state));
    memset(&gps_time_state, 0, sizeof(gps_time_state));
}

CANReceiver::~CANReceiver() {
#ifdef PLATFORM_WINDOWS
    if (platform_data) {
        mock_can_cleanup((MockCANData*)platform_data);
    }
#elif defined(PLATFORM_LINUX)
    if (platform_data) {
        socketcan_cleanup((SocketCANData*)platform_data);
    }
#endif
}

bool CANReceiver::init() {
#ifdef PLATFORM_WINDOWS
    platform_data = mock_can_init();
    return platform_data != nullptr;
#elif defined(PLATFORM_LINUX)
    platform_data = socketcan_init("can0");
    return platform_data != nullptr;
#else
    return false;
#endif
}

void CANReceiver::update() {
#ifdef PLATFORM_WINDOWS
    if (platform_data) {
        mock_can_update((MockCANData*)platform_data, this);
    }
#elif defined(PLATFORM_LINUX)
    if (platform_data) {
        CANMessage msg;
        while (socketcan_receive((SocketCANData*)platform_data, &msg)) {
            // Unpack messages based on CAN ID
            switch (msg.can_id) {
                case CAN_ID_INVERTER_TELEMETRY:
                    unpack_inverter_telemetry(msg.data, msg.len, &inverter_state);
                    break;
                case CAN_ID_BATTERY_SOC:
                    unpack_battery_soc(msg.data, msg.len, &battery_soc_state);
                    break;
                case CAN_ID_BATTERY_TEMP:
                    unpack_battery_temp(msg.data, msg.len, &battery_temp_state);
                    break;
                case CAN_ID_VEHICLE_SPEED:
                    unpack_vehicle_speed(msg.data, msg.len, &vehicle_speed_state);
                    break;
                case CAN_ID_MOTOR_RPM:
                    unpack_motor_rpm(msg.data, msg.len, &motor_rpm_state);
                    break;
                case CAN_ID_CHARGER_STATUS:
                    unpack_charger_status(msg.data, msg.len, &charger_state);
                    break;
                case CAN_ID_GPS_POSITION:
                    unpack_gps_position(msg.data, msg.len, &gps_position_state);
                    break;
                case CAN_ID_GPS_VELOCITY:
                    unpack_gps_velocity(msg.data, msg.len, &gps_velocity_state);
                    break;
                case CAN_ID_GPS_TIME:
                    unpack_gps_time(msg.data, msg.len, &gps_time_state);
                    break;
            }
        }
    }
#endif
}

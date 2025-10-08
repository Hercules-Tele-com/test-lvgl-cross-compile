#ifndef CAN_RECEIVER_H
#define CAN_RECEIVER_H

#include "LeafCANMessages.h"

class CANReceiver {
public:
    CANReceiver();
    ~CANReceiver();

    bool init();
    void update();

    // Get current state
    const InverterState& getInverterState() const { return inverter_state; }
    const BatterySOCState& getBatterySOCState() const { return battery_soc_state; }
    const BatteryTempState& getBatteryTempState() const { return battery_temp_state; }
    const VehicleSpeedState& getVehicleSpeedState() const { return vehicle_speed_state; }
    const MotorRPMState& getMotorRPMState() const { return motor_rpm_state; }
    const ChargerState& getChargerState() const { return charger_state; }
    const GPSPositionState& getGPSPositionState() const { return gps_position_state; }
    const GPSVelocityState& getGPSVelocityState() const { return gps_velocity_state; }
    const GPSTimeState& getGPSTimeState() const { return gps_time_state; }

    // Setters for testing/mocking
    void setInverterState(const InverterState& state) { inverter_state = state; }
    void setBatterySOCState(const BatterySOCState& state) { battery_soc_state = state; }
    void setBatteryTempState(const BatteryTempState& state) { battery_temp_state = state; }
    void setVehicleSpeedState(const VehicleSpeedState& state) { vehicle_speed_state = state; }
    void setMotorRPMState(const MotorRPMState& state) { motor_rpm_state = state; }
    void setChargerState(const ChargerState& state) { charger_state = state; }
    void setGPSPositionState(const GPSPositionState& state) { gps_position_state = state; }
    void setGPSVelocityState(const GPSVelocityState& state) { gps_velocity_state = state; }
    void setGPSTimeState(const GPSTimeState& state) { gps_time_state = state; }

private:
    // Platform-specific implementation
    void* platform_data;

    // State storage
    InverterState inverter_state;
    BatterySOCState battery_soc_state;
    BatteryTempState battery_temp_state;
    VehicleSpeedState vehicle_speed_state;
    MotorRPMState motor_rpm_state;
    ChargerState charger_state;
    GPSPositionState gps_position_state;
    GPSVelocityState gps_velocity_state;
    GPSTimeState gps_time_state;
};

#endif // CAN_RECEIVER_H

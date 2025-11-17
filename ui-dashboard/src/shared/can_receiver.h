#pragma once
#include <cstdint>
#include <atomic>

#ifdef PLATFORM_WINDOWS
  #include "windows/mock_can.h"
#elif defined(PLATFORM_LINUX)
  // Provides CANMessage + SocketCANData
  #include "platform/linux/socketcan.h"
#endif

class CANReceiver {
public:
    CANReceiver();
    ~CANReceiver();

    bool init();
    void update();

    // BMS Battery Limits (0x351) - Victron protocol
    int16_t getChargeVoltageSetpoint() const { return charge_voltage_setpoint_.load(std::memory_order_relaxed); } // V * 10
    int16_t getChargeCurrentLimit() const { return charge_current_limit_.load(std::memory_order_relaxed); }       // A * 10
    int16_t getDischargeCurrentLimit() const { return discharge_current_limit_.load(std::memory_order_relaxed); } // A * 10
    int16_t getDischargeVoltageLimit() const { return discharge_voltage_limit_.load(std::memory_order_relaxed); } // V * 10

    // BMS Battery State (0x355) - Victron protocol
    uint16_t getSOC() const { return soc_.load(std::memory_order_relaxed); }            // %
    uint16_t getSOH() const { return soh_.load(std::memory_order_relaxed); }            // %

    // BMS Battery Measurements (0x356) - Victron protocol
    uint16_t getBatteryVoltage() const { return battery_voltage_.load(std::memory_order_relaxed); }   // V * 100
    int16_t getBatteryCurrent() const { return battery_current_.load(std::memory_order_relaxed); }     // A * 10
    uint16_t getBatteryTemperature() const { return battery_temperature_.load(std::memory_order_relaxed); } // °C * 10

    // BMS Characteristics (0x35F) - Victron protocol
    uint8_t getCellType() const { return cell_type_.load(std::memory_order_relaxed); }
    uint8_t getCellQuantity() const { return cell_quantity_.load(std::memory_order_relaxed); }
    uint8_t getFirmwareMajor() const { return firmware_major_.load(std::memory_order_relaxed); }
    uint8_t getFirmwareMinor() const { return firmware_minor_.load(std::memory_order_relaxed); }
    uint16_t getBatteryCapacity() const { return battery_capacity_.load(std::memory_order_relaxed); } // Ah
    uint16_t getManufacturerId() const { return manufacturer_id_.load(std::memory_order_relaxed); }

    // BMS Cell Extrema (0x370) - Victron protocol
    uint16_t getMaxCellTemp() const { return max_cell_temp_.load(std::memory_order_relaxed); }       // °C
    uint16_t getMinCellTemp() const { return min_cell_temp_.load(std::memory_order_relaxed); }       // °C
    uint16_t getMaxCellVoltage() const { return max_cell_voltage_.load(std::memory_order_relaxed); } // mV
    uint16_t getMinCellVoltage() const { return min_cell_voltage_.load(std::memory_order_relaxed); } // mV

    // Vehicle data (0x1F2)
    uint16_t getSpeed() const { return speed_.load(std::memory_order_relaxed); }        // kph * 100
    uint8_t getGear() const { return gear_.load(std::memory_order_relaxed); }           // 0=P,1=R,2=N,3=D,4=B
    bool getReady() const { return ready_.load(std::memory_order_relaxed); }
    uint8_t getAccelPedal() const { return accel_pedal_.load(std::memory_order_relaxed); } // % * 2

    // Motor/Inverter (0x1D4)
    int16_t getMotorRPM() const { return motor_rpm_.load(std::memory_order_relaxed); }
    int16_t getMotorTorque() const { return motor_torque_.load(std::memory_order_relaxed); } // Nm * 10
    uint8_t getInverterTemp() const { return inverter_temp_.load(std::memory_order_relaxed); } // °C
    uint8_t getMotorTemp() const { return motor_temp_.load(std::memory_order_relaxed); }     // °C

    // GPS (0x710, 0x711)
    int32_t getLatitude() const { return gps_lat_.load(std::memory_order_relaxed); }    // deg * 1e7
    int32_t getLongitude() const { return gps_lon_.load(std::memory_order_relaxed); }   // deg * 1e7
    uint16_t getGPSSpeed() const { return gps_speed_.load(std::memory_order_relaxed); } // m/s * 100
    uint16_t getGPSHeading() const { return gps_heading_.load(std::memory_order_relaxed); } // deg * 100
    uint8_t getGPSFixType() const { return gps_fix_type_.load(std::memory_order_relaxed); }
    uint8_t getGPSSats() const { return gps_sats_.load(std::memory_order_relaxed); }

    // Process a raw CAN message (for both Linux SocketCAN and Windows mock data)
    void processCANMessage(uint32_t can_id, uint8_t len, const uint8_t* data);

private:
    void* platform_data = nullptr;     // MultiCan* on Linux, MockCANData* on Windows

    // BMS Battery Limits (0x351) - Victron protocol
    std::atomic<int16_t> charge_voltage_setpoint_{0};    // V * 10
    std::atomic<int16_t> charge_current_limit_{0};       // A * 10
    std::atomic<int16_t> discharge_current_limit_{0};    // A * 10
    std::atomic<int16_t> discharge_voltage_limit_{0};    // V * 10

    // BMS Battery State (0x355) - Victron protocol
    std::atomic<uint16_t> soc_{0};            // %
    std::atomic<uint16_t> soh_{0};            // %

    // BMS Battery Measurements (0x356) - Victron protocol
    std::atomic<uint16_t> battery_voltage_{0};      // V * 100
    std::atomic<int16_t> battery_current_{0};       // A * 10 (signed)
    std::atomic<uint16_t> battery_temperature_{0};  // °C * 10

    // BMS Characteristics (0x35F) - Victron protocol
    std::atomic<uint8_t> cell_type_{0};
    std::atomic<uint8_t> cell_quantity_{0};
    std::atomic<uint8_t> firmware_major_{0};
    std::atomic<uint8_t> firmware_minor_{0};
    std::atomic<uint16_t> battery_capacity_{0};     // Ah
    std::atomic<uint16_t> manufacturer_id_{0};

    // BMS Cell Extrema (0x370) - Victron protocol
    std::atomic<uint16_t> max_cell_temp_{0};        // °C
    std::atomic<uint16_t> min_cell_temp_{0};        // °C
    std::atomic<uint16_t> max_cell_voltage_{0};     // mV
    std::atomic<uint16_t> min_cell_voltage_{0};     // mV

    // Vehicle (0x1F2)
    std::atomic<uint16_t> speed_{0};          // kph * 100
    std::atomic<uint8_t> gear_{0};
    std::atomic<bool> ready_{false};
    std::atomic<uint8_t> accel_pedal_{0};     // % * 2

    // Motor/Inverter (0x1D4)
    std::atomic<int16_t> motor_rpm_{0};
    std::atomic<int16_t> motor_torque_{0};    // Nm * 10
    std::atomic<uint8_t> inverter_temp_{0};
    std::atomic<uint8_t> motor_temp_{0};

    // GPS (0x710, 0x711)
    std::atomic<int32_t> gps_lat_{0};         // deg * 1e7
    std::atomic<int32_t> gps_lon_{0};         // deg * 1e7
    std::atomic<uint16_t> gps_speed_{0};      // m/s * 100
    std::atomic<uint16_t> gps_heading_{0};    // deg * 100
    std::atomic<uint8_t> gps_fix_type_{0};
    std::atomic<uint8_t> gps_sats_{0};
};


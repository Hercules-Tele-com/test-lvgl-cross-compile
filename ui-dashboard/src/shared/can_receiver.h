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

    // BMS Pack data (Victron: 0x356 voltage/current, 0x355 SOC)
    uint16_t getPackVoltage() const { return pack_voltage_.load(std::memory_order_relaxed); } // V * 100 (0.01V)
    int16_t getPackCurrent() const { return pack_current_.load(std::memory_order_relaxed); }  // A * 10 (0.1A)
    uint16_t getSOC() const { return soc_.load(std::memory_order_relaxed); }                  // % (0-100)

    // BMS Current Limits (0x356)
    uint16_t getChargeCurrentMax() const { return ichg_max_.load(std::memory_order_relaxed); } // A * 2
    uint16_t getDischargeCurrentMax() const { return idis_max_.load(std::memory_order_relaxed); } // A * 2

    // BMS Temps (0x355)
    int8_t getTempMin() const { return temp_min_.load(std::memory_order_relaxed); }     // °C
    int8_t getTempMax() const { return temp_max_.load(std::memory_order_relaxed); }     // °C
    int8_t getTempAvg() const { return temp_avg_.load(std::memory_order_relaxed); }     // °C

    // BMS Limits (0x35F)
    uint8_t getSOH() const { return soh_.load(std::memory_order_relaxed); }             // %

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

    // BMS Pack (Victron: 0x356 voltage/current, 0x355 SOC)
    std::atomic<uint16_t> pack_voltage_{0};   // V * 100 (0.01V units)
    std::atomic<int16_t> pack_current_{0};    // A * 10 (0.1A units, signed)
    std::atomic<uint16_t> soc_{0};            // % (0-100)

    // BMS Current Limits (0x356)
    std::atomic<uint16_t> ichg_max_{0};       // A * 2
    std::atomic<uint16_t> idis_max_{0};       // A * 2

    // BMS Temps (0x355)
    std::atomic<int8_t> temp_min_{0};
    std::atomic<int8_t> temp_max_{0};
    std::atomic<int8_t> temp_avg_{0};

    // BMS Limits (0x35F)
    std::atomic<uint8_t> soh_{0};             // %

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


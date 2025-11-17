#include "shared/can_receiver.h"
#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_WINDOWS
  #include "windows/mock_can.h"
#elif defined(PLATFORM_LINUX)
  #include "platform/linux/socketcan.h"
#endif

// Toggle raw CAN spam: 1 = print all frames, 0 = print only SoC lines
#define CAN_DEBUG 0

CANReceiver::CANReceiver() = default;

CANReceiver::~CANReceiver() {
#ifdef PLATFORM_WINDOWS
    if (platform_data) {
        mock_can_cleanup((MockCANData*)platform_data);
    }
#elif defined(PLATFORM_LINUX)
    if (platform_data) {
        struct MultiCan { SocketCANData* ch0; SocketCANData* ch1; };
        auto* mc = (MultiCan*)platform_data;
        if (mc->ch0) socketcan_cleanup(mc->ch0);
        if (mc->ch1) socketcan_cleanup(mc->ch1);
        delete mc;
    }
#endif
}

void CANReceiver::processCANMessage(uint32_t can_id, uint8_t len, const uint8_t* data) {
    const uint32_t baseId = (can_id & 0x7FF); // 11-bit ID

    // 0x351: Victron BMS - Battery Charge/Discharge Limits (Little-Endian)
    if (baseId == 0x351 && len >= 8) {
        uint16_t vchg = data[0] | (data[1] << 8);   // Charge voltage limit (0.1V, LE)
        int16_t ichg = data[2] | (data[3] << 8);    // Charge current limit (0.1A, LE)
        int16_t idis = data[4] | (data[5] << 8);    // Discharge current limit (0.1A, LE)
        uint16_t vdis = data[6] | (data[7] << 8);   // Discharge voltage limit (0.1V, LE)
        ichg_max_.store(ichg, std::memory_order_relaxed);
        idis_max_.store(idis, std::memory_order_relaxed);
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x351] Limits: ChgV=%.1fV, ChgI=%.1fA, DisI=%.1fA, DisV=%.1fV\n",
               vchg/10.0f, ichg/10.0f, idis/10.0f, vdis/10.0f);
        fflush(stdout);
#endif
    }

    // 0x355: Victron BMS - State of Charge / State of Health (Little-Endian)
    else if (baseId == 0x355 && len >= 4) {
        uint16_t soc = data[0] | (data[1] << 8);    // SOC (%, LE)
        uint16_t soh = data[2] | (data[3] << 8);    // SOH (%, LE)
        soc_.store(soc, std::memory_order_relaxed);
        soh_.store(soh, std::memory_order_relaxed);
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x355] State: SOC=%u%%, SOH=%u%%\n", soc, soh);
        fflush(stdout);
#endif
    }

    // 0x356: Victron BMS - Battery Voltage, Current, Temperature (Little-Endian)
    else if (baseId == 0x356 && len >= 6) {
        uint16_t pack_v = data[0] | (data[1] << 8);  // Voltage (0.01V, LE)
        int16_t pack_i = data[2] | (data[3] << 8);   // Current (0.1A, signed, negative = discharge, LE)
        uint16_t pack_t = data[4] | (data[5] << 8);  // Temperature (0.1°C, LE)
        pack_voltage_.store(pack_v, std::memory_order_relaxed);
        pack_current_.store(pack_i, std::memory_order_relaxed);
        temp_avg_.store((int8_t)(pack_t/10), std::memory_order_relaxed);
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x356] Measurements: %.2fV, %.1fA, %.1f°C\n",
               pack_v/100.0f, pack_i/10.0f, pack_t/10.0f);
        fflush(stdout);
#endif
    }

    // 0x35F: Victron BMS - Battery Characteristics (Little-Endian)
    else if (baseId == 0x35F && len >= 8) {
        uint8_t cell_type = data[0];          // Cell chemistry type (0=Unknown, 1=LiFePO4, 2=NMC)
        uint8_t cell_qty = data[1];           // Number of cells
        uint8_t fw_major = data[2];           // Firmware major version
        uint8_t fw_minor = data[3];           // Firmware minor version
        uint16_t capacity = data[4] | (data[5] << 8);  // Capacity (Ah, LE) - NO scaling!
        uint16_t mfr_id = data[6] | (data[7] << 8);    // Manufacturer ID (LE)
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        const char* cell_types[] = {"Unknown", "LiFePO4", "NMC"};
        printf("[0x35F] Characteristics: CellType=%s, Qty=%u, FW=%u.%u, Cap=%uAh, MfrID=%u\n",
               cell_type <= 2 ? cell_types[cell_type] : "?", cell_qty, fw_major, fw_minor, capacity, mfr_id);
        fflush(stdout);
#endif
    }

    // 0x370-0x373: Victron BMS - Cell Module Extrema (Little-Endian)
    else if (baseId >= 0x370 && baseId <= 0x373 && len >= 8) {
        uint16_t maxT = data[0] | (data[1] << 8);   // Max cell temp (°C, LE, NO scaling!)
        uint16_t minT = data[2] | (data[3] << 8);   // Min cell temp (°C, LE, NO scaling!)
        uint16_t maxV = data[4] | (data[5] << 8);   // Max cell voltage (mV, LE)
        uint16_t minV = data[6] | (data[7] << 8);   // Min cell voltage (mV, LE)

        // Skip printing if all values are zero (empty module slot)
        if (maxT == 0 && minT == 0 && maxV == 0 && minV == 0) {
            return;
        }

#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x%03X] Cell Extrema: MaxT=%u°C, MinT=%u°C, MaxV=%umV, MinV=%umV\n",
               baseId, maxT, minT, maxV, minV);
        fflush(stdout);
#endif
    }

    // 0x1F2: Vehicle (speed, gear, accel pedal)
    else if (baseId == 0x1F2 && len >= 5) {
        uint16_t spd = (data[0] << 8) | data[1];     // kph * 100
        uint8_t gear = data[2];
        bool ready = (data[3] & 0x01) != 0;
        uint8_t accel = data[4];                          // % * 2
        speed_.store(spd, std::memory_order_relaxed);
        gear_.store(gear, std::memory_order_relaxed);
        ready_.store(ready, std::memory_order_relaxed);
        accel_pedal_.store(accel, std::memory_order_relaxed);
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        const char* gear_str[] = {"P", "R", "N", "D", "B"};
        printf("[0x1F2] Speed: %.1f kph, Gear: %s, Ready: %d, Accel: %.1f%%\n",
               spd/100.0f, gear < 5 ? gear_str[gear] : "?", ready, accel/2.0f);
        fflush(stdout);
#endif
    }

    // 0x1D4: Motor/Inverter
    else if (baseId == 0x1D4 && len >= 6) {
        int16_t rpm = (data[0] << 8) | data[1];      // signed RPM
        int16_t torque = (data[2] << 8) | data[3];   // Nm * 10 (signed)
        uint8_t inv_temp = data[4];
        uint8_t mot_temp = data[5];
        motor_rpm_.store(rpm, std::memory_order_relaxed);
        motor_torque_.store(torque, std::memory_order_relaxed);
        inverter_temp_.store(inv_temp, std::memory_order_relaxed);
        motor_temp_.store(mot_temp, std::memory_order_relaxed);
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x1D4] Motor: %d RPM, %.1f Nm, InvTemp: %u°C, MotTemp: %u°C\n",
               (int)rpm, torque/10.0f, (unsigned)inv_temp, (unsigned)mot_temp);
        fflush(stdout);
#endif
    }

    // 0x710: GPS Fix (Lat/Lon)
    else if (baseId == 0x710 && len >= 8) {
        int32_t lat = (data[0] << 24) | (data[1] << 16) |
                      (data[2] << 8) | data[3];
        int32_t lon = (data[4] << 24) | (data[5] << 16) |
                      (data[6] << 8) | data[7];
        gps_lat_.store(lat, std::memory_order_relaxed);
        gps_lon_.store(lon, std::memory_order_relaxed);
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x710] GPS: %.7f, %.7f\n", lat/1e7, lon/1e7);
        fflush(stdout);
#endif
    }

    // 0x711: GPS Speed/Heading
    else if (baseId == 0x711 && len >= 6) {
        uint16_t gspd = (data[0] << 8) | data[1];    // m/s * 100
        uint16_t heading = (data[2] << 8) | data[3]; // deg * 100
        uint8_t fix_type = data[4];
        uint8_t sats = data[5];
        gps_speed_.store(gspd, std::memory_order_relaxed);
        gps_heading_.store(heading, std::memory_order_relaxed);
        gps_fix_type_.store(fix_type, std::memory_order_relaxed);
        gps_sats_.store(sats, std::memory_order_relaxed);
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x711] GPS: %.2f m/s, %.2f°, Fix: %u, Sats: %u\n",
               gspd/100.0f, heading/100.0f, (unsigned)fix_type, (unsigned)sats);
        fflush(stdout);
#endif
    }
}

bool CANReceiver::init() {
#ifdef PLATFORM_WINDOWS
    platform_data = mock_can_init();
    return platform_data != nullptr;

#elif defined(PLATFORM_LINUX)
    struct MultiCan { SocketCANData* ch0; SocketCANData* ch1; };
    auto* mc = new MultiCan();
    mc->ch0 = socketcan_init("can0");
    if (mc->ch0) printf("[CANReceiver] Opened can0\n");
    else         printf("[CANReceiver] WARNING: can0 not available\n");

    mc->ch1 = socketcan_init("can1");
    if (mc->ch1) printf("[CANReceiver] Opened can1\n");
    else         printf("[CANReceiver] INFO: can1 not available (continuing)\n");

    if (!mc->ch0 && !mc->ch1) {
        printf("[CANReceiver] ERROR: no CAN channels opened\n");
        delete mc;
        return false;
    }
    platform_data = mc;
    return true;
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
    if (!platform_data) return;
    struct MultiCan { SocketCANData* ch0; SocketCANData* ch1; };
    auto* mc = (MultiCan*)platform_data;

    auto pump = [&](SocketCANData* ch) {
        if (!ch) return;
        CANMessage msg{};
        while (socketcan_receive(ch, &msg)) {
#if CAN_DEBUG
            printf("[CAN %s] ID=%03X DLC=%u DATA=", ch->interface,
                   (msg.can_id & 0x1FFFFFFF), msg.len);
            for (uint8_t i = 0; i < msg.len; ++i)
                printf("%s%02X", (i ? " " : ""), msg.data[i]);
            printf("\n");
#endif
            // Use the common processing method
            processCANMessage(msg.can_id, msg.len, msg.data);
        }
    };

    pump(mc->ch0);
    pump(mc->ch1);
#endif
}


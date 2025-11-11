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
            const uint32_t baseId = (msg.can_id & 0x7FF); // 11-bit ID

            // 0x351: BMS Pack Basic (voltage, current, SoC)
            if (baseId == 0x351 && msg.len >= 5) {
                uint16_t pack_v = (msg.data[0] << 8) | msg.data[1];  // V * 10
                int16_t pack_i = (msg.data[2] << 8) | msg.data[3];   // A * 10 (signed)
                uint16_t soc = (msg.data[4] << 8) | msg.data[5];     // % * 10
                pack_voltage_.store(pack_v, std::memory_order_relaxed);
                pack_current_.store(pack_i, std::memory_order_relaxed);
                soc_.store(soc, std::memory_order_relaxed);
                printf("[0x351] Pack: %.1fV, %.1fA, %.1f%% SoC\n",
                       pack_v/10.0f, pack_i/10.0f, soc/10.0f);
                fflush(stdout);
            }

            // 0x356: BMS Current Limits
            else if (baseId == 0x356 && msg.len >= 4) {
                uint16_t ichg = (msg.data[0] << 8) | msg.data[1];    // A * 2
                uint16_t idis = (msg.data[2] << 8) | msg.data[3];    // A * 2
                ichg_max_.store(ichg, std::memory_order_relaxed);
                idis_max_.store(idis, std::memory_order_relaxed);
                printf("[0x356] Limits: Chg %.1fA, Dis %.1fA\n",
                       ichg/2.0f, idis/2.0f);
                fflush(stdout);
            }

            // 0x355: BMS Temperatures
            else if (baseId == 0x355 && msg.len >= 3) {
                int8_t t_min = (int8_t)msg.data[0];
                int8_t t_max = (int8_t)msg.data[1];
                int8_t t_avg = (int8_t)msg.data[2];
                temp_min_.store(t_min, std::memory_order_relaxed);
                temp_max_.store(t_max, std::memory_order_relaxed);
                temp_avg_.store(t_avg, std::memory_order_relaxed);
                printf("[0x355] Temps: Min %d°C, Max %d°C, Avg %d°C\n",
                       (int)t_min, (int)t_max, (int)t_avg);
                fflush(stdout);
            }

            // 0x35F: BMS Limits (SOH)
            else if (baseId == 0x35F && msg.len >= 7) {
                uint8_t soh = msg.data[6];  // byte 6
                soh_.store(soh, std::memory_order_relaxed);
                printf("[0x35F] SOH: %u%%\n", (unsigned)soh);
                fflush(stdout);
            }

            // 0x1F2: Vehicle (speed, gear, accel pedal)
            else if (baseId == 0x1F2 && msg.len >= 5) {
                uint16_t spd = (msg.data[0] << 8) | msg.data[1];     // kph * 100
                uint8_t gear = msg.data[2];
                bool ready = (msg.data[3] & 0x01) != 0;
                uint8_t accel = msg.data[4];                          // % * 2
                speed_.store(spd, std::memory_order_relaxed);
                gear_.store(gear, std::memory_order_relaxed);
                ready_.store(ready, std::memory_order_relaxed);
                accel_pedal_.store(accel, std::memory_order_relaxed);
                const char* gear_str[] = {"P", "R", "N", "D", "B"};
                printf("[0x1F2] Speed: %.1f kph, Gear: %s, Ready: %d, Accel: %.1f%%\n",
                       spd/100.0f, gear < 5 ? gear_str[gear] : "?", ready, accel/2.0f);
                fflush(stdout);
            }

            // 0x1D4: Motor/Inverter
            else if (baseId == 0x1D4 && msg.len >= 6) {
                int16_t rpm = (msg.data[0] << 8) | msg.data[1];      // signed RPM
                int16_t torque = (msg.data[2] << 8) | msg.data[3];   // Nm * 10 (signed)
                uint8_t inv_temp = msg.data[4];
                uint8_t mot_temp = msg.data[5];
                motor_rpm_.store(rpm, std::memory_order_relaxed);
                motor_torque_.store(torque, std::memory_order_relaxed);
                inverter_temp_.store(inv_temp, std::memory_order_relaxed);
                motor_temp_.store(mot_temp, std::memory_order_relaxed);
                printf("[0x1D4] Motor: %d RPM, %.1f Nm, InvTemp: %u°C, MotTemp: %u°C\n",
                       (int)rpm, torque/10.0f, (unsigned)inv_temp, (unsigned)mot_temp);
                fflush(stdout);
            }

            // 0x710: GPS Fix (Lat/Lon)
            else if (baseId == 0x710 && msg.len >= 8) {
                int32_t lat = (msg.data[0] << 24) | (msg.data[1] << 16) |
                              (msg.data[2] << 8) | msg.data[3];
                int32_t lon = (msg.data[4] << 24) | (msg.data[5] << 16) |
                              (msg.data[6] << 8) | msg.data[7];
                gps_lat_.store(lat, std::memory_order_relaxed);
                gps_lon_.store(lon, std::memory_order_relaxed);
                printf("[0x710] GPS: %.7f, %.7f\n", lat/1e7, lon/1e7);
                fflush(stdout);
            }

            // 0x711: GPS Speed/Heading
            else if (baseId == 0x711 && msg.len >= 6) {
                uint16_t gspd = (msg.data[0] << 8) | msg.data[1];    // m/s * 100
                uint16_t heading = (msg.data[2] << 8) | msg.data[3]; // deg * 100
                uint8_t fix_type = msg.data[4];
                uint8_t sats = msg.data[5];
                gps_speed_.store(gspd, std::memory_order_relaxed);
                gps_heading_.store(heading, std::memory_order_relaxed);
                gps_fix_type_.store(fix_type, std::memory_order_relaxed);
                gps_sats_.store(sats, std::memory_order_relaxed);
                printf("[0x711] GPS: %.2f m/s, %.2f°, Fix: %u, Sats: %u\n",
                       gspd/100.0f, heading/100.0f, (unsigned)fix_type, (unsigned)sats);
                fflush(stdout);
            }
        }
    };

    pump(mc->ch0);
    pump(mc->ch1);
#endif
}


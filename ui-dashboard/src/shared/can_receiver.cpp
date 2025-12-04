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

    // 0x351: BMS Battery Limits (Victron protocol)
    if (baseId == 0x351 && len >= 8) {
        int16_t chg_v_setpoint = (int16_t)((data[0] << 8) | data[1]);  // V * 10
        int16_t chg_i_limit = (int16_t)((data[2] << 8) | data[3]);     // A * 10
        int16_t dis_i_limit = (int16_t)((data[4] << 8) | data[5]);     // A * 10
        int16_t dis_v_limit = (int16_t)((data[6] << 8) | data[7]);     // V * 10
        charge_voltage_setpoint_.store(chg_v_setpoint, std::memory_order_relaxed);
        charge_current_limit_.store(chg_i_limit, std::memory_order_relaxed);
        discharge_current_limit_.store(dis_i_limit, std::memory_order_relaxed);
        discharge_voltage_limit_.store(dis_v_limit, std::memory_order_relaxed);
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x351] Limits: ChgV=%.1fV, ChgI=%.1fA, DisI=%.1fA, DisV=%.1fV\n",
               chg_v_setpoint/10.0f, chg_i_limit/10.0f, dis_i_limit/10.0f, dis_v_limit/10.0f);
        fflush(stdout);
#endif
    }

    // 0x356: BMS Battery Measurements (Victron protocol)
    else if (baseId == 0x356 && len >= 6) {
        uint16_t bat_v = (data[0] << 8) | data[1];         // V * 100
        int16_t bat_i = (int16_t)((data[2] << 8) | data[3]);  // A * 10 (signed)
        uint16_t bat_t = (data[4] << 8) | data[5];         // °C * 10
        battery_voltage_.store(bat_v, std::memory_order_relaxed);
        battery_current_.store(bat_i, std::memory_order_relaxed);
        battery_temperature_.store(bat_t, std::memory_order_relaxed);
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x356] Measurements: %.2fV, %.1fA, %.1f°C\n",
               bat_v/100.0f, bat_i/10.0f, bat_t/10.0f);
        fflush(stdout);
#endif
    }

    // 0x355: BMS Battery State (Victron protocol)
    else if (baseId == 0x355 && len >= 4) {
        uint16_t soc = (data[0] << 8) | data[1];  // %
        uint16_t soh = (data[2] << 8) | data[3];  // %
        soc_.store(soc, std::memory_order_relaxed);
        soh_.store(soh, std::memory_order_relaxed);
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x355] State: SOC=%u%%, SOH=%u%%\n", soc, soh);
        fflush(stdout);
#endif
    }

    // 0x35F: BMS Characteristics (Victron protocol)
    else if (baseId == 0x35F && len >= 8) {
        uint8_t cell_type = data[0];
        uint8_t cell_qty = data[1];
        uint8_t fw_major = data[2];
        uint8_t fw_minor = data[3];
        uint16_t capacity = (data[4] << 8) | data[5];  // Ah
        uint16_t mfr_id = (data[6] << 8) | data[7];
        cell_type_.store(cell_type, std::memory_order_relaxed);
        cell_quantity_.store(cell_qty, std::memory_order_relaxed);
        firmware_major_.store(fw_major, std::memory_order_relaxed);
        firmware_minor_.store(fw_minor, std::memory_order_relaxed);
        battery_capacity_.store(capacity, std::memory_order_relaxed);
        manufacturer_id_.store(mfr_id, std::memory_order_relaxed);
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x35F] Characteristics: CellType=%u, Qty=%u, FW=%u.%u, Cap=%uAh, MfrID=%u\n",
               cell_type, cell_qty, fw_major, fw_minor, capacity, mfr_id);
        fflush(stdout);
#endif
    }

    // 0x370: BMS Cell Extrema (Victron protocol)
    else if (baseId == 0x370 && len >= 8) {
        uint16_t max_temp = (data[0] << 8) | data[1];     // °C
        uint16_t min_temp = (data[2] << 8) | data[3];     // °C
        uint16_t max_v = (data[4] << 8) | data[5];        // mV (scale 0.001 V)
        uint16_t min_v = (data[6] << 8) | data[7];        // mV (scale 0.001 V)
        max_cell_temp_.store(max_temp, std::memory_order_relaxed);
        min_cell_temp_.store(min_temp, std::memory_order_relaxed);
        max_cell_voltage_.store(max_v, std::memory_order_relaxed);
        min_cell_voltage_.store(min_v, std::memory_order_relaxed);
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x370] Cell Extrema: MaxT=%u°C, MinT=%u°C, MaxV=%umV, MinV=%umV\n",
               max_temp, min_temp, max_v, min_v);
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


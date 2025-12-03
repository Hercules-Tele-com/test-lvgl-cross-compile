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

#ifdef EMBOO_BATTERY
    // ========================================================================
    // EMBOO BATTERY MESSAGES (Orion BMS / ENNOID-style)
    // ========================================================================

    // 0x6B0: Pack Status (voltage, current, SOC)
    else if (baseId == 0x6B0 && len >= 8) {
        // Current (big-endian, signed, 0.1A scale)
        int16_t current_raw = (int16_t)((data[0] << 8) | data[1]);
        float pack_current = current_raw * 0.1f;

        // Voltage (big-endian, 0.1V scale)
        uint16_t voltage_raw = (data[2] << 8) | data[3];
        float pack_voltage = voltage_raw * 0.1f;

        // SOC (0.5% scale)
        uint8_t soc_raw = data[6];
        float pack_soc = soc_raw * 0.5f;

        // Store values (convert to match existing format)
        battery_voltage_.store((uint16_t)(pack_voltage * 100), std::memory_order_relaxed); // V * 100
        battery_current_.store((int16_t)(pack_current * 10), std::memory_order_relaxed);   // A * 10
        soc_.store((uint16_t)pack_soc, std::memory_order_relaxed);                        // %

#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x6B0] Pack Status: %.1fV, %.1fA, %.1f%% SOC\n",
               pack_voltage, pack_current, pack_soc);
        fflush(stdout);
#endif
    }

    // 0x6B1: Pack Stats (min/max cell voltages and temps)
    else if (baseId == 0x6B1 && len >= 8) {
        // High temperature (1.0°C scale)
        uint8_t high_temp = data[2];

        // Pack summed voltage (big-endian, 0.01V scale)
        uint16_t summed_v_raw = (data[5] << 8) | data[6];
        float summed_voltage = summed_v_raw * 0.01f;

        battery_temperature_.store((uint16_t)(high_temp * 10), std::memory_order_relaxed); // °C * 10

#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x6B1] Pack Stats: HighTemp=%u°C, SumV=%.2fV\n",
               (unsigned)high_temp, summed_voltage);
        fflush(stdout);
#endif
    }

    // 0x6B2: Status Flags
    else if (baseId == 0x6B2 && len >= 8) {
        uint8_t status_flags = data[0];
        uint8_t error_flags = data[3];

#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x6B2] Status: 0x%02X, Errors: 0x%02X\n",
               (unsigned)status_flags, (unsigned)error_flags);
        fflush(stdout);
#endif
    }

    // 0x6B3: Individual Cell Voltages
    else if (baseId == 0x6B3 && len >= 8) {
        uint8_t cell_id = data[0];

        // Skip header frames (cell_id > 100 indicates status frames)
        if (cell_id <= 100) {
            // Cell voltage (big-endian, 0.0001V scale)
            uint16_t cell_v_raw = (data[1] << 8) | data[2];
            float cell_voltage = cell_v_raw * 0.0001f;

            // Cell resistance (15 bits, 0.01 mOhm) + balancing (1 bit)
            uint16_t resistance_raw = (data[3] << 8) | data[4];
            float cell_resistance = (resistance_raw & 0x7FFF) * 0.01f;
            bool cell_balancing = (resistance_raw & 0x8000) != 0;

            // Update min/max cell voltages if this is within range
            uint16_t cell_v_mv = (uint16_t)(cell_voltage * 1000);
            uint16_t current_max = max_cell_voltage_.load(std::memory_order_relaxed);
            uint16_t current_min = min_cell_voltage_.load(std::memory_order_relaxed);

            if (current_max == 0 || cell_v_mv > current_max) {
                max_cell_voltage_.store(cell_v_mv, std::memory_order_relaxed);
            }
            if (current_min == 0 || cell_v_mv < current_min) {
                min_cell_voltage_.store(cell_v_mv, std::memory_order_relaxed);
            }

#if CAN_DEBUG
            printf("[0x6B3] Cell %u: %.4fV, %.2fmΩ, Balancing: %d\n",
                   (unsigned)cell_id, cell_voltage, cell_resistance, cell_balancing);
            fflush(stdout);
#endif
        }
    }

    // 0x6B4: Temperature Data
    else if (baseId == 0x6B4 && len >= 8) {
        uint8_t high_temp = data[2];
        uint8_t low_temp = data[3];

        max_cell_temp_.store((uint16_t)high_temp, std::memory_order_relaxed);
        min_cell_temp_.store((uint16_t)low_temp, std::memory_order_relaxed);

#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x6B4] Temps: High=%u°C, Low=%u°C\n",
               (unsigned)high_temp, (unsigned)low_temp);
        fflush(stdout);
#endif
    }

    // 0x35A: Pack Data 3 (Additional pack data)
    else if (baseId == 0x35A && len >= 6) {
#if CAN_DEBUG || defined(PLATFORM_LINUX)
        printf("[0x35A] Pack Data 3 (raw bytes)\n");
        fflush(stdout);
#endif
    }
#endif // EMBOO_BATTERY
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


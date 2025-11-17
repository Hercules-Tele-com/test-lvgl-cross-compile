/**
 * Nissan Leaf BMS to Victron Protocol Converter
 *
 * This module reads Nissan Leaf battery CAN messages and converts them to
 * Victron/Pylontech protocol format, enabling Leaf batteries to work with
 * Victron solar inverters, chargers, and ESS systems.
 *
 * Victron Protocol CAN IDs:
 * - 0x351: Battery voltage, current, SOC, SOH
 * - 0x355: Battery temperatures (min, max, avg)
 * - 0x356: Charge/discharge current limits
 * - 0x35E: Battery alarms and warnings
 * - 0x35F: Battery manufacturer info
 * - 0x370-0x373: Cell module voltages (optional)
 * - 0x374-0x377: Cell module temperatures (optional)
 *
 * Hardware:
 * - ESP32 DevKit
 * - TJA1050 CAN transceiver
 * - GPIO 5 (TX), GPIO 4 (RX)
 * - 500 kbps CAN bus
 */

#include <Arduino.h>
#include <LeafCANBus.h>
#include <LeafCANMessages.h>

// BMS State (aggregated from Leaf messages)
struct BMSState {
    // Pack data (from 0x1DB)
    float pack_voltage;      // V
    float pack_current;      // A (positive = discharge, negative = charge)
    uint8_t soc_percent;     // 0-100%
    uint16_t gids;           // Leaf capacity units

    // Temperature data (from 0x1DC)
    int8_t temp_min;         // °C
    int8_t temp_max;         // °C
    int8_t temp_avg;         // °C

    // Current limits (calculated from battery state)
    float charge_current_max;    // A
    float discharge_current_max; // A

    // Battery health
    uint8_t soh_percent;     // State of Health (0-100%)

    // Status flags
    bool ready;              // Battery ready
    bool charging;           // Charging active
    uint32_t last_update;    // millis() of last update
} bmsState;

LeafCANBus canBus;

// Forward declarations
void sendVictronMessages();
void packVictron0x351(uint8_t* data);
void packVictron0x355(uint8_t* data);
void packVictron0x356(uint8_t* data);
void packVictron0x35E(uint8_t* data);
void packVictron0x35F(uint8_t* data);

// Unpack Leaf battery SOC message (0x1DB)
void unpack_leaf_battery_soc(const uint8_t* data, uint8_t len, void* state) {
    if (len < 7) return;

    BMSState* bms = (BMSState*)state;

    // Parse Nissan Leaf 0x1DB format
    bms->soc_percent = data[0];  // Byte 0: SOC (0-100%)
    bms->gids = (data[1] << 8) | data[2];  // Bytes 1-2: GIDS
    bms->pack_voltage = ((data[3] << 8) | data[4]) * 0.1f;  // Bytes 3-4: Voltage * 10
    bms->pack_current = (int16_t)((data[5] << 8) | data[6]) * 0.1f;  // Bytes 5-6: Current * 10 (signed)

    bms->last_update = millis();

    Serial.printf("[0x1DB] SOC: %u%%, Voltage: %.1fV, Current: %.1fA, GIDS: %u\n",
                  bms->soc_percent, bms->pack_voltage, bms->pack_current, bms->gids);
}

// Unpack Leaf battery temperature message (0x1DC)
void unpack_leaf_battery_temp(const uint8_t* data, uint8_t len, void* state) {
    if (len < 4) return;

    BMSState* bms = (BMSState*)state;

    // Parse Nissan Leaf 0x1DC format
    bms->temp_max = (int8_t)data[0];  // Byte 0: Max temp
    bms->temp_min = (int8_t)data[1];  // Byte 1: Min temp
    bms->temp_avg = (int8_t)data[2];  // Byte 2: Avg temp

    Serial.printf("[0x1DC] Temps: Min %d°C, Max %d°C, Avg %d°C\n",
                  bms->temp_min, bms->temp_max, bms->temp_avg);
}

// Unpack Leaf charger status (0x390)
void unpack_leaf_charger_status(const uint8_t* data, uint8_t len, void* state) {
    if (len < 1) return;

    BMSState* bms = (BMSState*)state;
    bms->charging = (data[0] & 0x01) != 0;

    Serial.printf("[0x390] Charging: %s\n", bms->charging ? "YES" : "NO");
}

// Pack Victron 0x351: Battery voltage, current, SOC
void packVictron0x351(uint8_t* data) {
    // Byte 0-1: Pack voltage (V * 10, uint16_t big-endian)
    uint16_t voltage = (uint16_t)(bmsState.pack_voltage * 10.0f);
    data[0] = (voltage >> 8) & 0xFF;
    data[1] = voltage & 0xFF;

    // Byte 2-3: Pack current (A * 10, int16_t big-endian, positive = discharge)
    int16_t current = (int16_t)(bmsState.pack_current * 10.0f);
    data[2] = (current >> 8) & 0xFF;
    data[3] = current & 0xFF;

    // Byte 4-5: SOC (% * 10, uint16_t big-endian)
    uint16_t soc = (uint16_t)(bmsState.soc_percent * 10);
    data[4] = (soc >> 8) & 0xFF;
    data[5] = soc & 0xFF;

    // Byte 6-7: SOH (% * 10, uint16_t big-endian)
    uint16_t soh = (uint16_t)(bmsState.soh_percent * 10);
    data[6] = (soh >> 8) & 0xFF;
    data[7] = soh & 0xFF;
}

// Pack Victron 0x355: Battery temperatures
void packVictron0x355(uint8_t* data) {
    // Byte 0-1: Highest temperature (°C * 10, int16_t big-endian)
    int16_t temp_max = (int16_t)(bmsState.temp_max * 10);
    data[0] = (temp_max >> 8) & 0xFF;
    data[1] = temp_max & 0xFF;

    // Byte 2-3: Lowest temperature (°C * 10, int16_t big-endian)
    int16_t temp_min = (int16_t)(bmsState.temp_min * 10);
    data[2] = (temp_min >> 8) & 0xFF;
    data[3] = temp_min & 0xFF;

    // Byte 4-5: Average temperature (°C * 10, int16_t big-endian)
    int16_t temp_avg = (int16_t)(bmsState.temp_avg * 10);
    data[4] = (temp_avg >> 8) & 0xFF;
    data[5] = temp_avg & 0xFF;

    // Byte 6-7: Reserved
    data[6] = 0;
    data[7] = 0;
}

// Pack Victron 0x356: Charge/discharge current limits
void packVictron0x356(uint8_t* data) {
    // Calculate limits based on battery state
    // Leaf battery typical limits: 150A discharge, 50A charge
    // Adjust based on temperature and SOC

    float charge_limit = 50.0f;  // Base charge current limit
    float discharge_limit = 150.0f;  // Base discharge current limit

    // Reduce charging at high SOC
    if (bmsState.soc_percent > 95) {
        charge_limit *= 0.5f;  // 50% at >95%
    } else if (bmsState.soc_percent > 90) {
        charge_limit *= 0.7f;  // 70% at >90%
    }

    // Reduce discharge at low SOC
    if (bmsState.soc_percent < 10) {
        discharge_limit *= 0.3f;  // 30% at <10%
    } else if (bmsState.soc_percent < 20) {
        discharge_limit *= 0.5f;  // 50% at <20%
    }

    // Temperature derating
    if (bmsState.temp_max > 45 || bmsState.temp_min < 0) {
        charge_limit *= 0.5f;
        discharge_limit *= 0.7f;
    }

    // Byte 0-1: Max charge current (A * 10, uint16_t big-endian)
    uint16_t ichg = (uint16_t)(charge_limit * 10.0f);
    data[0] = (ichg >> 8) & 0xFF;
    data[1] = ichg & 0xFF;

    // Byte 2-3: Max discharge current (A * 10, uint16_t big-endian)
    uint16_t idis = (uint16_t)(discharge_limit * 10.0f);
    data[2] = (idis >> 8) & 0xFF;
    data[3] = idis & 0xFF;

    // Byte 4-5: Max charge voltage (V * 10, uint16_t big-endian)
    uint16_t vchg = 4040;  // 404.0V (typical for 96S Leaf pack)
    data[4] = (vchg >> 8) & 0xFF;
    data[5] = vchg & 0xFF;

    // Byte 6-7: Min discharge voltage (V * 10, uint16_t big-endian)
    uint16_t vdis = 2880;  // 288.0V (typical cutoff for 96S)
    data[6] = (vdis >> 8) & 0xFF;
    data[7] = vdis & 0xFF;
}

// Pack Victron 0x35E: Alarms and warnings
void packVictron0x35E(uint8_t* data) {
    memset(data, 0, 8);

    // Byte 0: General alarms
    if (bmsState.soc_percent < 10) data[0] |= 0x01;  // Low SOC alarm
    if (bmsState.temp_max > 50) data[0] |= 0x04;     // High temp alarm
    if (bmsState.temp_min < -10) data[0] |= 0x08;    // Low temp alarm

    // Byte 1: Warnings
    if (bmsState.soc_percent < 20) data[1] |= 0x01;  // Low SOC warning
    if (bmsState.temp_max > 45) data[1] |= 0x04;     // High temp warning
    if (bmsState.temp_min < 0) data[1] |= 0x08;      // Low temp warning

    // Byte 2-3: Number of modules (96 cells = 32 modules of 3 cells each)
    data[2] = 0;
    data[3] = 32;

    // Byte 4-7: Reserved
    data[4] = 0;
    data[5] = 0;
    data[6] = 0;
    data[7] = 0;
}

// Pack Victron 0x35F: Manufacturer info
void packVictron0x35F(uint8_t* data) {
    // Identify as Nissan Leaf BMS
    data[0] = 'N';  // Manufacturer: Nissan
    data[1] = 'I';
    data[2] = 'S';
    data[3] = 'S';
    data[4] = 'A';
    data[5] = 'N';
    data[6] = 0;
    data[7] = 0;
}

// Send all Victron protocol messages
void sendVictronMessages() {
    uint8_t data[8];

    // 0x351: Battery voltage, current, SOC
    packVictron0x351(data);
    canBus.sendMessage(0x351, data, 8);

    // 0x355: Battery temperatures
    packVictron0x355(data);
    canBus.sendMessage(0x355, data, 8);

    // 0x356: Charge/discharge limits
    packVictron0x356(data);
    canBus.sendMessage(0x356, data, 8);

    // 0x35E: Alarms and warnings
    packVictron0x35E(data);
    canBus.sendMessage(0x35E, data, 8);

    // 0x35F: Manufacturer info (send less frequently)
    static uint32_t last_info_send = 0;
    if (millis() - last_info_send > 5000) {  // Every 5 seconds
        packVictron0x35F(data);
        canBus.sendMessage(0x35F, data, 8);
        last_info_send = millis();
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== Nissan Leaf BMS to Victron Protocol ===");
    Serial.println("Version 1.0");
    Serial.println("CAN Bus: 500 kbps, GPIO 5 (TX), GPIO 4 (RX)");
    Serial.println();

    // Initialize BMS state
    memset(&bmsState, 0, sizeof(BMSState));
    bmsState.pack_voltage = 360.0f;  // Default nominal voltage
    bmsState.soc_percent = 50;       // Default SOC
    bmsState.soh_percent = 85;       // Default SOH (Leaf batteries degrade)
    bmsState.temp_avg = 25;          // Default temp
    bmsState.temp_min = 25;
    bmsState.temp_max = 25;

    // Initialize CAN bus
    if (!canBus.begin()) {
        Serial.println("ERROR: CAN bus initialization failed!");
        Serial.println("Check wiring: GPIO 5 (TX), GPIO 4 (RX), TJA1050 transceiver");
        while (1) delay(1000);
    }

    Serial.println("CAN bus initialized successfully");

    // Subscribe to Nissan Leaf battery messages
    canBus.subscribe(CAN_ID_BATTERY_SOC, unpack_leaf_battery_soc, &bmsState);
    canBus.subscribe(CAN_ID_BATTERY_TEMP, unpack_leaf_battery_temp, &bmsState);
    canBus.subscribe(CAN_ID_CHARGER_STATUS, unpack_leaf_charger_status, &bmsState);

    Serial.println("Subscribed to Leaf battery messages:");
    Serial.printf("  0x%03X - Battery SOC\n", CAN_ID_BATTERY_SOC);
    Serial.printf("  0x%03X - Battery Temperature\n", CAN_ID_BATTERY_TEMP);
    Serial.printf("  0x%03X - Charger Status\n", CAN_ID_CHARGER_STATUS);
    Serial.println();
    Serial.println("Publishing Victron protocol messages:");
    Serial.println("  0x351 - Pack voltage, current, SOC, SOH");
    Serial.println("  0x355 - Battery temperatures");
    Serial.println("  0x356 - Charge/discharge limits");
    Serial.println("  0x35E - Alarms and warnings");
    Serial.println("  0x35F - Manufacturer info");
    Serial.println();
    Serial.println("Ready to convert Leaf → Victron protocol...");
    Serial.println("==========================================\n");
}

void loop() {
    // Process incoming CAN messages from Leaf
    canBus.process();

    // Send Victron protocol messages every 1000ms
    static uint32_t last_victron_send = 0;
    if (millis() - last_victron_send >= 1000) {
        sendVictronMessages();
        last_victron_send = millis();

        // Print status every 5 seconds
        static uint32_t last_status_print = 0;
        if (millis() - last_status_print >= 5000) {
            Serial.println("--- BMS Status ---");
            Serial.printf("Pack: %.1fV, %.1fA\n", bmsState.pack_voltage, bmsState.pack_current);
            Serial.printf("SOC: %u%%, SOH: %u%%\n", bmsState.soc_percent, bmsState.soh_percent);
            Serial.printf("Temps: %d°C / %d°C / %d°C (min/avg/max)\n",
                         bmsState.temp_min, bmsState.temp_avg, bmsState.temp_max);
            Serial.printf("Charging: %s\n", bmsState.charging ? "YES" : "NO");
            Serial.printf("Last update: %lu ms ago\n", millis() - bmsState.last_update);
            Serial.println("------------------\n");
            last_status_print = millis();
        }
    }

    // Watchdog: warn if no data received for 5 seconds
    if (bmsState.last_update > 0 && (millis() - bmsState.last_update) > 5000) {
        static uint32_t last_warn = 0;
        if (millis() - last_warn > 10000) {
            Serial.println("WARNING: No Leaf battery data received for >5 seconds!");
            Serial.println("Check CAN connections to vehicle.");
            last_warn = millis();
        }
    }

    delay(10);
}

#include "dashboard_ui.h"
#include <algorithm>
#include <cstdio>

static constexpr int SPEED_MAX_KMH   = 160;
static constexpr int TEMP_MIN_C      = -20;
static constexpr int TEMP_MAX_C      = 140;

DashboardUI::DashboardUI() = default;
DashboardUI::~DashboardUI() = default;

void DashboardUI::init() {
    // SquareLine already created objects via ui_init()
    speed_arc          = ui_guage_speed;
    soc_arc_primary    = ui_guage_battery_soc;
    motor_temp_bar     = ui_guage_temp_motor;
    inverter_temp_bar  = ui_guage_temp_inverter;

    // One-time breadcrumb so we know pointers are valid
    printf("[UI] bind: speed_arc=%p, soc_arc=%p, mtemp=%p, itemp=%p\n",
           (void*)speed_arc, (void*)soc_arc_primary,
           (void*)motor_temp_bar, (void*)inverter_temp_bar);

    // Ranges
    if (speed_arc)         lv_arc_set_range(speed_arc, 0, SPEED_MAX_KMH);
    if (soc_arc_primary)   lv_arc_set_range(soc_arc_primary, 0, 100);
    if (motor_temp_bar)    lv_bar_set_range(motor_temp_bar, TEMP_MIN_C, TEMP_MAX_C);
    if (inverter_temp_bar) lv_bar_set_range(inverter_temp_bar, TEMP_MIN_C, TEMP_MAX_C);

    // Center labels over arcs
    if (soc_arc_primary && !soc_value_label) {
        soc_value_label = lv_label_create(lv_obj_get_parent(soc_arc_primary));
        lv_label_set_text(soc_value_label, "0%");
        lv_obj_align_to(soc_value_label, soc_arc_primary, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_align(soc_value_label, LV_TEXT_ALIGN_CENTER, 0);
    }
    if (speed_arc && !speed_value_label) {
        speed_value_label = lv_label_create(lv_obj_get_parent(speed_arc));
        lv_label_set_text(speed_value_label, "0");
        lv_obj_align_to(speed_value_label, speed_arc, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_align(speed_value_label, LV_TEXT_ALIGN_CENTER, 0);
    }
}

void DashboardUI::update(const CANReceiver& can) {
    // Get all CAN data (values are scaled, need to convert)
    uint16_t soc_raw = can.getSOC();              // % * 10
    uint16_t speed_raw = can.getSpeed();          // kph * 100
    int16_t rpm = can.getMotorRPM();
    uint8_t motor_temp = can.getMotorTemp();
    uint8_t inv_temp = can.getInverterTemp();

    // Convert scaled values to display units
    uint8_t soc = soc_raw / 10;                   // % * 10 → %
    float speed = speed_raw / 100.0f;             // kph * 100 → kph

    // Update battery SoC
    if (soc_arc_primary) {
        lv_arc_set_value(soc_arc_primary, soc);
    }
    if (soc_value_label) {
        lv_label_set_text_fmt(soc_value_label, "%u%%", (unsigned)soc);
        lv_obj_align_to(soc_value_label, soc_arc_primary, LV_ALIGN_CENTER, 0, 0);
    }

    // Update speed gauge
    updateSpeedGauge(speed);

    // Update motor RPM
    updateMotorRPM(rpm);

    // Update temperatures
    updateTemperatures(motor_temp, inv_temp);

    if (status_label) lv_label_set_text(status_label, "Running");
}

void DashboardUI::updateTime(uint8_t h, uint8_t m, uint8_t s) {
    if (!time_label) return;
    lv_label_set_text_fmt(time_label, "%02d:%02d:%02d", h, m, s);
}

void DashboardUI::updateSpeedGauge(float speed_kmh) {
    const int v = std::max(0, std::min((int)speed_kmh, SPEED_MAX_KMH));
    if (speed_arc)         lv_arc_set_value(speed_arc, v);
    if (speed_value_label) {
        lv_label_set_text_fmt(speed_value_label, "%d", v);
        lv_obj_align_to(speed_value_label, speed_arc, LV_ALIGN_CENTER, 0, 0);
    }
}

void DashboardUI::updateBatterySOC(uint8_t soc_percent) {
    const uint8_t clamped = (soc_percent > 100) ? 100 : soc_percent;
    if (soc_arc_primary)   lv_arc_set_value(soc_arc_primary, clamped);
    if (soc_value_label) {
        lv_label_set_text_fmt(soc_value_label, "%u%%", (unsigned)clamped);
        lv_obj_align_to(soc_value_label, soc_arc_primary, LV_ALIGN_CENTER, 0, 0);
    }
    if (soc_bar)           lv_bar_set_value(soc_bar, clamped, LV_ANIM_ON);
    if (soc_label)         lv_label_set_text_fmt(soc_label, "%u%%", (unsigned)clamped);
}

void DashboardUI::updateMotorRPM(int16_t rpm) {
    if (rpm_label) lv_label_set_text_fmt(rpm_label, "%d", rpm);
}

void DashboardUI::updateTemperatures(int16_t motor_temp_c, int16_t inverter_temp_c) {
    if (motor_temp_bar) {
        int v = std::max(TEMP_MIN_C, std::min((int)motor_temp_c, TEMP_MAX_C));
        lv_bar_set_value(motor_temp_bar, v, LV_ANIM_ON);
    }
    if (inverter_temp_bar) {
        int v = std::max(TEMP_MIN_C, std::min((int)inverter_temp_c, TEMP_MAX_C));
        lv_bar_set_value(inverter_temp_bar, v, LV_ANIM_ON);
    }
    if (motor_temp_label)    lv_label_set_text_fmt(motor_temp_label, "Motor: %d°C", (int)motor_temp_c);
    if (inverter_temp_label) lv_label_set_text_fmt(inverter_temp_label, "Inv: %d°C", (int)inverter_temp_c);
}


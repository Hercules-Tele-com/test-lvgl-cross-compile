#include "dashboard_ui.h"
#include <algorithm>
#include <cstdio>

// ---- Tunables ----
static constexpr int SPEED_MAX_KMH   = 160;   // left arc max
static constexpr int TEMP_MIN_C      = -20;   // temp bars range
static constexpr int TEMP_MAX_C      = 140;

// ------------------
// Lifecycle
// ------------------
DashboardUI::DashboardUI() = default;
DashboardUI::~DashboardUI() = default;

void DashboardUI::init() {
    // SquareLine has already built the screen via ui_init()
    // Bind widgets generated in ui_screen_main.c
    speed_arc          = ui_guage_speed;           // LEFT: speed arc
    soc_arc_primary    = ui_guage_battery_soc;     // RIGHT: SoC arc
    motor_temp_bar     = ui_guage_temp_motor;      // temp bars
    inverter_temp_bar  = ui_guage_temp_inverter;

    // Ranges
    if (speed_arc)          lv_arc_set_range(speed_arc, 0, SPEED_MAX_KMH);
    if (soc_arc_primary)    lv_arc_set_range(soc_arc_primary, 0, 100);
    if (motor_temp_bar)     lv_bar_set_range(motor_temp_bar, TEMP_MIN_C, TEMP_MAX_C);
    if (inverter_temp_bar)  lv_bar_set_range(inverter_temp_bar, TEMP_MIN_C, TEMP_MAX_C);

    // Create centered numeric labels as SIBLINGS of each arc
    if (speed_arc && !speed_value_label) {
        speed_value_label = lv_label_create(lv_obj_get_parent(speed_arc));
        lv_label_set_text(speed_value_label, "0");
        lv_obj_align_to(speed_value_label, speed_arc, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_align(speed_value_label, LV_TEXT_ALIGN_CENTER, 0);
    }
    if (soc_arc_primary && !soc_value_label) {
        soc_value_label = lv_label_create(lv_obj_get_parent(soc_arc_primary));
        lv_label_set_text(soc_value_label, "0%");
        lv_obj_align_to(soc_value_label, soc_arc_primary, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_align(soc_value_label, LV_TEXT_ALIGN_CENTER, 0);
    }
}

void DashboardUI::createLayout() {
    // Deprecated – SquareLine UI replaces custom layout.
}

// ------------------
// Updaters
// ------------------
void DashboardUI::updateSpeedGauge(float speed_kmh) {
    const int v = std::max(0, std::min((int)speed_kmh, SPEED_MAX_KMH));

    if (speed_arc) {
        lv_arc_set_value(speed_arc, v);
    }
    if (speed_value_label) {
        lv_label_set_text_fmt(speed_value_label, "%d", v);
        // keep centered even if layout changes
        lv_obj_align_to(speed_value_label, speed_arc, LV_ALIGN_CENTER, 0, 0);
    }

    // Legacy fallback (if no arc)
    if (!speed_arc && speed_indic && speed_meter) {
        lv_meter_set_indicator_value(speed_meter, speed_indic, (int32_t)v);
    }
}

void DashboardUI::updateBatterySOC(uint8_t soc_percent) {
    const uint8_t clamped = (soc_percent > 100) ? 100 : soc_percent;

    if (soc_arc_primary) {
        lv_arc_set_value(soc_arc_primary, clamped);
    }
    if (soc_value_label) {
        lv_label_set_text_fmt(soc_value_label, "%u%%", (unsigned)clamped);
        lv_obj_align_to(soc_value_label, soc_arc_primary, LV_ALIGN_CENTER, 0, 0);
    }

    if (soc_bar) {
        lv_bar_set_value(soc_bar, clamped, LV_ANIM_ON);
    }
    if (soc_label) {
        lv_label_set_text_fmt(soc_label, "%u%%", (unsigned)clamped);
    }
}

void DashboardUI::updateMotorRPM(int16_t rpm) {
    if (rpm_label) {
        lv_label_set_text_fmt(rpm_label, "%d", rpm);
    }
}

void DashboardUI::updateTemperatures(int16_t motor_temp_c, int16_t inverter_temp_c) {
    // Bars
    if (motor_temp_bar) {
        int v = std::max(TEMP_MIN_C, std::min((int)motor_temp_c, TEMP_MAX_C));
        lv_bar_set_value(motor_temp_bar, v, LV_ANIM_ON);
    }
    if (inverter_temp_bar) {
        int v = std::max(TEMP_MIN_C, std::min((int)inverter_temp_c, TEMP_MAX_C));
        lv_bar_set_value(inverter_temp_bar, v, LV_ANIM_ON);
    }
    // Optional text labels (if you later bind them)
    if (motor_temp_label) {
        lv_label_set_text_fmt(motor_temp_label, "Motor: %d°C", (int)motor_temp_c);
    }
    if (inverter_temp_label) {
        lv_label_set_text_fmt(inverter_temp_label, "Inv: %d°C", (int)inverter_temp_c);
    }
}

void DashboardUI::updateTime(uint8_t h, uint8_t m, uint8_t s) {
    if (!time_label) return;
    lv_label_set_text_fmt(time_label, "%02d:%02d:%02d", h, m, s);
}

void DashboardUI::update(const CANReceiver& /*can*/) {
    // TEMP: no CAN getters until we wire decoders.
    if (status_label) lv_label_set_text(status_label, "Running");
}


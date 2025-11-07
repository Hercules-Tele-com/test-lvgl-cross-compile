#include "dashboard_ui.h"
#include <stdio.h>
#include <algorithm>

// Make all pointers null; keep update methods harmless no-ops.
// This lets existing calls compile without drawing the old layout.

DashboardUI::DashboardUI() :
    screen(nullptr),
    speed_meter(nullptr),
    soc_bar(nullptr),
    soc_label(nullptr),
    rpm_label(nullptr),
    motor_temp_label(nullptr),
    inverter_temp_label(nullptr),
    gps_label(nullptr),
    status_label(nullptr),
    time_label(nullptr),
    speed_indic(nullptr)
{
}

DashboardUI::~DashboardUI() {
}

void DashboardUI::init() {
    // Intentionally do NOT create the old layout anymore.
    // SquareLine's ui_init() now builds and loads the new screen.
    // createLayout();  // <- removed on purpose
}

void DashboardUI::createLayout() {
    // Deprecated: left here only for reference. Not called anymore.
    // (SquareLine-generated UI replaces this.)
}

void DashboardUI::updateSpeedGauge(float speed) {
    if (speed_indic && speed_meter) {
        lv_meter_set_indicator_value(speed_meter, speed_indic, (int32_t)speed);
    }
}

void DashboardUI::updateBatterySOC(uint8_t soc) {
    if (soc_bar) {
        lv_bar_set_value(soc_bar, soc, LV_ANIM_ON);
    }
    if (soc_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u%%", (unsigned)soc);
        lv_label_set_text(soc_label, buf);
    }
}

void DashboardUI::updateMotorRPM(int16_t rpm) {
    if (rpm_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", rpm);
        lv_label_set_text(rpm_label, buf);
    }
}

void DashboardUI::updateTemperatures(int16_t motor_temp, int16_t inverter_temp) {
    if (motor_temp_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Motor: %d°C", motor_temp);
        lv_label_set_text(motor_temp_label, buf);
    }
    if (inverter_temp_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Inv: %d°C", inverter_temp);
        lv_label_set_text(inverter_temp_label, buf);
    }
}

void DashboardUI::updateGPSInfo(const GPSPositionState& pos, const GPSVelocityState& vel) {
    if (gps_label) {
        char buf[128];
        if (pos.satellites > 0 && pos.fix_quality > 0) {
            snprintf(buf, sizeof(buf), "Lat: %.6f  Lon: %.6f  Sats: %d  Hdg: %.1f°",
                     pos.latitude, pos.longitude, pos.satellites, vel.heading);
        } else {
            snprintf(buf, sizeof(buf), "No GPS fix (Satellites: %d)", pos.satellites);
        }
        lv_label_set_text(gps_label, buf);
    }
}

void DashboardUI::updateTime(uint8_t hours, uint8_t minutes, uint8_t seconds) {
    if (time_label) {
        uint8_t brightness = (uint8_t)std::min<int>(255, (seconds * 2) + 64);
        lv_color_t yellow = lv_color_make(brightness, brightness, 0);
        lv_label_set_text_fmt(time_label, "%02d:%02d:%02d", hours, minutes, seconds);
        lv_obj_set_style_text_color(time_label, yellow, 0);
        lv_obj_invalidate(time_label);
        lv_refr_now(NULL);
    }
}

void DashboardUI::update(const CANReceiver& can) {
    // Harmless no-ops unless you later bind these to SquareLine widgets.
    updateSpeedGauge(can.getVehicleSpeedState().speed_kmh);
    updateBatterySOC(can.getBatterySOCState().soc_percent);
    updateMotorRPM(can.getMotorRPMState().rpm);
    updateTemperatures(can.getInverterState().temp_motor, can.getInverterState().temp_inverter);
    updateGPSInfo(can.getGPSPositionState(), can.getGPSVelocityState());

    if (status_label) {
        lv_label_set_text(status_label, "Running");
    }
}

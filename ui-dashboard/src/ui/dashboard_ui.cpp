#include "dashboard_ui.h"
#include <stdio.h>

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
    createLayout();
}

void DashboardUI::createLayout() {
    // Create main screen
    screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0000FF), 0);

    // Title
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "NISSAN LEAF CAN DASHBOARD");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Speed meter (center-left)
    speed_meter = lv_meter_create(screen);
    lv_obj_set_size(speed_meter, 200, 200);
    lv_obj_align(speed_meter, LV_ALIGN_LEFT_MID, 20, 0);

    // Add speed scale
    lv_meter_scale_t* scale = lv_meter_add_scale(speed_meter);
    lv_meter_set_scale_ticks(speed_meter, scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(speed_meter, scale, 8, 4, 15, lv_color_hex(0xFFFFFF), 10);
    lv_meter_set_scale_range(speed_meter, scale, 0, 160, 270, 135);

    // Add speed needle
    speed_indic = lv_meter_add_needle_line(speed_meter, scale, 4, lv_palette_main(LV_PALETTE_BLUE), -10);

    // Speed label
    lv_obj_t* speed_label = lv_label_create(screen);
    lv_label_set_text(speed_label, "SPEED (km/h)");
    lv_obj_set_style_text_color(speed_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(speed_label, speed_meter, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    // Battery SOC bar (right side)
    lv_obj_t* soc_container = lv_obj_create(screen);
    lv_obj_set_size(soc_container, 200, 100);
    lv_obj_align(soc_container, LV_ALIGN_TOP_RIGHT, -20, 60);
    lv_obj_set_style_bg_color(soc_container, lv_color_hex(0x1A1A1A), 0);

    lv_obj_t* soc_title = lv_label_create(soc_container);
    lv_label_set_text(soc_title, "BATTERY SOC");
    lv_obj_set_style_text_color(soc_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(soc_title, LV_ALIGN_TOP_MID, 0, 5);

    soc_bar = lv_bar_create(soc_container);
    lv_obj_set_size(soc_bar, 180, 30);
    lv_obj_align(soc_bar, LV_ALIGN_CENTER, 0, 10);
    lv_bar_set_range(soc_bar, 0, 100);
    lv_bar_set_value(soc_bar, 0, LV_ANIM_OFF);

    soc_label = lv_label_create(soc_container);
    lv_label_set_text(soc_label, "0%");
    lv_obj_set_style_text_font(soc_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(soc_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(soc_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    // Motor RPM (right side)
    lv_obj_t* rpm_container = lv_obj_create(screen);
    lv_obj_set_size(rpm_container, 200, 80);
    lv_obj_align(rpm_container, LV_ALIGN_TOP_RIGHT, -20, 180);
    lv_obj_set_style_bg_color(rpm_container, lv_color_hex(0x1A1A1A), 0);

    lv_obj_t* rpm_title = lv_label_create(rpm_container);
    lv_label_set_text(rpm_title, "MOTOR RPM");
    lv_obj_set_style_text_color(rpm_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(rpm_title, LV_ALIGN_TOP_MID, 0, 5);

    rpm_label = lv_label_create(rpm_container);
    lv_label_set_text(rpm_label, "0");
    lv_obj_set_style_text_font(rpm_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(rpm_label, lv_color_hex(0x00FF00), 0);
    lv_obj_align(rpm_label, LV_ALIGN_CENTER, 0, 5);

    // Temperature display (right side)
    lv_obj_t* temp_container = lv_obj_create(screen);
    lv_obj_set_size(temp_container, 200, 100);
    lv_obj_align(temp_container, LV_ALIGN_TOP_RIGHT, -20, 280);
    lv_obj_set_style_bg_color(temp_container, lv_color_hex(0x1A1A1A), 0);

    lv_obj_t* temp_title = lv_label_create(temp_container);
    lv_label_set_text(temp_title, "TEMPERATURES");
    lv_obj_set_style_text_color(temp_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(temp_title, LV_ALIGN_TOP_MID, 0, 5);

    motor_temp_label = lv_label_create(temp_container);
    lv_label_set_text(motor_temp_label, "Motor: --°C");
    lv_obj_set_style_text_color(motor_temp_label, lv_color_hex(0xFFFF00), 0);
    lv_obj_align(motor_temp_label, LV_ALIGN_TOP_LEFT, 10, 30);

    inverter_temp_label = lv_label_create(temp_container);
    lv_label_set_text(inverter_temp_label, "Inv: --°C");
    lv_obj_set_style_text_color(inverter_temp_label, lv_color_hex(0xFFFF00), 0);
    lv_obj_align(inverter_temp_label, LV_ALIGN_TOP_LEFT, 10, 55);

    // GPS info (bottom)
    lv_obj_t* gps_container = lv_obj_create(screen);
    lv_obj_set_size(gps_container, 400, 80);
    lv_obj_align(gps_container, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(gps_container, lv_color_hex(0x1A1A1A), 0);

    lv_obj_t* gps_title = lv_label_create(gps_container);
    lv_label_set_text(gps_title, "GPS");
    lv_obj_set_style_text_color(gps_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(gps_title, LV_ALIGN_TOP_LEFT, 10, 5);

    gps_label = lv_label_create(gps_container);
    lv_label_set_text(gps_label, "No GPS fix");
    lv_obj_set_style_text_color(gps_label, lv_color_hex(0x888888), 0);
    lv_obj_align(gps_label, LV_ALIGN_TOP_LEFT, 10, 30);

    // Status label (top right corner)
    status_label = lv_label_create(screen);
    lv_label_set_text(status_label, "Initializing...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF00), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -10, 10);

    // Time label (centered horizontally, middle of screen vertically)
    time_label = lv_label_create(screen);
    lv_label_set_text(time_label, "00:00:00");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);  // Closest available to 55pt
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_bg_opa(time_label, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_pad_all(time_label, 0, 0);  // No padding
    lv_obj_set_size(time_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);  // Auto-size to content
    lv_obj_align(time_label, LV_ALIGN_CENTER, 80, -20);  // Offset to right of speed meter

    // Initialize time style for dynamic brightness
    lv_style_init(&time_style);
    lv_style_set_text_font(&time_style, &lv_font_montserrat_48);
}

void DashboardUI::updateSpeedGauge(float speed) {
    if (speed_indic) {
        lv_meter_set_indicator_value(speed_meter, speed_indic, (int32_t)speed);
    }
}

void DashboardUI::updateBatterySOC(uint8_t soc) {
    if (soc_bar) {
        lv_bar_set_value(soc_bar, soc, LV_ANIM_ON);
    }
    if (soc_label) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d%%", soc);
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
    static uint8_t last_second = 255;

    if (time_label) {
        // Calculate brightness: twice the number of seconds + 64
        uint8_t brightness = (seconds * 2) + 64;
        // Clamp to valid range (0-255)
        if (brightness > 255) brightness = 255;

        // Create yellow color with dynamic brightness
        lv_color_t yellow = lv_color_make(brightness, brightness, 0);

        // Update text and style
        lv_label_set_text_fmt(time_label, "%02d:%02d:%02d", hours, minutes, seconds);
        lv_obj_set_style_text_color(time_label, yellow, 0);

        // Force the object to be redrawn
        lv_obj_invalidate(time_label);
        lv_refr_now(NULL);
    }
}

void DashboardUI::update(const CANReceiver& can) {
    // Update all UI elements from CAN data
    updateSpeedGauge(can.getVehicleSpeedState().speed_kmh);
    updateBatterySOC(can.getBatterySOCState().soc_percent);
    updateMotorRPM(can.getMotorRPMState().rpm);
    updateTemperatures(can.getInverterState().temp_motor, can.getInverterState().temp_inverter);
    updateGPSInfo(can.getGPSPositionState(), can.getGPSVelocityState());

    // Update status
    if (status_label) {
        lv_label_set_text(status_label, "Running");
    }
}

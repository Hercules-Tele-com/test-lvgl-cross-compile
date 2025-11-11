#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include <cstdint>
#include "lvgl.h"
#include "can_receiver.h"
#include "ui.h"   // SquareLine globals (ui_guage_speed, ui_guage_battery_soc, etc.)

class DashboardUI {
public:
    DashboardUI();
    ~DashboardUI();

    void init();                     // bind SquareLine widgets
    void update(const CANReceiver&); // (temporary) no CAN getters used yet
    void updateTime(uint8_t h, uint8_t m, uint8_t s);

private:
    void createLayout();               // deprecated (SquareLine replaces this)
    void updateSpeedGauge(float speed_kmh);
    void updateBatterySOC(uint8_t soc_percent);
    void updateMotorRPM(int16_t rpm);
    void updateTemperatures(int16_t motor_temp_c, int16_t inverter_temp_c);

    // Legacy (kept for fallback)
    lv_obj_t* speed_meter = nullptr;
    lv_meter_indicator_t* speed_indic = nullptr;

    // SquareLine arcs (from ui_screen_main.c)
    lv_obj_t* speed_arc = nullptr;          // ui_guage_speed
    lv_obj_t* soc_arc_primary = nullptr;    // ui_guage_battery_soc

    // SquareLine bars (temps)
    lv_obj_t* motor_temp_bar = nullptr;     // ui_guage_temp_motor
    lv_obj_t* inverter_temp_bar = nullptr;  // ui_guage_temp_inverter

    // Centered numeric labels we create on top of arcs
    lv_obj_t* speed_value_label = nullptr;
    lv_obj_t* soc_value_label   = nullptr;

    // Optional labels if you add them later
    lv_obj_t* soc_bar = nullptr;
    lv_obj_t* soc_label = nullptr;
    lv_obj_t* rpm_label = nullptr;
    lv_obj_t* motor_temp_label = nullptr;
    lv_obj_t* inverter_temp_label = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* time_label = nullptr;
};

#endif // DASHBOARD_UI_H


#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include <cstdint>
#include "lvgl.h"
#include "ui.h"           // SquareLine globals
#include "can_receiver.h"

class DashboardUI {
public:
    DashboardUI();
    ~DashboardUI();

    void init();                     // bind SquareLine widgets
    void update(const CANReceiver&); // push data into widgets
    void updateTime(uint8_t h, uint8_t m, uint8_t s);

private:
    // Helpers
    void updateSpeedGauge(float speed_kmh);
    void updateBatterySOC(uint8_t soc_percent);
    void updateMotorRPM(int16_t rpm);
    void updateTemperatures(int16_t motor_temp_c, int16_t inverter_temp_c);

    // Bound widgets
    lv_obj_t* speed_arc = nullptr;          // ui_guage_speed
    lv_obj_t* soc_arc_primary = nullptr;    // ui_guage_battery_soc
    lv_obj_t* motor_temp_bar = nullptr;     // ui_guage_temp_motor
    lv_obj_t* inverter_temp_bar = nullptr;  // ui_guage_temp_inverter

    // Centered labels (created dynamically)
    lv_obj_t* speed_value_label = nullptr;
    lv_obj_t* soc_value_label   = nullptr;

    // Optional labels (safe if null)
    lv_obj_t* soc_bar = nullptr;
    lv_obj_t* soc_label = nullptr;
    lv_obj_t* rpm_label = nullptr;
    lv_obj_t* motor_temp_label = nullptr;
    lv_obj_t* inverter_temp_label = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* time_label = nullptr;
};

#endif // DASHBOARD_UI_H


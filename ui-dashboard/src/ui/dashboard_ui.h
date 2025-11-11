#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include <cstdint>
#include "lvgl.h"
#include "can_receiver.h"

class DashboardUI {
public:
    DashboardUI();
    ~DashboardUI();

    void init();                       // bind SquareLine widgets
    void update(const CANReceiver&);   // push CAN state to UI
    void updateTime(uint8_t h, uint8_t m, uint8_t s);

private:
    void createLayout();               // deprecated (SquareLine replaces this)
    void updateSpeedGauge(float speed_kmh);
    void updateBatterySOC(uint8_t soc_percent);
    void updateMotorRPM(int16_t rpm);
    void updateTemperatures(int16_t motor_temp_c, int16_t inverter_temp_c);
    void updateGPSInfo(const GPSPositionState& pos, const GPSVelocityState& vel);

    // Legacy (kept for fallback)
    lv_obj_t* speed_meter = nullptr;
    lv_meter_indicator_t* speed_indic = nullptr;

    // SquareLine arcs (from ui_screen_main.c)
    lv_obj_t* speed_arc = nullptr;          // ui_guage_soc (left) – speed
    lv_obj_t* soc_arc_primary = nullptr;    // ui_guage_battery_soc (right) – SoC

    // SquareLine bars (temps)
    lv_obj_t* motor_temp_bar = nullptr;     // ui_guage_temp_motor
    lv_obj_t* inverter_temp_bar = nullptr;  // ui_guage_temp_inverter

    // Centered numeric labels on arcs
    lv_obj_t* speed_value_label = nullptr;
    lv_obj_t* soc_value_label = nullptr;

    // Optional fallbacks / misc
    lv_obj_t* soc_bar = nullptr;
    lv_obj_t* soc_label = nullptr;

    lv_obj_t* rpm_label = nullptr;
    lv_obj_t* motor_temp_label = nullptr;
    lv_obj_t* inverter_temp_label = nullptr;
    lv_obj_t* gps_label = nullptr;
    lv_obj_t* status_label = nullptr;
    lv_obj_t* time_label = nullptr;
};

#endif // DASHBOARD_UI_H

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
    void updateSpeedDisplay(float speed_kmh);
    void updateBatterySOC(uint8_t soc_percent);
    void updateGearDisplay(uint8_t gear);
    void updateTorqueGauge(float torque_nm);
    void updatePowerGauge(float power_kw);
    void updateBatteryTemp(int8_t temp_c);
    void updateMotorTemp(uint8_t temp_c);
    void updateInverterTemp(uint8_t temp_c);

    // New SquareLine UI components from ui_screen_main
    lv_obj_t* time_label = nullptr;           // ui_Time
    lv_obj_t* gear_label = nullptr;           // ui_DNRlabel
    lv_obj_t* torque_gauge = nullptr;         // ui_TRQgauge (Arc) - positive torque
    lv_obj_t* torque_gauge_regen = nullptr;   // ui_TRQgauge1 (Arc) - regen/negative torque
    lv_obj_t* speed_label = nullptr;          // ui_SPEEDlabel
    lv_obj_t* power_gauge = nullptr;          // ui_BATPOWERgauge
    lv_obj_t* soc_label = nullptr;            // ui_BATSOClabel

    // Battery temperature (TEMPbar, TEMPvalue, TEMPname)
    lv_obj_t* battery_temp_bar = nullptr;     // ui_TEMPbar
    lv_obj_t* battery_temp_value = nullptr;   // ui_TEMPvalue
    lv_obj_t* battery_temp_name = nullptr;    // ui_TEMPname

    // Motor temperature (TEMPbar1, TEMPvalue1, TEMPname1)
    lv_obj_t* motor_temp_bar = nullptr;       // ui_TEMPbar1
    lv_obj_t* motor_temp_value = nullptr;     // ui_TEMPvalue1
    lv_obj_t* motor_temp_name = nullptr;      // ui_TEMPname1

    // Inverter temperature (TEMPbar2, TEMPvalue2, TEMPname2)
    lv_obj_t* inverter_temp_bar = nullptr;    // ui_TEMPbar2
    lv_obj_t* inverter_temp_value = nullptr;  // ui_TEMPvalue2
    lv_obj_t* inverter_temp_name = nullptr;   // ui_TEMPname2
};

#endif // DASHBOARD_UI_H


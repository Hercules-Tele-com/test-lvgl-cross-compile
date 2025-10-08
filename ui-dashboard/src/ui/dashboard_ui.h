#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include "lvgl.h"
#include "can_receiver.h"

class DashboardUI {
public:
    DashboardUI();
    ~DashboardUI();

    void init();
    void update(const CANReceiver& can);
    void updateTime(uint8_t hours, uint8_t minutes, uint8_t seconds);

private:
    void createLayout();
    void updateSpeedGauge(float speed);
    void updateBatterySOC(uint8_t soc);
    void updateMotorRPM(int16_t rpm);
    void updateTemperatures(int16_t motor_temp, int16_t inverter_temp);
    void updateGPSInfo(const GPSPositionState& pos, const GPSVelocityState& vel);

    // LVGL objects
    lv_obj_t* screen;
    lv_obj_t* speed_meter;
    lv_obj_t* soc_bar;
    lv_obj_t* soc_label;
    lv_obj_t* rpm_label;
    lv_obj_t* motor_temp_label;
    lv_obj_t* inverter_temp_label;
    lv_obj_t* gps_label;
    lv_obj_t* status_label;
    lv_obj_t* time_label;

    lv_meter_indicator_t* speed_indic;
    lv_style_t time_style;
};

#endif // DASHBOARD_UI_H

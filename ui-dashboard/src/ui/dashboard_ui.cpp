#include "dashboard_ui.h"
#include "screens/ui_screen_main.h"
#include <algorithm>
#include <cstdio>
#include <ctime>

DashboardUI::DashboardUI() = default;
DashboardUI::~DashboardUI() = default;

void DashboardUI::init() {
    printf("[UI] Starting UI component binding...\n");

    // Bind SquareLine UI components from ui_screen_main
    time_label = ui_Time;
    printf("[UI] time_label bound: %p\n", (void*)time_label);

    gear_label = ui_DNRlabel;
    printf("[UI] gear_label bound: %p\n", (void*)gear_label);

    torque_gauge = ui_TRQgauge;
    printf("[UI] torque_gauge bound: %p\n", (void*)torque_gauge);

    torque_gauge_regen = ui_TRQgauge1;
    printf("[UI] torque_gauge_regen bound: %p\n", (void*)torque_gauge_regen);

    speed_label = ui_SPEEDlabel;
    printf("[UI] speed_label bound: %p\n", (void*)speed_label);

    power_gauge = ui_BATPOWERgauge;
    printf("[UI] power_gauge bound: %p\n", (void*)power_gauge);

    soc_label = ui_BATSOClabel;
    printf("[UI] soc_label bound: %p\n", (void*)soc_label);

    // Battery temperature components
    battery_temp_bar = ui_TEMPbar;
    printf("[UI] battery_temp_bar bound: %p\n", (void*)battery_temp_bar);

    battery_temp_value = ui_TEMPvalue;
    printf("[UI] battery_temp_value bound: %p\n", (void*)battery_temp_value);

    battery_temp_name = ui_TEMPname;
    printf("[UI] battery_temp_name bound: %p\n", (void*)battery_temp_name);

    // Motor temperature components
    motor_temp_bar = ui_TEMPbar1;
    printf("[UI] motor_temp_bar bound: %p\n", (void*)motor_temp_bar);

    motor_temp_value = ui_TEMPvalue1;
    printf("[UI] motor_temp_value bound: %p\n", (void*)motor_temp_value);

    motor_temp_name = ui_TEMPname1;
    printf("[UI] motor_temp_name bound: %p\n", (void*)motor_temp_name);

    // Inverter temperature components
    inverter_temp_bar = ui_TEMPbar2;
    printf("[UI] inverter_temp_bar bound: %p\n", (void*)inverter_temp_bar);

    inverter_temp_value = ui_TEMPvalue2;
    printf("[UI] inverter_temp_value bound: %p\n", (void*)inverter_temp_value);

    inverter_temp_name = ui_TEMPname2;
    printf("[UI] inverter_temp_name bound: %p\n", (void*)inverter_temp_name);

    printf("[UI] All components bound successfully\n");

    // Configure torque gauge (Arc: 0 to 320 Nm for positive torque)
    if (torque_gauge) {
        printf("[UI] Configuring torque gauge...\n");
        lv_arc_set_range(torque_gauge, 0, 320);
        lv_arc_set_value(torque_gauge, 0);
        printf("[UI] Torque gauge configured (range: 0 to 320 Nm)\n");
    } else {
        printf("[UI] WARNING: torque_gauge is NULL!\n");
    }

    // Configure regen torque gauge (Arc: 0 to 100 Nm for regen, reverse mode)
    if (torque_gauge_regen) {
        printf("[UI] Configuring regen torque gauge...\n");
        lv_arc_set_range(torque_gauge_regen, 0, 100);
        lv_arc_set_value(torque_gauge_regen, 0);
        printf("[UI] Regen torque gauge configured (range: 0 to 100 Nm, reverse mode)\n");
    } else {
        printf("[UI] WARNING: torque_gauge_regen is NULL!\n");
    }

    // Configure temperature bars
    if (battery_temp_bar) {
        printf("[UI] Configuring battery temp bar...\n");
        lv_bar_set_range(battery_temp_bar, 20, 50);  // Battery: 20-50°C
        lv_bar_set_value(battery_temp_bar, 25, LV_ANIM_OFF);
    }
    if (motor_temp_bar) {
        printf("[UI] Configuring motor temp bar...\n");
        lv_bar_set_range(motor_temp_bar, 20, 110);   // Motor: 20-110°C
        lv_bar_set_value(motor_temp_bar, 25, LV_ANIM_OFF);
    }
    if (inverter_temp_bar) {
        printf("[UI] Configuring inverter temp bar...\n");
        lv_bar_set_range(inverter_temp_bar, 0, 70);  // Inverter: 0-70°C
        lv_bar_set_value(inverter_temp_bar, 25, LV_ANIM_OFF);
    }

    // Set initial temperature labels
    if (battery_temp_name) lv_label_set_text(battery_temp_name, "BATTERY");
    if (motor_temp_name) lv_label_set_text(motor_temp_name, "MOTOR");
    if (inverter_temp_name) lv_label_set_text(inverter_temp_name, "INVERTER");

    printf("[UI] Initialization complete\n");
    fflush(stdout);
}

void DashboardUI::update(const CANReceiver& can) {
    // Get CAN data
    uint16_t soc_raw = can.getSOC();                  // % * 10
    uint16_t speed_raw = can.getSpeed();              // kph * 100
    uint8_t gear = can.getGear();                     // 0=P, 1=R, 2=N, 3=D, 4=B
    int16_t torque_raw = can.getMotorTorque();        // Nm * 10
    uint16_t pack_voltage = can.getPackVoltage();     // V * 10
    int16_t pack_current = can.getPackCurrent();      // A * 10
    int8_t battery_temp = can.getTempAvg();           // °C
    uint8_t motor_temp = can.getMotorTemp();          // °C
    uint8_t inverter_temp = can.getInverterTemp();    // °C

    // Convert scaled values
    uint8_t soc = soc_raw / 10;                       // % * 10 → %
    float speed = speed_raw / 10.0f;                   // kph (not scaled)
    float torque = torque_raw / 10.0f;                // Nm * 10 → Nm
    float voltage = pack_voltage / 10.0f;             // V * 10 → V
    float current = pack_current / 10.0f;             // A * 10 → A
    float power = (voltage * current) / 1000.0f;      // kW

    // Update UI components
    updateBatterySOC(soc);
    updateSpeedDisplay(speed);
    updateGearDisplay(gear);
    updateTorqueGauge(torque);
    updatePowerGauge(power);
    updateBatteryTemp(battery_temp);
    updateMotorTemp(motor_temp);
    updateInverterTemp(inverter_temp);
}

void DashboardUI::updateTime(uint8_t h, uint8_t m, uint8_t s) {
    if (!time_label) return;
    lv_label_set_text_fmt(time_label, "%02d:%02d", h, m);
}

void DashboardUI::updateSpeedDisplay(float speed_kmh) {
    if (!speed_label) return;
    int speed = (int)(speed_kmh + 0.5f);  // Round to nearest integer
    lv_label_set_text_fmt(speed_label, "%d", speed);
}

void DashboardUI::updateBatterySOC(uint8_t soc_percent) {
    if (!soc_label) return;
    const uint8_t clamped = (soc_percent > 100) ? 100 : soc_percent;
    lv_label_set_text_fmt(soc_label, "%u%%", (unsigned)clamped);
}

void DashboardUI::updateGearDisplay(uint8_t gear) {
    if (!gear_label) return;
    const char* gear_str[] = {"P", "R", "N", "D", "B"};
    const char* display = (gear < 5) ? gear_str[gear] : "?";
    lv_label_set_text(gear_label, display);
}

void DashboardUI::updateTorqueGauge(float torque_nm) {
    // Handle positive torque (0 to 320 Nm) on ui_TRQgauge
    if (torque_gauge) {
        if (torque_nm >= 0) {
            int torque = (int)torque_nm;
            torque = std::min(torque, 320);
            lv_arc_set_value(torque_gauge, torque);
        } else {
            lv_arc_set_value(torque_gauge, 0);
        }
    }

    // Handle negative/regen torque (-100 to 0 Nm) on ui_TRQgauge1
    if (torque_gauge_regen) {
        if (torque_nm < 0) {
            int regen = (int)(-torque_nm);  // Convert to positive value
            regen = std::min(regen, 100);
            lv_arc_set_value(torque_gauge_regen, regen);
        } else {
            lv_arc_set_value(torque_gauge_regen, 0);
        }
    }
}

void DashboardUI::updatePowerGauge(float power_kw) {
    if (!power_gauge) return;
    // power_gauge is an Arc widget (-100 to 100 range for regen/power)
    int power = (int)power_kw;
    power = std::max(-100, std::min(power, 100));
    lv_arc_set_value(power_gauge, power);
}

void DashboardUI::updateBatteryTemp(int8_t temp_c) {
    if (battery_temp_bar) {
        int v = std::max(20, std::min((int)temp_c, 50));
        lv_bar_set_value(battery_temp_bar, v, LV_ANIM_ON);
    }
    if (battery_temp_value) {
        lv_label_set_text_fmt(battery_temp_value, "%dC", (int)temp_c);
    }
}

void DashboardUI::updateMotorTemp(uint8_t temp_c) {
    if (motor_temp_bar) {
        int v = std::max(20, std::min((int)temp_c, 110));
        lv_bar_set_value(motor_temp_bar, v, LV_ANIM_ON);
    }
    if (motor_temp_value) {
        lv_label_set_text_fmt(motor_temp_value, "%dC", (int)temp_c);
    }
}

void DashboardUI::updateInverterTemp(uint8_t temp_c) {
    if (inverter_temp_bar) {
        int v = std::max(0, std::min((int)temp_c, 70));
        lv_bar_set_value(inverter_temp_bar, v, LV_ANIM_ON);
    }
    if (inverter_temp_value) {
        lv_label_set_text_fmt(inverter_temp_value, "%dC", (int)temp_c);
    }
}

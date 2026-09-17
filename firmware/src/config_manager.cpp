#include "config_manager.h"

#include "hardware_config.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

const AppConfig& loadConfig() {
    // RMS thresholds remain provisional until P2/P3 collect abnormal/fault data
    // and calibrate them. They are intentionally not inferred from mock data.
    static const AppConfig config{
        "motor_01",
        WIFI_SSID,
        WIFI_PASSWORD,
        MQTT_HOST,
        MQTT_USER,
        MQTT_PASSWORD,
        1883,
        1000,
        5000,
        0.016f,
        0.020f,
        0.30f,
        0.24f,
        0.70f,
        0.56f,
        P1_ALARM_PIN,
        P1_ALARM_ACTIVE_HIGH != 0};
    return config;
}

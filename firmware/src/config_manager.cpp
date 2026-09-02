#include "config_manager.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

const AppConfig& loadConfig() {
    static const AppConfig config {
        "motor_01", WIFI_SSID, WIFI_PASSWORD, MQTT_HOST,
        MQTT_USER, MQTT_PASSWORD, 1883,
        1000, 5000, 0.30f, 0.70f, -1
    };
    return config;
}


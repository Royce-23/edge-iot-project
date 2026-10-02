#include "config_manager.h"

#include "hardware_config.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

// Keep older secrets.h files buildable until their owners add TLS settings.
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USE_TLS
#define MQTT_USE_TLS 0
#endif
#ifndef MQTT_ROOT_CA
#define MQTT_ROOT_CA ""
#endif

const AppConfig& loadConfig() {
    // Calibrated from the 2026-10-01 OFF/NORMAL runs. OFF enters at 0.017 g:
    // all three OFF runs contain at least three consecutive windows below it,
    // while no NORMAL run does. The 0.018 g clear threshold adds hysteresis.
    // The abnormal thresholds remain fallback-only; the generated P3 model
    // handles running vibration.
    static const AppConfig config{
        "motor_01",
        WIFI_SSID,
        WIFI_PASSWORD,
        MQTT_HOST,
        MQTT_USER,
        MQTT_PASSWORD,
        MQTT_PORT,
        MQTT_USE_TLS != 0,
        MQTT_ROOT_CA,
        1000,
        5000,
        0.017f,
        0.018f,
        0.30f,
        0.24f,
        0.70f,
        0.56f,
        P1_ALARM_PIN,
        P1_ALARM_ACTIVE_HIGH != 0};
    return config;
}

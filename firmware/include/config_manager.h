#pragma once

#include <stdint.h>

struct AppConfig {
    const char* deviceId;
    const char* wifiSsid;
    const char* wifiPassword;
    const char* mqttHost;
    const char* mqttUser;
    const char* mqttPassword;
    uint16_t mqttPort;
    uint32_t recordIntervalMs;
    uint32_t reconnectIntervalMs;
    float warningRms;
    float faultRms;
    int alarmPin;  // -1: log only; do not drive a GPIO alarm.
};

// Compile-time configuration skeleton. P1 may migrate this to Preferences/NVS.
const AppConfig& loadConfig();

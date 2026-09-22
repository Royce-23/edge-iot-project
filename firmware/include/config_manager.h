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
    bool mqttTls;
    const char* mqttRootCa;
    uint32_t recordIntervalMs;
    uint32_t reconnectIntervalMs;
    float offRms;           // Enter OFF at or below this value.
    float offClearRms;      // Leave OFF only at or above this value.
    float warningRms;       // Enter WARNING at or above this value.
    float warningClearRms;  // Leave WARNING only below this value.
    float faultRms;         // Enter FAULT at or above this value.
    float faultClearRms;    // Leave FAULT only below this value.
    int alarmPin;  // -1: log only; do not drive a GPIO alarm.
    bool alarmActiveHigh;
};

// Compile-time configuration skeleton. P1 may migrate this to Preferences/NVS.
const AppConfig& loadConfig();

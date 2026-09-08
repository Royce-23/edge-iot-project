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
    int alarmPin; // -1: chi log Serial, chua dieu khien GPIO.
};

// Ban suon: cau hinh compile-time. TODO P1: Preferences/NVS neu can.
const AppConfig& loadConfig();


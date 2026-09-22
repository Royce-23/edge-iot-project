#pragma once
// Copy to secrets.h and fill in real credentials. Never commit secrets.h.
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define MQTT_HOST ""  // Broker hostname, without mqtts:// or https://
#define MQTT_PORT 8883
#define MQTT_USE_TLS 1
#define MQTT_USER ""
#define MQTT_PASSWORD ""
// Optional PEM root CA for a private broker CA. Empty uses ESP32's CA bundle.
#define MQTT_ROOT_CA ""

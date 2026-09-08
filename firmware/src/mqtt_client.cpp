#include "mqtt_client.h"
#include "device_state.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <atomic>

namespace {
const AppConfig* config;
OfflineQueue* pending;
std::atomic<bool> online{false};
char bootId[17];

void networkTask(void*) {
    // Chi task nay truy cap WiFiClient/PubSubClient.
    WiFiClient tcp;
    PubSubClient mqtt(tcp);
    mqtt.setServer(config->mqttHost, config->mqttPort);
    mqtt.setSocketTimeout(2);
    mqtt.setKeepAlive(15);
    if (!mqtt.setBufferSize(1024)) {
        Serial.println("[NET] Khong du RAM cho MQTT");
        vTaskDelete(nullptr);
        return;
    }
    const String prefix = String("machine/") + config->deviceId;
    const String featuresTopic = prefix + "/features";
    const String statusTopic = prefix + "/status";
    const String clientId = String(config->deviceId) + "-" + bootId;
    WiFi.mode(WIFI_STA);
    uint32_t lastWifi = millis() - config->reconnectIntervalMs;
    uint32_t lastMqtt = lastWifi;
    uint32_t lastHeartbeat = 0;

    for (;;) {
        const uint32_t now = millis();
        if (WiFi.status() != WL_CONNECTED) {
            online.store(false);
            tcp.stop();
            if (config->wifiSsid[0] &&
                now - lastWifi >= config->reconnectIntervalMs) {
                lastWifi = now;
                Serial.println("[NET] Thu ket noi Wi-Fi");
                WiFi.begin(config->wifiSsid, config->wifiPassword);
            }
        } else {
            if (!mqtt.connected() && now - lastMqtt >= config->reconnectIntervalMs) {
                lastMqtt = now;
                // LWT retained: broker danh dau OFFLINE neu ket noi bi mat.
                const char* user = config->mqttUser[0] ? config->mqttUser : nullptr;
                const char* pass = user ? config->mqttPassword : nullptr;
                if (mqtt.connect(clientId.c_str(), user, pass,
                                 statusTopic.c_str(), 1, true, "OFFLINE")) {
                    mqtt.publish(statusTopic.c_str(), "ONLINE", true);
                    Serial.println("[NET] MQTT connected");
                } else {
                    Serial.printf("[NET] MQTT error=%d\n", mqtt.state());
                }
            }
            if (mqtt.connected()) {
                mqtt.loop();
                if (now - lastHeartbeat >= 5000) {
                    lastHeartbeat = now;
                    mqtt.publish(statusTopic.c_str(), "ONLINE", true);
                }
                TelemetryRecord record;
                if (pending->peek(record)) {
                    StaticJsonDocument<768> doc;
                    doc["schema_version"] = 1;
                    doc["device_id"] = config->deviceId;
                    doc["boot_id"] = bootId;
                    doc["seq"] = record.seq;
                    doc["uptime_ms"] = record.uptimeMs;
                    doc["timestamp"] = nullptr; // TODO P1: NTP + UTC khi da dong bo.
                    doc["data_source"] = "sensor";
                    doc["method"] = "rms_demo_dc_removed";
                    doc["rms"] = record.features.rms;
                    doc["peak_to_peak"] = record.features.peakToPeak;
                    doc["crest_factor"] = record.features.crestFactor;
                    doc["health_state"] = healthName(record.health);
                    doc["dropped_total"] = record.droppedTotal;
                    char payload[768];
                    if (!doc.overflowed() && measureJson(doc) < sizeof(payload)) {
                        const size_t length = serializeJson(doc, payload, sizeof(payload));
                        // QoS 0: true chi la chap nhan gui, KHONG phai ACK backend.
                        if (mqtt.publish(featuresTopic.c_str(),
                            reinterpret_cast<const uint8_t*>(payload), length, false)) {
                            pending->pop();
                        }
                    }
                }
            }
            online.store(mqtt.connected());
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Toi da khoang 10 ban ghi/giay.
    }
}
}

bool startNetworkTask(const AppConfig& cfg, OfflineQueue& queue) {
    config = &cfg;
    pending = &queue;
    snprintf(bootId, sizeof(bootId), "%08lx%08lx",
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()));
    // MQTT connect co the cho timeout; task rieng giu loop phat hien tiep tuc.
    return xTaskCreate(networkTask, "network", 8192, nullptr, 1, nullptr) == pdPASS;
}
bool networkOnline() { return online.load(); }
const char* sessionId() { return bootId; }

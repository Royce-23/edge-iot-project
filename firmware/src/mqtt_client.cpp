#include "mqtt_client.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <atomic>
#include <esp_system.h>

#include "device_state.h"

namespace {

const AppConfig* config = nullptr;
OfflineQueue* pending = nullptr;
std::atomic<bool> online{false};
char bootId[17] = "not_started";

void networkTask(void*) {
    // Only this task accesses WiFiClient/PubSubClient.
    WiFiClient tcp;
    PubSubClient mqtt(tcp);
    mqtt.setServer(config->mqttHost, config->mqttPort);
    mqtt.setSocketTimeout(2);
    mqtt.setKeepAlive(15);
    if (!mqtt.setBufferSize(1024)) {
        Serial.println("[NET] Not enough RAM for MQTT buffer");
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
            if (config->wifiSsid[0] != '\0' &&
                now - lastWifi >= config->reconnectIntervalMs) {
                lastWifi = now;
                Serial.println("[NET] Connecting to Wi-Fi");
                WiFi.begin(config->wifiSsid, config->wifiPassword);
            }
        } else {
            if (!mqtt.connected() &&
                now - lastMqtt >= config->reconnectIntervalMs) {
                lastMqtt = now;
                const char* user =
                    config->mqttUser[0] != '\0' ? config->mqttUser : nullptr;
                const char* password = user ? config->mqttPassword : nullptr;
                // Retained last will lets the broker mark a broken connection.
                if (mqtt.connect(clientId.c_str(), user, password,
                                 statusTopic.c_str(), 1, true, "OFFLINE")) {
                    mqtt.publish(statusTopic.c_str(), "ONLINE", true);
                    Serial.println("[NET] MQTT connected");
                } else {
                    Serial.printf("[NET] MQTT error=%d\n", mqtt.state());
                }
            }

            if (mqtt.connected()) {
                mqtt.loop();
                if (now - lastHeartbeat >= 5000U) {
                    lastHeartbeat = now;
                    mqtt.publish(statusTopic.c_str(), "ONLINE", true);
                }

                TelemetryRecord record{};
                if (pending->peek(record)) {
                    StaticJsonDocument<768> document;
                    document["schema_version"] = 1;
                    document["device_id"] = config->deviceId;
                    document["boot_id"] = bootId;
                    document["sequence"] = record.sequence;
                    document["uptime_ms"] = record.uptimeMs;
                    document["timestamp"] = nullptr;
                    document["data_source"] = "real_adxl345";
                    document["method"] = "z_axis_dc_removed_time_domain";
                    document["sample_rate_hz"] = record.sampleRateHz;
                    document["sample_count"] = record.sampleCount;
                    document["rms"] = record.features.rms;
                    document["peak_to_peak"] = record.features.peakToPeak;
                    document["crest_factor"] = record.features.crestFactor;
                    document["health_state"] = healthName(record.health);
                    document["dropped_total"] = record.droppedTotal;
                    if (record.hasTemperature) {
                        document["temperature_c"] = record.temperatureC;
                    } else {
                        document["temperature_c"] = nullptr;
                    }

                    char payload[768];
                    if (!document.overflowed() &&
                        measureJson(document) < sizeof(payload)) {
                        const size_t length =
                            serializeJson(document, payload, sizeof(payload));
                        // PubSubClient uses QoS 0 here: true only means accepted
                        // for sending, not acknowledged by the backend.
                        if (mqtt.publish(
                                featuresTopic.c_str(),
                                reinterpret_cast<const uint8_t*>(payload),
                                length, false)) {
                            pending->pop();
                        }
                    }
                }
            }
            online.store(mqtt.connected());
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

}  // namespace

bool startNetworkTask(const AppConfig& appConfig, OfflineQueue& queue) {
    config = &appConfig;
    pending = &queue;
    snprintf(bootId, sizeof(bootId), "%08lx%08lx",
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()));
    return xTaskCreate(networkTask, "network", 8192, nullptr, 1, nullptr) ==
           pdPASS;
}

bool networkOnline() { return online.load(); }

const char* sessionId() { return bootId; }

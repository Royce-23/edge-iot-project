#include "network_client.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <esp_event.h>
#include <esp_system.h>

extern "C" {
#include <mqtt_client.h>
}

#include "device_state.h"
#include "hardware_config.h"
#include "network_policy.h"

namespace {

const AppConfig* config = nullptr;
OfflineQueue* pending = nullptr;
std::atomic<bool> online{false};
std::atomic<bool> mqttReconnectRequested{false};
char bootId[17] = "not_started";
char clientId[80] = {};
char featuresTopic[128] = {};
char statusTopic[128] = {};
char onlineStatusPayload[192] = {};
char offlineStatusPayload[192] = {};
esp_mqtt_client_handle_t mqttClient = nullptr;
QueueHandle_t deliveryEvents = nullptr;

struct DeliveryEvent {
    int messageId;
    bool acknowledged;
};

uint32_t initialReconnectBackoffMs() {
    return network_policy::initialReconnectBackoffMs(
        config->reconnectIntervalMs);
}

uint32_t nextReconnectBackoffMs(uint32_t currentDelayMs) {
    return network_policy::nextReconnectBackoffMs(currentDelayMs);
}

bool deadlineReached(uint32_t now, uint32_t deadline) {
    return network_policy::deadlineReached(now, deadline);
}

bool writeStatusPayload(bool isOnline, char* output, size_t capacity) {
    StaticJsonDocument<160> document;
    document["device_id"] = config->deviceId;
    document["online"] = isOnline;
    document["timestamp"] = nullptr;
    if (document.overflowed() || measureJson(document) >= capacity) {
        return false;
    }
    serializeJson(document, output, capacity);
    return true;
}

bool writeFeaturePayload(const TelemetryRecord& record, char* output,
                         size_t capacity, size_t& length) {
    StaticJsonDocument<768> document;
    document["schema_version"] = 1;
    document["device_id"] = config->deviceId;
    document["boot_id"] = bootId;
    document["sequence"] = record.sequence;
    document["timestamp"] = nullptr;
    document["uptime_ms"] = record.uptimeMs;
    document["sample_rate_hz"] = record.sampleRateHz;
    document["sample_count"] = record.sampleCount;
    document["rms"] = record.features.rms;
    document["peak_to_peak"] = record.features.peakToPeak;
    document["crest_factor"] = record.features.crestFactor;
    document["dominant_frequency"] = record.dominantFrequency;
    document["band_energy"] = record.bandEnergy;
    if (record.hasAnomalyScore && std::isfinite(record.anomalyScore)) {
        document["anomaly_score"] = record.anomalyScore;
    } else {
        document["anomaly_score"] = nullptr;
    }
    document["health_state"] = healthName(record.health);
    if (record.hasTemperature && std::isfinite(record.temperatureC)) {
        document["temperature_c"] = record.temperatureC;
    } else {
        document["temperature_c"] = nullptr;
    }

    if (document.overflowed() || measureJson(document) >= capacity) {
        return false;
    }
    length = serializeJson(document, output, capacity);
    return length > 0;
}

void mqttEventHandler(void*, esp_event_base_t, int32_t eventId,
                      void* eventData) {
    const auto id = static_cast<esp_mqtt_event_id_t>(eventId);
    const auto event = static_cast<esp_mqtt_event_handle_t>(eventData);
    switch (id) {
        case MQTT_EVENT_CONNECTED:
            online.store(true);
            mqttReconnectRequested.store(false);
            break;
        case MQTT_EVENT_DISCONNECTED:
            online.store(false);
            mqttReconnectRequested.store(true);
            break;
        case MQTT_EVENT_PUBLISHED:
        case MQTT_EVENT_DELETED: {
            if (deliveryEvents != nullptr) {
                const DeliveryEvent delivery{
                    event->msg_id, id == MQTT_EVENT_PUBLISHED};
                xQueueSendToBack(deliveryEvents, &delivery, 0);
            }
            break;
        }
        case MQTT_EVENT_ERROR:
            online.store(false);
            mqttReconnectRequested.store(true);
            break;
        default:
            break;
    }
}

int enqueueStatus(bool isOnline) {
    const char* payload =
        isOnline ? onlineStatusPayload : offlineStatusPayload;
    return esp_mqtt_client_enqueue(mqttClient, statusTopic, payload, 0, 1, 1,
                                   true);
}

void networkTask(void*) {
    WiFi.mode(WIFI_STA);
    // Reconnect explicitly so failed Wi-Fi/MQTT attempts use bounded
    // exponential backoff instead of repeatedly competing with sampling.
    WiFi.setAutoReconnect(false);

    esp_mqtt_client_config_t mqttConfig = {};
    mqttConfig.host = config->mqttHost;
    mqttConfig.port = config->mqttPort;
    mqttConfig.client_id = clientId;
    mqttConfig.username =
        config->mqttUser[0] != '\0' ? config->mqttUser : nullptr;
    mqttConfig.password =
        mqttConfig.username != nullptr ? config->mqttPassword : nullptr;
    mqttConfig.lwt_topic = statusTopic;
    mqttConfig.lwt_msg = offlineStatusPayload;
    mqttConfig.lwt_qos = 1;
    mqttConfig.lwt_retain = 1;
    mqttConfig.disable_clean_session = 1;
    mqttConfig.keepalive = 15;
    mqttConfig.reconnect_timeout_ms = config->reconnectIntervalMs;
    mqttConfig.network_timeout_ms = 2000;
    mqttConfig.disable_auto_reconnect = true;
    mqttConfig.buffer_size = 1024;
    mqttConfig.out_buffer_size = 1024;
    // ESP-MQTT otherwise defaults to priority 5, above the priority-3
    // sampling task. Telemetry is only 1 Hz, so priority 1 is sufficient and
    // prevents broker failures from pre-empting time-critical sampling.
    mqttConfig.task_prio = P1_NETWORK_TASK_PRIORITY;
    mqttConfig.task_stack = 8192;

    mqttClient = esp_mqtt_client_init(&mqttConfig);
    if (mqttClient == nullptr ||
        esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_ANY,
                                       mqttEventHandler, nullptr) != ESP_OK) {
        Serial.println("[NET] Cannot initialize ESP-MQTT client");
        online.store(false);
        vTaskDelete(nullptr);
        return;
    }

    int inFlightMessageId = -1;
    bool onlineStatusAnnounced = false;
    bool wifiWasConnected = false;
    bool mqttWasOnline = false;
    bool mqttStarted = false;
    bool mqttRetryScheduled = false;
    const uint32_t baseBackoffMs = initialReconnectBackoffMs();
    uint32_t wifiBackoffMs = baseBackoffMs;
    uint32_t mqttBackoffMs = baseBackoffMs;
    uint32_t nextWifiAttemptMs = millis();
    uint32_t nextMqttAttemptMs = 0;
    uint32_t lastHeartbeat = 0;

    for (;;) {
        const uint32_t now = millis();

        if (WiFi.status() != WL_CONNECTED) {
            if (wifiWasConnected) {
                wifiWasConnected = false;
                mqttWasOnline = false;
                online.store(false);
                mqttReconnectRequested.store(false);
                mqttRetryScheduled = false;
                wifiBackoffMs = baseBackoffMs;
                nextWifiAttemptMs = now;
                Serial.println(
                    "[NET] Wi-Fi disconnected; local detection continues");
            }

            if (deadlineReached(now, nextWifiAttemptMs)) {
                const uint32_t retryDelayMs = wifiBackoffMs;
                Serial.printf(
                    "[NET] Connecting Wi-Fi ssid=%s; next retry in %lu ms\n",
                    config->wifiSsid,
                    static_cast<unsigned long>(retryDelayMs));
                // WiFi.begin() starts association asynchronously; this
                // low-priority task never blocks waiting for the connection.
                WiFi.begin(config->wifiSsid, config->wifiPassword);
                nextWifiAttemptMs = now + retryDelayMs;
                wifiBackoffMs = nextReconnectBackoffMs(wifiBackoffMs);
            }

            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!wifiWasConnected) {
            wifiWasConnected = true;
            wifiBackoffMs = baseBackoffMs;
            mqttBackoffMs = baseBackoffMs;
            mqttRetryScheduled = false;
            const String localIp = WiFi.localIP().toString();
            Serial.printf(
                "[NET] Wi-Fi connected ssid=%s ip=%s mqtt=%s:%u\n",
                config->wifiSsid, localIp.c_str(), config->mqttHost,
                static_cast<unsigned>(config->mqttPort));

            if (!mqttStarted) {
                if (esp_mqtt_client_start(mqttClient) != ESP_OK) {
                    Serial.println("[NET] Cannot start ESP-MQTT client");
                    mqttReconnectRequested.store(true);
                } else {
                    mqttStarted = true;
                }
            } else {
                // The MQTT client remains allocated while Wi-Fi is down. It
                // reconnects only after the station owns an IP address again.
                mqttReconnectRequested.store(true);
            }
        }

        const bool mqttIsOnline = online.load();
        if (mqttIsOnline && !mqttWasOnline) {
            mqttWasOnline = true;
            Serial.println("[NET] MQTT connected");
        } else if (!mqttIsOnline && mqttWasOnline) {
            mqttWasOnline = false;
            Serial.println("[NET] MQTT disconnected; queued data retained");
        }

        if (mqttIsOnline) {
            mqttReconnectRequested.store(false);
            mqttRetryScheduled = false;
            mqttBackoffMs = baseBackoffMs;
        } else {
            if (mqttReconnectRequested.exchange(false) &&
                !mqttRetryScheduled) {
                nextMqttAttemptMs = now + mqttBackoffMs;
                Serial.printf("[NET] MQTT retry scheduled in %lu ms\n",
                              static_cast<unsigned long>(mqttBackoffMs));
                mqttBackoffMs = nextReconnectBackoffMs(mqttBackoffMs);
                mqttRetryScheduled = true;
            }

            if (mqttRetryScheduled &&
                deadlineReached(now, nextMqttAttemptMs)) {
                mqttRetryScheduled = false;
                const esp_err_t result =
                    mqttStarted ? esp_mqtt_client_reconnect(mqttClient)
                                : esp_mqtt_client_start(mqttClient);
                if (result == ESP_OK) {
                    mqttStarted = true;
                } else {
                    mqttReconnectRequested.store(true);
                    Serial.println("[NET] Cannot start/reconnect MQTT client");
                }
            }
        }

        DeliveryEvent delivery{};
        while (xQueueReceive(deliveryEvents, &delivery, 0) == pdTRUE) {
            const auto disposition = network_policy::deliveryDisposition(
                delivery.messageId, inFlightMessageId,
                delivery.acknowledged);
            if (disposition == network_policy::DeliveryDisposition::IGNORE) {
                continue;
            }
            if (disposition ==
                network_policy::DeliveryDisposition::POP_ACKNOWLEDGED_RECORD) {
                pending->pop();
                Serial.printf("[NET] QoS1 acknowledged message_id=%d\n",
                              delivery.messageId);
            } else {
                Serial.printf("[NET] MQTT outbox dropped message_id=%d\n",
                              delivery.messageId);
            }
            // Keep an unacknowledged record at the queue head so it can be
            // enqueued again; backend deduplicates by boot_id/sequence.
            inFlightMessageId = -1;
        }

        if (!online.load() || WiFi.status() != WL_CONNECTED) {
            onlineStatusAnnounced = false;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (!onlineStatusAnnounced) {
            if (enqueueStatus(true) >= 0) {
                onlineStatusAnnounced = true;
                lastHeartbeat = now;
            }
        } else if (now - lastHeartbeat >= 5000U) {
            if (enqueueStatus(true) >= 0) {
                lastHeartbeat = now;
            }
        }

        if (inFlightMessageId < 0) {
            TelemetryRecord record{};
            if (pending->peek(record)) {
                char payload[768] = {};
                size_t length = 0;
                if (!writeFeaturePayload(record, payload, sizeof(payload),
                                         length)) {
                    Serial.println("[NET] Feature payload serialization failed");
                } else {
                    const int messageId = esp_mqtt_client_enqueue(
                        mqttClient, featuresTopic, payload,
                        static_cast<int>(length), 1, 0, true);
                    if (messageId >= 0) {
                        inFlightMessageId = messageId;
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

}  // namespace

bool startNetworkTask(const AppConfig& appConfig, OfflineQueue& queue) {
    config = &appConfig;
    pending = &queue;
    snprintf(bootId, sizeof(bootId), "%08lx%08lx",
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()));

    if (config->wifiSsid == nullptr || config->wifiSsid[0] == '\0' ||
        config->mqttHost == nullptr || config->mqttHost[0] == '\0' ||
        config->mqttPort == 0U) {
        online.store(false);
        Serial.println(
            "[NET] Network disabled: configure WIFI_SSID, MQTT_HOST and "
            "MQTT_PORT; local detection continues");
        return true;
    }

    const int clientLength = snprintf(clientId, sizeof(clientId), "%s-%s",
                                      config->deviceId, bootId);
    const int featureTopicLength =
        snprintf(featuresTopic, sizeof(featuresTopic), "machine/%s/features",
                 config->deviceId);
    const int statusTopicLength =
        snprintf(statusTopic, sizeof(statusTopic), "machine/%s/status",
                 config->deviceId);
    if (clientLength < 0 || static_cast<size_t>(clientLength) >= sizeof(clientId) ||
        featureTopicLength < 0 ||
        static_cast<size_t>(featureTopicLength) >= sizeof(featuresTopic) ||
        statusTopicLength < 0 ||
        static_cast<size_t>(statusTopicLength) >= sizeof(statusTopic) ||
        !writeStatusPayload(true, onlineStatusPayload,
                            sizeof(onlineStatusPayload)) ||
        !writeStatusPayload(false, offlineStatusPayload,
                            sizeof(offlineStatusPayload))) {
        Serial.println("[NET] Device ID is too long for MQTT topics/client ID");
        return false;
    }

    deliveryEvents = xQueueCreate(32, sizeof(DeliveryEvent));
    if (deliveryEvents == nullptr) {
        return false;
    }
    // Keep Wi-Fi/MQTT coordination on core 0. Sampling is pinned to core 1 at
    // a higher priority; JSON and network retries cannot pre-empt acquisition.
    if (xTaskCreatePinnedToCore(networkTask, "network", 8192, nullptr,
                                P1_NETWORK_TASK_PRIORITY, nullptr,
                                P1_NETWORK_TASK_CORE) != pdPASS) {
        vQueueDelete(deliveryEvents);
        deliveryEvents = nullptr;
        return false;
    }
    return true;
}

bool networkOnline() { return online.load(); }

const char* sessionId() { return bootId; }

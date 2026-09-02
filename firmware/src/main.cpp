#include <Arduino.h>
#include <esp_timer.h>
#include "config_manager.h"
#include "device_state.h"
#include "module_interfaces.h"
#include "mqtt_client.h"
#include "offline_queue.h"

namespace {
const AppConfig* config;
OfflineQueue pending;
DeviceState device;
SampleWindow window;
uint32_t sequence = 0;
uint32_t dropped = 0;
uint32_t lastRecord = 0;
bool sensorsReady = false;
bool queueReady = false;
}

void setup() {
    Serial.begin(115200);
    config = &loadConfig();
    if (config->alarmPin >= 0) {
        pinMode(config->alarmPin, OUTPUT);
        digitalWrite(config->alarmPin, LOW);
    }
    sensorsReady = initSensors();          // Module nguoi 2.
    queueReady = pending.begin();          // Bo dem RAM.
    if (queueReady) {
        if (!startNetworkTask(*config, pending))
            Serial.println("[ERROR] Khong tao duoc network task; van phat hien cuc bo");
    } else {
        Serial.println("[ERROR] Khong tao duoc queue; van phat hien cuc bo");
    }
    Serial.println("[BOOT] P1 skeleton - DU LIEU GIA, khong phai phep do that");
    Serial.printf("[BOOT] device=%s session=%s\n", config->deviceId, sessionId());
}

void loop() {
    const uint32_t now = millis();
    if (now - lastRecord < config->recordIntervalMs) {
        delay(1); // Nhuong CPU; khong cho ket noi mang trong loop.
        return;
    }
    lastRecord = now;
    const uint64_t sampledAt = static_cast<uint64_t>(esp_timer_get_time()) / 1000;
    if (!sensorsReady) sensorsReady = initSensors();
    if (!sensorsReady || !collectWindow(window, sampledAt)) {
        device.update(HealthState::UNKNOWN);
        if (config->alarmPin >= 0) digitalWrite(config->alarmPin, LOW);
        Serial.println("[SENSOR] Khong co du lieu hop le; health=UNKNOWN");
        // TODO P1/P4: gui su kien loi cam bien rieng; dashboard can stale timeout.
        return;
    }

    const VibrationFeatures features = extractFeatures(window); // Module nguoi 3.
    const HealthState result = classifyCondition(features,
        config->warningRms, config->faultRms);
    if (device.update(result)) {
        Serial.printf("[STATE] %s\n", healthName(device.health()));
        // TODO P1: tach health_events/gui topic events khi thay doi trang thai.
    }
    if (config->alarmPin >= 0)
        digitalWrite(config->alarmPin, device.alarmActive() ? HIGH : LOW);

    const TelemetryRecord record {
        ++sequence, sampledAt, features, device.health(), dropped
    };
    if (!queueReady || !pending.push(record)) {
        ++dropped;
        Serial.printf("[QUEUE] Bo ban ghi moi, dropped_total=%lu\n",
                      static_cast<unsigned long>(dropped));
    }
    Serial.printf("[DATA] seq=%lu rms=%.3f health=%s network=%s queued=%u\n",
        static_cast<unsigned long>(sequence), features.rms,
        healthName(device.health()), networkOnline() ? "ONLINE" : "OFFLINE",
        static_cast<unsigned>(queueReady ? pending.size() : 0));
}


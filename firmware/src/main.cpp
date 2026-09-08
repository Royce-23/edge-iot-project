#include <Arduino.h>
#include <esp_timer.h>
#include <math.h>

#include "config_manager.h"
#include "device_state.h"
#include "module_interfaces.h"
#include "mqtt_client.h"
#include "offline_queue.h"

namespace {

const AppConfig* config = nullptr;
OfflineQueue pending;
DeviceState device;
SampleWindow window{};
uint32_t sequence = 0;
uint32_t dropped = 0;
uint32_t lastRecord = 0;
bool sensorsReady = false;
bool queueReady = false;

}  // namespace

void setup() {
    Serial.begin(115200);
    config = &loadConfig();
    if (config->alarmPin >= 0) {
        pinMode(config->alarmPin, OUTPUT);
        digitalWrite(config->alarmPin, LOW);
    }

    sensorsReady = initSensors();
    queueReady = pending.begin();
    if (queueReady) {
        if (!startNetworkTask(*config, pending)) {
            Serial.println(
                "[ERROR] Cannot create network task; local detection continues");
        }
    } else {
        Serial.println(
            "[ERROR] Cannot create offline queue; local detection continues");
    }
    Serial.println("[BOOT] Integrated firmware: real ADXL345 sampling");
    Serial.printf("[BOOT] device=%s session=%s\n", config->deviceId,
                  sessionId());
}

void loop() {
    const uint32_t now = millis();
    if (now - lastRecord < config->recordIntervalMs) {
        delay(1);
        return;
    }
    lastRecord = now;

    const uint64_t sampledAt =
        static_cast<uint64_t>(esp_timer_get_time()) / 1000ULL;
    if (!sensorsReady) {
        sensorsReady = initSensors();
    }
    if (!sensorsReady || !collectWindow(window, sampledAt)) {
        device.update(HealthState::UNKNOWN);
        if (config->alarmPin >= 0) {
            digitalWrite(config->alarmPin, LOW);
        }
        Serial.println("[SENSOR] No valid window; health=UNKNOWN");
        return;
    }

    const VibrationFeatures features = extractFeatures(window);
    const HealthState result = classifyCondition(
        features, config->warningRms, config->faultRms);
    if (device.update(result)) {
        Serial.printf("[STATE] %s\n", healthName(device.health()));
    }
    if (config->alarmPin >= 0) {
        digitalWrite(config->alarmPin,
                     device.alarmActive() ? HIGH : LOW);
    }

    float temperatureC = NAN;
    const bool hasTemperature = readTemperature(temperatureC);
    const TelemetryRecord record{
        ++sequence,
        sampledAt,
        window.sampleRateHz,
        static_cast<uint16_t>(SAMPLE_COUNT),
        features,
        device.health(),
        temperatureC,
        hasTemperature,
        dropped,
    };
    if (!queueReady || !pending.push(record)) {
        ++dropped;
        Serial.printf("[QUEUE] Dropped newest record, dropped_total=%lu\n",
                      static_cast<unsigned long>(dropped));
    }
    Serial.printf(
        "[DATA] seq=%lu rms=%.5f g health=%s network=%s queued=%u\n",
        static_cast<unsigned long>(sequence), features.rms,
        healthName(device.health()), networkOnline() ? "ONLINE" : "OFFLINE",
        static_cast<unsigned>(queueReady ? pending.size() : 0));
}

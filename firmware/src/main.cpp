#include <Arduino.h>
#include <esp_timer.h>
#include "config_manager.h"
#include "device_state.h"
#include "module_interfaces.h"
#include "mqtt_client.h"
#include "offline_queue.h"
#include "sampling.h"
#include "hardware_config.h"

namespace {
const AppConfig* config;
OfflineQueue pending;
DeviceState device;
SampleWindow window;
// P3 currently consumes one axis. Capture XYZ and pass Z (in g) to P3.
// Static storage avoids putting three sample arrays on the loop task's stack.
float ax[SAMPLE_COUNT];
float ay[SAMPLE_COUNT];
static_assert(SAMPLE_COUNT <= P2_SAMPLING_BUFFER_CAPACITY,
              "P2 buffer must fit the application window");
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
    Serial.printf("[BOOT] P1+P2 ADXL345 SPI, axis=Z, count=%u, target_hz=%u; processing=rms_demo_dc_removed\n",
        static_cast<unsigned>(SAMPLE_COUNT), static_cast<unsigned>(P2_SAMPLE_RATE_HZ));
    Serial.printf("[BOOT] device=%s session=%s\n", config->deviceId, sessionId());
}

void loop() {
    const uint32_t now = millis();
    if (now - lastRecord < config->recordIntervalMs) {
        delay(1); // Nhuong CPU; khong cho ket noi mang trong loop.
        return;
    }
    lastRecord = now;
    SamplingMetrics metrics = {};
    if (!sensorsReady) sensorsReady = initSensors();
    if (!sensorsReady || !collectWindowWithDiagnostics(
            ax, ay, window.values, nullptr, SAMPLE_COUNT, metrics)) {
        device.update(HealthState::UNKNOWN);
        if (config->alarmPin >= 0) digitalWrite(config->alarmPin, LOW);
        Serial.printf("[SENSOR] Cua so loi; health=UNKNOWN timer=%lu buffer=%lu read=%lu dropped=%lu\n",
            static_cast<unsigned long>(metrics.timerOverruns),
            static_cast<unsigned long>(metrics.bufferOverruns),
            static_cast<unsigned long>(metrics.sensorReadErrors),
            static_cast<unsigned long>(metrics.droppedSamples));
        // TODO P1/P4: gui su kien loi cam bien rieng; dashboard can stale timeout.
        return;
    }
    window.sampleRateHz = metrics.actualSampleRateHz;
    const uint64_t sampledAt = metrics.firstTimestampUs / 1000ULL;
    Serial.printf("[SAMPLE] count=%u hz=%.2f jitter_us=%.2f start_us=%llu end_us=%llu\n",
        static_cast<unsigned>(SAMPLE_COUNT), window.sampleRateHz, metrics.jitterRmsUs,
        static_cast<unsigned long long>(metrics.firstTimestampUs),
        static_cast<unsigned long long>(metrics.lastTimestampUs));

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

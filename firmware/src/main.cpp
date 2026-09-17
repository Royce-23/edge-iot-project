#include <Arduino.h>
#include <cmath>
#include <esp_timer.h>

#include "config_manager.h"
#include "device_state.h"
#include "features.h"
#include "hardware_config.h"
#include "module_interfaces.h"
#include "network_client.h"
#include "offline_queue.h"
#include "sampling.h"

#if __has_include("p3_model.generated.h")
#include "p3_model.generated.h"
#define P3_GENERATED_MODEL_AVAILABLE 1
#else
#define P3_GENERATED_MODEL_AVAILABLE 0
#endif

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
bool stateThresholdsReady = false;
bool rgbReady = false;

constexpr bool rgbLedEnabled =
    P1_RGB_LED_RED_PIN >= 0 && P1_RGB_LED_GREEN_PIN >= 0 &&
    P1_RGB_LED_BLUE_PIN >= 0;

void writeRgbChannel(int pin, bool on) {
    const bool outputHigh = P1_RGB_LED_COMMON_ANODE ? !on : on;
    digitalWrite(pin, outputHigh ? HIGH : LOW);
}

void updateRgbOutput() {
    if (!rgbLedEnabled || !rgbReady) {
        return;
    }

    bool red = false;
    bool green = false;
    bool blue = false;
    if (device.sensorErrorActive()) {
        blue = true;
    } else {
        switch (device.health()) {
            case HealthState::OFF:
                red = true;
                break;
            case HealthState::NORMAL:
                green = true;
                break;
            case HealthState::WARNING:
                red = true;
                green = true;
                break;
            case HealthState::FAULT:
                red = true;
                break;
            default:
                break;
        }
    }

    writeRgbChannel(P1_RGB_LED_RED_PIN, red);
    writeRgbChannel(P1_RGB_LED_GREEN_PIN, green);
    writeRgbChannel(P1_RGB_LED_BLUE_PIN, blue);
}

void updateAlarmOutput() {
    if (config->alarmPin >= 0) {
        const bool outputHigh =
            device.alarmActive() == config->alarmActiveHigh;
        digitalWrite(config->alarmPin, outputHigh ? HIGH : LOW);
    }
    updateRgbOutput();
}

void markMeasurementInvalid(const char* reason) {
    const StateChange change = device.measurementFailed();
    if (change.sensorErrorChanged) {
        Serial.printf("[STATE] SENSOR_ERROR retained_health=%s\n",
                      healthName(device.health()));
    }
    updateAlarmOutput();
    Serial.printf("[MEASUREMENT] Rejected: %s\n", reason);
}

void printSamplingMetrics(const char* result,
                          const SamplingMetrics& metrics) {
    Serial.printf(
        "[SAMPLE] result=%s reason=%s timer_overruns=%lu buffer_overruns=%lu "
        "sensor_read_errors=%lu dropped_samples=%lu actual_hz=%.3f\n",
        result, samplingFailureReasonName(metrics.failureReason),
        static_cast<unsigned long>(metrics.timerOverruns),
        static_cast<unsigned long>(metrics.bufferOverruns),
        static_cast<unsigned long>(metrics.sensorReadErrors),
        static_cast<unsigned long>(metrics.droppedSamples),
        metrics.actualSampleRateHz);
}

bool validRmsStateThresholds() {
    return std::isfinite(config->offRms) &&
           std::isfinite(config->offClearRms) &&
           std::isfinite(config->warningRms) &&
           std::isfinite(config->warningClearRms) &&
           std::isfinite(config->faultRms) &&
           std::isfinite(config->faultClearRms) &&
           config->offRms >= 0.0f &&
           config->offRms < config->offClearRms &&
           config->offClearRms < config->warningClearRms &&
           config->warningClearRms >= 0.0f &&
           config->warningClearRms < config->warningRms &&
           config->warningRms < config->faultClearRms &&
           config->faultClearRms < config->faultRms;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    config = &loadConfig();
    if (config->alarmPin >= 0) {
        const uint8_t inactiveLevel =
            config->alarmActiveHigh ? LOW : HIGH;
        // Prime the output latch before changing direction, avoiding a short
        // active pulse on active-low buzzer modules during boot.
        digitalWrite(config->alarmPin, inactiveLevel);
        pinMode(config->alarmPin, OUTPUT);
        updateAlarmOutput();
        Serial.printf("[ALARM] pin=%d active_%s\n", config->alarmPin,
                      config->alarmActiveHigh ? "HIGH" : "LOW");
    } else {
        Serial.println(
            "[ALARM] GPIO disabled; configure P1_ALARM_PIN after wiring");
    }

    if (rgbLedEnabled) {
        // Set the inactive latch before enabling output to prevent a visible
        // boot flash, especially for common-anode LEDs (inactive is HIGH).
        writeRgbChannel(P1_RGB_LED_RED_PIN, false);
        writeRgbChannel(P1_RGB_LED_GREEN_PIN, false);
        writeRgbChannel(P1_RGB_LED_BLUE_PIN, false);
        pinMode(P1_RGB_LED_RED_PIN, OUTPUT);
        pinMode(P1_RGB_LED_GREEN_PIN, OUTPUT);
        pinMode(P1_RGB_LED_BLUE_PIN, OUTPUT);
        rgbReady = true;
        updateRgbOutput();
        Serial.printf("[RGB] red=%d green=%d blue=%d common_%s\n",
                      P1_RGB_LED_RED_PIN, P1_RGB_LED_GREEN_PIN,
                      P1_RGB_LED_BLUE_PIN,
                      P1_RGB_LED_COMMON_ANODE ? "ANODE" : "CATHODE");
    } else {
        Serial.println("[RGB] disabled");
    }

    stateThresholdsReady = validRmsStateThresholds();
    if (!stateThresholdsReady) {
        Serial.println("[ERROR] Invalid RMS state/hysteresis thresholds");
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

    const FeatureConfig featureConfig{P3_EXPERIMENTAL_BAND_LOW_HZ,
                                      P3_EXPERIMENTAL_BAND_HIGH_HZ, true};
    if (!configureFeatures(featureConfig)) {
        Serial.println("[ERROR] Invalid P3 feature configuration");
    }

#if P3_GENERATED_MODEL_AVAILABLE
    const ClassifierConfig classifierConfig = p3_model::classifierConfig();
    if (!configureClassifier(classifierConfig)) {
        Serial.println("[ERROR] Generated P3 model is invalid; using RMS baseline");
    } else {
        Serial.printf("[P3] Loaded calibrated model id=%s\n",
                      p3_model::modelId());
    }
#endif

    Serial.println(
        "[BOOT] Integrated firmware: ADXL345 + time/FFT features");
    Serial.printf("[BOOT] device=%s session=%s\n", config->deviceId,
                  sessionId());
    if (isClassifierConfigured()) {
        Serial.println("[P3] Calibrated anomaly classifier enabled");
    } else {
        Serial.println(
            "[P3] No calibrated model; RMS baseline remains active");
    }
}

void loop() {
    const uint32_t now = millis();
    if (now - lastRecord < config->recordIntervalMs) {
        delay(1);
        return;
    }
    lastRecord = now;

    const uint64_t collectionAttemptAt =
        static_cast<uint64_t>(esp_timer_get_time()) / 1000ULL;
    if (!sensorsReady) {
        sensorsReady = initSensors();
    }
    if (!sensorsReady) {
        markMeasurementInvalid("sensor initialization failure");
        return;
    }

    const bool windowCollected = collectWindow(window, collectionAttemptAt);
    SamplingMetrics samplingMetrics{};
    const bool metricsAvailable = getLastSamplingMetrics(samplingMetrics);
    if (!windowCollected) {
        if (metricsAvailable) {
            printSamplingMetrics("REJECTED", samplingMetrics);
        } else {
            Serial.println("[SAMPLE] result=REJECTED metrics=UNAVAILABLE");
        }
        markMeasurementInvalid("sampling window failure");
        return;
    }
    if (!metricsAvailable) {
        markMeasurementInvalid("sampling metrics unavailable");
        return;
    }
    if (samplingMetrics.droppedSamples != 0U) {
        printSamplingMetrics("REJECTED", samplingMetrics);
        markMeasurementInvalid("non-consecutive sampling window");
        return;
    }

    FanVibrationFeatures fanFeatures{};
    if (!extractFanFeatures(window.values, SAMPLE_COUNT, window.sampleRateHz,
                            fanFeatures)) {
        markMeasurementInvalid("P3 feature extraction failure");
        return;
    }

    const VibrationFeatures features{fanFeatures.rms, fanFeatures.peakToPeak,
                                     fanFeatures.crestFactor};
    float anomalyScore = NAN;
    HealthState result = HealthState::UNKNOWN;
    StateEvidence stateEvidence{HealthState::UNKNOWN, false, false, false,
                                false};
    const bool useAnomalyClassifier = isClassifierConfigured();
    if (!stateThresholdsReady) {
        markMeasurementInvalid("invalid RMS state thresholds");
        return;
    }
    if (useAnomalyClassifier) {
        anomalyScore = calculateAnomalyScore(fanFeatures);
        result = classifyCondition(fanFeatures);
        float scoreThreshold = NAN;
        float scoreClearThreshold = NAN;
        if (!std::isfinite(anomalyScore) || result == HealthState::UNKNOWN ||
            !getClassifierScoreThresholds(scoreThreshold,
                                          scoreClearThreshold)) {
            markMeasurementInvalid("P3 classifier failure");
            return;
        }
        // P3 currently has binary NORMAL/WARNING labels, so do not invent a
        // FAULT score. P3 must learn/provide its separate clear threshold.
        if (fanFeatures.rms <= config->offRms) {
            result = HealthState::OFF;
        }
        const bool belowClear = anomalyScore < scoreClearThreshold;
        stateEvidence = {result, belowClear, belowClear,
                         fanFeatures.rms <= config->offRms,
                         fanFeatures.rms >= config->offClearRms};
    } else {
        // Until P3 supplies parameters learned from real labelled runs, retain
        // the existing local RMS baseline instead of inventing an anomaly score.
        result = classifyCondition(features, config->warningRms,
                                   config->faultRms);
        if (fanFeatures.rms <= config->offRms) {
            result = HealthState::OFF;
        }
        stateEvidence = {result,
                         fanFeatures.rms < config->warningClearRms,
                         fanFeatures.rms < config->faultClearRms,
                         fanFeatures.rms <= config->offRms,
                         fanFeatures.rms >= config->offClearRms};
    }
    if (result == HealthState::UNKNOWN) {
        markMeasurementInvalid("classification result unavailable");
        return;
    }

    const StateChange stateChange = device.observe(stateEvidence);
    if (stateChange.sensorErrorChanged && !device.sensorErrorActive()) {
        Serial.println("[STATE] SENSOR_RECOVERED");
    }
    if (stateChange.healthChanged) {
        Serial.printf("[STATE] %s\n", healthName(device.health()));
    } else if (device.health() == HealthState::UNKNOWN) {
        Serial.printf("[STATE] pending confirmation candidate=%s\n",
                      healthName(result));
    }
    updateAlarmOutput();

    float temperatureC = NAN;
    const bool hasTemperature = readTemperature(temperatureC);
    const uint64_t sampledAt = samplingMetrics.firstTimestampUs / 1000ULL;
    const TelemetryRecord record{
        ++sequence,
        sampledAt,
        window.sampleRateHz,
        static_cast<uint16_t>(SAMPLE_COUNT),
        features,
        fanFeatures.dominantFrequency,
        fanFeatures.bandEnergy,
        anomalyScore,
        useAnomalyClassifier,
        device.health(),
        temperatureC,
        hasTemperature,
        dropped,
    };
    if (device.health() == HealthState::UNKNOWN) {
        Serial.println(
            "[QUEUE] Record not queued until health state is confirmed");
    } else if (!queueReady || !pending.push(record)) {
        ++dropped;
        Serial.printf(
            "[QUEUE] Dropped newest record, queue_dropped_total=%lu\n",
            static_cast<unsigned long>(dropped));
    }

    char temperatureText[20] = "NA";
    if (hasTemperature) {
        snprintf(temperatureText, sizeof(temperatureText), "%.2fC",
                 temperatureC);
    }
    if (useAnomalyClassifier) {
        Serial.printf(
            "[DATA] seq=%lu rms=%.5f g dominant=%.3f Hz band=%.6f g2 "
            "anomaly=%.4f temperature=%s sample_rate=%.3f Hz "
            "dropped_samples=%lu health=%s network=%s queued=%u "
            "queue_dropped_total=%lu\n",
            static_cast<unsigned long>(sequence), fanFeatures.rms,
            fanFeatures.dominantFrequency, fanFeatures.bandEnergy, anomalyScore,
            temperatureText, samplingMetrics.actualSampleRateHz,
            static_cast<unsigned long>(samplingMetrics.droppedSamples),
            healthName(device.health()), networkOnline() ? "ONLINE" : "OFFLINE",
            static_cast<unsigned>(queueReady ? pending.size() : 0),
            static_cast<unsigned long>(dropped));
    } else {
        Serial.printf(
            "[DATA] seq=%lu rms=%.5f g dominant=%.3f Hz band=%.6f g2 "
            "anomaly=UNAVAILABLE mode=RMS_BASELINE temperature=%s "
            "sample_rate=%.3f Hz dropped_samples=%lu health=%s network=%s "
            "queued=%u queue_dropped_total=%lu\n",
            static_cast<unsigned long>(sequence), fanFeatures.rms,
            fanFeatures.dominantFrequency, fanFeatures.bandEnergy,
            temperatureText, samplingMetrics.actualSampleRateHz,
            static_cast<unsigned long>(samplingMetrics.droppedSamples),
            healthName(device.health()), networkOnline() ? "ONLINE" : "OFFLINE",
            static_cast<unsigned>(queueReady ? pending.size() : 0),
            static_cast<unsigned long>(dropped));
    }
}

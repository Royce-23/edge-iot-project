// Standalone P3 hardware diagnostic. This file is built only by the
// p3-fan-vibration-diagnostic environment, so it does not replace P1's main.cpp.
#include <Arduino.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "classifier.h"
#include "sampling.h"

namespace {

constexpr std::size_t kCalibrationWindowCount = 30;
constexpr std::size_t kSampleCount = 512;
constexpr uint32_t kInitializationRetryMs = 2000;
constexpr float kRmsSigmaMultiplier = 4.0f;
constexpr float kRmsCeilingMargin = 1.10f;
constexpr float kMinimumScoreThreshold = 3.0f;
constexpr float kScoreMargin = 1.20f;
constexpr float kRelativeScaleFloor = 0.02f;

constexpr float kScaleFloors[4] = {
    1.0e-6f,  // RMS (g)
    1.0e-6f,  // crest factor
    1.0e-12f,  // band energy (g^2)
    0.0f,      // dominant-frequency floor is Fs/N and is set at runtime
};

float gAx[kSampleCount] = {};
float gAy[kSampleCount] = {};
float gAz[kSampleCount] = {};
float gSampleRateHz = 0.0f;
FanVibrationFeatures gCalibration[kCalibrationWindowCount] = {};
std::size_t gCalibrationCount = 0;
uint32_t gWindowId = 0;
uint32_t gLastInitializationAttemptMs = 0;
bool gSensorsReady = false;
bool gModelReady = false;
float gRmsThreshold = NAN;
float gScoreThreshold = NAN;

const float* scoreValues(const FanVibrationFeatures& features,
                         float output[4]) {
    output[0] = features.rms;
    output[1] = features.crestFactor;
    output[2] = features.bandEnergy;
    output[3] = features.dominantFrequency;
    return output;
}

const char* healthLabel(HealthState state) {
    switch (state) {
        case HealthState::NORMAL:
            return "NORMAL";
        case HealthState::WARNING:
            return "ABNORMAL";
        default:
            return "UNKNOWN";
    }
}

void printCsvHeader() {
    Serial.println(
        "window_id,phase,sample_rate_hz,rms_g,peak_to_peak_g,crest_factor,"
        "dominant_frequency_hz,band_energy_g2,rms_threshold_g,rms_baseline,"
        "anomaly_score,score_threshold,multi_feature");
}

void printFeaturePrefix(const char* phase,
                        const FanVibrationFeatures& features) {
    Serial.print(gWindowId);
    Serial.print(',');
    Serial.print(phase);
    Serial.print(',');
    Serial.print(gSampleRateHz, 3);
    Serial.print(',');
    Serial.print(features.rms, 7);
    Serial.print(',');
    Serial.print(features.peakToPeak, 7);
    Serial.print(',');
    Serial.print(features.crestFactor, 5);
    Serial.print(',');
    Serial.print(features.dominantFrequency, 3);
    Serial.print(',');
    Serial.print(features.bandEnergy, 9);
}

void printCalibrationRow(const FanVibrationFeatures& features) {
    printFeaturePrefix("CALIBRATING", features);
    Serial.println(",,CALIBRATING,,,CALIBRATING");
}

void printMonitoringRow(const FanDetectionResult& result,
                        HealthState baseline) {
    printFeaturePrefix("MONITORING", result.features);
    Serial.print(',');
    Serial.print(gRmsThreshold, 7);
    Serial.print(',');
    Serial.print(healthLabel(baseline));
    Serial.print(',');
    Serial.print(result.anomalyScore, 5);
    Serial.print(',');
    Serial.print(gScoreThreshold, 5);
    Serial.print(',');
    Serial.println(healthLabel(result.health));
}

float diagnosticScore(const FanVibrationFeatures& features,
                      const ClassifierConfig& config) {
    float values[4] = {};
    scoreValues(features, values);
    float score = 0.0f;
    for (std::size_t index = 0; index < 4; ++index) {
        score += config.weights[index] *
                 std::fabs((values[index] - config.means[index]) /
                           config.scales[index]);
    }
    return score;
}

bool finishCalibration() {
    ClassifierConfig config{};
    for (std::size_t featureIndex = 0; featureIndex < 4; ++featureIndex) {
        double sum = 0.0;
        for (const auto& features : gCalibration) {
            float values[4] = {};
            sum += scoreValues(features, values)[featureIndex];
        }
        config.means[featureIndex] =
            static_cast<float>(sum / kCalibrationWindowCount);
    }

    float maximumRms = 0.0f;
    for (const auto& features : gCalibration) {
        maximumRms = std::max(maximumRms, features.rms);
    }
    for (std::size_t featureIndex = 0; featureIndex < 4; ++featureIndex) {
        double squaredDifferenceSum = 0.0;
        for (const auto& features : gCalibration) {
            float values[4] = {};
            const double difference =
                scoreValues(features, values)[featureIndex] -
                config.means[featureIndex];
            squaredDifferenceSum += difference * difference;
        }
        const float measuredScale = static_cast<float>(
            std::sqrt(squaredDifferenceSum / kCalibrationWindowCount));
        const float floor =
            featureIndex == 3
                ? gSampleRateHz / kSampleCount
                : std::max(kScaleFloors[featureIndex],
                           std::fabs(config.means[featureIndex]) *
                               kRelativeScaleFloor);
        config.scales[featureIndex] = std::max(measuredScale, floor);
        config.weights[featureIndex] = 0.25f;
    }

    config.rmsThreshold =
        std::max(maximumRms * kRmsCeilingMargin,
                 config.means[0] + kRmsSigmaMultiplier * config.scales[0]);

    float maximumScore = 0.0f;
    for (const auto& features : gCalibration) {
        maximumScore =
            std::max(maximumScore, diagnosticScore(features, config));
    }
    config.scoreThreshold =
        std::max(kMinimumScoreThreshold, maximumScore * kScoreMargin);

    if (!configureClassifier(config)) {
        Serial.println("# ERROR: P3 could not configure the temporary model");
        return false;
    }
    gRmsThreshold = config.rmsThreshold;
    gScoreThreshold = config.scoreThreshold;

    Serial.println("# CALIBRATION_COMPLETE");
    Serial.println("# Temporary model; do not use these values in the report");
    Serial.print("# rms_threshold_g=");
    Serial.println(config.rmsThreshold, 7);
    Serial.print("# score_threshold=");
    Serial.println(config.scoreThreshold, 5);
    Serial.print("# means=");
    for (std::size_t index = 0; index < 4; ++index) {
        if (index != 0) {
            Serial.print(',');
        }
        Serial.print(config.means[index], 9);
    }
    Serial.println();
    Serial.print("# scales=");
    for (std::size_t index = 0; index < 4; ++index) {
        if (index != 0) {
            Serial.print(',');
        }
        Serial.print(config.scales[index], 9);
    }
    Serial.println();
    Serial.println("# Monitoring started: NORMAL or ABNORMAL is printed per window");
    return true;
}

void restartCalibration() {
    resetClassifier();
    gCalibrationCount = 0;
    gModelReady = false;
    gRmsThreshold = NAN;
    gScoreThreshold = NAN;
    Serial.println("# Calibration restarted; keep the fan running normally");
}

void handleSerialCommand() {
    while (Serial.available() > 0) {
        const char command = static_cast<char>(Serial.read());
        if (command == 'r' || command == 'R') {
            restartCalibration();
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1200);
    Serial.println("# P3 fan-vibration hardware diagnostic");
    Serial.println("# Keep the fan ON and NORMAL during the first 30 valid windows (~20 s)");
    Serial.println("# Send r to restart calibration at any time");

    if (!configureFeatures({10.0f, 200.0f, true})) {
        Serial.println("# ERROR: invalid P3 feature configuration");
        return;
    }

    gSensorsReady = initSensors();
    if (!gSensorsReady) {
        Serial.println("# ERROR: ADXL345 initialization failed; check 3.3 V and SPI wiring");
        gLastInitializationAttemptMs = millis();
        return;
    }
    printCsvHeader();
}

void loop() {
    handleSerialCommand();

    if (!gSensorsReady) {
        if (millis() - gLastInitializationAttemptMs >=
            kInitializationRetryMs) {
            gLastInitializationAttemptMs = millis();
            Serial.println("# Retrying ADXL345 initialization...");
            gSensorsReady = initSensors();
            if (gSensorsReady) {
                Serial.println("# ADXL345 initialization recovered");
                printCsvHeader();
                restartCalibration();
            }
        }
        delay(50);
        return;
    }

    if (!collectWindow(gAx, gAy, gAz, kSampleCount, gSampleRateHz)) {
        Serial.println("# ERROR: invalid sampling window; classification skipped");
        delay(250);
        return;
    }

    if (!gModelReady) {
        FanVibrationFeatures features{};
        if (!extractFanFeatures(gAz, kSampleCount, gSampleRateHz, features)) {
            Serial.println("# ERROR: feature extraction failed; calibration window skipped");
            return;
        }
        gCalibration[gCalibrationCount++] = features;
        printCalibrationRow(features);
        ++gWindowId;
        if (gCalibrationCount == kCalibrationWindowCount) {
            gModelReady = finishCalibration();
            if (!gModelReady) {
                restartCalibration();
            }
        }
        return;
    }

    FanDetectionResult result{};
    if (!analyzeFanVibration(gAz, kSampleCount, gSampleRateHz, result)) {
        Serial.println("# ERROR: P3 analysis failed; classification skipped");
        return;
    }
    printMonitoringRow(result, classifyRmsBaseline(result.features));
    ++gWindowId;
}

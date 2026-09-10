// Owner: P3
#include "classifier.h"

#include <cmath>

namespace {

ClassifierConfig activeConfig{};
bool configured = false;

FanVibrationFeatures invalidFeatures() {
    return {NAN, NAN, NAN, NAN, NAN};
}

}  // namespace

bool configureClassifier(const ClassifierConfig& config) {
    configured = false;
    if (!std::isfinite(config.rmsThreshold) || config.rmsThreshold < 0.0f ||
        !std::isfinite(config.scoreThreshold) || config.scoreThreshold < 0.0f) {
        return false;
    }
    double weightSum = 0.0;
    for (std::size_t index = 0; index < 4; ++index) {
        if (!std::isfinite(config.means[index]) ||
            !std::isfinite(config.scales[index]) ||
            config.scales[index] <= 0.0f ||
            !std::isfinite(config.weights[index]) ||
            config.weights[index] < 0.0f) {
            return false;
        }
        weightSum += config.weights[index];
    }
    if (weightSum <= 0.0) {
        return false;
    }
    activeConfig = config;
    for (std::size_t index = 0; index < 4; ++index) {
        activeConfig.weights[index] =
            static_cast<float>(config.weights[index] / weightSum);
    }
    configured = true;
    return true;
}

void resetClassifier() { configured = false; }

bool isClassifierConfigured() { return configured; }

float calculateAnomalyScore(const FanVibrationFeatures& features) {
    if (!configured || !areFeaturesValid(features)) {
        return NAN;
    }
    const float values[] = {features.rms, features.crestFactor,
                            features.bandEnergy, features.dominantFrequency};
    double score = 0.0;
    for (std::size_t index = 0; index < 4; ++index) {
        const double z =
            (static_cast<double>(values[index]) - activeConfig.means[index]) /
            activeConfig.scales[index];
        score += activeConfig.weights[index] * std::fabs(z);
    }
    return static_cast<float>(score);
}

HealthState classifyRmsBaseline(const FanVibrationFeatures& features) {
    if (!configured || !areFeaturesValid(features)) {
        return HealthState::UNKNOWN;
    }
    return features.rms > activeConfig.rmsThreshold
               ? HealthState::WARNING
               : HealthState::NORMAL;
}

HealthState classifyCondition(const FanVibrationFeatures& features) {
    const float score = calculateAnomalyScore(features);
    if (!std::isfinite(score)) {
        return HealthState::UNKNOWN;
    }
    // Dataset labels are binary, so P3 reports abnormal as WARNING. A FAULT
    // state needs a separately labelled severity rule approved by the team.
    return score > activeConfig.scoreThreshold
               ? HealthState::WARNING
               : HealthState::NORMAL;
}

bool analyzeFanVibration(const float* samples, std::size_t count,
                         float sampleRateHz, FanDetectionResult& output) {
    output = {invalidFeatures(), NAN, HealthState::UNKNOWN};
    if (!extractFanFeatures(samples, count, sampleRateHz, output.features)) {
        return false;
    }
    output.anomalyScore = calculateAnomalyScore(output.features);
    output.health = classifyCondition(output.features);
    return std::isfinite(output.anomalyScore) &&
           output.health != HealthState::UNKNOWN;
}

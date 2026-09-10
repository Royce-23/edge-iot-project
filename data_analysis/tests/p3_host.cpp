#include "classifier.h"
#include "features.h"

#include <cassert>
#include <cmath>
#include <cstdio>

int main() {
    constexpr std::size_t count = 512;
    constexpr float rate = 800.0f;
    float samples[count]{};
    for (std::size_t index = 0; index < count; ++index) {
        samples[index] = 1.0f + 0.1f * std::sin(
            2.0f * 3.14159265358979323846f * 50.0f * index / rate);
    }

    assert(configureFeatures({10.0f, 200.0f, true}));
    FanVibrationFeatures features{};
    assert(extractFanFeatures(samples, count, rate, features));
    assert(std::fabs(features.rms - 0.1f / std::sqrt(2.0f)) < 0.00001f);
    assert(std::fabs(features.peakToPeak - 0.2f) < 0.00001f);
    assert(std::fabs(features.crestFactor - std::sqrt(2.0f)) < 0.001f);
    assert(std::fabs(features.dominantFrequency - 50.0f) < 0.001f);
    assert(std::fabs(features.bandEnergy - 0.005f) < 0.00001f);

    VibrationFeatures shared{};
    assert(extractFeatures(samples, count, rate, shared));
    assert(std::fabs(shared.rms - features.rms) < 0.000001f);

    resetClassifier();
    FanDetectionResult result{};
    assert(!analyzeFanVibration(samples, count, rate, result));
    assert(result.health == HealthState::UNKNOWN);
    const ClassifierConfig model{0.08f,
                                 {0.07f, 1.4f, 0.004f, 50.0f},
                                 {0.01f, 0.1f, 0.001f, 1.5625f},
                                 {1.0f, 1.0f, 1.0f, 1.0f}, 0.2f};
    assert(configureClassifier(model));
    assert(classifyRmsBaseline(features) == HealthState::NORMAL);
    assert(analyzeFanVibration(samples, count, rate, result));
    assert(result.health == HealthState::WARNING);
    assert(std::isfinite(result.anomalyScore));

    samples[0] = NAN;
    assert(!extractFanFeatures(samples, count, rate, features));
    assert(!extractFanFeatures(nullptr, count, rate, features));
    assert(!extractFanFeatures(samples, count - 1, rate, features));
    std::puts("PASS: P3 fan features, FFT, baseline, anomaly and guards");
}

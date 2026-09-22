// Host test for the real P3 FFT, feature extractor, and classifier.
#include "features.h"

#include <assert.h>
#include <cmath>
#include <cstdio>

namespace {

constexpr std::size_t kCount = 512;
constexpr float kSampleRateHz = 800.0f;
constexpr float kPi = 3.14159265358979323846f;

void makeSine(float frequencyHz, float amplitude, float* samples) {
    for (std::size_t index = 0; index < kCount; ++index) {
        samples[index] = amplitude * std::sin(
            2.0f * kPi * frequencyHz * static_cast<float>(index) /
            kSampleRateHz);
    }
}

}  // namespace

int main() {
    assert(configureFeatures({P3_EXPERIMENTAL_BAND_LOW_HZ,
                              P3_EXPERIMENTAL_BAND_HIGH_HZ, true}));
    assert(!configureFeatures({260.0f, 200.0f, true}));

    float inBand[kCount]{};
    // Exact FFT bin 147 at 800/512 Hz, representative of the measured
    // 228-230 Hz fan vibration.
    constexpr float kInBandFrequencyHz = 229.6875f;
    makeSine(kInBandFrequencyHz, 0.20f, inBand);

    FanVibrationFeatures normal{};
    assert(extractFanFeatures(inBand, kCount, kSampleRateHz, normal));
    assert(std::fabs(normal.rms - 0.20f / std::sqrt(2.0f)) < 0.001f);
    assert(std::fabs(normal.peakToPeak - 0.40f) < 0.002f);
    assert(std::fabs(normal.crestFactor - std::sqrt(2.0f)) < 0.01f);
    assert(std::fabs(normal.dominantFrequency - kInBandFrequencyHz) < 0.01f);
    assert(normal.bandEnergy > 0.015f);

    float outOfBand[kCount]{};
    makeSine(100.0f, 0.20f, outOfBand);
    FanVibrationFeatures outside{};
    assert(extractFanFeatures(outOfBand, kCount, kSampleRateHz, outside));
    assert(std::fabs(outside.dominantFrequency - 100.0f) < 0.01f);
    assert(normal.bandEnergy > outside.bandEnergy * 1000.0f);

    float invalid[kCount]{};
    invalid[5] = NAN;
    FanVibrationFeatures rejected{};
    assert(!extractFanFeatures(invalid, kCount, kSampleRateHz, rejected));
    assert(!extractFanFeatures(inBand, kCount - 1U, kSampleRateHz, rejected));

    ClassifierConfig model{
        0.30f,
        {normal.rms, normal.crestFactor, normal.bandEnergy,
         normal.dominantFrequency},
        {0.01f, 0.10f, 0.005f, 2.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        3.0f,
        2.0f,
    };
    assert(configureClassifier(model));
    assert(std::fabs(calculateAnomalyScore(normal)) < 0.0001f);
    assert(classifyCondition(normal) == HealthState::NORMAL);

    FanVibrationFeatures abnormal = normal;
    abnormal.rms += 0.20f;
    assert(calculateAnomalyScore(abnormal) > model.scoreThreshold);
    assert(classifyCondition(abnormal) == HealthState::WARNING);

    FanDetectionResult result{};
    assert(analyzeFanVibration(inBand, kCount, kSampleRateHz, result));
    assert(result.health == HealthState::NORMAL);

    model.scoreClearThreshold = model.scoreThreshold;
    assert(!configureClassifier(model));
    assert(!isClassifierConfigured());

    std::puts("PASS: P3 FFT band, features, validation, classifier");
}

// Owner: P3
#include "features.h"

#include <algorithm>
#include <cmath>

#include "fft_processor.h"

namespace {

FeatureConfig activeConfig{10.0f, 200.0f, true};

bool validWindow(const float* samples, std::size_t count, float sampleRateHz) {
    return samples != nullptr && count >= 4 && count <= P3_MAX_SAMPLE_COUNT &&
           (count & (count - 1U)) == 0U && std::isfinite(sampleRateHz) &&
           sampleRateHz > 0.0f && activeConfig.bandHighHz <= sampleRateHz / 2.0f;
}

FanVibrationFeatures invalidFeatures() {
    return {NAN, NAN, NAN, NAN, NAN};
}

}  // namespace

bool configureFeatures(const FeatureConfig& config) {
    if (!std::isfinite(config.bandLowHz) ||
        !std::isfinite(config.bandHighHz) || config.bandLowHz < 0.0f ||
        config.bandHighHz <= config.bandLowHz) {
        return false;
    }
    activeConfig = config;
    return true;
}

bool areFeaturesValid(const FanVibrationFeatures& features) {
    const float values[] = {
        features.rms, features.peakToPeak, features.crestFactor,
        features.dominantFrequency, features.bandEnergy};
    for (float value : values) {
        if (!std::isfinite(value) || value < 0.0f) {
            return false;
        }
    }
    return true;
}

bool extractFanFeatures(const float* samples, std::size_t count,
                        float sampleRateHz, FanVibrationFeatures& output) {
    output = invalidFeatures();
    if (!validWindow(samples, count, sampleRateHz)) {
        return false;
    }

    double total = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        if (!std::isfinite(samples[index])) {
            return false;
        }
        total += samples[index];
    }
    const float mean = activeConfig.removeMean
                           ? static_cast<float>(total / count)
                           : 0.0f;

    double sumSquares = 0.0;
    float minimum = INFINITY;
    float maximum = -INFINITY;
    float peak = 0.0f;
    for (std::size_t index = 0; index < count; ++index) {
        const float value = samples[index] - mean;
        sumSquares += static_cast<double>(value) * value;
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
        peak = std::max(peak, std::fabs(value));
    }

    const float rms = static_cast<float>(std::sqrt(sumSquares / count));
    SpectralFeatures spectral{};
    if (!processFFT(samples, count, sampleRateHz, mean,
                    activeConfig.bandLowHz, activeConfig.bandHighHz, spectral)) {
        return false;
    }

    output = {rms, maximum - minimum, rms > 0.0f ? peak / rms : 0.0f,
              spectral.dominantFrequency, spectral.bandEnergy};
    return areFeaturesValid(output);
}

bool extractFeatures(const float* samples, std::size_t count,
                     float sampleRateHz, VibrationFeatures& output) {
    FanVibrationFeatures full{};
    if (!extractFanFeatures(samples, count, sampleRateHz, full)) {
        output = {NAN, NAN, NAN};
        return false;
    }
    output = {full.rms, full.peakToPeak, full.crestFactor};
    return true;
}

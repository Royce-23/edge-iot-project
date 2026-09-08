#include "module_interfaces.h"

#include <math.h>

// Valid time-domain baseline, not the final FFT/anomaly implementation.
VibrationFeatures extractFeatures(const SampleWindow& window) {
    float sumSquares = 0.0f;
    float peak = 0.0f;
    float minimum = window.values[0];
    float maximum = window.values[0];
    for (float sample : window.values) {
        if (!isfinite(sample)) {
            return {NAN, NAN, NAN};
        }
        sumSquares += sample * sample;
        peak = fmaxf(peak, fabsf(sample));
        minimum = fminf(minimum, sample);
        maximum = fmaxf(maximum, sample);
    }
    const float rms = sqrtf(sumSquares / SAMPLE_COUNT);
    return {rms, maximum - minimum, rms > 0.0f ? peak / rms : 0.0f};
}

HealthState classifyCondition(const VibrationFeatures& features,
                              float warningRms, float faultRms) {
    if (!isfinite(features.rms) || features.rms < 0.0f ||
        !isfinite(warningRms) || !isfinite(faultRms) || warningRms < 0.0f ||
        faultRms <= warningRms) {
        return HealthState::UNKNOWN;
    }
    if (features.rms >= faultRms) {
        return HealthState::FAULT;
    }
    if (features.rms >= warningRms) {
        return HealthState::WARNING;
    }
    return HealthState::NORMAL;
}

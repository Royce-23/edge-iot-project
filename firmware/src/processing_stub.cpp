#include "module_interfaces.h"
#include <math.h>

// Baseline demo mot truc, khong phai anomaly score/TinyML hoan chinh.
// Remove the window mean (DC/gravity) before computing vibration RMS in g.
// P3 still owns calibrated thresholds and FFT features.
VibrationFeatures extractFeatures(const SampleWindow& window) {
    float mean = 0;
    for (float sample : window.values) {
        if (!isfinite(sample)) return {NAN, NAN, NAN};
        mean += sample;
    }
    mean /= SAMPLE_COUNT;
    float sumSq = 0, peak = 0;
    float minimum = window.values[0] - mean, maximum = minimum;
    for (float rawSample : window.values) {
        const float sample = rawSample - mean;
        sumSq += sample * sample;
        peak = fmaxf(peak, fabsf(sample));
        minimum = fminf(minimum, sample);
        maximum = fmaxf(maximum, sample);
    }
    const float rms = sqrtf(sumSq / SAMPLE_COUNT);
    return {rms, maximum - minimum, rms > 0 ? peak / rms : 0.0f};
}

HealthState classifyCondition(const VibrationFeatures& f,
                              float warningRms, float faultRms) {
    if (!isfinite(f.rms) || f.rms < 0 || !isfinite(warningRms) ||
        !isfinite(faultRms) || warningRms < 0 || faultRms <= warningRms)
        return HealthState::UNKNOWN;
    if (f.rms >= faultRms) return HealthState::FAULT;
    if (f.rms >= warningRms) return HealthState::WARNING;
    return HealthState::NORMAL;
}

#include "module_interfaces.h"
#include <math.h>

// Baseline demo mot truc, khong phai anomaly score/TinyML hoan chinh.
// P3 can thong nhat don vi, khu DC/trong luc va cac dac trung FFT.
VibrationFeatures extractFeatures(const SampleWindow& window) {
    float sumSq = 0, peak = 0;
    float minimum = window.values[0], maximum = window.values[0];
    for (float sample : window.values) {
        if (!isfinite(sample)) return {NAN, NAN, NAN};
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


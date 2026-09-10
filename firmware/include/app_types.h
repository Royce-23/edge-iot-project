#pragma once

#include <stddef.h>
#include <stdint.h>

// UNKNOWN means that no valid measurement is available. Network connectivity
// is tracked separately and must not be encoded as a machine health state.
enum class HealthState : uint8_t { UNKNOWN, NORMAL, WARNING, FAULT };

// One processing window at the measured 800 Hz ODR. P2 fills values[] with the
// Z-axis acceleration in g after removing the per-window DC component.
constexpr size_t SAMPLE_COUNT = 512;

struct SampleWindow {
    float values[SAMPLE_COUNT];
    float sampleRateHz;
};

// Time-domain baseline shared with P3. FFT/PSD/anomaly fields are deliberately
// not represented until P3 implements them; never publish fake zero values.
struct VibrationFeatures {
    float rms;
    float peakToPeak;
    float crestFactor;
};

struct TelemetryRecord {
    uint32_t sequence;
    uint64_t uptimeMs;  // Measurement time since boot, not a Unix timestamp.
    float sampleRateHz;
    uint16_t sampleCount;
    VibrationFeatures features;
    HealthState health;
    float temperatureC;
    bool hasTemperature;
    uint32_t droppedTotal;
};

#pragma once
#include <stddef.h>
#include <stdint.h>

// UNKNOWN: chua co phep do hop le; OFFLINE thuoc trang thai mang.
enum class HealthState : uint8_t { UNKNOWN, NORMAL, WARNING, FAULT };
constexpr size_t SAMPLE_COUNT = 128;

struct SampleWindow {
    float values[SAMPLE_COUNT];
    float sampleRateHz;
};

struct VibrationFeatures {
    float rms;
    float peakToPeak;
    float crestFactor;
    // TODO P3: them dominantFrequency, bandEnergy va anomalyScore.
    // Chua tinh thi KHONG gui gia tri 0 gia lam ket qua FFT/AI.
};

struct TelemetryRecord {
    uint32_t seq;
    uint64_t uptimeMs; // Thoi diem do; KHONG phai Unix timestamp.
    VibrationFeatures features;
    HealthState health;
    uint32_t droppedTotal;
};


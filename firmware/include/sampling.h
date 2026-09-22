#pragma once
#include <cstddef>
#include <cstdint>

#include "app_types.h"

enum class SamplingFailureReason : uint8_t {
    NONE,
    INVALID_ARGUMENT,
    RECEIVE_TIMEOUT,
    SENSOR_READ_ERROR,
    NON_MONOTONIC_TIMESTAMP,
    INVALID_SAMPLE_RATE,
    DROPPED_SAMPLES,
};

struct SamplingMetrics {
    float targetSampleRateHz;
    float actualSampleRateHz;
    float meanPeriodUs;
    float jitterRmsUs;
    float maxAbsJitterUs;
    uint32_t timerOverruns;
    uint32_t bufferOverruns;
    uint32_t sensorReadErrors;
    uint32_t droppedSamples;
    uint64_t firstTimestampUs;
    uint64_t lastTimestampUs;
    SamplingFailureReason failureReason;
};

const char* samplingFailureReasonName(SamplingFailureReason reason);

// P2: false means invalid/missing window. Do not classify a failed window.
bool initSensors();

// Compatibility API used by P1/P3. It returns a fresh, consecutive XYZ window
// in g and the rate measured from the first and last sample timestamps.
bool collectWindow(float* ax, float* ay, float* az, std::size_t count,
                   float& actualSampleRateHz);

// Integrated P1/P3 adapter. The output is a 512-sample, DC-removed Z-axis
// window in g. uptimeMs is retained in the shared interface for record identity;
// P2 timestamps samples independently with esp_timer.
bool collectWindow(SampleWindow& output, uint64_t uptimeMs);

// Diagnostic API for P2 evidence/CSV capture. timestampsUs may be nullptr when
// timestamps are not needed. This function has a single-consumer contract.
// A window is rejected when acquisition observes a timer/buffer overrun, a
// sensor read error, or non-monotonic timestamps. Metrics are retained for both
// successful and failed acquisition attempts whenever sampling has started.
bool collectWindowWithDiagnostics(float* ax, float* ay, float* az,
                                  uint64_t* timestampsUs, std::size_t count,
                                  SamplingMetrics& metrics);

// Returns metrics from the latest acquisition attempt, which may have failed.
bool getLastSamplingMetrics(SamplingMetrics& metrics);
std::size_t pendingSampleCount();

// The DS18B20 is optional. Missing/invalid temperature returns false without
// invalidating vibration samples.
bool readTemperature(float& celsius);

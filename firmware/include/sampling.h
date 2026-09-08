#pragma once
#include <cstddef>
#include <cstdint>

#include "app_types.h"

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
};

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
bool collectWindowWithDiagnostics(float* ax, float* ay, float* az,
                                  uint64_t* timestampsUs, std::size_t count,
                                  SamplingMetrics& metrics);

bool getLastSamplingMetrics(SamplingMetrics& metrics);
std::size_t pendingSampleCount();

// The DS18B20 is optional. Missing/invalid temperature returns false without
// invalidating vibration samples.
bool readTemperature(float& celsius);

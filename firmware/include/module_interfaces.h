#pragma once

#include "app_types.h"

// P2: real ADXL345/optional temperature sensor implementation.
bool initSensors();
bool collectWindow(SampleWindow& output, uint64_t uptimeMs);
bool readTemperature(float& celsius);

// P3: current time-domain baseline. Keep this interface stable when the full
// FFT/anomaly implementation is integrated.
VibrationFeatures extractFeatures(const SampleWindow& window);
HealthState classifyCondition(const VibrationFeatures& features,
                              float warningRms, float faultRms);

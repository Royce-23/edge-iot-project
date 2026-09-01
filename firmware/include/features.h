#pragma once
#include "types.h"
// P3: define preprocessing, units and validity rules in contracts first.
bool extractFeatures(const float* samples, std::size_t count,
                     float sampleRateHz, VibrationFeatures& output);
float calculateAnomalyScore(const VibrationFeatures& features);
HealthState classifyCondition(const VibrationFeatures& features);

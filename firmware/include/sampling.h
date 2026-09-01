#pragma once
#include <cstddef>
// P2: false means invalid/missing window. Do not classify a failed window.
bool initSensors();
bool collectWindow(float* ax, float* ay, float* az, std::size_t count,
                   float& actualSampleRateHz);
bool readTemperature(float& celsius);

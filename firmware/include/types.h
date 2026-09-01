#pragma once
#include <cstddef>

enum class HealthState { NORMAL, WARNING, FAULT };
struct VibrationFeatures {
    float rms;
    float peakToPeak;
    float crestFactor;
    float dominantFrequency;
    float bandEnergy;
};

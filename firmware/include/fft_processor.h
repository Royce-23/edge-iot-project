#pragma once

#include <cstddef>

struct SpectralFeatures {
    float dominantFrequency;
    float bandEnergy;
};

// Radix-2 FFT with a symmetric Hann window. Uses a shared static workspace;
// call it from one processing task, never concurrently or from an ISR.
bool processFFT(const float* samples, std::size_t count, float sampleRateHz,
                float meanToRemove, float bandLowHz, float bandHighHz,
                SpectralFeatures& output);
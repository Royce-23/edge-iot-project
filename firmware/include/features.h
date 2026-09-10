#pragma once

#include <cstddef>

#include "types.h"

// P3 keeps the three shared time-domain fields in app_types.h unchanged and
// adds its spectral fields in a P3-owned structure. P1 can map this result to
// the shared telemetry contract after the team approves that interface change.
struct FanVibrationFeatures {
    float rms;                 // g, after per-window DC removal
    float peakToPeak;          // g
    float crestFactor;         // dimensionless
    float dominantFrequency;   // Hz, strongest non-DC FFT bin
    float bandEnergy;          // g^2, integrated one-sided spectral power
};

struct FeatureConfig {
    float bandLowHz;
    float bandHighHz;
    bool removeMean;
};

struct ClassifierConfig {
    float rmsThreshold;
    // Order: RMS, crest factor, band energy, dominant frequency.
    float means[4];
    float scales[4];
    float weights[4];
    float scoreThreshold;
};

struct FanDetectionResult {
    FanVibrationFeatures features;
    float anomalyScore;  // NaN when no calibrated model is loaded.
    HealthState health;  // NORMAL/WARNING; UNKNOWN for invalid/unconfigured.
};

constexpr std::size_t P3_MAX_SAMPLE_COUNT = 1024;

bool configureFeatures(const FeatureConfig& config);
bool configureClassifier(const ClassifierConfig& config);
void resetClassifier();
bool isClassifierConfigured();

bool areFeaturesValid(const FanVibrationFeatures& features);

// Original scaffold interface: time-domain output for shared app_types.h.
bool extractFeatures(const float* samples, std::size_t count,
                     float sampleRateHz, VibrationFeatures& output);

// Complete P3 interface used for abnormal fan-vibration detection.
bool extractFanFeatures(const float* samples, std::size_t count,
                        float sampleRateHz, FanVibrationFeatures& output);
float calculateAnomalyScore(const FanVibrationFeatures& features);
HealthState classifyRmsBaseline(const FanVibrationFeatures& features);
HealthState classifyCondition(const FanVibrationFeatures& features);
bool analyzeFanVibration(const float* samples, std::size_t count,
                         float sampleRateHz, FanDetectionResult& output);

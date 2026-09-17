#pragma once

#include <stdint.h>

#include "app_types.h"

const char* healthName(HealthState state);

struct StatePolicy {
    uint8_t offConfirmations = 3;
    uint8_t offClearConfirmations = 3;
    uint8_t warningConfirmations = 3;
    uint8_t faultConfirmations = 2;
    uint8_t warningClearConfirmations = 3;
    uint8_t faultClearConfirmations = 3;
    uint8_t sensorErrorClearConfirmations = 3;
};

// candidate is calculated with the enter thresholds. Clear/enter evidence is
// supplied separately to provide hysteresis without coupling this state
// machine to RMS or anomaly units.
struct StateEvidence {
    StateEvidence(HealthState candidateValue = HealthState::UNKNOWN,
                  bool belowWarningClearValue = false,
                  bool belowFaultClearValue = false,
                  bool belowOffEnterValue = false,
                  bool aboveOffClearValue = false)
        : candidate(candidateValue),
          belowWarningClear(belowWarningClearValue),
          belowFaultClear(belowFaultClearValue),
          belowOffEnter(belowOffEnterValue),
          aboveOffClear(aboveOffClearValue) {}

    HealthState candidate;
    bool belowWarningClear;
    bool belowFaultClear;
    bool belowOffEnter;
    bool aboveOffClear;
};

struct StateChange {
    bool healthChanged;
    bool sensorErrorChanged;
};

class DeviceState {
public:
    explicit DeviceState(const StatePolicy& policy = StatePolicy{});

    StateChange observe(const StateEvidence& evidence);
    StateChange measurementFailed();

    // Compatibility helper for code that has only a discrete classification.
    // It still applies confirmation/debounce; NORMAL is treated as clear
    // evidence because no separate numeric recovery threshold was supplied.
    bool update(HealthState next);
    HealthState health() const { return current_; }
    bool sensorErrorActive() const { return sensorError_; }
    bool alarmActive() const;

private:
    void resetTransitionStreaks();
    void transitionTo(HealthState next, StateChange& change);

    StatePolicy policy_{};
    HealthState current_ = HealthState::UNKNOWN;
    bool sensorError_ = false;
    uint8_t offStreak_ = 0;
    uint8_t abnormalStreak_ = 0;
    uint8_t faultStreak_ = 0;
    uint8_t recoveryStreak_ = 0;
    uint8_t validAfterSensorErrorStreak_ = 0;
};

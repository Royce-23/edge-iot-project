#include "device_state.h"

#include <limits.h>

namespace {

uint8_t atLeastOne(uint8_t value) { return value == 0 ? 1 : value; }

void incrementSaturating(uint8_t& value) {
    if (value < UCHAR_MAX) {
        ++value;
    }
}

}  // namespace

const char* healthName(HealthState state) {
    switch (state) {
        case HealthState::NORMAL:
            return "NORMAL";
        case HealthState::OFF:
            return "OFF";
        case HealthState::WARNING:
            return "WARNING";
        case HealthState::FAULT:
            return "FAULT";
        default:
            return "UNKNOWN";
    }
}

DeviceState::DeviceState(const StatePolicy& policy) : policy_(policy) {
    policy_.offConfirmations = atLeastOne(policy_.offConfirmations);
    policy_.offClearConfirmations = atLeastOne(policy_.offClearConfirmations);
    policy_.warningConfirmations =
        atLeastOne(policy_.warningConfirmations);
    policy_.faultConfirmations = atLeastOne(policy_.faultConfirmations);
    policy_.warningClearConfirmations =
        atLeastOne(policy_.warningClearConfirmations);
    policy_.faultClearConfirmations =
        atLeastOne(policy_.faultClearConfirmations);
    policy_.sensorErrorClearConfirmations =
        atLeastOne(policy_.sensorErrorClearConfirmations);
}

void DeviceState::resetTransitionStreaks() {
    offStreak_ = 0;
    abnormalStreak_ = 0;
    faultStreak_ = 0;
    recoveryStreak_ = 0;
}

void DeviceState::transitionTo(HealthState next, StateChange& change) {
    if (next != current_) {
        current_ = next;
        change.healthChanged = true;
    }
    resetTransitionStreaks();
}

StateChange DeviceState::observe(const StateEvidence& evidence) {
    if (evidence.candidate == HealthState::UNKNOWN) {
        return measurementFailed();
    }

    StateChange change{false, false};
    if (sensorError_) {
        incrementSaturating(validAfterSensorErrorStreak_);
        if (validAfterSensorErrorStreak_ >=
            policy_.sensorErrorClearConfirmations) {
            sensorError_ = false;
            validAfterSensorErrorStreak_ = 0;
            change.sensorErrorChanged = true;
        }
    } else {
        validAfterSensorErrorStreak_ = 0;
    }

    switch (current_) {
        case HealthState::UNKNOWN:
        case HealthState::NORMAL:
            recoveryStreak_ = 0;
            if (evidence.candidate == HealthState::OFF) {
                abnormalStreak_ = 0;
                faultStreak_ = 0;
                incrementSaturating(offStreak_);
                if (offStreak_ >= policy_.offConfirmations) {
                    transitionTo(HealthState::OFF, change);
                }
                break;
            }
            offStreak_ = 0;
            if (evidence.candidate == HealthState::NORMAL) {
                resetTransitionStreaks();
                if (current_ == HealthState::UNKNOWN) {
                    transitionTo(HealthState::NORMAL, change);
                }
                break;
            }

            incrementSaturating(abnormalStreak_);
            if (evidence.candidate == HealthState::FAULT) {
                incrementSaturating(faultStreak_);
            } else {
                faultStreak_ = 0;
            }

            if (faultStreak_ >= policy_.faultConfirmations) {
                transitionTo(HealthState::FAULT, change);
            } else if (abnormalStreak_ >= policy_.warningConfirmations) {
                // If the third abnormal window is the first FAULT window,
                // preserve that evidence across the NORMAL -> WARNING step so
                // the next consecutive FAULT reaches the configured count.
                const uint8_t carriedFaultStreak = faultStreak_;
                transitionTo(HealthState::WARNING, change);
                faultStreak_ = carriedFaultStreak;
            }
            break;

        case HealthState::OFF:
            offStreak_ = 0;
            if (evidence.candidate == HealthState::FAULT) {
                incrementSaturating(faultStreak_);
            } else {
                faultStreak_ = 0;
            }
            if (faultStreak_ >= policy_.faultConfirmations) {
                transitionTo(HealthState::FAULT, change);
                break;
            }

            if (evidence.candidate == HealthState::WARNING) {
                incrementSaturating(abnormalStreak_);
            } else {
                abnormalStreak_ = 0;
            }
            if (abnormalStreak_ >= policy_.warningConfirmations) {
                transitionTo(HealthState::WARNING, change);
                break;
            }

            if (evidence.candidate == HealthState::NORMAL &&
                evidence.aboveOffClear) {
                incrementSaturating(recoveryStreak_);
            } else {
                recoveryStreak_ = 0;
            }
            if (recoveryStreak_ >= policy_.offClearConfirmations) {
                transitionTo(HealthState::NORMAL, change);
            }
            break;

        case HealthState::WARNING:
            abnormalStreak_ = 0;
            if (evidence.candidate == HealthState::FAULT) {
                incrementSaturating(faultStreak_);
            } else {
                faultStreak_ = 0;
            }
            if (faultStreak_ >= policy_.faultConfirmations) {
                transitionTo(HealthState::FAULT, change);
                break;
            }

            if ((evidence.candidate == HealthState::NORMAL ||
                 evidence.candidate == HealthState::OFF) &&
                evidence.belowWarningClear) {
                incrementSaturating(recoveryStreak_);
            } else {
                recoveryStreak_ = 0;
            }
            if (recoveryStreak_ >= policy_.warningClearConfirmations) {
                transitionTo(evidence.belowOffEnter ? HealthState::OFF
                                                    : HealthState::NORMAL,
                             change);
            }
            break;

        case HealthState::FAULT:
            abnormalStreak_ = 0;
            faultStreak_ = 0;
            // Recovery is deliberately stepwise: FAULT first becomes WARNING,
            // then WARNING must satisfy its own lower clear threshold.
            if (evidence.candidate != HealthState::FAULT &&
                evidence.belowFaultClear) {
                incrementSaturating(recoveryStreak_);
            } else {
                recoveryStreak_ = 0;
            }
            if (recoveryStreak_ >= policy_.faultClearConfirmations) {
                transitionTo(HealthState::WARNING, change);
            }
            break;
    }
    return change;
}

StateChange DeviceState::measurementFailed() {
    StateChange change{false, !sensorError_};
    sensorError_ = true;
    validAfterSensorErrorStreak_ = 0;
    // A failed window breaks every consecutive-window sequence but must not
    // erase the last valid machine health state.
    resetTransitionStreaks();
    return change;
}

bool DeviceState::update(HealthState next) {
    const bool normal = next == HealthState::NORMAL;
    const bool off = next == HealthState::OFF;
    return observe({next, normal || off, normal || off, off, normal})
        .healthChanged;
}

bool DeviceState::alarmActive() const {
    return current_ == HealthState::FAULT;
}

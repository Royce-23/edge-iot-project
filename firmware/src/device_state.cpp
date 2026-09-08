#include "device_state.h"

const char* healthName(HealthState state) {
    switch (state) {
        case HealthState::NORMAL:
            return "NORMAL";
        case HealthState::WARNING:
            return "WARNING";
        case HealthState::FAULT:
            return "FAULT";
        default:
            return "UNKNOWN";
    }
}

bool DeviceState::update(HealthState next) {
    if (next == current_) {
        return false;
    }
    current_ = next;
    return true;
}

bool DeviceState::alarmActive() const {
    return current_ == HealthState::WARNING || current_ == HealthState::FAULT;
}

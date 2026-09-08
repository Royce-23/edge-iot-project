#pragma once

#include "app_types.h"

const char* healthName(HealthState state);

class DeviceState {
public:
    bool update(HealthState next);
    HealthState health() const { return current_; }
    bool alarmActive() const;

private:
    HealthState current_ = HealthState::UNKNOWN;
};

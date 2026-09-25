#pragma once

#include <cstdint>

// Configures the PWM output on P1_FAN_PWM_PIN. No-op if that pin is disabled
// (-1). Must run after Serial.begin().
void initFanControl();

// Applies a fan command: on/off plus a 0-100 duty cycle percentage. Safe to
// call from any task (network task included); speedPercent above 100 is
// clamped.
void setFanCommand(bool on, uint8_t speedPercent);

bool fanIsOn();
uint8_t fanSpeedPercent();

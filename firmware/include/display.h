#pragma once

#include <cstdint>

#include "app_types.h"

// Initializes the I2C OLED (if P1_OLED_SDA_PIN/SCL_PIN are configured). Safe
// to call even when disabled; every other function then becomes a no-op.
void initDisplay();

// Redraws the status screen. Call once per measurement cycle from loop().
void updateDisplay(HealthState health, float rms, bool hasTemperature,
                   float temperatureC, bool fanOn, uint8_t fanSpeedPercent,
                   bool networkOnline);

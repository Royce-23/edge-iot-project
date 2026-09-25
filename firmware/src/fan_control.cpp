#include "fan_control.h"

#include <Arduino.h>
#include <atomic>

#include "hardware_config.h"

namespace {

std::atomic<bool> fanOnState{false};
std::atomic<uint8_t> fanSpeedState{0};
bool fanReady = false;

constexpr uint32_t maxDutyValue() {
    return (1UL << P1_FAN_PWM_RESOLUTION_BITS) - 1UL;
}

}  // namespace

void initFanControl() {
#if P1_FAN_PWM_PIN >= 0
    // Arduino-ESP32 2.x LEDC API: a channel is configured with a frequency and
    // resolution, then attached to the physical pin.
    ledcSetup(P1_FAN_PWM_CHANNEL, P1_FAN_PWM_FREQ_HZ,
              P1_FAN_PWM_RESOLUTION_BITS);
    ledcAttachPin(P1_FAN_PWM_PIN, P1_FAN_PWM_CHANNEL);
    ledcWrite(P1_FAN_PWM_CHANNEL, 0);
    fanReady = true;
    Serial.printf("[FAN] pin=%d channel=%d freq=%uHz resolution=%ubit\n",
                  P1_FAN_PWM_PIN, P1_FAN_PWM_CHANNEL, P1_FAN_PWM_FREQ_HZ,
                  P1_FAN_PWM_RESOLUTION_BITS);
#else
    Serial.println(
        "[FAN] GPIO disabled; configure P1_FAN_PWM_PIN after wiring");
#endif
}

void setFanCommand(bool on, uint8_t speedPercent) {
    const uint8_t clampedSpeed = speedPercent > 100 ? 100 : speedPercent;
    fanOnState.store(on);
    fanSpeedState.store(clampedSpeed);

#if P1_FAN_PWM_PIN >= 0
    if (!fanReady) {
        return;
    }
    const uint32_t duty =
        on ? (maxDutyValue() * clampedSpeed) / 100UL : 0UL;
    ledcWrite(P1_FAN_PWM_CHANNEL, duty);
    Serial.printf("[FAN] command on=%d speed=%u%% duty=%lu/%lu\n", on ? 1 : 0,
                  clampedSpeed, static_cast<unsigned long>(duty),
                  static_cast<unsigned long>(maxDutyValue()));
#else
    Serial.println("[FAN] Command received but P1_FAN_PWM_PIN is disabled");
#endif
}

bool fanIsOn() { return fanOnState.load(); }

uint8_t fanSpeedPercent() { return fanSpeedState.load(); }

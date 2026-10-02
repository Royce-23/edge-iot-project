#include <Arduino.h>

#include "hardware_config.h"
#include "sensors/temperature.h"

namespace {

constexpr uint32_t kRetryIntervalMs = 2000;
uint32_t lastAttemptMs = 0;
bool detected = false;

void detectSensor() {
    detected = sensors::initTemperatureSensor();
    Serial.printf("[DS18B20] pin=GPIO%d status=%s error=%s\n",
                  P2_DS18B20_PIN,
                  detected ? "DETECTED" : "NOT_DETECTED",
                  sensors::temperatureErrorName(
                      sensors::lastTemperatureError()));
    if (!detected) {
        Serial.println(
            "[DS18B20] Check GND/DQ/VDD order and 4.7k pull-up from DQ to 3V3");
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("[DS18B20] Standalone temperature diagnostic");
    detectSensor();
}

void loop() {
    const uint32_t now = millis();
    if (now - lastAttemptMs < kRetryIntervalMs) {
        delay(10);
        return;
    }
    lastAttemptMs = now;

    if (!detected) {
        detectSensor();
        return;
    }

    float temperatureC = 0.0f;
    if (sensors::readTemperatureC(temperatureC)) {
        Serial.printf("[DS18B20] temperature=%.2f C\n", temperatureC);
    } else {
        Serial.printf("[DS18B20] READ_FAILED error=%s; retrying detection\n",
                      sensors::temperatureErrorName(
                          sensors::lastTemperatureError()));
        detected = false;
    }
}

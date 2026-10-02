#include <Arduino.h>
#include <OneWire.h>

#include "hardware_config.h"

namespace {

OneWire bus(P2_DS18B20_PIN);
uint8_t address[8] = {};
bool found = false;

bool findSensor() {
    pinMode(P2_DS18B20_PIN, INPUT_PULLUP);
    delay(2);
    const int idleLevel = digitalRead(P2_DS18B20_PIN);
    const bool presence = bus.reset() != 0;
    Serial.printf("[ONEWIRE] DQ_IDLE=%s PRESENCE=%s\n",
                  idleLevel == HIGH ? "HIGH" : "LOW",
                  presence ? "YES" : "NO");
    if (idleLevel != HIGH) {
        Serial.println(
            "[ONEWIRE] BUS_STUCK_LOW: check DQ short to GND or pin order");
        return false;
    }
    if (!presence) {
        Serial.println(
            "[ONEWIRE] NO_PRESENCE: check sensor orientation, DQ continuity, or replace sensor");
        return false;
    }

    bus.reset_search();
    if (!bus.search(address)) {
        Serial.println("[ONEWIRE] NOT_FOUND");
        return false;
    }
    if (OneWire::crc8(address, 7) != address[7]) {
        Serial.println("[ONEWIRE] ROM_CRC_ERROR");
        return false;
    }
    if (address[0] != 0x28) {
        Serial.printf("[ONEWIRE] UNSUPPORTED_FAMILY=0x%02X\n", address[0]);
        return false;
    }

    Serial.print("[ONEWIRE] DS18B20 address=");
    for (uint8_t value : address) {
        Serial.printf("%02X", value);
    }
    Serial.println();
    return true;
}

bool readTemperature(float& temperatureC) {
    if (!bus.reset()) {
        Serial.println("[ONEWIRE] PRESENCE_LOST before conversion");
        return false;
    }
    bus.select(address);
    bus.write(0x44, 1);
    delay(800);
    bus.depower();

    if (!bus.reset()) {
        Serial.println("[ONEWIRE] PRESENCE_LOST before scratchpad read");
        return false;
    }
    bus.select(address);
    bus.write(0xBE);

    uint8_t scratchpad[9] = {};
    for (uint8_t& value : scratchpad) {
        value = bus.read();
    }
    if (OneWire::crc8(scratchpad, 8) != scratchpad[8]) {
        Serial.print("[ONEWIRE] SCRATCHPAD_CRC_ERROR raw=");
        for (uint8_t value : scratchpad) {
            Serial.printf("%02X", value);
        }
        Serial.println();
        return false;
    }

    const int16_t raw = static_cast<int16_t>(
        static_cast<uint16_t>(scratchpad[0]) |
        (static_cast<uint16_t>(scratchpad[1]) << 8));
    temperatureC = static_cast<float>(raw) / 16.0f;
    return temperatureC >= -55.0f && temperatureC <= 125.0f;
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.printf("[ONEWIRE] Independent DS18B20 test on GPIO%d\n",
                  P2_DS18B20_PIN);
}

void loop() {
    if (!found) {
        found = findSensor();
    } else {
        float temperatureC = 0.0f;
        if (readTemperature(temperatureC)) {
            Serial.printf("[ONEWIRE] temperature=%.2f C\n", temperatureC);
        } else {
            found = false;
        }
    }
    delay(2000);
}

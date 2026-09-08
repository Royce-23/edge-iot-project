// Owner: P2
#include "sensors/temperature.h"

#include <Arduino.h>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "hardware_config.h"

namespace {

constexpr uint8_t kSkipRom = 0xCC;
constexpr uint8_t kConvertTemperature = 0x44;
constexpr uint8_t kWriteScratchpad = 0x4E;
constexpr uint8_t kReadScratchpad = 0xBE;
constexpr uint8_t kTenBitResolution = 0x3F;
constexpr uint32_t kConversionTimeoutMs = 250;

bool gAvailable = false;

void driveBusLow() {
    pinMode(P2_DS18B20_PIN, OUTPUT);
    digitalWrite(P2_DS18B20_PIN, LOW);
}

void releaseBus() {
    pinMode(P2_DS18B20_PIN, INPUT_PULLUP);
}

bool resetBus() {
    driveBusLow();
    delayMicroseconds(480);
    releaseBus();
    delayMicroseconds(70);
    const bool present = digitalRead(P2_DS18B20_PIN) == LOW;
    delayMicroseconds(410);
    return present;
}

void writeBit(bool value) {
    noInterrupts();
    driveBusLow();
    if (value) {
        delayMicroseconds(6);
        releaseBus();
        interrupts();
        delayMicroseconds(64);
    } else {
        delayMicroseconds(60);
        releaseBus();
        interrupts();
        delayMicroseconds(10);
    }
}

bool readBit() {
    noInterrupts();
    driveBusLow();
    delayMicroseconds(3);
    releaseBus();
    delayMicroseconds(10);
    const bool value = digitalRead(P2_DS18B20_PIN) == HIGH;
    interrupts();
    delayMicroseconds(53);
    return value;
}

void writeByte(uint8_t value) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
        writeBit((value & 0x01U) != 0);
        value >>= 1;
    }
}

uint8_t readByte() {
    uint8_t value = 0;
    for (uint8_t bit = 0; bit < 8; ++bit) {
        if (readBit()) {
            value |= static_cast<uint8_t>(1U << bit);
        }
    }
    return value;
}

uint8_t crc8(const uint8_t* data, std::size_t count) {
    uint8_t crc = 0;
    for (std::size_t index = 0; index < count; ++index) {
        uint8_t input = data[index];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            const uint8_t mix = (crc ^ input) & 0x01U;
            crc >>= 1;
            if (mix != 0) {
                crc ^= 0x8CU;
            }
            input >>= 1;
        }
    }
    return crc;
}

bool selectOnlyDevice() {
    if (!resetBus()) {
        return false;
    }
    writeByte(kSkipRom);
    return true;
}

}  // namespace

namespace sensors {

bool initTemperatureSensor() {
    releaseBus();
    delay(2);
    if (!selectOnlyDevice()) {
        gAvailable = false;
        return false;
    }

    // Configure 10-bit conversion (maximum 187.5 ms). Alarm bytes are kept at
    // harmless defaults. The setting is not copied to EEPROM, avoiding wear.
    writeByte(kWriteScratchpad);
    writeByte(75);
    writeByte(70);
    writeByte(kTenBitResolution);
    gAvailable = true;
    return true;
}

bool isTemperatureSensorAvailable() {
    return gAvailable;
}

bool readTemperatureC(float& celsius) {
    if (!gAvailable || !selectOnlyDevice()) {
        gAvailable = false;
        return false;
    }

    writeByte(kConvertTemperature);
    const uint32_t startedAtMs = millis();
    while (!readBit()) {
        if (millis() - startedAtMs >= kConversionTimeoutMs) {
            return false;
        }
        delay(2);
    }

    if (!selectOnlyDevice()) {
        gAvailable = false;
        return false;
    }
    writeByte(kReadScratchpad);

    uint8_t scratchpad[9] = {};
    for (uint8_t index = 0; index < sizeof(scratchpad); ++index) {
        scratchpad[index] = readByte();
    }
    if (crc8(scratchpad, 8) != scratchpad[8]) {
        return false;
    }

    const int16_t raw = static_cast<int16_t>(
        static_cast<uint16_t>(scratchpad[0]) |
        (static_cast<uint16_t>(scratchpad[1]) << 8));
    celsius = static_cast<float>(raw) / 16.0f;
    return std::isfinite(celsius) && celsius >= -55.0f && celsius <= 125.0f;
}

}  // namespace sensors
// Owner: P2
#include "sensors/temperature.h"

#include <Arduino.h>
#include <OneWire.h>
#include <cmath>
#include <cstdint>

#include "hardware_config.h"

namespace {

constexpr uint8_t kDs18b20Family = 0x28;
constexpr uint8_t kConvertTemperature = 0x44;
constexpr uint8_t kWriteScratchpad = 0x4E;
constexpr uint8_t kReadScratchpad = 0xBE;
constexpr uint8_t kTenBitResolution = 0x3F;
constexpr uint32_t kTenBitConversionMs = 200;

OneWire gBus(P2_DS18B20_PIN);
uint8_t gAddress[8] = {};
bool gAvailable = false;
sensors::TemperatureError gLastError = sensors::TemperatureError::NONE;

bool busIdleHigh() {
    pinMode(P2_DS18B20_PIN, INPUT_PULLUP);
    delay(2);
    return digitalRead(P2_DS18B20_PIN) == HIGH;
}

bool selectSensor() {
    if (!gBus.reset()) {
        gAvailable = false;
        gLastError = sensors::TemperatureError::NOT_PRESENT;
        return false;
    }
    gBus.select(gAddress);
    return true;
}

bool readScratchpad(uint8_t (&scratchpad)[9]) {
    if (!selectSensor()) {
        return false;
    }
    gBus.write(kReadScratchpad);
    for (uint8_t& value : scratchpad) {
        value = gBus.read();
    }
    if (OneWire::crc8(scratchpad, 8) != scratchpad[8]) {
        gLastError = sensors::TemperatureError::CRC_ERROR;
        return false;
    }
    return true;
}

}  // namespace

namespace sensors {

bool initTemperatureSensor() {
    gAvailable = false;
    if (!busIdleHigh()) {
        gLastError = TemperatureError::BUS_STUCK_LOW;
        return false;
    }

    gBus.reset_search();
    if (!gBus.search(gAddress)) {
        gLastError = TemperatureError::NOT_PRESENT;
        return false;
    }
    if (OneWire::crc8(gAddress, 7) != gAddress[7] ||
        gAddress[0] != kDs18b20Family) {
        gLastError = TemperatureError::CRC_ERROR;
        return false;
    }

    if (!selectSensor()) {
        return false;
    }
    // Configure 10-bit conversion (maximum 187.5 ms). The setting is kept in
    // scratchpad RAM only, avoiding EEPROM wear.
    gBus.write(kWriteScratchpad);
    gBus.write(75);
    gBus.write(70);
    gBus.write(kTenBitResolution);

    gAvailable = true;
    gLastError = TemperatureError::NONE;
    return true;
}

bool isTemperatureSensorAvailable() { return gAvailable; }

bool readTemperatureC(float& celsius) {
    // Retry discovery automatically after a loose wire or late connection.
    if (!gAvailable && !initTemperatureSensor()) {
        return false;
    }
    if (!busIdleHigh()) {
        gAvailable = false;
        gLastError = TemperatureError::BUS_STUCK_LOW;
        return false;
    }
    if (!selectSensor()) {
        return false;
    }

    gBus.write(kConvertTemperature);
    delay(kTenBitConversionMs);

    uint8_t scratchpad[9] = {};
    if (!readScratchpad(scratchpad)) {
        return false;
    }

    const int16_t raw = static_cast<int16_t>(
        static_cast<uint16_t>(scratchpad[0]) |
        (static_cast<uint16_t>(scratchpad[1]) << 8));
    celsius = static_cast<float>(raw) / 16.0f;
    if (!std::isfinite(celsius) || celsius < -55.0f || celsius > 125.0f) {
        gLastError = TemperatureError::OUT_OF_RANGE;
        return false;
    }

    gLastError = TemperatureError::NONE;
    return true;
}

TemperatureError lastTemperatureError() { return gLastError; }

const char* temperatureErrorName(TemperatureError error) {
    switch (error) {
        case TemperatureError::NONE:
            return "NONE";
        case TemperatureError::NOT_AVAILABLE:
            return "NOT_AVAILABLE";
        case TemperatureError::BUS_STUCK_LOW:
            return "BUS_STUCK_LOW";
        case TemperatureError::NOT_PRESENT:
            return "NOT_PRESENT";
        case TemperatureError::CONVERSION_TIMEOUT:
            return "CONVERSION_TIMEOUT";
        case TemperatureError::CRC_ERROR:
            return "CRC_ERROR";
        case TemperatureError::OUT_OF_RANGE:
            return "OUT_OF_RANGE";
        default:
            return "UNKNOWN";
    }
}

}  // namespace sensors

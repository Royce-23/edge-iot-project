#pragma once

#include <stdint.h>

namespace sensors {

enum class TemperatureError : uint8_t {
    NONE,
    NOT_AVAILABLE,
    BUS_STUCK_LOW,
    NOT_PRESENT,
    CONVERSION_TIMEOUT,
    CRC_ERROR,
    OUT_OF_RANGE,
};

// The temperature sensor is optional. A missing DS18B20 must not disable the
// vibration pipeline.
bool initTemperatureSensor();
bool isTemperatureSensorAvailable();
bool readTemperatureC(float& celsius);
TemperatureError lastTemperatureError();
const char* temperatureErrorName(TemperatureError error);

}  // namespace sensors

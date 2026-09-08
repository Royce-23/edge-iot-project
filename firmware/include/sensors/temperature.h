#pragma once

namespace sensors {

// The temperature sensor is optional. A missing DS18B20 must not disable the
// vibration pipeline.
bool initTemperatureSensor();
bool isTemperatureSensorAvailable();
bool readTemperatureC(float& celsius);

}  // namespace sensors


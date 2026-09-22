#pragma once

namespace sensors {

struct AccelerationSample {
    float xG;
    float yG;
    float zG;
};

// Initializes the ADXL345 in 4-wire SPI, full-resolution measurement mode.
// Returns false when the device ID or a configuration register is invalid.
bool initAdxl345();

// Reads one XYZ sample in g and applies the configured software biases.
bool readAdxl345(AccelerationSample& sample);

}  // namespace sensors


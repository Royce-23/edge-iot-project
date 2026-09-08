#include "module_interfaces.h"

#include <math.h>

// Host-test-only fake source. platformio.ini explicitly excludes this file from
// every ESP32 build, so firmware can never silently replace the real ADXL345.
bool initSensors() { return true; }

bool collectWindow(SampleWindow& output, uint64_t uptimeMs) {
    const float amplitudes[] = {0.10f, 0.60f, 1.20f};
    const float amplitude = amplitudes[(uptimeMs / 10000ULL) % 3ULL];
    output.sampleRateHz = 800.0f;
    for (size_t index = 0; index < SAMPLE_COUNT; ++index) {
        output.values[index] =
            amplitude * sinf(2.0f * 3.14159265f * 50.0f * index /
                             output.sampleRateHz);
    }
    return true;
}

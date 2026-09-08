#include "module_interfaces.h"
#include <math.h>

// DU LIEU GIA. Khong doc ADXL345, khong mo phong thoi gian lay mau that.
bool initSensors() { return true; }

bool collectWindow(SampleWindow& output, uint64_t uptimeMs) {
    const float amplitudes[] = {0.10f, 0.60f, 1.20f};
    const float amplitude = amplitudes[(uptimeMs / 10000) % 3];
    output.sampleRateHz = 800.0f;
    for (size_t i = 0; i < SAMPLE_COUNT; ++i) {
        output.values[i] = amplitude * sinf(
            2.0f * 3.14159265f * 50.0f * i / output.sampleRateHz);
    }
    return true;
}


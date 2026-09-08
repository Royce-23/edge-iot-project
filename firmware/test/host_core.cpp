// Kiem tra logic thuan C++, chay tren PC bang g++; khong thay the test ESP32.
#include "module_interfaces.h"
#include "device_state.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main() {
    SampleWindow w{};
    const HealthState expected[] = {HealthState::NORMAL, HealthState::WARNING,
                                    HealthState::FAULT, HealthState::NORMAL};
    const float amplitudes[] = {.10f, .60f, 1.20f, .10f};
    for (unsigned i = 0; i < 4; ++i) {
        w.sampleRateHz = 800.0f;
        for (size_t j = 0; j < SAMPLE_COUNT; ++j)
            w.values[j] = 1.0f + amplitudes[i] * sinf(
                2.0f * 3.14159265f * 50.0f * j / w.sampleRateHz);
        auto f = extractFeatures(w);
        assert(classifyCondition(f, .3f, .7f) == expected[i]);
        assert(fabsf(f.crestFactor - sqrtf(2.0f)) < .001f);
    }
    VibrationFeatures f{.3f, 0, 0};
    assert(classifyCondition(f, .3f, .7f) == HealthState::WARNING);
    f.rms = .7f;
    assert(classifyCondition(f, .3f, .7f) == HealthState::FAULT);
    f.rms = NAN;
    assert(classifyCondition(f, .3f, .7f) == HealthState::UNKNOWN);
    w = {};
    assert(extractFeatures(w).crestFactor == 0);
    for (float& sample : w.values) sample = 1.0f;
    assert(extractFeatures(w).rms == 0);
    assert(classifyCondition(extractFeatures(w), .3f, .7f) == HealthState::NORMAL);
    w.values[0] = NAN;
    assert(classifyCondition(extractFeatures(w), .3f, .7f) == HealthState::UNKNOWN);
    DeviceState device;
    assert(device.health() == HealthState::UNKNOWN);
    assert(device.update(HealthState::FAULT));
    assert(device.alarmActive());
    assert(!device.update(HealthState::FAULT));
    assert(device.update(HealthState::NORMAL));
    assert(!device.alarmActive());
    puts("PASS: XYZ Z-axis fixture, DC/gravity removal, RMS/crest, boundaries, invalid samples, state/alarm");
}

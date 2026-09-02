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
    for (unsigned i = 0; i < 4; ++i) {
        assert(collectWindow(w, i * 10000ULL));
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
    w.values[0] = NAN;
    assert(classifyCondition(extractFeatures(w), .3f, .7f) == HealthState::UNKNOWN);
    DeviceState device;
    assert(device.health() == HealthState::UNKNOWN);
    assert(device.update(HealthState::FAULT));
    assert(device.alarmActive());
    assert(!device.update(HealthState::FAULT));
    assert(device.update(HealthState::NORMAL));
    assert(!device.alarmActive());
    puts("PASS: mock cycle, RMS/crest, boundaries, invalid samples, state/alarm");
}

// Pure C++ logic test. This does not replace an ESP32 hardware test.
#include "device_state.h"
#include "module_interfaces.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

int main() {
    SampleWindow window{};
    const HealthState expected[] = {HealthState::NORMAL, HealthState::WARNING,
                                    HealthState::FAULT, HealthState::NORMAL};
    for (unsigned index = 0; index < 4; ++index) {
        assert(collectWindow(window, index * 10000ULL));
        const auto features = extractFeatures(window);
        assert(classifyCondition(features, 0.3f, 0.7f) == expected[index]);
        assert(fabsf(features.crestFactor - sqrtf(2.0f)) < 0.001f);
    }

    VibrationFeatures features{0.3f, 0.0f, 0.0f};
    assert(classifyCondition(features, 0.3f, 0.7f) == HealthState::WARNING);
    features.rms = 0.7f;
    assert(classifyCondition(features, 0.3f, 0.7f) == HealthState::FAULT);
    features.rms = NAN;
    assert(classifyCondition(features, 0.3f, 0.7f) == HealthState::UNKNOWN);

    window = {};
    assert(extractFeatures(window).crestFactor == 0.0f);
    window.values[0] = NAN;
    assert(classifyCondition(extractFeatures(window), 0.3f, 0.7f) ==
           HealthState::UNKNOWN);

    DeviceState device;
    assert(device.health() == HealthState::UNKNOWN);
    assert(device.update(HealthState::FAULT));
    assert(device.alarmActive());
    assert(!device.update(HealthState::FAULT));
    assert(device.update(HealthState::NORMAL));
    assert(!device.alarmActive());
    puts("PASS: mock cycle, RMS/crest, boundaries, invalid samples, state/alarm");
}

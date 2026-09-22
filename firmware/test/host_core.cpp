// Pure C++ logic test. This does not replace an ESP32 hardware test.
#include "device_state.h"
#include "module_interfaces.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

namespace {

StateEvidence rmsEvidence(float rms) {
    const HealthState candidate =
        rms <= 0.016f ? HealthState::OFF
        : rms >= 0.70f ? HealthState::FAULT
        : rms >= 0.30f ? HealthState::WARNING
                       : HealthState::NORMAL;
    return {candidate, rms < 0.24f, rms < 0.56f, rms <= 0.016f,
            rms >= 0.020f};
}

}  // namespace

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

    DeviceState warningDevice;
    assert(warningDevice.health() == HealthState::UNKNOWN);
    assert(warningDevice.observe(rmsEvidence(0.10f)).healthChanged);
    assert(warningDevice.health() == HealthState::NORMAL);
    assert(!warningDevice.observe(rmsEvidence(0.35f)).healthChanged);
    assert(!warningDevice.observe(rmsEvidence(0.35f)).healthChanged);
    assert(warningDevice.observe(rmsEvidence(0.35f)).healthChanged);
    assert(warningDevice.health() == HealthState::WARNING);
    assert(!warningDevice.alarmActive());

    // Below the enter threshold but above the lower clear threshold must not
    // chatter back to NORMAL. Three readings below 0.24 g are required.
    for (unsigned index = 0; index < 4; ++index) {
        assert(!warningDevice.observe(rmsEvidence(0.25f)).healthChanged);
    }
    assert(!warningDevice.observe(rmsEvidence(0.20f)).healthChanged);
    assert(!warningDevice.observe(rmsEvidence(0.20f)).healthChanged);
    assert(warningDevice.observe(rmsEvidence(0.20f)).healthChanged);
    assert(warningDevice.health() == HealthState::NORMAL);
    assert(!warningDevice.alarmActive());

    // Preserve consecutive FAULT evidence when the first FAULT also completes
    // the three-window WARNING confirmation.
    DeviceState mixedEscalationDevice;
    mixedEscalationDevice.observe(rmsEvidence(0.10f));
    mixedEscalationDevice.observe(rmsEvidence(0.35f));
    mixedEscalationDevice.observe(rmsEvidence(0.35f));
    assert(mixedEscalationDevice.observe(rmsEvidence(0.80f)).healthChanged);
    assert(mixedEscalationDevice.health() == HealthState::WARNING);
    assert(mixedEscalationDevice.observe(rmsEvidence(0.80f)).healthChanged);
    assert(mixedEscalationDevice.health() == HealthState::FAULT);

    DeviceState faultDevice;
    faultDevice.observe(rmsEvidence(0.10f));
    assert(!faultDevice.observe(rmsEvidence(0.80f)).healthChanged);
    assert(faultDevice.observe(rmsEvidence(0.80f)).healthChanged);
    assert(faultDevice.health() == HealthState::FAULT);
    assert(faultDevice.alarmActive());

    const StateChange faultMeasurementFailed = faultDevice.measurementFailed();
    assert(!faultMeasurementFailed.healthChanged);
    assert(faultMeasurementFailed.sensorErrorChanged);
    assert(faultDevice.health() == HealthState::FAULT);
    assert(faultDevice.sensorErrorActive());
    assert(faultDevice.alarmActive());

    // FAULT holds above its 0.56 g clear threshold, then recovers one state at
    // a time after three consecutive readings below each clear threshold.
    for (unsigned index = 0; index < 4; ++index) {
        assert(!faultDevice.observe(rmsEvidence(0.60f)).healthChanged);
    }
    assert(!faultDevice.sensorErrorActive());
    assert(!faultDevice.observe(rmsEvidence(0.50f)).healthChanged);
    assert(!faultDevice.observe(rmsEvidence(0.50f)).healthChanged);
    assert(faultDevice.observe(rmsEvidence(0.50f)).healthChanged);
    assert(faultDevice.health() == HealthState::WARNING);
    assert(!faultDevice.observe(rmsEvidence(0.20f)).healthChanged);
    assert(!faultDevice.observe(rmsEvidence(0.20f)).healthChanged);
    assert(faultDevice.observe(rmsEvidence(0.20f)).healthChanged);
    assert(faultDevice.health() == HealthState::NORMAL);

    DeviceState sensorErrorDevice;
    sensorErrorDevice.observe(rmsEvidence(0.10f));
    const StateChange failed = sensorErrorDevice.measurementFailed();
    assert(failed.sensorErrorChanged);
    assert(sensorErrorDevice.health() == HealthState::NORMAL);
    assert(sensorErrorDevice.sensorErrorActive());
    assert(!sensorErrorDevice.alarmActive());
    assert(!sensorErrorDevice.observe(rmsEvidence(0.10f)).sensorErrorChanged);
    assert(!sensorErrorDevice.observe(rmsEvidence(0.10f)).sensorErrorChanged);
    assert(sensorErrorDevice.observe(rmsEvidence(0.10f)).sensorErrorChanged);
    assert(!sensorErrorDevice.sensorErrorActive());
    assert(!sensorErrorDevice.alarmActive());

    // A failed window breaks an abnormal confirmation streak.
    DeviceState interruptedDevice;
    interruptedDevice.observe(rmsEvidence(0.10f));
    interruptedDevice.observe(rmsEvidence(0.35f));
    interruptedDevice.observe(rmsEvidence(0.35f));
    interruptedDevice.measurementFailed();
    interruptedDevice.observe(rmsEvidence(0.35f));
    interruptedDevice.observe(rmsEvidence(0.35f));
    assert(interruptedDevice.health() == HealthState::NORMAL);
    assert(interruptedDevice.observe(rmsEvidence(0.35f)).healthChanged);
    assert(interruptedDevice.health() == HealthState::WARNING);

    DeviceState bootFailureDevice;
    bootFailureDevice.measurementFailed();
    assert(bootFailureDevice.health() == HealthState::UNKNOWN);
    assert(!bootFailureDevice.alarmActive());

    DeviceState stoppedDevice;
    assert(!stoppedDevice.observe(rmsEvidence(0.013f)).healthChanged);
    assert(!stoppedDevice.observe(rmsEvidence(0.013f)).healthChanged);
    assert(stoppedDevice.observe(rmsEvidence(0.013f)).healthChanged);
    assert(stoppedDevice.health() == HealthState::OFF);
    assert(!stoppedDevice.alarmActive());
    assert(!stoppedDevice.observe(rmsEvidence(0.040f)).healthChanged);
    assert(!stoppedDevice.observe(rmsEvidence(0.040f)).healthChanged);
    assert(stoppedDevice.observe(rmsEvidence(0.040f)).healthChanged);
    assert(stoppedDevice.health() == HealthState::NORMAL);
    assert(!stoppedDevice.alarmActive());

    puts("PASS: features, OFF detection, debounce, hysteresis, FAULT-only alarm");
}

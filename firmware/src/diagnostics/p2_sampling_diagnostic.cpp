// Standalone P2 diagnostic firmware. This source is excluded from the normal
// application environment so it never conflicts with P1's setup()/loop().
#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "sampling.h"

namespace {

constexpr std::size_t kWindowSize = 512;
float gAx[kWindowSize] = {};
float gAy[kWindowSize] = {};
float gAz[kWindowSize] = {};
uint64_t gTimestampsUs[kWindowSize] = {};
uint32_t gWindowId = 0;
bool gReady = false;
uint32_t gLastInitializationAttemptMs = 0;

constexpr uint32_t kInitializationRetryMs = 2000;

void printHeader() {
    Serial.println(
        "window_id,sample_index,timestamp_us,ax_g,ay_g,az_g,target_hz,"
        "actual_hz,mean_period_us,jitter_rms_us,max_abs_jitter_us,"
        "timer_overruns,buffer_overruns,sensor_read_errors,dropped_samples");
}

void printRow(std::size_t index, const SamplingMetrics& metrics) {
    Serial.print(gWindowId);
    Serial.print(',');
    Serial.print(index);
    Serial.print(',');
    Serial.print(static_cast<unsigned long long>(gTimestampsUs[index]));
    Serial.print(',');
    Serial.print(gAx[index], 6);
    Serial.print(',');
    Serial.print(gAy[index], 6);
    Serial.print(',');
    Serial.print(gAz[index], 6);
    Serial.print(',');
    Serial.print(metrics.targetSampleRateHz, 3);
    Serial.print(',');
    Serial.print(metrics.actualSampleRateHz, 3);
    Serial.print(',');
    Serial.print(metrics.meanPeriodUs, 3);
    Serial.print(',');
    Serial.print(metrics.jitterRmsUs, 3);
    Serial.print(',');
    Serial.print(metrics.maxAbsJitterUs, 3);
    Serial.print(',');
    Serial.print(metrics.timerOverruns);
    Serial.print(',');
    Serial.print(metrics.bufferOverruns);
    Serial.print(',');
    Serial.print(metrics.sensorReadErrors);
    Serial.print(',');
    Serial.println(metrics.droppedSamples);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(1200);
    Serial.println("# P2 diagnostic starting");
    gReady = initSensors();
    if (!gReady) {
        Serial.println("# ERROR: ADXL345 initialization failed; check 3.3 V and SPI wiring");
        gLastInitializationAttemptMs = millis();
        return;
    }
    printHeader();
}

void loop() {
    if (!gReady) {
        if (millis() - gLastInitializationAttemptMs >= kInitializationRetryMs) {
            gLastInitializationAttemptMs = millis();
            Serial.println("# Retrying ADXL345 initialization...");
            gReady = initSensors();
            if (gReady) {
                Serial.println("# ADXL345 initialization recovered");
                printHeader();
            } else {
                Serial.println("# ERROR: ADXL345 initialization failed; check 3.3 V and SPI wiring");
            }
        }
        delay(50);
        return;
    }

    SamplingMetrics metrics = {};
    if (!collectWindowWithDiagnostics(gAx, gAy, gAz, gTimestampsUs,
                                      kWindowSize, metrics)) {
        Serial.println("# ERROR: invalid sampling window; not exported");
        delay(500);
        return;
    }

    for (std::size_t index = 0; index < kWindowSize; ++index) {
        printRow(index, metrics);
    }
    ++gWindowId;

    // Temperature conversion is outside the captured window. The vibration
    // collector drains any interim samples before starting the next window.
    float temperatureC = 0.0f;
    if (readTemperature(temperatureC)) {
        Serial.print("# temperature_c=");
        Serial.println(temperatureC, 2);
    }
    delay(250);
}

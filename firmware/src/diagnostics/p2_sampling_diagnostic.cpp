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
bool gCapturing = false;
uint32_t gLastInitializationAttemptMs = 0;
String gLabel;
String gFaultType;
String gRunId;
String gLoad;
uint8_t gSeverity = 0;
uint8_t gSpeedPercent = 0;

constexpr uint32_t kInitializationRetryMs = 2000;

void printHeader();

bool validToken(const String& value) {
    if (value.isEmpty() || value.length() > 32U) {
        return false;
    }
    for (std::size_t index = 0; index < value.length(); ++index) {
        const char character = value[index];
        if (!isAlphaNumeric(character) && character != '_' && character != '-') {
            return false;
        }
    }
    return true;
}

bool parseUnsignedByte(const String& value, uint8_t& output) {
    if (value.isEmpty()) {
        return false;
    }
    for (std::size_t index = 0; index < value.length(); ++index) {
        if (!isDigit(value[index])) {
            return false;
        }
    }
    const long parsed = value.toInt();
    if (parsed < 0 || parsed > 255) {
        return false;
    }
    output = static_cast<uint8_t>(parsed);
    return true;
}

bool splitStartCommand(const String& command, String (&fields)[6]) {
    int start = 6;  // Skip "START,".
    for (std::size_t index = 0; index < 6U; ++index) {
        const int separator = command.indexOf(',', start);
        if (index < 5U) {
            if (separator < 0) {
                return false;
            }
            fields[index] = command.substring(start, separator);
            start = separator + 1;
        } else {
            if (separator >= 0) {
                return false;
            }
            fields[index] = command.substring(start);
        }
        fields[index].trim();
    }
    return true;
}

void printCaptureInstructions() {
    Serial.println(
        "# START,<off|normal|abnormal>,<fault_type>,<severity_0_to_3>,"
        "<run_id>,<speed_pct>,<load>");
    Serial.println("# Examples:");
    Serial.println("# START,off,stopped,0,off01,0,no_load");
    Serial.println("# START,normal,healthy,0,normal01,100,no_load");
    Serial.println("# START,abnormal,imbalance,1,imbalance01,100,no_load");
    Serial.println("# Allowed abnormal fault_type examples: imbalance, looseness, rubbing");
    Serial.println("# Send STOP after enough complete windows have been captured");
}

void handleSerialCommand() {
    if (Serial.available() <= 0) {
        return;
    }
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.equalsIgnoreCase("STOP")) {
        gCapturing = false;
        Serial.println("# Capture stopped");
        printCaptureInstructions();
        return;
    }
    if (!command.startsWith("START,")) {
        Serial.println("# ERROR: expected START,... or STOP");
        return;
    }

    String fields[6];
    uint8_t severity = 0;
    uint8_t speedPercent = 0;
    if (!splitStartCommand(command, fields) ||
        !validToken(fields[0]) || !validToken(fields[1]) ||
        !parseUnsignedByte(fields[2], severity) || !validToken(fields[3]) ||
        !parseUnsignedByte(fields[4], speedPercent) ||
        !validToken(fields[5]) || severity > 3U || speedPercent > 100U) {
        Serial.println("# ERROR: invalid START metadata");
        printCaptureInstructions();
        return;
    }

    fields[0].toLowerCase();
    fields[1].toLowerCase();
    const bool off = fields[0] == "off";
    const bool normal = fields[0] == "normal";
    const bool abnormal = fields[0] == "abnormal";
    if ((!off && !normal && !abnormal) ||
        (off && (fields[1] != "stopped" || severity != 0U ||
                 speedPercent != 0U)) ||
        (normal && (fields[1] != "healthy" || severity != 0U)) ||
        (abnormal && (fields[1] == "healthy" || severity == 0U)) ||
        ((normal || abnormal) && speedPercent == 0U)) {
        Serial.println(
            "# ERROR: off requires stopped/severity 0/speed 0; normal "
            "requires healthy/severity 0/speed 1-100; abnormal requires "
            "a fault/severity 1-3/speed 1-100");
        return;
    }

    gLabel = fields[0];
    gFaultType = fields[1];
    gSeverity = severity;
    gRunId = fields[3];
    gSpeedPercent = speedPercent;
    gLoad = fields[5];
    gWindowId = 0;
    gCapturing = true;
    Serial.printf(
        "# Capture started label=%s fault_type=%s severity=%u run_id=%s "
        "speed_pct=%u load=%s\n",
        gLabel.c_str(), gFaultType.c_str(), static_cast<unsigned>(gSeverity),
        gRunId.c_str(), static_cast<unsigned>(gSpeedPercent), gLoad.c_str());
    printHeader();
}

void printHeader() {
    Serial.println(
        "label,fault_type,severity,run_id,speed_pct,load,"
        "window_id,sample_index,timestamp_us,ax_g,ay_g,az_g,target_hz,"
        "actual_hz,mean_period_us,jitter_rms_us,max_abs_jitter_us,"
        "timer_overruns,buffer_overruns,sensor_read_errors,dropped_samples");
}

void printRow(std::size_t index, const SamplingMetrics& metrics) {
    Serial.print(gLabel);
    Serial.print(',');
    Serial.print(gFaultType);
    Serial.print(',');
    Serial.print(gSeverity);
    Serial.print(',');
    Serial.print(gRunId);
    Serial.print(',');
    Serial.print(gSpeedPercent);
    Serial.print(',');
    Serial.print(gLoad);
    Serial.print(',');
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
    printCaptureInstructions();
}

void loop() {
    if (!gReady) {
        if (millis() - gLastInitializationAttemptMs >= kInitializationRetryMs) {
            gLastInitializationAttemptMs = millis();
            Serial.println("# Retrying ADXL345 initialization...");
            gReady = initSensors();
            if (gReady) {
                Serial.println("# ADXL345 initialization recovered");
                printCaptureInstructions();
            } else {
                Serial.println("# ERROR: ADXL345 initialization failed; check 3.3 V and SPI wiring");
            }
        }
        delay(50);
        return;
    }

    handleSerialCommand();
    if (!gCapturing) {
        delay(20);
        return;
    }

    SamplingMetrics metrics = {};
    if (!collectWindowWithDiagnostics(gAx, gAy, gAz, gTimestampsUs,
                                      kWindowSize, metrics)) {
        Serial.print("# ERROR: invalid sampling window; reason=");
        Serial.print(samplingFailureReasonName(metrics.failureReason));
        Serial.print(" timer_overruns=");
        Serial.print(metrics.timerOverruns);
        Serial.print(" buffer_overruns=");
        Serial.print(metrics.bufferOverruns);
        Serial.print(" sensor_read_errors=");
        Serial.print(metrics.sensorReadErrors);
        Serial.print(" dropped_samples=");
        Serial.print(metrics.droppedSamples);
        Serial.print(" actual_hz=");
        Serial.println(metrics.actualSampleRateHz, 3);
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
    handleSerialCommand();
}

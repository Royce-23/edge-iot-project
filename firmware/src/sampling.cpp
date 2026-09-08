// Owner: P2
#include "sampling.h"

#include <Arduino.h>
#include <cmath>
#include <cstdint>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hardware_config.h"
#include "sensors/adxl345.h"
#include "sensors/temperature.h"

namespace {

struct TimedSample {
    float ax;
    float ay;
    float az;
    uint64_t timestampUs;
    bool valid;
};

QueueHandle_t gSampleQueue = nullptr;
TaskHandle_t gSamplingTask = nullptr;
esp_timer_handle_t gSamplingTimer = nullptr;
bool gInitialized = false;
bool gHasLastMetrics = false;
SamplingMetrics gLastMetrics = {};
uint64_t gWindowTimestamps[P2_SAMPLING_BUFFER_CAPACITY] = {};

// Integration adapter storage. Keeping these buffers static avoids placing
// three 512-float arrays on Arduino loopTask's stack.
float gIntegrationAx[SAMPLE_COUNT] = {};
float gIntegrationAy[SAMPLE_COUNT] = {};
float gIntegrationAz[SAMPLE_COUNT] = {};

portMUX_TYPE gCounterLock = portMUX_INITIALIZER_UNLOCKED;
uint32_t gTimerOverruns = 0;
uint32_t gBufferOverruns = 0;
uint32_t gSensorReadErrors = 0;

constexpr uint64_t targetPeriodUs() {
    return 1000000ULL / static_cast<uint64_t>(P2_SAMPLE_RATE_HZ);
}

void incrementCounter(uint32_t& counter, uint32_t amount = 1) {
    portENTER_CRITICAL(&gCounterLock);
    counter += amount;
    portEXIT_CRITICAL(&gCounterLock);
}

void readCounters(uint32_t& timerOverruns, uint32_t& bufferOverruns,
                  uint32_t& sensorReadErrors) {
    portENTER_CRITICAL(&gCounterLock);
    timerOverruns = gTimerOverruns;
    bufferOverruns = gBufferOverruns;
    sensorReadErrors = gSensorReadErrors;
    portEXIT_CRITICAL(&gCounterLock);
}

void samplingTimerCallback(void*) {
    if (gSamplingTask != nullptr) {
        xTaskNotifyGive(gSamplingTask);
    }
}

void samplingTask(void*) {
    for (;;) {
        const uint32_t notifications = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (notifications > 1) {
            incrementCounter(gTimerOverruns, notifications - 1);
        }

        sensors::AccelerationSample acceleration = {};
        TimedSample sample = {};
        sample.valid = sensors::readAdxl345(acceleration);
        sample.timestampUs = static_cast<uint64_t>(esp_timer_get_time());
        if (sample.valid) {
            sample.ax = acceleration.xG;
            sample.ay = acceleration.yG;
            sample.az = acceleration.zG;
        } else {
            incrementCounter(gSensorReadErrors);
        }

        if (xQueueSendToBack(gSampleQueue, &sample, 0) != pdTRUE) {
            // Keep the freshest data and explicitly count each overwritten
            // sample instead of silently blocking the sampling task.
            TimedSample discarded = {};
            xQueueReceive(gSampleQueue, &discarded, 0);
            incrementCounter(gBufferOverruns);
            xQueueSendToBack(gSampleQueue, &sample, 0);
        }
    }
}

void drainQueue() {
    TimedSample discarded = {};
    while (xQueueReceive(gSampleQueue, &discarded, 0) == pdTRUE) {
    }
}

TickType_t receiveTimeoutTicks() {
    // Four target periods plus scheduler margin. pdMS_TO_TICKS may round down,
    // so force at least one RTOS tick.
    const uint32_t timeoutMs =
        static_cast<uint32_t>((4ULL * targetPeriodUs() + 999ULL) / 1000ULL) + 10U;
    const TickType_t ticks = pdMS_TO_TICKS(timeoutMs);
    return ticks == 0 ? 1 : ticks;
}

SamplingMetrics calculateMetrics(const uint64_t* timestampsUs, std::size_t count,
                                 uint32_t timerOverruns,
                                 uint32_t bufferOverruns,
                                 uint32_t sensorReadErrors) {
    SamplingMetrics metrics = {};
    metrics.targetSampleRateHz = static_cast<float>(P2_SAMPLE_RATE_HZ);
    metrics.firstTimestampUs = timestampsUs[0];
    metrics.lastTimestampUs = timestampsUs[count - 1];
    metrics.timerOverruns = timerOverruns;
    metrics.bufferOverruns = bufferOverruns;
    metrics.sensorReadErrors = sensorReadErrors;
    metrics.droppedSamples = timerOverruns + bufferOverruns + sensorReadErrors;

    const uint64_t elapsedUs = metrics.lastTimestampUs - metrics.firstTimestampUs;
    if (elapsedUs == 0 || count < 2) {
        return metrics;
    }

    metrics.meanPeriodUs =
        static_cast<float>(elapsedUs) / static_cast<float>(count - 1);
    metrics.actualSampleRateHz =
        static_cast<float>(count - 1) * 1000000.0f /
        static_cast<float>(elapsedUs);

    double squaredJitterSum = 0.0;
    float maxAbsJitterUs = 0.0f;
    for (std::size_t index = 1; index < count; ++index) {
        const float periodUs =
            static_cast<float>(timestampsUs[index] - timestampsUs[index - 1]);
        const float jitterUs = periodUs - metrics.meanPeriodUs;
        const float absoluteJitterUs = std::fabs(jitterUs);
        squaredJitterSum += static_cast<double>(jitterUs) * jitterUs;
        if (absoluteJitterUs > maxAbsJitterUs) {
            maxAbsJitterUs = absoluteJitterUs;
        }
    }
    metrics.jitterRmsUs = static_cast<float>(
        std::sqrt(squaredJitterSum / static_cast<double>(count - 1)));
    metrics.maxAbsJitterUs = maxAbsJitterUs;
    return metrics;
}

}  // namespace

bool initSensors() {
    if (gInitialized) {
        return true;
    }
    if (P2_SAMPLE_RATE_HZ == 0 || !sensors::initAdxl345()) {
        return false;
    }

    // Temperature is deliberately optional.
    sensors::initTemperatureSensor();

    gSampleQueue =
        xQueueCreate(P2_SAMPLING_BUFFER_CAPACITY, sizeof(TimedSample));
    if (gSampleQueue == nullptr) {
        return false;
    }

    const BaseType_t taskCreated = xTaskCreatePinnedToCore(
        samplingTask, "p2_sampling", P2_SAMPLING_TASK_STACK_SIZE, nullptr,
        P2_SAMPLING_TASK_PRIORITY, &gSamplingTask, 0);
    if (taskCreated != pdPASS) {
        vQueueDelete(gSampleQueue);
        gSampleQueue = nullptr;
        return false;
    }

    esp_timer_create_args_t timerArguments = {};
    timerArguments.callback = &samplingTimerCallback;
    timerArguments.dispatch_method = ESP_TIMER_TASK;
    timerArguments.name = "p2_sample_tick";
    if (esp_timer_create(&timerArguments, &gSamplingTimer) != ESP_OK ||
        esp_timer_start_periodic(gSamplingTimer, targetPeriodUs()) != ESP_OK) {
        if (gSamplingTimer != nullptr) {
            esp_timer_delete(gSamplingTimer);
            gSamplingTimer = nullptr;
        }
        vTaskDelete(gSamplingTask);
        gSamplingTask = nullptr;
        vQueueDelete(gSampleQueue);
        gSampleQueue = nullptr;
        return false;
    }

    gInitialized = true;
    return true;
}

bool collectWindow(float* ax, float* ay, float* az, std::size_t count,
                   float& actualSampleRateHz) {
    SamplingMetrics metrics = {};
    if (!collectWindowWithDiagnostics(ax, ay, az, nullptr, count, metrics)) {
        actualSampleRateHz = 0.0f;
        return false;
    }
    actualSampleRateHz = metrics.actualSampleRateHz;
    return true;
}

bool collectWindow(SampleWindow& output, uint64_t uptimeMs) {
    (void)uptimeMs;
    static_assert(SAMPLE_COUNT <= P2_SAMPLING_BUFFER_CAPACITY,
                  "Shared processing window exceeds P2 buffer capacity");

    float actualSampleRateHz = 0.0f;
    if (!collectWindow(gIntegrationAx, gIntegrationAy, gIntegrationAz,
                       SAMPLE_COUNT, actualSampleRateHz)) {
        output.sampleRateHz = 0.0f;
        return false;
    }

    // The current shared contract defines a single signal. Use the mounted
    // sensor's Z axis and remove its window mean so RMS measures vibration, not
    // gravity/DC bias. Raw XYZ remains available through the P2 diagnostic API.
    double sumZ = 0.0;
    for (std::size_t index = 0; index < SAMPLE_COUNT; ++index) {
        sumZ += gIntegrationAz[index];
    }
    const float meanZ = static_cast<float>(sumZ / SAMPLE_COUNT);
    for (std::size_t index = 0; index < SAMPLE_COUNT; ++index) {
        output.values[index] = gIntegrationAz[index] - meanZ;
    }
    output.sampleRateHz = actualSampleRateHz;
    return true;
}

bool collectWindowWithDiagnostics(float* ax, float* ay, float* az,
                                  uint64_t* timestampsUs, std::size_t count,
                                  SamplingMetrics& metrics) {
    metrics = {};
    if (!gInitialized || gSampleQueue == nullptr || ax == nullptr || ay == nullptr ||
        az == nullptr || count < 2) {
        return false;
    }

    // Each call starts from fresh data so a slow consumer never receives an old
    // window. Only one task may call collectWindow* at a time.
    drainQueue();

    uint32_t timerStart = 0;
    uint32_t bufferStart = 0;
    uint32_t errorsStart = 0;
    readCounters(timerStart, bufferStart, errorsStart);

    // Metrics need timestamps even when the caller does not request them. Use a
    // fixed global buffer: the API has a documented single-consumer contract and
    // this avoids heap fragmentation and an 8 KiB task-stack allocation.
    if (count > P2_SAMPLING_BUFFER_CAPACITY) {
        return false;
    }

    const TickType_t timeout = receiveTimeoutTicks();
    for (std::size_t index = 0; index < count; ++index) {
        TimedSample sample = {};
        if (xQueueReceive(gSampleQueue, &sample, timeout) != pdTRUE ||
            !sample.valid) {
            drainQueue();
            return false;
        }
        ax[index] = sample.ax;
        ay[index] = sample.ay;
        az[index] = sample.az;
        gWindowTimestamps[index] = sample.timestampUs;
        if (timestampsUs != nullptr) {
            timestampsUs[index] = sample.timestampUs;
        }
    }

    uint32_t timerEnd = 0;
    uint32_t bufferEnd = 0;
    uint32_t errorsEnd = 0;
    readCounters(timerEnd, bufferEnd, errorsEnd);
    metrics = calculateMetrics(gWindowTimestamps, count, timerEnd - timerStart,
                               bufferEnd - bufferStart, errorsEnd - errorsStart);
    if (metrics.actualSampleRateHz <= 0.0f) {
        return false;
    }

    gLastMetrics = metrics;
    gHasLastMetrics = true;
    return true;
}

bool getLastSamplingMetrics(SamplingMetrics& metrics) {
    if (!gHasLastMetrics) {
        return false;
    }
    metrics = gLastMetrics;
    return true;
}

std::size_t pendingSampleCount() {
    return gSampleQueue == nullptr
               ? 0
               : static_cast<std::size_t>(uxQueueMessagesWaiting(gSampleQueue));
}

bool readTemperature(float& celsius) {
    return sensors::readTemperatureC(celsius);
}

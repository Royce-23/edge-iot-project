#pragma once

#include "app_types.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// FreeRTOS queue synchronizes loop() with the network task. Only the network
// task may peek/pop; loop() only pushes.
class OfflineQueue {
public:
    bool begin();
    bool push(const TelemetryRecord& record);
    bool peek(TelemetryRecord& record) const;
    void pop();
    size_t size() const;

private:
    QueueHandle_t handle_ = nullptr;
};

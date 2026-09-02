#pragma once
#include "app_types.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// FreeRTOS queue dong bo giua loop() va network task.
// Chi network task duoc peek/pop; loop() chi push.
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


#include "offline_queue.h"

bool OfflineQueue::begin() {
    if (handle_ != nullptr) {
        return true;
    }
    handle_ = xQueueCreate(60, sizeof(TelemetryRecord));
    return handle_ != nullptr;
}

bool OfflineQueue::push(const TelemetryRecord& record) {
    // Reject the newest record when full, preserving the ordering of records
    // already waiting for transmission.
    return handle_ != nullptr && xQueueSend(handle_, &record, 0) == pdTRUE;
}

bool OfflineQueue::peek(TelemetryRecord& record) const {
    return handle_ != nullptr && xQueuePeek(handle_, &record, 0) == pdTRUE;
}

void OfflineQueue::pop() {
    if (handle_ == nullptr) {
        return;
    }
    TelemetryRecord ignored{};
    xQueueReceive(handle_, &ignored, 0);
}

size_t OfflineQueue::size() const {
    return handle_ == nullptr ? 0 : uxQueueMessagesWaiting(handle_);
}

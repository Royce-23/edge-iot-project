#include "offline_queue.h"

bool OfflineQueue::begin() {
    handle_ = xQueueCreate(60, sizeof(TelemetryRecord));
    return handle_ != nullptr;
}
bool OfflineQueue::push(const TelemetryRecord& r) {
    // Day: tu choi ban ghi MOI, giu nguyen thu tu ban ghi dang cho gui.
    return xQueueSend(handle_, &r, 0) == pdTRUE;
}
bool OfflineQueue::peek(TelemetryRecord& r) const {
    return xQueuePeek(handle_, &r, 0) == pdTRUE;
}
void OfflineQueue::pop() {
    TelemetryRecord ignored;
    xQueueReceive(handle_, &ignored, 0);
}
size_t OfflineQueue::size() const { return uxQueueMessagesWaiting(handle_); }


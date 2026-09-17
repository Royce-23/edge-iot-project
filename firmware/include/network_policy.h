#pragma once

#include <cstdint>

namespace network_policy {

constexpr uint32_t MINIMUM_RECONNECT_BACKOFF_MS = 1000U;
constexpr uint32_t MAXIMUM_RECONNECT_BACKOFF_MS = 60000U;

constexpr uint32_t initialReconnectBackoffMs(uint32_t configuredDelayMs) {
    return configuredDelayMs < MINIMUM_RECONNECT_BACKOFF_MS
               ? MINIMUM_RECONNECT_BACKOFF_MS
           : configuredDelayMs > MAXIMUM_RECONNECT_BACKOFF_MS
               ? MAXIMUM_RECONNECT_BACKOFF_MS
               : configuredDelayMs;
}

constexpr uint32_t nextReconnectBackoffMs(uint32_t currentDelayMs) {
    return currentDelayMs >= MAXIMUM_RECONNECT_BACKOFF_MS / 2U
               ? MAXIMUM_RECONNECT_BACKOFF_MS
               : currentDelayMs * 2U;
}

// Signed subtraction is the standard wrap-safe comparison for millis() as
// long as no individual deadline is more than INT32_MAX milliseconds away.
constexpr bool deadlineReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

enum class DeliveryDisposition : uint8_t {
    IGNORE,
    POP_ACKNOWLEDGED_RECORD,
    RETAIN_RECORD,
};

// Only an acknowledgement matching the one in-flight QoS 1 message may remove
// the queue head. A failed/outbox-deleted delivery remains queued for retry.
constexpr DeliveryDisposition deliveryDisposition(int eventMessageId,
                                                   int inFlightMessageId,
                                                   bool acknowledged) {
    return eventMessageId != inFlightMessageId
               ? DeliveryDisposition::IGNORE
           : acknowledged
               ? DeliveryDisposition::POP_ACKNOWLEDGED_RECORD
               : DeliveryDisposition::RETAIN_RECORD;
}

}  // namespace network_policy

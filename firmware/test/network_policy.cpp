// Host test for MQTT reconnect and QoS 1 queue-removal policy.
#include "network_policy.h"

#include <assert.h>
#include <cstdint>
#include <cstdio>

int main() {
    using network_policy::DeliveryDisposition;

    assert(network_policy::initialReconnectBackoffMs(0U) == 1000U);
    assert(network_policy::initialReconnectBackoffMs(5000U) == 5000U);
    assert(network_policy::initialReconnectBackoffMs(90000U) == 60000U);
    assert(network_policy::nextReconnectBackoffMs(1000U) == 2000U);
    assert(network_policy::nextReconnectBackoffMs(30000U) == 60000U);
    assert(network_policy::nextReconnectBackoffMs(60000U) == 60000U);

    assert(!network_policy::deadlineReached(999U, 1000U));
    assert(network_policy::deadlineReached(1000U, 1000U));
    assert(network_policy::deadlineReached(5U, UINT32_MAX - 5U));

    assert(network_policy::staleRecordCount(0U) == 0U);
    assert(network_policy::staleRecordCount(1U) == 0U);
    assert(network_policy::staleRecordCount(2U) == 1U);
    assert(network_policy::staleRecordCount(600U) == 599U);
    assert(network_policy::staleRecordCount(8U, 3U) == 5U);

    assert(network_policy::deliveryDisposition(41, 42, true) ==
           DeliveryDisposition::IGNORE);
    assert(network_policy::deliveryDisposition(42, 42, true) ==
           DeliveryDisposition::POP_ACKNOWLEDGED_RECORD);
    assert(network_policy::deliveryDisposition(42, 42, false) ==
           DeliveryDisposition::RETAIN_RECORD);

    std::puts("PASS: reconnect, realtime queue, and QoS1 delivery policy");
}

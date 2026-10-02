#pragma once

#include "config_manager.h"
#include "offline_queue.h"

// Config and queue must outlive the network task. A queued telemetry record is
// removed only after the broker acknowledges its QoS 1 PUBLISH packet.
bool startNetworkTask(const AppConfig& config, OfflineQueue& queue);
bool networkOnline();
const char* sessionId();

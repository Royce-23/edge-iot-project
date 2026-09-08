#pragma once

#include "config_manager.h"
#include "offline_queue.h"

// Config and queue must outlive the network task.
bool startNetworkTask(const AppConfig& config, OfflineQueue& queue);
bool networkOnline();
const char* sessionId();

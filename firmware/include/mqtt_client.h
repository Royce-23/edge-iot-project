#pragma once
#include "config_manager.h"
#include "offline_queue.h"

// Config va queue phai ton tai suot chuong trinh.
bool startNetworkTask(const AppConfig& config, OfflineQueue& queue);
bool networkOnline();
const char* sessionId();


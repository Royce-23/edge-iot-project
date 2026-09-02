#pragma once
#include "app_types.h"

// P2 thay noi dung sampling_stub.cpp bang driver/lap lich lay mau that.
bool initSensors();
bool collectWindow(SampleWindow& output, uint64_t uptimeMs);

// P3 thay noi dung processing_stub.cpp; giu giao dien da thong nhat.
VibrationFeatures extractFeatures(const SampleWindow& window);
HealthState classifyCondition(const VibrationFeatures& features,
                              float warningRms, float faultRms);


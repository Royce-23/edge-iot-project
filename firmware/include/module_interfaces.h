#pragma once
#include "app_types.h"

// P2's real XYZ sampling API lives in sampling.h.

// P3 thay noi dung processing_stub.cpp; giu giao dien da thong nhat.
VibrationFeatures extractFeatures(const SampleWindow& window);
HealthState classifyCondition(const VibrationFeatures& features,
                              float warningRms, float faultRms);

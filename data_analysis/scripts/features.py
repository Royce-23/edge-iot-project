"""P3 feature definitions matching firmware/src/features.cpp."""

from dataclasses import asdict, dataclass, replace
import math

import numpy as np

from fft_processor import spectral_features

FEATURE_NAMES = ("rms", "peak_to_peak", "crest_factor",
                 "dominant_frequency", "band_energy")


@dataclass(frozen=True)
class FeatureConfig:
    sample_rate_hz: float = 800.0
    window_size: int = 512
    band_low_hz: float = 10.0
    band_high_hz: float = 200.0
    remove_mean: bool = True

    def validate(self):
        count = self.window_size
        if not isinstance(count, int) or count < 4 or count > 1024 or count & (count - 1):
            raise ValueError("window_size phải là lũy thừa của 2, từ 4 đến 1024.")
        if not math.isfinite(self.sample_rate_hz) or self.sample_rate_hz <= 0:
            raise ValueError("sample_rate_hz phải hữu hạn và lớn hơn 0.")
        if not (math.isfinite(self.band_low_hz) and math.isfinite(self.band_high_hz)
                and 0 <= self.band_low_hz < self.band_high_hz <= self.sample_rate_hz / 2):
            raise ValueError("Dải tần phải thỏa 0 <= low < high <= Fs/2.")

    def with_sample_rate(self, rate):
        return replace(self, sample_rate_hz=rate)

    def to_dict(self):
        return asdict(self)


def extract_features(samples, config):
    config.validate()
    values = np.asarray(samples, dtype=float)
    if values.ndim != 1 or len(values) != config.window_size or not np.all(np.isfinite(values)):
        raise ValueError("Cửa sổ phải đủ số mẫu hữu hạn.")
    if config.remove_mean:
        values = values - np.mean(values)
    rms = float(np.sqrt(np.mean(values ** 2)))
    dominant, energy = spectral_features(values, config.sample_rate_hz,
                                         config.band_low_hz, config.band_high_hz)
    result = {
        "rms": rms,
        "peak_to_peak": float(np.ptp(values)),
        "crest_factor": float(np.max(np.abs(values)) / rms) if rms > 0 else 0.0,
        "dominant_frequency": dominant,
        "band_energy": energy,
    }
    if not all(math.isfinite(value) and value >= 0 for value in result.values()):
        raise ValueError("Đặc trưng không hợp lệ.")
    return result

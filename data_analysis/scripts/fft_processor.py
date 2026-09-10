"""P3 spectral processing shared by the fan-analysis pipeline."""

import numpy as np


def spectral_features(samples, sample_rate_hz, band_low_hz, band_high_hz):
    count = len(samples)
    window = np.hanning(count)
    spectrum = np.fft.rfft(samples * window)
    power = np.abs(spectrum) ** 2 / (count * np.sum(window ** 2))
    if count > 2:
        power[1:-1] *= 2
    frequencies = np.fft.rfftfreq(count, d=1.0 / sample_rate_hz)
    band = (frequencies >= band_low_hz) & (frequencies <= band_high_hz)
    strongest_bin = int(np.argmax(power[1:])) + 1
    dominant = float(frequencies[strongest_bin]) if power[strongest_bin] > 0 else 0.0
    return dominant, float(np.sum(power[band]))

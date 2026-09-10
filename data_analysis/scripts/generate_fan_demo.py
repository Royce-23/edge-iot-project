"""Generate labelled fan-like vibration for software tests only."""

import csv
from pathlib import Path

import numpy as np

from dataset import MANIFEST_FIELDS, P2_FIELDS, write_csv


def generate_demo(directory, config, seed=42):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    randomizer = np.random.default_rng(seed)
    manifest = []
    window_count = 8
    for label in (0, 1):
        for run_index in range(6):
            run_id = f"fan_{'normal' if label == 0 else 'abnormal'}_{run_index:02d}"
            condition = "normal" if label == 0 else ("imbalance", "loose_mount", "bearing_like")[run_index % 3]
            rows = []
            base_timestamp = 1_000_000
            for window_id in range(window_count):
                rate = 800.0
                indices = np.arange(config.window_size)
                time = indices / rate
                amplitude = 0.045 + randomizer.uniform(-0.004, 0.004)
                signal = amplitude * np.sin(2 * np.pi * 50 * time + randomizer.uniform(0, 2 * np.pi))
                if condition == "imbalance":
                    signal *= 2.2
                elif condition == "loose_mount":
                    signal += 0.025 * np.sin(2 * np.pi * 100 * time)
                    signal[config.window_size // 2] += 0.22
                elif condition == "bearing_like":
                    # Same overall amplitude as normal, but energy moves to a
                    # higher frequency: an RMS-only rule should struggle here.
                    signal = amplitude * np.sin(
                        2 * np.pi * 170 * time + randomizer.uniform(0, 2 * np.pi))
                signal += randomizer.normal(0, 0.004, config.window_size)
                timestamps = base_timestamp + np.round(indices * 1e6 / rate).astype(int)
                periods = np.diff(timestamps)
                mean_period = float(periods.mean())
                actual_rate = 1e6 / mean_period
                jitter = periods - mean_period
                for index, (timestamp, z_value) in enumerate(zip(timestamps, signal)):
                    rows.append({
                        "window_id": window_id, "sample_index": index,
                        "timestamp_us": int(timestamp), "ax_g": 0, "ay_g": 0,
                        "az_g": float(1.0 + z_value), "target_hz": 800,
                        "actual_hz": actual_rate, "mean_period_us": mean_period,
                        "jitter_rms_us": float(np.sqrt(np.mean(jitter ** 2))),
                        "max_abs_jitter_us": float(np.max(np.abs(jitter))),
                        "timer_overruns": 0, "buffer_overruns": 0,
                        "sensor_read_errors": 0, "dropped_samples": 0,
                    })
                base_timestamp = int(timestamps[-1] + 250_000 + round(1e6 / rate))
            data_name = f"{run_id}.csv"
            write_csv(directory / data_name, rows, P2_FIELDS)
            manifest.append({"path": data_name, "run_id": run_id,
                             "condition": condition,
                             "speed_configuration": "demo_fixed_speed",
                             "label": label, "source": "simulated"})
    write_csv(directory / "manifest.csv", manifest, MANIFEST_FIELDS)
    return directory / "manifest.csv"

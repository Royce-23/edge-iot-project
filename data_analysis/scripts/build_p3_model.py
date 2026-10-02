#!/usr/bin/env python3
"""Build and validate the P3 anomaly model from real diagnostic CSV runs.

The generated C++ header is written only when there are at least three
independent normal runs, three abnormal runs, and the held-out metrics meet the
requested minimum. Thresholds and normalization are derived from normal
training runs only; abnormal runs are used only for evaluation.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import statistics
import sys
from dataclasses import dataclass
from pathlib import Path


FEATURE_NAMES = ("rms", "crest_factor", "band_energy", "dominant_frequency")
SAMPLE_COUNT = 512
DEFAULT_SAMPLE_RATE_HZ = 800.0
DEFAULT_BAND_LOW_HZ = 200.0
DEFAULT_BAND_HIGH_HZ = 260.0
MIN_RUNS_PER_CLASS = 3
MIN_WINDOWS_PER_RUN = 20


@dataclass(frozen=True)
class FeatureRow:
    rms: float
    crest_factor: float
    band_energy: float
    dominant_frequency: float

    def values(self) -> tuple[float, float, float, float]:
        return (
            self.rms,
            self.crest_factor,
            self.band_energy,
            self.dominant_frequency,
        )


@dataclass(frozen=True)
class Run:
    label: str
    run_id: str
    path: Path
    features: tuple[FeatureRow, ...]


def parse_args() -> argparse.Namespace:
    project_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(
        description="Build a validated P3 model from normal/abnormal CSV runs"
    )
    parser.add_argument(
        "--input-dir",
        type=Path,
        default=project_root / "data_analysis" / "datasets" / "raw",
    )
    parser.add_argument(
        "--header",
        type=Path,
        default=project_root / "firmware" / "include" / "p3_model.generated.h",
    )
    parser.add_argument(
        "--metrics",
        type=Path,
        default=project_root / "data_analysis" / "results" / "p3_metrics.json",
    )
    parser.add_argument("--band-low-hz", type=float, default=DEFAULT_BAND_LOW_HZ)
    parser.add_argument("--band-high-hz", type=float, default=DEFAULT_BAND_HIGH_HZ)
    parser.add_argument(
        "--minimum-balanced-accuracy", type=float, default=0.80
    )
    args = parser.parse_args()
    if (
        not math.isfinite(args.band_low_hz)
        or not math.isfinite(args.band_high_hz)
        or args.band_low_hz < 0.0
        or args.band_high_hz <= args.band_low_hz
        or args.band_high_hz > DEFAULT_SAMPLE_RATE_HZ / 2.0
    ):
        parser.error("invalid band; it must fit inside 0..400 Hz")
    if not 0.0 <= args.minimum_balanced_accuracy <= 1.0:
        parser.error("--minimum-balanced-accuracy must be from 0 to 1")
    return args


def fft(values: list[float]) -> list[complex]:
    count = len(values)
    result = [complex(value, 0.0) for value in values]
    reversed_index = 0
    for index in range(1, count):
        bit = count >> 1
        while reversed_index & bit:
            reversed_index ^= bit
            bit >>= 1
        reversed_index ^= bit
        if index < reversed_index:
            result[index], result[reversed_index] = (
                result[reversed_index],
                result[index],
            )

    length = 2
    while length <= count:
        angle = -2.0 * math.pi / length
        step = complex(math.cos(angle), math.sin(angle))
        for start in range(0, count, length):
            phase = complex(1.0, 0.0)
            for offset in range(length // 2):
                even = result[start + offset]
                odd = result[start + offset + length // 2] * phase
                result[start + offset] = even + odd
                result[start + offset + length // 2] = even - odd
                phase *= step
        length <<= 1
    return result


def extract_features(
    samples: list[float], sample_rate_hz: float, band_low_hz: float, band_high_hz: float
) -> FeatureRow:
    if len(samples) != SAMPLE_COUNT:
        raise ValueError(f"expected {SAMPLE_COUNT} samples, got {len(samples)}")
    mean = statistics.fmean(samples)
    centered = [value - mean for value in samples]
    rms = math.sqrt(statistics.fmean(value * value for value in centered))
    peak = max(abs(value) for value in centered)
    crest_factor = peak / rms if rms > 0.0 else 0.0

    hann = [
        0.5 - 0.5 * math.cos(2.0 * math.pi * index / (SAMPLE_COUNT - 1))
        for index in range(SAMPLE_COUNT)
    ]
    spectrum = fft([value * window for value, window in zip(centered, hann)])
    denominator = SAMPLE_COUNT * sum(window * window for window in hann)
    strongest_power = -1.0
    dominant_frequency = 0.0
    band_energy = 0.0
    for bin_index in range(SAMPLE_COUNT // 2 + 1):
        power = abs(spectrum[bin_index]) ** 2 / denominator
        if 0 < bin_index < SAMPLE_COUNT // 2:
            power *= 2.0
        frequency = bin_index * sample_rate_hz / SAMPLE_COUNT
        if band_low_hz <= frequency <= band_high_hz:
            band_energy += power
        if bin_index > 0 and power > strongest_power:
            strongest_power = power
            dominant_frequency = frequency
    return FeatureRow(rms, crest_factor, band_energy, dominant_frequency)


def label_and_run_id(path: Path) -> tuple[str, str] | None:
    lower = path.stem.lower()
    if lower.startswith("normal_"):
        label = "normal"
        remainder = path.stem[len("normal_") :]
    elif lower.startswith("abnormal_"):
        label = "abnormal"
        remainder = path.stem[len("abnormal_") :]
    else:
        return None
    parts = remainder.rsplit("_", 1)
    return label, parts[0]


def serial_csv_lines(stream):
    """Yield one CSV header and data rows from a PlatformIO serial capture."""
    header_seen = False
    for line in stream:
        stripped = line.strip()
        is_header = (
            stripped.startswith("label,fault_type,severity,run_id,speed_pct,load,")
            or stripped.startswith("window_id,sample_index,timestamp_us,")
        )
        if not header_seen:
            if is_header:
                header_seen = True
                yield line
            continue
        if (not stripped or stripped.startswith("#") or is_header
                or stripped == "STOP" or stripped.startswith("START,")):
            continue
        yield line


def load_run(path: Path, band_low_hz: float, band_high_hz: float) -> Run | None:
    parsed = label_and_run_id(path)
    if parsed is None:
        return None
    label, run_id = parsed
    windows: dict[int, list[dict[str, str]]] = {}
    rejected_windows: set[int] = set()
    with path.open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(serial_csv_lines(stream))
        required = {
            "window_id",
            "sample_index",
            "az_g",
            "actual_hz",
            "timer_overruns",
            "buffer_overruns",
            "sensor_read_errors",
            "dropped_samples",
        }
        if reader.fieldnames is None or not required.issubset(reader.fieldnames):
            raise ValueError(f"{path.name}: missing required columns")
        metadata_columns = {
            "label", "fault_type", "severity", "run_id", "speed_pct", "load"
        }
        present_metadata = metadata_columns.intersection(reader.fieldnames)
        if present_metadata and present_metadata != metadata_columns:
            missing = sorted(metadata_columns - present_metadata)
            raise ValueError(f"{path.name}: incomplete metadata; missing {missing}")
        expected_metadata: tuple[str, str, str, str, str, str] | None = None
        for row in reader:
            if present_metadata:
                metadata = tuple(row[column].strip() for column in (
                    "label", "fault_type", "severity", "run_id", "speed_pct", "load"
                ))
                if expected_metadata is None:
                    expected_metadata = metadata
                elif metadata != expected_metadata:
                    raise ValueError(f"{path.name}: metadata changes within one run")
                csv_label, fault_type, severity_text, csv_run_id, speed_text, _ = metadata
                try:
                    severity = int(severity_text)
                    speed_percent = int(speed_text)
                except ValueError as error:
                    raise ValueError(f"{path.name}: invalid severity/speed metadata") from error
                if csv_label.lower() != label or csv_run_id != run_id:
                    raise ValueError(
                        f"{path.name}: label/run_id metadata does not match filename"
                    )
                if not 1 <= speed_percent <= 100 or not 0 <= severity <= 3:
                    raise ValueError(f"{path.name}: invalid severity/speed range")
                if ((label == "normal" and (fault_type != "healthy" or severity != 0))
                        or (label == "abnormal" and
                            (fault_type == "healthy" or severity == 0))):
                    raise ValueError(f"{path.name}: inconsistent label/fault/severity")
            try:
                window_id = int(row["window_id"])
            except (TypeError, ValueError) as error:
                raise ValueError(f"{path.name}: row has no usable window_id") from error
            if any(row.get(column) in (None, "") for column in required):
                rejected_windows.add(window_id)
                continue
            try:
                int(row["sample_index"])
                float(row["az_g"])
                float(row["actual_hz"])
                for column in (
                    "timer_overruns", "buffer_overruns",
                    "sensor_read_errors", "dropped_samples",
                ):
                    int(row[column])
            except (TypeError, ValueError):
                rejected_windows.add(window_id)
                continue
            windows.setdefault(window_id, []).append(row)

    features: list[FeatureRow] = []
    for window_id, rows in sorted(windows.items()):
        if window_id in rejected_windows:
            continue
        rows.sort(key=lambda row: int(row["sample_index"]))
        indexes = [int(row["sample_index"]) for row in rows]
        if indexes != list(range(SAMPLE_COUNT)):
            rejected_windows.add(window_id)
            continue
        error_total = sum(
            int(row[column])
            for row in rows
            for column in (
                "timer_overruns",
                "buffer_overruns",
                "sensor_read_errors",
                "dropped_samples",
            )
        )
        if error_total != 0:
            rejected_windows.add(window_id)
            continue
        sample_rate_hz = statistics.median(float(row["actual_hz"]) for row in rows)
        if abs(sample_rate_hz - DEFAULT_SAMPLE_RATE_HZ) > 8.0:
            rejected_windows.add(window_id)
            continue
        samples = [float(row["az_g"]) for row in rows]
        features.append(
            extract_features(samples, sample_rate_hz, band_low_hz, band_high_hz)
        )
    if rejected_windows:
        print(
            f"Skipped {len(rejected_windows)} invalid window(s) from {path.name}: "
            + ", ".join(str(value) for value in sorted(rejected_windows))
        )
    if len(features) < MIN_WINDOWS_PER_RUN:
        raise ValueError(
            f"{path.name}: only {len(features)} valid windows; "
            f"need at least {MIN_WINDOWS_PER_RUN}"
        )
    return Run(label, run_id, path, tuple(features))


def percentile(values: list[float], probability: float) -> float:
    ordered = sorted(values)
    position = (len(ordered) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    fraction = position - lower
    return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction


def cpp_float(value: float) -> str:
    return f"{value:.9g}f"


def classification_metrics(
    normal_values: list[float], abnormal_values: list[float], threshold: float
) -> dict[str, object]:
    true_negative = sum(value < threshold for value in normal_values)
    false_positive = len(normal_values) - true_negative
    true_positive = sum(value >= threshold for value in abnormal_values)
    false_negative = len(abnormal_values) - true_positive
    specificity = true_negative / len(normal_values)
    sensitivity = true_positive / len(abnormal_values)
    return {
        "threshold": threshold,
        "confusion_matrix": {
            "true_negative": true_negative,
            "false_positive": false_positive,
            "false_negative": false_negative,
            "true_positive": true_positive,
        },
        "specificity": specificity,
        "sensitivity": sensitivity,
        "balanced_accuracy": (specificity + sensitivity) / 2.0,
    }


def build_header(
    model_id: str,
    rms_threshold: float,
    means: list[float],
    scales: list[float],
    enter_threshold: float,
    clear_threshold: float,
) -> str:
    means_text = ", ".join(cpp_float(value) for value in means)
    scales_text = ", ".join(cpp_float(value) for value in scales)
    return f"""#pragma once

// Generated by data_analysis/scripts/build_p3_model.py from validated real runs.
// Do not hand-edit thresholds; regenerate this file after changing the rig.
#include \"features.h\"

namespace p3_model {{

inline ClassifierConfig classifierConfig() {{
    return ClassifierConfig{{
        {cpp_float(rms_threshold)},
        {{{means_text}}},
        {{{scales_text}}},
        {{1.0f, 1.0f, 1.0f, 1.0f}},
        {cpp_float(enter_threshold)},
        {cpp_float(clear_threshold)},
    }};
}}

inline const char* modelId() {{ return \"{model_id}\"; }}

}}  // namespace p3_model
"""


def main() -> int:
    args = parse_args()
    try:
        paths = sorted(args.input_dir.glob("*.csv"))
        runs = [
            run
            for path in paths
            if (run := load_run(path, args.band_low_hz, args.band_high_hz))
            is not None
        ]
    except (OSError, ValueError, csv.Error) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1

    normal_runs = [run for run in runs if run.label == "normal"]
    abnormal_runs = [run for run in runs if run.label == "abnormal"]
    print(
        f"Found {len(normal_runs)} normal runs and "
        f"{len(abnormal_runs)} abnormal runs"
    )
    if (
        len(normal_runs) < MIN_RUNS_PER_CLASS
        or len(abnormal_runs) < MIN_RUNS_PER_CLASS
    ):
        print(
            "ERROR: model not generated; collect at least 3 independent "
            "normal runs and 3 independent abnormal runs",
            file=sys.stderr,
        )
        return 2

    # Hold out one complete normal run. No abnormal data is used to normalize
    # features or choose thresholds, so all abnormal runs remain independent
    # evaluation data.
    normal_runs.sort(key=lambda run: (run.run_id, run.path.name))
    held_out_normal = normal_runs[-1]
    training_rows = [row for run in normal_runs[:-1] for row in run.features]
    evaluation_normal = list(held_out_normal.features)
    evaluation_abnormal = [row for run in abnormal_runs for row in run.features]

    columns = list(zip(*(row.values() for row in training_rows)))
    means = [statistics.fmean(column) for column in columns]
    scales = [statistics.pstdev(column) for column in columns]
    if any(not math.isfinite(scale) or scale <= 1e-12 for scale in scales):
        print("ERROR: a training feature has zero/invalid scale", file=sys.stderr)
        return 3

    def score(row: FeatureRow) -> float:
        return statistics.fmean(
            abs((value - mean) / scale)
            for value, mean, scale in zip(row.values(), means, scales)
        )

    training_scores = [score(row) for row in training_rows]
    enter_threshold = percentile(training_scores, 0.99)
    clear_threshold = percentile(training_scores, 0.95)
    if enter_threshold <= 0.0:
        print("ERROR: invalid learned score threshold", file=sys.stderr)
        return 3
    if clear_threshold >= enter_threshold:
        clear_threshold = enter_threshold * 0.8
    rms_threshold = percentile([row.rms for row in training_rows], 0.99)

    normal_scores = [score(row) for row in evaluation_normal]
    abnormal_scores = [score(row) for row in evaluation_abnormal]
    multi_feature_metrics = classification_metrics(
        normal_scores, abnormal_scores, enter_threshold
    )
    rms_metrics = classification_metrics(
        [row.rms for row in evaluation_normal],
        [row.rms for row in evaluation_abnormal],
        rms_threshold,
    )
    balanced_accuracy = float(multi_feature_metrics["balanced_accuracy"])

    digest = hashlib.sha256()
    for run in runs:
        digest.update(run.path.read_bytes())
    digest.update(f"{args.band_low_hz}:{args.band_high_hz}".encode())
    model_id = digest.hexdigest()[:12]
    metrics = {
        "model_id": model_id,
        "feature_order": FEATURE_NAMES,
        "band_hz": [args.band_low_hz, args.band_high_hz],
        "normal_training_runs": [run.path.name for run in normal_runs[:-1]],
        "normal_test_runs": [held_out_normal.path.name],
        "abnormal_test_runs": [run.path.name for run in abnormal_runs],
        "thresholds": {
            "score_enter": enter_threshold,
            "score_clear": clear_threshold,
            "rms_fallback": rms_threshold,
        },
        # Keep the original top-level fields for existing result consumers.
        "confusion_matrix": multi_feature_metrics["confusion_matrix"],
        "specificity": multi_feature_metrics["specificity"],
        "sensitivity": multi_feature_metrics["sensitivity"],
        "balanced_accuracy": balanced_accuracy,
        "comparison": {
            "multi_feature_anomaly_score": multi_feature_metrics,
            "rms_baseline": rms_metrics,
            "balanced_accuracy_delta": (
                balanced_accuracy - float(rms_metrics["balanced_accuracy"])
            ),
        },
    }
    print(json.dumps(metrics, indent=2))
    if balanced_accuracy < args.minimum_balanced_accuracy:
        print(
            "ERROR: model not generated; held-out balanced accuracy is below "
            f"{args.minimum_balanced_accuracy:.3f}",
            file=sys.stderr,
        )
        return 4

    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.metrics.parent.mkdir(parents=True, exist_ok=True)
    args.header.write_text(
        build_header(
            model_id,
            rms_threshold,
            means,
            scales,
            enter_threshold,
            clear_threshold,
        ),
        encoding="utf-8",
    )
    args.metrics.write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
    print(f"Generated: {args.header}")
    print(f"Metrics: {args.metrics}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

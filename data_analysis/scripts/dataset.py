"""Import P2 diagnostic CSV files and split P3 data by independent run_id."""

import csv
import math
import random
from collections import defaultdict
from pathlib import Path

import numpy as np

from features import FEATURE_NAMES, extract_features

MANIFEST_FIELDS = ("path", "run_id", "condition", "speed_configuration",
                   "label", "source")
P2_FIELDS = ("window_id", "sample_index", "timestamp_us", "ax_g", "ay_g", "az_g",
             "target_hz", "actual_hz", "mean_period_us", "jitter_rms_us",
             "max_abs_jitter_us", "timer_overruns", "buffer_overruns",
             "sensor_read_errors", "dropped_samples")
INTEGER_FIELDS = ("window_id", "sample_index", "timestamp_us", "timer_overruns",
                  "buffer_overruns", "sensor_read_errors", "dropped_samples")
COUNTERS = ("timer_overruns", "buffer_overruns", "sensor_read_errors", "dropped_samples")
OUTPUT_FIELDS = ("run_id", "window_id", "timestamp", "condition",
                 "speed_configuration", "sample_rate", "sample_count", "axis",
                 "unit", "source", *FEATURE_NAMES, "label")


def write_csv(path, records, fields):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(records)


def read_manifest(path):
    with Path(path).open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames or not set(MANIFEST_FIELDS).issubset(reader.fieldnames):
            raise ValueError("Manifest thiếu cột; xem p2_manifest.template.csv.")
        records = list(reader)
    if not records:
        raise ValueError("Manifest chưa có run.")
    return records


def read_p2_windows(path):
    windows = {}
    with Path(path).open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(line for line in stream if not line.startswith("#"))
        if not reader.fieldnames or not set(P2_FIELDS).issubset(reader.fieldnames):
            raise ValueError(f"{Path(path).name}: thiếu cột CSV chẩn đoán P2.")
        for row in reader:
            try:
                values = {name: int(row[name]) if name in INTEGER_FIELDS else float(row[name])
                          for name in P2_FIELDS}
            except (TypeError, ValueError) as error:
                raise ValueError(f"{Path(path).name}, dòng {reader.line_num}: số không hợp lệ.") from error
            if not all(math.isfinite(value) for value in values.values()):
                raise ValueError(f"{Path(path).name}: có NaN/Infinity.")
            windows.setdefault(values["window_id"], []).append(values)
    return windows


def rejection_reason(rows, config):
    if len(rows) != config.window_size:
        return "wrong_window_size"
    if [row["sample_index"] for row in rows] != list(range(config.window_size)):
        return "missing_duplicate_or_unordered_sample"
    first = rows[0]
    repeated = ("target_hz", "actual_hz", "mean_period_us", "jitter_rms_us",
                "max_abs_jitter_us", *COUNTERS)
    if any(row[name] != first[name] for row in rows for name in repeated):
        return "inconsistent_window_metrics"
    if any(first[name] != 0 for name in COUNTERS):
        return "sampling_error_or_dropped_sample"
    if abs(first["target_hz"] - config.sample_rate_hz) > 0.001:
        return "wrong_target_rate"
    actual_rate = first["actual_hz"]
    if actual_rate <= 0 or abs(actual_rate - config.sample_rate_hz) > config.sample_rate_hz * 0.05:
        return "actual_rate_outside_model_domain"
    timestamps = np.asarray([row["timestamp_us"] for row in rows], dtype=float)
    periods = np.diff(timestamps)
    if np.any(periods <= 0):
        return "timestamps_not_increasing"
    mean_period = float(np.mean(periods))
    if not math.isclose(actual_rate, 1e6 / mean_period, rel_tol=0.002):
        return "actual_rate_disagrees_with_timestamps"
    observed_max_jitter = float(np.max(np.abs(periods - mean_period)))
    if max(observed_max_jitter, first["max_abs_jitter_us"]) > mean_period * 0.05:
        return "jitter_above_five_percent"
    return None


def build_feature_dataset(manifest_path, config):
    config.validate()
    if not config.remove_mean:
        raise ValueError("Luồng P2 dùng tín hiệu đã bỏ DC; remove_mean phải là true.")
    manifest_path = Path(manifest_path).resolve()
    records, rejected, seen_runs, seen_paths = [], [], set(), set()
    context = None
    for run in read_manifest(manifest_path):
        if any(not run.get(field, "").strip() for field in MANIFEST_FIELDS):
            raise ValueError("Manifest có ô trống.")
        if run["label"] not in ("0", "1") or run["source"] not in ("measured", "simulated"):
            raise ValueError("label phải là 0/1; source là measured/simulated.")
        run_id = run["run_id"]
        path = (manifest_path.parent / run["path"]).resolve()
        if run_id in seen_runs or path in seen_paths:
            raise ValueError("Không được dùng lại run_id hoặc file đo.")
        seen_runs.add(run_id)
        seen_paths.add(path)
        current_context = {"axis": "z", "unit": "g", "source": run["source"],
                           "speed_configuration": run["speed_configuration"]}
        if context is not None and context != current_context:
            raise ValueError("Một model chỉ dùng một nguồn và một cấu hình tốc độ/tải.")
        context = current_context
        valid_windows = 0
        for window_id, rows in sorted(read_p2_windows(path).items()):
            reason = rejection_reason(rows, config)
            if reason:
                rejected.append({"run_id": run_id, "window_id": window_id, "reason": reason})
                continue
            actual_rate = rows[0]["actual_hz"]
            features = extract_features([row["az_g"] for row in rows],
                                        config.with_sample_rate(actual_rate))
            records.append({
                "run_id": run_id, "window_id": window_id,
                "timestamp": rows[0]["timestamp_us"] / 1e6,
                "condition": run["condition"],
                "speed_configuration": run["speed_configuration"],
                "sample_rate": actual_rate, "sample_count": config.window_size,
                "axis": "z", "unit": "g", "source": run["source"],
                **features, "label": int(run["label"]),
            })
            valid_windows += 1
        if valid_windows == 0:
            raise ValueError(f"Run {run_id} không có cửa sổ hợp lệ.")
    return records, {"context": context, "feature_config": config.to_dict(),
                     "rejected_windows": rejected}


def split_by_run(records, test_fraction=0.33, seed=42):
    if not 0 < test_fraction < 1:
        raise ValueError("test_fraction phải nằm giữa 0 và 1.")
    labels_by_run = {}
    conditions_by_run = {}
    for record in records:
        run_id, label = record["run_id"], record["label"]
        if run_id in labels_by_run and labels_by_run[run_id] != label:
            raise ValueError("Một run_id không được có nhiều nhãn.")
        if run_id in conditions_by_run and conditions_by_run[run_id] != record["condition"]:
            raise ValueError("Một run_id không được thay đổi condition.")
        labels_by_run[run_id] = label
        conditions_by_run[run_id] = record["condition"]
    randomizer = random.Random(seed)
    test_runs = set()
    groups = defaultdict(list)
    for run_id, label in labels_by_run.items():
        groups[(label, conditions_by_run[run_id])].append(run_id)
    if set(labels_by_run.values()) != {0, 1}:
        raise ValueError("Dataset phải có cả normal (0) và abnormal (1).")
    for (label, condition), group in sorted(groups.items()):
        run_ids = sorted(group)
        if len(run_ids) < 2:
            raise ValueError(
                f"Cần ít nhất 2 run độc lập cho label={label}, condition={condition}."
            )
        randomizer.shuffle(run_ids)
        count = min(len(run_ids) - 1, max(1, round(len(run_ids) * test_fraction)))
        test_runs.update(run_ids[:count])
    train = [record for record in records if record["run_id"] not in test_runs]
    test = [record for record in records if record["run_id"] in test_runs]
    manifest = {"seed": seed, "test_fraction": test_fraction,
                "stratified_by": ["label", "condition"],
                "train_run_ids": sorted(set(labels_by_run) - test_runs),
                "test_run_ids": sorted(test_runs)}
    return train, test, manifest

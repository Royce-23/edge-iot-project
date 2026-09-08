#!/usr/bin/env python3
"""Capture validated P2 diagnostic rows and summarize sampling/calibration."""

from __future__ import annotations

import argparse
import csv
import math
import re
import statistics
import sys
import time
from datetime import datetime
from pathlib import Path

import serial


HEADER = [
    "window_id",
    "sample_index",
    "timestamp_us",
    "ax_g",
    "ay_g",
    "az_g",
    "target_hz",
    "actual_hz",
    "mean_period_us",
    "jitter_rms_us",
    "max_abs_jitter_us",
    "timer_overruns",
    "buffer_overruns",
    "sensor_read_errors",
    "dropped_samples",
]


def safe_name(value: str) -> str:
    normalized = re.sub(r"[^A-Za-z0-9_-]+", "_", value.strip())
    if not normalized:
        raise argparse.ArgumentTypeError("name must contain a letter or number")
    return normalized


def parse_args() -> argparse.Namespace:
    project_root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(
        description="Capture clean CSV rows from p2-sampling-diagnostic firmware"
    )
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--condition", type=safe_name, required=True)
    parser.add_argument("--run-id", type=safe_name, required=True)
    parser.add_argument("--samples", type=int, default=1024)
    parser.add_argument("--timeout", type=float, default=45.0)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=project_root / "data_analysis" / "datasets" / "raw",
    )
    args = parser.parse_args()
    if args.samples < 2:
        parser.error("--samples must be at least 2")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    return args


def validated_row(line: str) -> list[str] | None:
    if not line or line.startswith("#") or line.startswith("window_id,"):
        return None
    fields = next(csv.reader([line]))
    if len(fields) != len(HEADER):
        return None
    try:
        int(fields[0])
        int(fields[1])
        int(fields[2])
        for index in range(3, 11):
            float(fields[index])
        for index in range(11, 15):
            int(fields[index])
    except ValueError:
        return None
    return fields


def capture(args: argparse.Namespace) -> list[list[str]]:
    rows: list[list[str]] = []
    deadline = time.monotonic() + args.timeout
    with serial.Serial(args.port, args.baud, timeout=1.0) as device:
        # Release the common ESP32 auto-reset/boot control lines. Keeping RTS or
        # DTR asserted can hold some USB-UART boards in reset.
        device.dtr = False
        device.rts = False
        time.sleep(0.1)
        device.reset_input_buffer()
        while len(rows) < args.samples and time.monotonic() < deadline:
            raw = device.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if line.startswith("#"):
                print(f"Device: {line}", file=sys.stderr, flush=True)
            row = validated_row(line)
            if row is not None:
                rows.append(row)
                if len(rows) % 256 == 0:
                    print(f"Captured {len(rows)}/{args.samples} samples...", flush=True)
    if len(rows) < args.samples:
        raise RuntimeError(
            f"only received {len(rows)}/{args.samples} valid rows before timeout"
        )
    return rows


def summarize(rows: list[list[str]]) -> dict[str, float | int | list[float]]:
    axes = [[float(row[index]) for row in rows] for index in (3, 4, 5)]
    means = [statistics.fmean(axis) for axis in axes]
    standard_deviations = [statistics.pstdev(axis) for axis in axes]
    mean_vector_magnitude = math.sqrt(sum(value * value for value in means))
    gravity_direction = (
        [value / mean_vector_magnitude for value in means]
        if mean_vector_magnitude > 1e-9
        else [0.0, 0.0, 0.0]
    )

    windows: dict[int, list[str]] = {}
    for row in rows:
        windows.setdefault(int(row[0]), row)
    window_rows = list(windows.values())
    actual_rates = [float(row[7]) for row in window_rows]
    jitter_values = [float(row[9]) for row in window_rows]
    dropped = sum(int(row[14]) for row in window_rows)

    return {
        "sample_count": len(rows),
        "window_count": len(window_rows),
        "axis_means_g": means,
        "axis_stddev_g": standard_deviations,
        "mean_vector_magnitude_g": mean_vector_magnitude,
        "median_actual_hz": statistics.median(actual_rates),
        "max_jitter_rms_us": max(jitter_values),
        "total_dropped_samples": dropped,
        "gravity_direction": gravity_direction,
    }


def write_csv(args: argparse.Namespace, rows: list[list[str]]) -> Path:
    args.output_dir.mkdir(parents=True, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    output = args.output_dir / f"{args.condition}_{args.run_id}_{timestamp}.csv"
    with output.open("x", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(HEADER)
        writer.writerows(rows)
    return output


def main() -> int:
    args = parse_args()
    try:
        rows = capture(args)
        output = write_csv(args, rows)
        summary = summarize(rows)
    except (OSError, RuntimeError, serial.SerialException) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1

    means = summary["axis_means_g"]
    deviations = summary["axis_stddev_g"]
    gravity_direction = summary["gravity_direction"]
    assert isinstance(means, list)
    assert isinstance(deviations, list)
    assert isinstance(gravity_direction, list)

    print(f"Saved: {output}")
    print(
        "Mean XYZ (g): "
        f"{means[0]:.6f}, {means[1]:.6f}, {means[2]:.6f}"
    )
    print(
        "Stddev XYZ (g): "
        f"{deviations[0]:.6f}, {deviations[1]:.6f}, {deviations[2]:.6f}"
    )
    print(f"Mean vector magnitude (g): {summary['mean_vector_magnitude_g']:.6f}")
    print(f"Median actual rate (Hz): {summary['median_actual_hz']:.3f}")
    print(f"Maximum RMS jitter (us): {summary['max_jitter_rms_us']:.3f}")
    print(f"Dropped samples: {summary['total_dropped_samples']}")
    print(
        "Gravity direction XYZ: "
        f"{gravity_direction[0]:.6f}, {gravity_direction[1]:.6f}, "
        f"{gravity_direction[2]:.6f}"
    )
    print(
        "Calibration note: one static position cannot separate sensor bias from "
        "gravity; do not copy axis means into the firmware bias flags."
    )

    if max(deviations) > 0.05:
        print("WARNING: sensor was not stationary enough for calibration")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

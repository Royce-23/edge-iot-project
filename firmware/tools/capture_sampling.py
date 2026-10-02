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

try:
    import serial
    from serial import SerialException
except ModuleNotFoundError:
    serial = None

    class SerialException(Exception):
        """Fallback used only to keep non-serial helpers importable."""


HEADER = [
    "label",
    "fault_type",
    "severity",
    "run_id",
    "speed_pct",
    "load",
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
    parser.add_argument(
        "--label", choices=("off", "normal", "abnormal"), required=True
    )
    parser.add_argument("--fault-type", type=safe_name, required=True)
    parser.add_argument("--severity", type=int, required=True)
    parser.add_argument("--run-id", type=safe_name, required=True)
    parser.add_argument("--speed-pct", type=int, required=True)
    parser.add_argument("--load", type=safe_name, default="no_load")
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
    if args.samples % 512 != 0:
        parser.error("--samples must be a multiple of the 512-sample window")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if not 0 <= args.speed_pct <= 100:
        parser.error("--speed-pct must be between 0 and 100")
    if not 0 <= args.severity <= 3:
        parser.error("--severity must be between 0 and 3")
    if args.label == "off" and (
        args.fault_type.lower() != "stopped"
        or args.severity != 0
        or args.speed_pct != 0
    ):
        parser.error("off data requires --fault-type stopped --severity 0 --speed-pct 0")
    if args.label == "normal" and (
        args.fault_type.lower() != "healthy"
        or args.severity != 0
        or args.speed_pct == 0
    ):
        parser.error(
            "normal data requires --fault-type healthy --severity 0 and speed 1-100"
        )
    if args.label == "abnormal" and (
        args.fault_type.lower() in ("healthy", "stopped")
        or args.severity == 0
        or args.speed_pct == 0
    ):
        parser.error(
            "abnormal data requires a fault, severity 1-3 and speed 1-100"
        )
    # The diagnostic firmware normalizes this field before echoing it in CSV.
    args.fault_type = args.fault_type.lower()
    return args


def validated_row(line: str) -> list[str] | None:
    if not line or line.startswith("#") or line.startswith("label,"):
        return None
    fields = next(csv.reader([line]))
    if len(fields) != len(HEADER):
        return None
    try:
        int(fields[2])
        int(fields[4])
        int(fields[6])
        int(fields[7])
        int(fields[8])
        for index in range(9, 17):
            float(fields[index])
        for index in range(17, 21):
            int(fields[index])
    except ValueError:
        return None
    return fields


def capture(args: argparse.Namespace) -> list[list[str]]:
    if serial is None:
        raise RuntimeError(
            "pyserial is required for capture; install it with "
            "'python -m pip install pyserial'"
        )
    target_windows = args.samples // 512
    windows: dict[int, dict[int, list[str]]] = {}
    window_order: list[int] = []
    reported_complete = 0
    deadline = time.monotonic() + args.timeout
    with serial.Serial(args.port, args.baud, timeout=1.0) as device:
        # Release the common ESP32 auto-reset/boot control lines. Keeping RTS or
        # DTR asserted can hold some USB-UART boards in reset.
        device.dtr = False
        device.rts = False
        time.sleep(0.1)
        device.reset_input_buffer()
        start_command = (
            f"START,{args.label},{args.fault_type},{args.severity},"
            f"{args.run_id},{args.speed_pct},{args.load}\n"
        )
        device.write(start_command.encode("ascii"))
        device.flush()
        try:
            while time.monotonic() < deadline:
                raw = device.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").strip()
                if line.startswith("#"):
                    print(f"Device: {line}", file=sys.stderr, flush=True)
                row = validated_row(line)
                if row is not None:
                    expected_metadata = [
                        args.label,
                        args.fault_type,
                        str(args.severity),
                        args.run_id,
                        str(args.speed_pct),
                        args.load,
                    ]
                    if row[:6] != expected_metadata:
                        continue
                    window_id = int(row[6])
                    sample_index = int(row[7])
                    if not 0 <= sample_index < 512:
                        continue
                    if window_id not in windows:
                        windows[window_id] = {}
                        window_order.append(window_id)
                    windows[window_id][sample_index] = row

                    complete = sum(
                        len(windows[seen_window_id]) == 512
                        for seen_window_id in window_order
                    )
                    if complete > reported_complete:
                        reported_complete = complete
                        print(
                            f"Captured {complete}/{target_windows} complete "
                            "windows...",
                            flush=True,
                        )
                    if complete >= target_windows:
                        break
        finally:
            device.write(b"STOP\n")
            device.flush()
    complete_window_ids = [
        window_id
        for window_id in window_order
        if len(windows[window_id]) == 512
    ]
    if len(complete_window_ids) < target_windows:
        valid_rows = sum(len(window) for window in windows.values())
        raise RuntimeError(
            f"only received {len(complete_window_ids)}/{target_windows} complete "
            f"windows ({valid_rows} valid rows) before timeout"
        )
    selected_window_ids = complete_window_ids[:target_windows]
    return [
        windows[window_id][sample_index]
        for window_id in selected_window_ids
        for sample_index in range(512)
    ]


def summarize(rows: list[list[str]]) -> dict[str, float | int | list[float]]:
    axes = [[float(row[index]) for row in rows] for index in (9, 10, 11)]
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
        windows.setdefault(int(row[6]), row)
    window_rows = list(windows.values())
    actual_rates = [float(row[13]) for row in window_rows]
    jitter_values = [float(row[15]) for row in window_rows]
    dropped = sum(int(row[20]) for row in window_rows)

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
    output = args.output_dir / f"{args.label}_{args.run_id}_{timestamp}.csv"
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
    except (OSError, RuntimeError, SerialException) as error:
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

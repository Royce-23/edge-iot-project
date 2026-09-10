"""P3 result plots; figures state whether data is simulated or measured."""

import os
from pathlib import Path
import tempfile

os.environ.setdefault("MPLCONFIGDIR", str(Path(tempfile.gettempdir()) / "p3-matplotlib"))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from features import FEATURE_NAMES


def create_plots(train, metrics, output_dir, source):
    output_dir = Path(output_dir)
    heading = "SIMULATED FAN DATA — SOFTWARE TEST" if source == "simulated" else "MEASURED FAN DATA"
    units = {"rms": "g", "peak_to_peak": "g", "crest_factor": "ratio",
             "dominant_frequency": "Hz", "band_energy": "g²"}
    figure, axes = plt.subplots(2, 3, figsize=(13, 7))
    for axis, feature in zip(axes.flat, FEATURE_NAMES):
        values = [record[feature] for record in train]
        bins = np.histogram_bin_edges(values, bins=15)
        for label, name in ((0, "Normal"), (1, "Abnormal")):
            axis.hist([record[feature] for record in train if record["label"] == label],
                      bins=bins, alpha=0.55, label=name)
        axis.set(title=feature, xlabel=units[feature], ylabel="Window count")
        axis.legend()
    axes.flat[-1].axis("off")
    figure.suptitle(heading + "\nFeature distributions — train runs")
    figure.tight_layout()
    figure.savefig(output_dir / "feature_distributions.png", dpi=150)
    plt.close(figure)

    figure, axes = plt.subplots(1, 2, figsize=(9, 4))
    for axis, (method, result) in zip(axes, metrics.items()):
        matrix = np.asarray(result["confusion_matrix"])
        axis.imshow(matrix, cmap="Blues", vmin=0, vmax=max(1, int(matrix.max())))
        for row in range(2):
            for column in range(2):
                axis.text(column, row, str(matrix[row, column]), ha="center", va="center")
        axis.set(xticks=[0, 1], yticks=[0, 1], xticklabels=["Normal", "Abnormal"],
                 yticklabels=["Normal", "Abnormal"], xlabel="Predicted",
                 ylabel="Actual", title=method)
    figure.suptitle(heading + "\nTest runs only")
    figure.tight_layout()
    figure.savefig(output_dir / "confusion_matrix.png", dpi=150)
    plt.close(figure)

    names = ("accuracy", "precision", "recall", "false_alarm_rate", "missed_detection_rate")
    figure, axis = plt.subplots(figsize=(11, 5))
    positions = np.arange(len(names))
    for index, (method, result) in enumerate(metrics.items()):
        values = [np.nan if result[name] is None else result[name] for name in names]
        axis.bar(positions + (index - 0.5) * 0.35, values, 0.35, label=method)
    axis.set(xticks=positions, xticklabels=names, ylim=(0, 1.08), ylabel="Rate",
             title=heading + "\nTest comparison; last two metrics are lower-is-better")
    axis.legend()
    figure.tight_layout()
    figure.savefig(output_dir / "method_comparison.png", dpi=150)
    plt.close(figure)

"""P3 fan-vibration pipeline: P2 CSV -> features -> model -> metrics -> report."""

import argparse
from pathlib import Path
import tempfile

from classifier import fit_model, predict
from dataset import OUTPUT_FIELDS, build_feature_dataset, split_by_run, write_csv
from evaluation import evaluate
from export_model import export_header, save_json
from features import FeatureConfig
from generate_fan_demo import generate_demo
from plots import create_plots

DATA_ANALYSIS = Path(__file__).resolve().parents[1]


def run(manifest, output_dir, config, test_fraction=0.33, seed=42,
        quantile=0.99, weights=(1, 1, 1, 1)):
    records, metadata = build_feature_dataset(manifest, config)
    train, test, split = split_by_run(records, test_fraction, seed)
    model = fit_model(train, config, metadata["context"], quantile, weights)
    baseline, advanced, scores = predict(test, model)
    labels = [record["label"] for record in test]
    metrics = {"rms_baseline": evaluate(labels, baseline),
               "multi_feature": evaluate(labels, advanced)}

    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    write_csv(output_dir / "features.csv", records, OUTPUT_FIELDS)
    write_csv(output_dir / "train.csv", train, OUTPUT_FIELDS)
    write_csv(output_dir / "test.csv", test, OUTPUT_FIELDS)
    predictions = [dict(record, rms_prediction=rms_prediction,
                        anomaly_score=score, multi_feature_prediction=multi_prediction)
                   for record, rms_prediction, score, multi_prediction
                   in zip(test, baseline, scores, advanced)]
    write_csv(output_dir / "test_predictions.csv", predictions,
              (*OUTPUT_FIELDS, "rms_prediction", "anomaly_score", "multi_feature_prediction"))
    save_json(output_dir / "dataset_metadata.json", metadata)
    save_json(output_dir / "split.json", split)
    save_json(output_dir / "model.json", model)
    save_json(output_dir / "metrics.json", metrics)
    export_header(model, output_dir / "p3_model_parameters.h")
    create_plots(train, metrics, output_dir, metadata["context"]["source"])

    conditions = sorted({record["condition"] for record in test})
    metric_names = ("accuracy", "precision", "recall",
                    "false_alarm_rate", "missed_detection_rate")
    lines = ["# P3 fan-vibration test result", "",
             f"Data source: **{metadata['context']['source']}**.", "",
             "Simulated data tests software flow only; it is not experimental accuracy."
             if metadata["context"]["source"] == "simulated"
             else "Results use the measured files declared in the manifest.",
             "", f"Train: {len(split['train_run_ids'])} runs / {len(train)} windows.",
             f"Test: {len(split['test_run_ids'])} runs / {len(test)} windows.",
             f"Test conditions: {', '.join(conditions)}.",
             f"Rejected windows: {len(metadata['rejected_windows'])}.", "",
             "| Method | Accuracy | Precision | Recall | False alarm | Missed detection |",
             "|---|---:|---:|---:|---:|---:|"]
    for method, result in metrics.items():
        values = ["N/A" if result[name] is None else f"{result[name]:.4f}"
                  for name in metric_names]
        lines.append("| " + " | ".join((method, *values)) + " |")
    lines.extend(["", "Positive label 1 means abnormal vibration. Metrics are per window.",
                  "Rows are actual and columns are predicted: [[TN, FP], [FN, TP]].",
                  "Thresholds and normalization use normal train windows only.",
                  "Do not tune using this final test split.", "",
                  "![Features](feature_distributions.png)",
                  "![Confusion matrix](confusion_matrix.png)",
                  "![Comparison](method_comparison.png)"])
    (output_dir / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return metrics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--demo", action="store_true", help="Generate simulated fan data")
    source.add_argument("--manifest", type=Path, help="P2 run manifest")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--test-fraction", type=float, default=0.33)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--quantile", type=float, default=0.99)
    parser.add_argument("--weights", type=float, nargs=4, default=(1, 1, 1, 1),
                        metavar=("RMS", "CREST", "ENERGY", "FREQUENCY"))
    args = parser.parse_args()
    config = FeatureConfig()
    output = args.output_dir or DATA_ANALYSIS / "results" / "demo"
    manifest = args.manifest
    try:
        if args.demo:
            with tempfile.TemporaryDirectory(prefix="p3-fan-demo-") as directory:
                manifest = generate_demo(Path(directory), config, args.seed)
                metrics = run(manifest, output, config, args.test_fraction,
                              args.seed, args.quantile, args.weights)
        else:
            metrics = run(manifest, output, config, args.test_fraction,
                          args.seed, args.quantile, args.weights)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Lỗi P3: {error}\n")
    print(f"Kết quả P3: {output.resolve()}")
    for method, result in metrics.items():
        print(f"{method}: accuracy={result['accuracy']:.4f}, "
              f"false_alarm={result['false_alarm_rate']:.4f}, "
              f"missed_detection={result['missed_detection_rate']:.4f}")


if __name__ == "__main__":
    main()

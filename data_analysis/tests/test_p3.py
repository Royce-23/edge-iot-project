import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "data_analysis" / "scripts"))
from classifier import fit_model, predict
from dataset import build_feature_dataset, split_by_run
from evaluation import evaluate
from features import FeatureConfig, extract_features
from generate_fan_demo import generate_demo
from run_pipeline import run


class P3FeatureTests(unittest.TestCase):
    def test_known_fan_tone(self):
        config = FeatureConfig()
        time = np.arange(config.window_size) / config.sample_rate_hz
        samples = 1.0 + 0.1 * np.sin(2 * np.pi * 50 * time)
        features = extract_features(samples, config)
        self.assertAlmostEqual(features["rms"], 0.1 / np.sqrt(2), places=10)
        self.assertAlmostEqual(features["peak_to_peak"], 0.2, places=10)
        self.assertAlmostEqual(features["crest_factor"], np.sqrt(2), places=10)
        self.assertEqual(features["dominant_frequency"], 50.0)
        self.assertAlmostEqual(features["band_energy"], 0.005, places=5)

    def test_invalid_windows(self):
        config = FeatureConfig()
        for samples in ([0] * 511, [float("nan")] * 512):
            with self.assertRaises(ValueError):
                extract_features(samples, config)


class P3DatasetAndModelTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.config = FeatureConfig()
        self.manifest = generate_demo(self.root / "raw", self.config, seed=7)

    def test_no_run_leakage(self):
        records, metadata = build_feature_dataset(self.manifest, self.config)
        self.assertEqual(len(records), 96)
        self.assertEqual(metadata["rejected_windows"], [])
        train, test, split = split_by_run(records)
        self.assertFalse(set(split["train_run_ids"]) & set(split["test_run_ids"]))
        self.assertEqual({record["label"] for record in train}, {0, 1})
        self.assertEqual({record["label"] for record in test}, {0, 1})

    def test_only_normal_train_fits_model(self):
        records, metadata = build_feature_dataset(self.manifest, self.config)
        train, test, _ = split_by_run(records)
        reference = fit_model(train, self.config, metadata["context"])
        changed = fit_model([*train, dict(train[0], run_id="extra_fault", label=1, rms=999)],
                            self.config, metadata["context"])
        self.assertEqual(reference, changed)
        baseline, advanced, scores = predict(test, reference)
        self.assertEqual(len(scores), len(test))
        self.assertEqual(evaluate([record["label"] for record in test], baseline)["window_count"], len(test))
        self.assertEqual(evaluate([record["label"] for record in test], advanced)["window_count"], len(test))

    def test_complete_pipeline_outputs(self):
        output = self.root / "results"
        metrics = run(self.manifest, output, self.config)
        self.assertEqual(set(metrics), {"rms_baseline", "multi_feature"})
        split = json.loads((output / "split.json").read_text())
        self.assertFalse(set(split["train_run_ids"]) & set(split["test_run_ids"]))
        self.assertIn("IS_MEASURED_DATA = false", (output / "p3_model_parameters.h").read_text())
        for name in ("report.md", "features.csv", "model.json", "metrics.json",
                     "feature_distributions.png", "confusion_matrix.png", "method_comparison.png"):
            self.assertGreater((output / name).stat().st_size, 0)


@unittest.skipUnless(shutil.which("g++"), "g++ is required for P3 host test")
class P3FirmwareTests(unittest.TestCase):
    def test_firmware_algorithms(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "p3_host"
            command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                       "-iquote", str(ROOT / "firmware/include"),
                       str(ROOT / "data_analysis/tests/p3_host.cpp"),
                       str(ROOT / "firmware/src/features.cpp"),
                       str(ROOT / "firmware/src/fft_processor.cpp"),
                       str(ROOT / "firmware/src/classifier.cpp"), "-o", str(binary)]
            subprocess.run(command, check=True, capture_output=True, text=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
            self.assertIn("PASS", result.stdout)

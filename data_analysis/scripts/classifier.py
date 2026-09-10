"""P3 RMS baseline and multi-feature anomaly score."""

import numpy as np

SCORE_FEATURES = ("rms", "crest_factor", "band_energy", "dominant_frequency")


def feature_matrix(records):
    values = np.asarray([[record[name] for name in SCORE_FEATURES]
                         for record in records], dtype=float)
    if values.ndim != 2 or values.shape[1] != 4 or not np.all(np.isfinite(values)):
        raise ValueError("Dataset đặc trưng không hợp lệ.")
    return values


def fit_model(train_records, config, context, quantile=0.99, weights=(1, 1, 1, 1)):
    if not 0 < quantile < 1:
        raise ValueError("quantile phải nằm giữa 0 và 1.")
    weights = np.asarray(weights, dtype=float)
    if weights.shape != (4,) or np.any(weights < 0) or not np.all(np.isfinite(weights)) or weights.sum() <= 0:
        raise ValueError("Cần 4 trọng số không âm và tổng lớn hơn 0.")
    weights /= weights.sum()
    normal = [record for record in train_records if record["label"] == 0]
    if len(normal) < 2:
        raise ValueError("Cần ít nhất 2 cửa sổ normal trong train.")
    values = feature_matrix(normal)
    means = np.mean(values, axis=0)
    floors = np.asarray([1e-6, 1e-6, 1e-12,
                         config.sample_rate_hz / config.window_size])
    scales = np.maximum(np.std(values, axis=0), floors)
    scores = np.abs((values - means) / scales) @ weights
    return {
        "version": 1,
        "purpose": "abnormal_fan_vibration",
        "source": context["source"],
        "feature_config": config.to_dict(),
        "context": context,
        "score_features": list(SCORE_FEATURES),
        "normal_training_run_ids": sorted({record["run_id"] for record in normal}),
        "normal_training_window_count": len(normal),
        "quantile": quantile,
        "rms_threshold": float(np.quantile(values[:, 0], quantile)),
        "means": means.tolist(), "scales": scales.tolist(),
        "weights": weights.tolist(),
        "score_threshold": float(np.quantile(scores, quantile)),
    }


def predict(records, model):
    values = feature_matrix(records)
    scores = np.abs((values - np.asarray(model["means"])) /
                    np.asarray(model["scales"])) @ np.asarray(model["weights"])
    baseline = (values[:, 0] > model["rms_threshold"]).astype(int)
    advanced = (scores > model["score_threshold"]).astype(int)
    return baseline.tolist(), advanced.tolist(), scores.tolist()

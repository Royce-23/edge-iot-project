"""Binary metrics where label 1 means abnormal fan vibration."""


def evaluate(labels, predictions):
    if not labels or len(labels) != len(predictions):
        raise ValueError("Nhãn và dự đoán phải cùng độ dài, không rỗng.")
    tn = fp = fn = tp = 0
    for actual, predicted in zip(labels, predictions):
        if actual not in (0, 1) or predicted not in (0, 1):
            raise ValueError("Nhãn và dự đoán chỉ nhận 0 hoặc 1.")
        if actual == 0 and predicted == 0:
            tn += 1
        elif actual == 0:
            fp += 1
        elif predicted == 0:
            fn += 1
        else:
            tp += 1

    def ratio(top, bottom):
        return top / bottom if bottom else None

    return {
        "positive_label": 1, "window_count": len(labels),
        "confusion_matrix": [[tn, fp], [fn, tp]],
        "tn": tn, "fp": fp, "fn": fn, "tp": tp,
        "accuracy": ratio(tp + tn, len(labels)),
        "precision": ratio(tp, tp + fp),
        "recall": ratio(tp, tp + fn),
        "false_alarm_rate": ratio(fp, fp + tn),
        "missed_detection_rate": ratio(fn, fn + tp),
    }

# P3 — phát hiện rung bất thường của quạt

Phạm vi P3 là nhận cửa sổ gia tốc từ P2, tính đặc trưng, phát hiện rung bất
thường và đánh giá thuật toán. P3 không đọc cảm biến, gửi MQTT, ghi backend hay
vẽ dashboard. Kết quả được bàn giao để P1/P4 nối vào luồng chung.

## Chạy thử không cần quạt thật

Tại thư mục gốc repo:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r data_analysis/requirements.txt
.venv/bin/python data_analysis/scripts/run_pipeline.py --demo
```

Windows PowerShell dùng `.venv\Scripts\python.exe` thay `.venv/bin/python`.
Mở `data_analysis/results/demo/report.md` sau khi chạy.

Demo tạo tín hiệu quạt mô phỏng gồm normal, imbalance, loose_mount và
bearing_like. Nó chỉ kiểm tra code; không dùng các chỉ số demo làm kết quả
thực nghiệm hoặc dùng ngưỡng demo trên ESP32 thật.

## Code được chia theo nhiệm vụ

| File | Nhiệm vụ |
|---|---|
| scripts/dataset.py | Đọc manifest và CSV chẩn đoán P2, kiểm tra cửa sổ, chia theo run_id |
| scripts/features.py | RMS, peak-to-peak, crest factor |
| scripts/fft_processor.py | Hann, FFT, dominant frequency, band energy |
| scripts/classifier.py | Baseline RMS và anomaly score nhiều đặc trưng |
| scripts/evaluation.py | Confusion matrix và các chỉ số |
| scripts/plots.py | Biểu đồ phân bố và so sánh |
| scripts/generate_fan_demo.py | Dữ liệu quạt mô phỏng để test phần mềm |
| scripts/export_model.py | Xuất tham số sang header C++ |
| scripts/run_pipeline.py | Lệnh chạy toàn bộ quy trình |

## Dùng dữ liệu quạt thật từ P2

P2 đã có `firmware/tools/capture_sampling.py`. Mỗi file CSV của P2 chứa các
cửa sổ XYZ 512 mẫu, tần số mục tiêu 800 Hz và thông tin jitter/drop.

Tạo manifest theo `datasets/samples/p2_manifest.template.csv`:

```csv
path,run_id,condition,speed_configuration,label,source
normal_01.csv,normal_01,normal,speed_3_no_load,0,measured
normal_02.csv,normal_02,normal,speed_3_no_load,0,measured
abnormal_01.csv,abnormal_01,imbalance,speed_3_no_load,1,measured
abnormal_02.csv,abnormal_02,loose_mount,speed_3_no_load,1,measured
```

`path` tính từ vị trí file manifest. Mỗi `run_id` là một lần bật quạt và đo độc
lập. Label 0 là bình thường; label 1 là rung bất thường được tạo/xác nhận trong
thí nghiệm. Không lấy dự đoán của thuật toán làm nhãn.

```bash
.venv/bin/python data_analysis/scripts/run_pipeline.py \
  --manifest data_analysis/datasets/raw/runs.csv \
  --output-dir data_analysis/results/fan_experiment_01
```

Code dùng trục Z đơn vị g đúng adapter hiện tại của P2, 512 mẫu/cửa sổ, nominal
800 Hz và `actual_hz` của từng cửa sổ. Nó loại cửa sổ thiếu/trùng/đảo mẫu, có
lỗi hoặc dropped sample, Fs lệch quá 5%, hoặc jitter tối đa quá 5% chu kỳ mẫu.
Một run không còn cửa sổ hợp lệ sẽ báo lỗi.

Mỗi model chỉ dùng một `speed_configuration` và một nguồn measured/simulated.
Nếu đổi tốc độ quạt, tải, vị trí hoặc hướng gắn cảm biến thì thu dataset/model
riêng. Cần ít nhất 2 run mỗi nhãn để code chia tập; số này chỉ là tối thiểu kỹ
thuật, chưa đủ để kết luận học thuật.

## Thuật toán

Mặc định trừ trung bình cửa sổ để loại trọng lực/DC. Năm đặc trưng là:

- RMS: độ rung tổng thể, đơn vị g.
- Peak-to-peak: khoảng từ mẫu nhỏ nhất đến lớn nhất, đơn vị g.
- Crest factor: đỉnh chia RMS, không có đơn vị.
- Dominant frequency: bin tần số mạnh nhất ngoài DC, đơn vị Hz.
- Band energy: tổng công suất phổ 10–200 Hz, đơn vị g².

FFT dùng cửa sổ Hann đối xứng. Độ phân giải tần số là Fs/N; với 800/512 là
1,5625 Hz. Band energy là đại lượng từ tín hiệu gia tốc, không phải năng lượng
cơ học tính bằng joule.

Baseline: `RMS > rms_threshold` thì abnormal. Ngưỡng lấy từ phân vị 0,99 của
các cửa sổ normal trong train.

Phương pháp nâng cao dùng RMS, crest factor, band energy và dominant frequency:

```text
z_i = (feature_i - mean_normal_train_i) / scale_normal_train_i
score = sum(weight_i * abs(z_i))
score > score_threshold => abnormal
```

Mặc định bốn trọng số bằng nhau. `abs(z)` phát hiện độ lệch theo cả hai hướng
và tránh các thành phần triệt tiêu nhau. Score không phải xác suất hỏng.

Train/test được chia theo `run_id` và phân tầng theo label + condition; mọi cửa
sổ cùng lần đo nằm trong cùng một tập và mỗi condition cần ít nhất 2 run.
Trung bình, scale và ngưỡng chỉ học từ normal train. Nếu điều chỉnh trọng
số/ngưỡng, phải tách thêm validation theo run và giữ test cuối độc lập.

## Đầu ra bàn giao

Thư mục kết quả gồm `features.csv`, `train.csv`, `test.csv`, `split.json`,
`model.json`, `p3_model_parameters.h`, `test_predictions.csv`, `metrics.json`,
ba biểu đồ và `report.md`.

Confusion matrix có hàng là nhãn thật, cột là dự đoán:
`[[TN, FP], [FN, TP]]`. Lớp dương là abnormal. Các chỉ số gồm accuracy,
precision, recall, false-alarm rate và missed-detection rate, tính theo cửa sổ.

Header C++ chỉ chứa tham số P3. Model xuất từ simulated data có
`IS_MEASURED_DATA=false` và không được dùng làm ngưỡng thật.

## Chạy kiểm tra

```bash
.venv/bin/python -m unittest discover -s data_analysis/tests -v
```

Test gồm tín hiệu 50 Hz có kết quả biết trước, dữ liệu lỗi, chia run không rò
rỉ, fit chỉ từ normal train, toàn bộ pipeline và C++ host test.

Firmware P3 nằm ở `firmware/include/features.h`, `fft_processor.h`,
`classifier.h` và ba file `.cpp` cùng tên. Xem
`firmware/docs/p3-fan-vibration.md` để bàn giao kết quả cho P1/P4.

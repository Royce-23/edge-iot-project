# P3 — giao diện phát hiện rung bất thường của quạt

P3 chỉ sửa các module được phân công: features, fft_processor, classifier và
data_analysis. `main.cpp`, sampling, MQTT, backend, contract chung và dashboard
không bị sửa.

P2 hiện bàn giao `SampleWindow` gồm 512 mẫu trục Z, đơn vị g, đã bỏ DC và có
`sampleRateHz` đo được. P3 có thể nhận mảng đó như sau:

```cpp
#include "classifier.h"

configureFeatures({10.0f, 200.0f, true});
configureClassifier(p3_fan_model::classifierConfig());

FanDetectionResult result{};
const bool valid = analyzeFanVibration(
    window.values, SAMPLE_COUNT, window.sampleRateHz, result);
```

Khi `valid=true`, P3 bàn giao cho P1/P4 các trường:

```text
rms
peak_to_peak
crest_factor
dominant_frequency
band_energy
anomaly_score
health_state
```

`health_state` của model nhị phân là NORMAL hoặc WARNING. P3 không tự tạo FAULT
vì dataset chưa có nhãn mức độ hỏng. Dữ liệu/cấu hình lỗi hoặc chưa nạp model
trả UNKNOWN và score NaN; bên gửi không được đổi thành 0 hoặc NORMAL.

`p3_model_parameters.h` do pipeline Python tạo. Chỉ copy header từ thí nghiệm
measured phù hợp vào vùng tích hợp sau khi review. P3 chưa tự include header đó
vào `main.cpp`, vì `main.cpp` thuộc P1 và việc thay payload thuộc P1/P4.

`VibrationFeatures` ba trường trong `app_types.h` được giữ nguyên. Hàm
`extractFeatures(..., VibrationFeatures&)` vẫn cung cấp RMS/P2P/crest theo
scaffold. API đầy đủ dùng `FanVibrationFeatures`; cách này tránh sửa kiểu dùng
chung khi chưa có phê duyệt của P1/P4.

FFT dùng bộ đệm tĩnh khoảng 8 KiB ở N tối đa 1024, không cấp phát heap trong
mỗi cửa sổ. Chỉ gọi từ một task xử lý, không gọi đồng thời hoặc trong ISR.

Chạy test C++ cùng Python bằng lệnh trong `data_analysis/README.md`. PlatformIO
vẫn build toàn repo vì các file P3 không thay đổi entry point hay module P1/P2.

## Test trực tiếp bằng ESP32 và quạt

Firmware chẩn đoán P3 chạy độc lập, không thay `main.cpp`, MQTT hoặc backend:

```bash
pio run -d firmware -e p3-fan-vibration-diagnostic
pio run -d firmware -e p3-fan-vibration-diagnostic -t upload
pio device monitor -b 115200
```

Môi trường P3 đưa `Serial` ra cổng `COM/UART` qua chip CH343. Khi dùng cổng
này, mở monitor với `--dtr 0 --rts 0` nếu bo bị reset do tín hiệu điều khiển:

```bash
pio device monitor -p /dev/ttyACM0 -b 115200 --dtr 0 --rts 0
```

Sau khi mở Serial Monitor, bật quạt ở đúng tốc độ/cấu hình bình thường và giữ
nguyên trong 30 cửa sổ đầu (khoảng 20 giây). Firmware dùng các cửa sổ đó tạo
model tạm rồi in `NORMAL` hoặc `ABNORMAL` cho cả baseline RMS và anomaly score
nhiều đặc trưng. Mỗi dòng còn có RMS, peak-to-peak, crest factor, dominant
frequency và band energy. Gửi ký tự `r` qua Serial để hiệu chỉnh lại.

Model tự hiệu chỉnh này chỉ giúp kiểm tra nhanh cảm biến và code trên bàn thử;
không dùng ngưỡng hay kết quả đó trong báo cáo. Dataset và model chính thức vẫn
phải tạo từ nhiều `run_id` measured bằng quy trình trong `data_analysis/README.md`.

Nếu Serial báo `ADXL345 initialization failed`, kiểm tra nguồn 3,3 V, mass và
bốn dây SPI theo tài liệu P2. Không được chạm vào cánh quạt đang quay; chỉ thử
trạng thái bất thường trên rig có che chắn và cách tạo rung an toàn.

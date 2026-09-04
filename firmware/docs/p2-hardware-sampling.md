# P2 — Hardware và sampling

Tài liệu này là phần bàn giao của P2 cho P1/P3. Code mặc định dành cho
ESP32-S3 DevKitC-1, ADXL345 nối SPI 4 dây và một DS18B20 tùy chọn. Phải đối
chiếu pin với đúng board thật trước khi cấp nguồn.

## Cấu hình đã chọn

| Mục | Giá trị ban đầu | Lý do |
|---|---:|---|
| Dải ADXL345 | ±8 g, full resolution | Có headroom khi thử rig; độ nhạy xấp xỉ 3,9 mg/LSB |
| ODR ADXL345 | 800 Hz | Bandwidth danh định 400 Hz |
| Timer lấy mẫu | 800 Hz | Khớp ODR cảm biến |
| Cửa sổ | 512 mẫu | 0,64 s/cửa sổ; độ phân giải FFT 1,5625 Hz |
| Dải phân tích ban đầu | 0–300 Hz | Nyquist ở 400 Hz, còn biên trước giới hạn Nyquist |
| Giao tiếp | SPI mode 3, 5 MHz | Thời gian đọc XYZ ngắn và ổn định hơn I2C trong cấu hình này |
| Nhiệt độ | DS18B20, 10-bit | Tối đa 187,5 ms/chuyển đổi; không nằm trong cửa sổ rung |

`fs = 800 Hz` chỉ là cấu hình khởi đầu có căn cứ, không phải số đo để đưa thẳng
vào báo cáo. Trên rig thật phải dùng cột `actual_hz`, `jitter_rms_us` và
`dropped_samples` do firmware chẩn đoán xuất ra. Nếu cần quan sát thành phần
trên 300 Hz, nhóm phải xem lại ODR, bandwidth, anti-aliasing và tần số lấy mẫu.

## Sơ đồ nối dây mặc định

### ADXL345 (SPI 4 dây)

| ADXL345 | ESP32-S3 | Ghi chú |
|---|---:|---|
| VCC | 3V3 | Không đưa mức logic 5 V vào ESP32-S3 |
| GND | GND | Chung mass |
| CS | GPIO10 | Kéo HIGH khi không giao tiếp |
| SDA/SDI/MOSI | GPIO11 | Dữ liệu từ ESP32 đến ADXL345 |
| SCL/SCLK | GPIO12 | Clock SPI |
| SDO/MISO | GPIO13 | Dữ liệu từ ADXL345 về ESP32 |
| INT1/INT2 | Không nối | Phiên bản hiện tại dùng `esp_timer` |

Tên chân in trên module có thể khác tên trong datasheet. Chỉ cấp 3,3 V nếu
không chắc breakout có regulator/level shifter. Dây SPI phải ngắn, cố định và
không đi sát dây motor.

### DS18B20 tùy chọn

| DS18B20 | ESP32-S3 | Ghi chú |
|---|---:|---|
| VDD | 3V3 | Dùng chế độ cấp nguồn 3 dây |
| GND | GND | Chung mass |
| DQ | GPIO4 | Điện trở kéo lên 4,7 kΩ từ DQ đến 3V3 |

Nếu không gắn DS18B20, `initSensors()` vẫn thành công khi ADXL345 hoạt động và
`readTemperature()` trả `false`.

## Cách lắp cảm biến và rig

- Bắt ADXL345 cứng lên bệ gần ổ bi/gối đỡ, không để module treo bằng dây.
- Ghi lại hướng X/Y/Z và giữ nguyên vị trí trong mọi run so sánh.
- Dùng motor/quạt điện áp thấp, tấm che và nút ngắt nguồn dễ tiếp cận.
- Không dán vật rời lên cánh quay và không tạo lỗi phá hủy. Tạo abnormal bằng
  bộ rung phụ hoặc thay đổi gá đỡ đã được thiết kế an toàn.
- Kiểm tra dây, ốc, tấm che và khoảng cách an toàn trước mỗi lần chạy.

## API bàn giao cho P1/P3

```cpp
#include "sampling.h"

constexpr std::size_t N = 512;
float ax[N], ay[N], az[N];
float actualFs = 0.0f;

void setup() {
    Serial.begin(115200);
    if (!initSensors()) {
        // Chuyển state sang SENSOR_ERROR; không phân loại dữ liệu giả.
    }
}

void loop() {
    if (collectWindow(ax, ay, az, N, actualFs)) {
        // P3 tiền xử lý/chọn trục hoặc magnitude rồi mới extractFeatures().
    }
}
```

`collectWindow()` chỉ trả `true` khi nhận đủ một cửa sổ mới và mọi lần đọc cảm
biến trong cửa sổ hợp lệ. API có một consumer duy nhất. Kích thước tối đa mặc
định là 1024 mẫu (`P2_SAMPLING_BUFFER_CAPACITY`).

Khi cần bằng chứng chi tiết, dùng `collectWindowWithDiagnostics()` để lấy cả
timestamp và `SamplingMetrics`. Queue không block sampling task: khi đầy, mẫu
cũ nhất bị thay và `bufferOverruns` tăng. Timer notification bị gộp,
buffer overwrite và lỗi đọc cảm biến đều được tính vào `droppedSamples`.

## Build, upload và thu CSV

```bash
pio run -d firmware -e p2-sampling-diagnostic
pio run -d firmware -e p2-sampling-diagnostic -t upload
pio device list
pio device monitor -b 115200
```

Môi trường chẩn đoán là firmware độc lập, không sửa `setup()/loop()` của P1.
Mỗi dòng dữ liệu chứa XYZ theo đơn vị g, timestamp microsecond và metrics của
cả cửa sổ. Lưu output serial, bỏ các dòng bắt đầu bằng `#`, rồi đặt file thật
theo quy ước:

```text
data_analysis/datasets/raw/<condition>_<run_id>_<yyyymmdd-hhmmss>.csv
```

Ví dụ `normal_run01_20260903-090000.csv`. File
`test-data/p2_sampling_example.synthetic.csv` chỉ minh họa schema, tuyệt đối
không dùng làm kết quả thực nghiệm.

## Kiểm tra và số liệu phải ghi

1. Để rig đứng yên, kiểm tra tổng gia tốc gần 1 g và ghi offset từng trục.
2. Cập nhật bias bằng `P2_ADXL345_BIAS_X_G/Y_G/Z_G`; không xóa thành phần rung.
3. Chạy ít nhất 10 cửa sổ đứng yên, 10 cửa sổ normal và 10 cửa sổ abnormal.
4. Kiểm tra không clipping: trị tuyệt đối mỗi trục phải còn cách giới hạn 8 g.
5. Báo cáo trung vị/p95/max của `actual_hz`, `jitter_rms_us` và
   `max_abs_jitter_us`; tổng `dropped_samples` theo mỗi run.
6. Chạy dài 10–15 phút, kiểm tra dây không rơi, không reset và không tăng lỗi.
7. Bàn giao CSV thật cho P3 kèm condition, run_id, RPM/tải, vị trí cảm biến,
   cấu hình pin, range, ODR và ghi chú bất thường.

## Giới hạn cần nêu trong báo cáo

- Chưa có phép đo phần cứng thì chưa được khẳng định sampling đạt 800 Hz.
- ADXL345 không phải cảm biến rung công nghiệp; kết quả phụ thuộc mạnh vào cách
  gá, dải đo, nhiễu điện từ và rig.
- `esp_timer` tạo nhịp đều cho task nhưng Wi-Fi/RTOS vẫn có thể gây jitter; các
  counter và timestamp tồn tại để đo tác động đó.
- Driver DS18B20 hiện hỗ trợ một cảm biến trên bus bằng lệnh Skip ROM.
- Chống aliasing chỉ dựa vào bandwidth nội bộ của ADXL345; cần thận trọng với
  nguồn rung mạnh ngoài dải quan tâm.

## Nội dung P2 trình bày

P2 trình bày đường đi từ cảm biến đến buffer, lý do chọn range/ODR/fs/N,
Nyquist và aliasing, kết quả fs/jitter/dropped samples, cách gá cảm biến và an
toàn rig. Phần số liệu cuối phải lấy từ CSV thật, không lấy từ file synthetic.

# Firmware — P1/P2/P3

Mở folder này bằng PlatformIO. `platformio.ini` dùng ESP32-S3 DevKitC-1 làm
mặc định và bật USB CDC cho cổng `/dev/ttyACM0`. Luồng tích hợp hiện dùng driver
ADXL345 thật của P2, baseline time-domain của P3, state/queue/MQTT của P1.
Chạy `pio run -d firmware` từ root khi đã cài PlatformIO.

- P1: main.cpp, mqtt_client, device_state, offline_queue, config_manager.
- P2: sensors/, sampling.cpp, sampling.h.
- P3: features.cpp, fft_processor.cpp, classifier.cpp, features.h.
- `app_types.h` là kiểu dùng chung; `types.h` chỉ là include tương thích cho P3.

P2 cung cấp raw XYZ cho chẩn đoán. Adapter tích hợp tạo `SampleWindow` 512 mẫu
từ trục Z sau khi bỏ giá trị trung bình của cửa sổ, đơn vị g; `sampleRateHz` là
tần số đo thực tế. `sampling_stub.cpp` chỉ dành cho host test và bị loại khỏi
mọi ESP32 build. Không đổi nghĩa dữ liệu này nếu chưa thống nhất P1/P2/P3.

Baseline hiện chỉ tính RMS, peak-to-peak và crest factor. Ngưỡng 0.30/0.70 g
trong `config_manager.cpp` là tạm thời; chưa dùng làm kết luận bảo trì cho tới
khi P3 hiệu chuẩn bằng dữ liệu bất thường/fault. Không gửi giá trị 0 giả cho
dominant frequency, band energy hoặc anomaly score.

## Build và kiểm tra tích hợp

```bash
pio run -d firmware -e esp32-s3-devkitc-1
g++ -std=c++17 -iquote firmware/include firmware/test/host_core.cpp \
  firmware/src/sampling_stub.cpp firmware/src/processing_stub.cpp \
  firmware/src/device_state.cpp -o /tmp/edge_iot_host_core
/tmp/edge_iot_host_core
```

Để kết nối mạng, copy `include/secrets.example.h` thành `include/secrets.h`,
điền Wi-Fi và địa chỉ IPv4 của máy chạy Mosquitto. `secrets.h` đã bị ignore.

## Chạy riêng phần P2

Đọc [hướng dẫn phần cứng và sampling](docs/p2-hardware-sampling.md), sau đó:

```bash
pio run -d firmware -e p2-sampling-diagnostic
pio run -d firmware -e p2-sampling-diagnostic -t upload
pio device monitor -b 115200
```

Firmware chẩn đoán xuất cửa sổ XYZ và các cột `actual_hz`, `jitter_rms_us`,
`timer_overruns`, `buffer_overruns`, `sensor_read_errors`, `dropped_samples`.
Dòng bắt đầu bằng `#` là trạng thái/lỗi, không phải bản ghi CSV.

Pin và cấu hình mặc định nằm trong `include/hardware_config.h`. Có thể override
bằng `build_flags` mà không sửa driver. Ví dụ:

```ini
build_flags =
    -DP2_ADXL345_PIN_CS=9
    -DP2_DS18B20_PIN=5
    -DP2_ADXL345_BIAS_X_G=0.0125f
```

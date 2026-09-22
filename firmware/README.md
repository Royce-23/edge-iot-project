# Firmware — P1/P2/P3

Mở folder này bằng PlatformIO. `platformio.ini` dùng ESP32-S3 DevKitC-1 làm
mặc định và bật USB CDC cho cổng `/dev/ttyACM0`. Luồng tích hợp hiện dùng driver
ADXL345 thật của P2, đặc trưng time-domain/FFT của P3 và state/queue/MQTT của P1.
Chạy `pio run -d firmware` từ root khi đã cài PlatformIO.

- P1: main.cpp, mqtt_client, device_state, offline_queue, config_manager.
- P2: sensors/, sampling.cpp, sampling.h.
- P3: features.cpp, fft_processor.cpp, classifier.cpp, features.h.
- `app_types.h` là kiểu dùng chung; `types.h` chỉ là include tương thích cho P3.

P2 cung cấp raw XYZ cho chẩn đoán. Adapter tích hợp tạo `SampleWindow` 512 mẫu
từ trục Z sau khi bỏ giá trị trung bình của cửa sổ, đơn vị g; `sampleRateHz` là
tần số đo thực tế. `sampling_stub.cpp` chỉ dành cho host test và bị loại khỏi
mọi ESP32 build. Không đổi nghĩa dữ liệu này nếu chưa thống nhất P1/P2/P3.

Firmware tính RMS, peak-to-peak, crest factor, dominant frequency và band energy.
Ngưỡng RMS 0.30/0.70 g trong `config_manager.cpp` là fallback tạm thời; anomaly
score là `null` cho tới khi P3 nạp model hiệu chuẩn từ dữ liệu thật.

Feature và status được gửi MQTT QoS 1 bằng ESP-MQTT. Record feature chỉ bị xóa
khỏi queue sau PUBACK; khi reconnect, cùng `boot_id`/`sequence` được gửi lại để
backend dedup. Queue RAM mặc định chứa 600 record, xấp xỉ 10 phút ở chu kỳ 1 giây,
và có thể đổi bằng `-DP1_OFFLINE_QUEUE_CAPACITY=<số_record>`. Queue không tồn tại
qua lần reboot; nếu yêu cầu giữ dữ liệu sau mất nguồn thì phải bổ sung lưu bền vững.

## Build và kiểm tra tích hợp

```bash
pio run -d firmware -e esp32-s3-devkitc-1
g++ -std=c++17 -iquote firmware/include firmware/test/host_core.cpp \
  firmware/src/sampling_stub.cpp firmware/src/processing_stub.cpp \
  firmware/src/device_state.cpp -o /tmp/edge_iot_host_core
/tmp/edge_iot_host_core

g++ -std=c++17 -iquote firmware/include firmware/test/p3_features.cpp \
  firmware/src/features.cpp firmware/src/fft_processor.cpp \
  firmware/src/classifier.cpp -o /tmp/edge_iot_p3_features
/tmp/edge_iot_p3_features

g++ -std=c++17 -iquote firmware/include firmware/test/network_policy.cpp \
  -o /tmp/edge_iot_network_policy
/tmp/edge_iot_network_policy
```

Band P3 thử nghiệm là 200-260 Hz, bao phủ các đỉnh khoảng 208-230 Hz đã đo
trên rig hiện tại. Sau khi có ít nhất ba run `normal_*` và ba run `abnormal_*`
độc lập, tạo model và metrics bằng:

```bash
python data_analysis/scripts/build_p3_model.py
```

Script chỉ tạo `include/p3_model.generated.h` khi dữ liệu sạch và balanced
accuracy trên dữ liệu giữ lại đạt tối thiểu 0.80. Firmware tự nạp header này;
nếu chưa có model hợp lệ thì tiếp tục báo `mode=RMS_BASELINE`.

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

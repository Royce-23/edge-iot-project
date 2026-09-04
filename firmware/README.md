# Firmware — P1/P2/P3

Mở folder này bằng PlatformIO. `platformio.ini` dùng esp32-s3-devkitc-1 làm
mặc định; P2 xác nhận đúng board thực tế, pin và dung lượng trước khi build/upload.
`main.cpp` của luồng tích hợp vẫn là skeleton của P1. Module P2 đã có driver
ADXL345/DS18B20, sampling task, buffer và thống kê chất lượng lấy mẫu.
Chạy `pio run -d firmware` từ root khi đã cài PlatformIO.

- P1: main.cpp, mqtt_client, device_state, offline_queue, config_manager.
- P2: sensors/, sampling.cpp, sampling.h.
- P3: features.cpp, fft_processor.cpp, classifier.cpp, features.h.
- `types.h` dùng chung, đổi phải trao đổi cả ba người.

P2 cung cấp cửa sổ ax/ay/az; P3 hiện nhận một trục qua extractFeatures.
P1 chọn trục đã thống nhất và gọi tuần tự; không trộn main của nhiều project.
Không thêm hàm giả luôn trả NORMAL vào luồng đo thật.

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

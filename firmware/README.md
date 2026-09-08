# Firmware — tích hợp người 1 và người 2

Firmware chính gọi module lấy mẫu ADXL345 thật trên ESP32-S3 DevKitC-1.
Phần xử lý hiện là RMS demo có bỏ DC/trọng lực, chưa có FFT của người 3.

## Luồng tích hợp

```text
ADXL345 SPI → sampling task + timer 800 Hz → queue mẫu XYZ
                                               ↓
main → collectWindowWithDiagnostics(ax, ay, Z, ..., 512, metrics)
     → kiểm tra cửa sổ → P3 demo bỏ DC trên Z → RMS → phân loại
     → trạng thái/cảnh báo tại chỗ → queue bản tin → network task → MQTT
```

- Người 1: `src/main.cpp`, `mqtt_client.cpp`, `device_state.cpp`,
  `offline_queue.cpp`, `config_manager.cpp`.
- Người 2: `src/sampling.cpp`, `include/sampling.h`, `src/sensors/`,
  `include/sensors/`, `include/hardware_config.h`.
- Dùng chung: `app_types.h` có cửa sổ 512 mẫu; `module_interfaces.h` chứa API
  xử lý demo của người 3. API lấy mẫu thật nằm trong `sampling.h`.
- `sampling_stub.cpp` là dữ liệu giả cũ, bị loại khỏi cả hai bản build.
- `diagnostics/p2_sampling_diagnostic.cpp` chỉ build trong `p2-diagnostic`,
  tránh trùng `setup()/loop()` với main của người 1.

## API bàn giao

```cpp
bool initSensors();
bool collectWindow(float* ax, float* ay, float* az, std::size_t count,
                   float& actualSampleRateHz);
bool collectWindowWithDiagnostics(float* ax, float* ay, float* az,
                                  uint64_t* timestampsUs, std::size_t count,
                                  SamplingMetrics& metrics);
bool readTemperature(float& celsius);
```

`main` dùng API diagnostics để lấy tần số thực tế, jitter và thời điểm đo.
`window.values` làm mảng Z; X/Y được lấy cùng cửa sổ. Mảng nằm trong vùng nhớ
tĩnh. `uptime_ms` lấy từ timestamp mẫu đầu; tần số thực tế đi vào
`window.sampleRateHz` cho người 3.

Chỉ một task được gọi `collectWindow*`. Mỗi lần gọi bỏ dữ liệu cũ rồi đợi cửa
sổ mới. 512 mẫu ở 800 Hz mất khoảng 0,64 giây; lịch hiện tại bắt đầu một cửa
sổ khoảng mỗi giây, không phân tích mọi mẫu giữa hai cửa sổ.

Lỗi đọc, timeout, timestamp không tăng hoặc counter báo mất mẫu làm hàm trả
`false`. Main báo `UNKNOWN`, bỏ phân loại và không gửi features cửa sổ lỗi.
Metrics được giữ kể cả khi lỗi; timeout có thể không làm tăng counter overrun.
Luôn kiểm tra giá trị trả về, không dùng metrics một mình để kết luận hợp lệ.

## Build và nạp

Chạy tại thư mục gốc repository:

```powershell
pio run -d firmware -e esp32-s3-devkitc-1
pio run -d firmware -e p2-diagnostic

# Nạp bản tích hợp sau khi kiểm tra board/chân nối
pio run -d firmware -e esp32-s3-devkitc-1 -t upload
pio device monitor -b 115200
```

Nếu `pio` chưa có trong PATH, dùng executable tại
`$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe`.
Copy `include/secrets.example.h` thành `include/secrets.h` rồi điền Wi-Fi/MQTT.
Chân mặc định: SCK=12, MISO=13, MOSI=11, CS=10; DS18B20 tùy chọn ở GPIO4.
Xác nhận với mô hình thật trước khi nạp. Temperature hiện được đọc trong bản
diagnostics, chưa đưa vào bản tin của main.

## Kiểm tra demo

1. Kiểm tra `[BOOT]` báo ADXL345, 512 mẫu, trục Z; `[SAMPLE]` có tần số quanh
   800 Hz, timestamp đầu/cuối và jitter.
2. Tạo điều kiện bình thường/bất thường an toàn, quan sát `[STATE]`/`[DATA]`.
   Ngưỡng 0,30/0,70 g đang là ngưỡng demo, cần người 3 hiệu chuẩn.
3. Ngắt Wi-Fi: đo và phân loại vẫn tiếp tục, log báo `OFFLINE`, queue tăng.
   Kết nối lại: kiểm tra queue giảm và bản tin cũ được gửi trước.
4. Nạp `p2-diagnostic` riêng để xuất CSV XYZ và jitter; không chạy đồng thời
   hai consumer lấy mẫu trong cùng firmware.

## Kiểm chứng và giới hạn

- Đã build thành công cả `esp32-s3-devkitc-1` và `p2-diagnostic` bằng PlatformIO.
  Test C++ trên PC đã qua: tín hiệu có offset 1 g, bỏ DC, RMS/crest, biên ngưỡng,
  NaN và trạng thái/alarm. Chưa nạp/test ESP32, jitter thực tế hoặc reconnect.
- `NORMAL/WARNING/FAULT/UNKNOWN` là tình trạng đo; `ONLINE/OFFLINE` là trạng
  thái mạng riêng. Cửa sổ lỗi hiện tắt alarm GPIO và chỉ log, chưa có topic lỗi.
- Queue offline chứa 60 bản tin trong RAM, đầy thì bỏ bản tin mới; mất điện sẽ
  mất queue. MQTT QoS 0 chưa bảo đảm backend nhận từng bản tin.
- MQTT ghi `data_source=sensor`, `method=rms_demo_dc_removed`. Payload chưa đủ
  contract v1: FFT, anomaly score, metadata, tên `sequence` và các trường mở rộng
  cần thống nhất với người 3/4. Chưa xác nhận backend/dashboard.
- `features.h/types.h` vẫn là scaffold khác với
  `module_interfaces.h/app_types.h`; cần thống nhất trước khi ghép FFT thật.
- Driver dùng timer phần mềm, chưa dùng DATA_READY/FIFO xác nhận mỗi lần đọc
  là mẫu cảm biến mới. Timestamp là thời điểm ESP32 đọc; counter phần mềm không
  chứng minh không lặp/bỏ mẫu bên trong ADXL345. SPI hiện cũng chưa phát hiện
  chắc chắn cảm biến bị rút sau khi khởi tạo thành công.

Chạy test logic trên PC (không dùng `sampling_stub.cpp`):

```powershell
New-Item -ItemType Directory -Force firmware/.pio/host-tests | Out-Null
g++ -std=c++11 -Wall -Wextra -Werror -Ifirmware/include firmware/test/host_core.cpp firmware/src/processing_stub.cpp firmware/src/device_state.cpp -o firmware/.pio/host-tests/host_core.exe
./firmware/.pio/host-tests/host_core.exe
```

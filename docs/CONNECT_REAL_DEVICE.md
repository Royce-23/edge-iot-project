# Kết nối ESP32-S3 để dashboard hiện dữ liệu thật

## Trạng thái hiện tại

Mã nguồn đã có luồng **ESP32-S3 → MQTT broker → backend Render → dashboard Render** và đã build firmware. Trang đang chạy chỉ đổi sau khi bạn cập nhật mã lên repo Render, cấu hình broker và nạp firmware vào bo mạch. ID mặc định của thiết bị là `motor_01`.

## 1. Chuẩn bị MQTT broker

Bạn cần một broker MQTT mà **cả ESP32 trên Wi-Fi và backend trên Render** truy cập được. `edge-iot-project-1.onrender.com` là backend HTTP, không phải MQTT broker. Nếu chưa có broker, có thể tạo một cluster HiveMQ Cloud và credentials trong **Access Management** theo [hướng dẫn chính thức](https://docs.hivemq.com/hivemq-cloud/quick-start-guide.html). Ghi lại hostname, port (thường là `8883` cho TLS), username và password. Không đặt `https://` hoặc `mqtts://` trong hostname.

## 2. Cập nhật backend trên Render

Đưa mã nguồn mới lên **repo GitHub mà Render đang theo dõi**. Nếu dùng file ZIP được cung cấp, giải nén và chép **nội dung** thư mục `edge-iot-project-main` vào gốc repo hiện tại rồi commit/push. Trong Web Service backend, kiểm tra:

- Root Directory: `backend`
- Build Command: `pip install -r requirements.txt`
- Start Command: `uvicorn app.main:app --host 0.0.0.0 --port $PORT`
- Instance/worker: một process Uvicorn để chỉ có một MQTT subscriber

Trong **Environment** của Web Service, điền:

```text
MQTT_HOST=<hostname-cua-broker>
MQTT_PORT=8883
MQTT_TLS=true
MQTT_USER=<username>
MQTT_PASSWORD=<password>
MQTT_CLIENT_ID=edge-iot-backend
DASHBOARD_ORIGINS=https://edge-iot-project-2.onrender.com
```

Không cần `DEVICE_API_KEY` cho luồng MQTT. Có thể để SQLite mặc định để thử nhanh, nhưng Render có filesystem tạm thời: dữ liệu SQLite sẽ mất khi service khởi động lại hoặc deploy lại. Để lưu lâu dài, tạo Render Postgres rồi đặt `DATABASE_URL` bằng **Internal Database URL** của nó; mã đã hỗ trợ `postgresql://` và `postgres://`. Xem [Render Persistent Disks](https://render.com/docs/disks) và [Render Postgres](https://render.com/docs/postgresql-creating-connecting).

Deploy backend, sau đó mở `https://<backend-cua-ban>/api/health`. Kết quả cần có `"mqtt_enabled":true` và `"mqtt_connected":true`.

## 3. Cập nhật dashboard trên Render

Dịch vụ static site cần deploy lại thư mục `dashboard`. Nếu Root Directory để trống, Publish Directory là `dashboard`. Nếu Root Directory là `dashboard`, Publish Directory là `.`. [Render giải thích các đường dẫn này](https://render.com/docs/monorepo-support).

Trong `dashboard/js/app.js`, `API_BASE` mặc định là `https://edge-iot-project-1.onrender.com`. Nếu Web Service backend của bạn có URL khác, sửa giá trị đó rồi deploy lại static site. Dashboard mặc định xem `motor_01`; để xem thiết bị khác, thêm `?device=<device_id>` vào URL.

## 4. Nạp firmware cho ESP32-S3

Copy `firmware/include/secrets.example.h` thành `firmware/include/secrets.h`, rồi điền trên máy của bạn:

```cpp
#pragma once
#define WIFI_SSID "TEN_WIFI"
#define WIFI_PASSWORD "MAT_KHAU_WIFI"
#define MQTT_HOST "hostname-cua-broker"
#define MQTT_PORT 8883
#define MQTT_USE_TLS 1
#define MQTT_USER "MQTT_USERNAME"
#define MQTT_PASSWORD "MQTT_PASSWORD"
#define MQTT_ROOT_CA ""  // Broker dùng CA công khai: dùng bộ CA có sẵn
```

Dùng đúng broker host/port/tài khoản đã cấu hình trên Render. `secrets.h` được Git ignore; không đưa mật khẩu lên GitHub. Nếu broker dùng CA riêng, thay `MQTT_ROOT_CA` bằng PEM CA của broker. ESP32 cần ra Internet để đồng bộ NTP trước khi kết nối TLS.

Từ thư mục gốc dự án, cắm ESP32-S3 qua USB và chạy:

```bash
pio run -d firmware -e esp32-s3-devkitc-1 -t upload
pio device monitor -b 115200
```

Serial Monitor cần hiện `Wi-Fi connected` rồi `MQTT connected`. Khi ADXL345 trả về cửa sổ mẫu hợp lệ, firmware gửi `machine/motor_01/features` và heartbeat `machine/motor_01/status`.

## 5. Xác nhận dữ liệu thật

Theo đúng thứ tự này:

1. `https://<backend>/api/health` → `mqtt_connected: true`.
2. Serial Monitor của ESP32 → `Wi-Fi connected`, `MQTT connected` và log đo mẫu hợp lệ.
3. `https://<backend>/api/devices/motor_01/latest` → JSON có `sequence`, `rms`, `received_at`; `sequence` tăng khi thiết bị gửi tiếp.
4. `https://<backend>/api/devices/motor_01/status` → `online: true`, `stale: false`.
5. Mở `https://edge-iot-project-2.onrender.com/` → số đo thay đổi theo thiết bị. Nhiệt độ có thể là `-` nếu chưa gắn DS18B20; anomaly score có thể là `-` nếu chưa nạp model đã hiệu chuẩn.

| Triệu chứng | Kiểm tra |
|---|---|
| `mqtt_connected: false` | Host/port/TLS/user/password trong Render; broker cho phép subscribe `machine/+/features` và `machine/+/status`. |
| ESP32 không có `MQTT connected` | Wi-Fi, NTP, host/port/user/password, TLS và CA; xem log Serial Monitor. |
| MQTT kết nối nhưng `/latest` trả 404 | ADXL345 có đọc được không, device ID có phải `motor_01` không, backend có nhận topic `machine/motor_01/features` không. |
| `/latest` có dữ liệu nhưng dashboard trống | URL backend trong `dashboard/js/app.js`, `DASHBOARD_ORIGINS`, và ID ở URL dashboard. |
| Dữ liệu hiện rồi mất sau deploy | Chuyển `DATABASE_URL` sang Postgres hoặc dùng persistent disk. |

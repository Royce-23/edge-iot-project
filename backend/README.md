# Backend nhận dữ liệu thật

Luồng chạy: ESP32-S3 → MQTT broker → subscriber trong FastAPI → database → REST/WebSocket → dashboard. Backend **không tạo dữ liệu mẫu**. Nếu chưa có broker hoặc thiết bị, `/api/devices/motor_01/latest` trả 404.

## Chạy local

```bash
cd backend
python -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
# Điền MQTT_HOST, MQTT_PORT, MQTT_TLS, MQTT_USER, MQTT_PASSWORD
set -a; . .env; set +a
uvicorn app.main:app --host 0.0.0.0 --port 8000
```

`MQTT_HOST` chỉ là hostname/IP của **MQTT broker**, không phải URL `https://...onrender.com`. Broker phải cho cả ESP32 và backend kết nối. Dùng cùng host/port/user/password ở `firmware/include/secrets.h`. Khi broker ở ngoài LAN, dùng MQTT TLS và tài khoản riêng. Backend tin cậy chứng chỉ hệ thống; `MQTT_CA_FILE` chỉ cần khi dùng CA riêng. Một backend process dùng một `MQTT_CLIENT_ID` cố định để broker giữ subscription khi kết nối lại. Chạy một Uvicorn worker để tránh hai subscriber dùng cùng client ID.

## Deploy trên Render

- Tạo **Web Service** cho thư mục `backend`. Build command: `pip install -r requirements.txt`. Start command: `uvicorn app.main:app --host 0.0.0.0 --port $PORT`.
- Cấu hình các biến từ `.env.example` trong Render Environment. `DASHBOARD_ORIGINS` phải chứa URL dashboard (mặc định là `https://edge-iot-project-2.onrender.com`).
- Dùng `DATABASE_URL` trỏ đến Postgres hoặc SQLite trên persistent disk. SQLite nằm trong filesystem mặc định của Render sẽ mất khi restart/redeploy.
- Kiểm tra `/api/health`: `mqtt_enabled=true`, `mqtt_connected=true`. Sau khi ESP32 gửi, kiểm tra `/api/devices/motor_01/status`, `/latest`, `/history`.

Subscriber chấp nhận `machine/{device_id}/features` theo `contracts/payload-schema.json` và `machine/{device_id}/status` theo `contracts/mqtt-topics.md`. Payload sai schema hoặc `device_id` khác topic sẽ bị từ chối. QoS 1 có thể gửi lại; database chống trùng bằng `(device_id, boot_id, sequence)`. Trạng thái online hết hạn sau 15 giây nếu không nhận heartbeat. Bản tin retained `online=true` không được tính là heartbeat mới.

`POST /api/ingest/features` và `/api/ingest/status` chỉ dành cho đường HTTP dự phòng; hai route này yêu cầu `DEVICE_API_KEY` và header `X-Device-Key`. Không cần bật chúng khi dùng MQTT.

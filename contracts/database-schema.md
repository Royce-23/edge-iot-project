# SQLite đích — P4 triển khai

| Bảng | Trường/constraint chính |
|---|---|
| devices | device_id PK, name, created_at |
| feature_records | device_id FK, boot_id, sequence, timestamp nullable, received_at, các đặc trưng; UNIQUE(device_id, boot_id, sequence) |
| health_events | event_id UNIQUE, device_id FK, timestamp nullable, received_at, type, message |
| device_status | device_id PK/FK, online, last_seen |

INSERT dữ liệu gửi lại phải bỏ qua khóa trùng, không tăng số bản ghi.
Thêm index (device_id, timestamp) cho truy vấn lịch sử và bật foreign_keys.
Không commit database chạy thật. Sơ đồ này là hợp đồng dự kiến, chưa có migration.

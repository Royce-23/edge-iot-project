# Phân công và bàn giao

Thay cột "Tên / GitHub" bằng thành viên thật trước khi bắt đầu.

| Người | Tên / GitHub | Sở hữu code | Bàn giao |
|---|---|---|---|
| P1 | Chưa điền | main.cpp, device_state, mqtt_client, offline_queue, config_manager | Firmware tích hợp, reconnect, phát hiện offline |
| P2 | Chưa điền | sensors/, sampling.cpp, sampling.h | Driver, cửa sổ ax/ay/az, tần số đo được, CSV mẫu |
| P3 | Chưa điền | features, fft_processor, classifier, data_analysis/ | Đặc trưng, baseline, score, dataset và metrics |
| P4 | Chưa điền | backend/, contracts/ điều phối | MQTT subscriber, SQLite, REST, dedup, status |
| P5 | Chưa điền | dashboard/, docs/test-cases/, tổng hợp docs | Dashboard, E2E, báo cáo, slide, video |

P1–P3 chia file bên trong cùng firmware; không tạo ba firmware độc lập.
P1 gọi giao diện P2/P3 trong `firmware/include/`. Chữ ký hàm do cả ba thống nhất.
P4/P5 làm song song bằng `test-data/`. P3 dùng dữ liệu giả để thử code nhưng
chỉ dùng dữ liệu đo thật, có nhãn để báo cáo kết quả thực nghiệm.

Mỗi lần bàn giao: code + cách chạy + đầu vào/đầu ra + bằng chứng kiểm thử + hạn chế.
P1/P4 review giao tiếp thiết bị; P4/P5 review API; P2/P3 review xử lý tín hiệu.

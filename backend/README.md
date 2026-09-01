# Backend — P4

Đề xuất Python + FastAPI + paho-mqtt + SQLite.
Các file app/ hiện là TODO, chưa phải server. Viết requirements.txt khi chốt
thư viện/version. Chạy mock độc lập bằng `python test-data/mock_api.py` từ root
để P5 phát triển trước. Mock không nhận MQTT hoặc lưu database.

Thứ tự: schema → database → API → MQTT subscriber → dedup → heartbeat/status → test.
Đọc toàn bộ contracts/ và phối hợp P1/P3/P5 khi thay đổi.

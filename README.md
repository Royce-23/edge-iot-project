# Edge IoT — Predictive Maintenance

Project môn IoT • Nhóm 5 người • Kế hoạch 7 tuần.

Repo đang ở giai đoạn tích hợp. Firmware đã có luồng P1 + P2 chạy trên
ESP32-S3: đọc ADXL345 thật, lấy mẫu định thời, trích đặc trưng time-domain,
phân loại ngưỡng, cảnh báo cục bộ, xếp hàng RAM và gửi MQTT. FFT/anomaly của P3
và backend/API/dashboard thật vẫn đang được hoàn thiện.

Luồng mục tiêu: ESP32-S3 → MQTT → Python backend → SQLite → dashboard.
ESP32 phải phát hiện bất thường cục bộ ngay cả khi mất mạng.

## Bắt đầu
1. Trưởng nhóm đọc [hướng dẫn GitHub](docs/GITHUB_SETUP.md), đưa repo lên GitHub và mời 4 bạn.
2. Mỗi người clone toàn bộ repo, tạo nhánh của mình, đọc [phân công](docs/TEAM.md).
3. Đọc `contracts/` trước khi thay đổi hàm, MQTT topic hoặc JSON.
4. Xem [quy trình làm nhóm](CONTRIBUTING.md) và [kế hoạch 7 tuần](docs/PLAN_7_WEEKS.md).

| Thư mục | Phụ trách | Nội dung |
|---|---|---|
| firmware/ | P1, P2, P3 | ESP32, cảm biến, xử lý tín hiệu |
| backend/ | P4 | MQTT, API, database |
| dashboard/ | P5 | Giao diện và biểu đồ |
| data_analysis/ | P3, phối hợp P2 | Dataset, phân tích, đánh giá |
| contracts/ | P4 điều phối, cả nhóm thống nhất | Hợp đồng dữ liệu/hàm |
| test-data/ | Cả nhóm | Dữ liệu giả và công cụ mock |
| docs/ | Cả nhóm, P5 tổng hợp | Hướng dẫn, test, báo cáo, slide |

## Chạy thử ngay, không cần ESP32
Mở terminal ở thư mục gốc repo, chạy:
```bash
python test-data/mock_api.py
```
Mở http://127.0.0.1:8000/dashboard/ để thấy bản tin mẫu.
API mẫu: http://127.0.0.1:8000/api/devices/motor_01/latest
Nhấn Ctrl+C để dừng. Mock chỉ phục vụ máy local và dữ liệu cố định.
Nếu Windows không nhận `python`, thử `py`.

## Trạng thái ban đầu
- [x] Cấu trúc thư mục và phân công module.
- [x] Đề xuất contract v1, payload mẫu và mock API/dashboard.
- [x] Driver ADXL345, sampling 800 Hz, kiểm tra jitter/drop và dataset phần cứng P2.
- [x] Ghép luồng P1: cấu hình, state machine, queue offline và MQTT.
- [x] Baseline RMS/peak-to-peak/crest trên cửa sổ Z đã bỏ DC.
- [ ] Chốt contract với cả nhóm trong tuần 1.
- [ ] P3 bổ sung FFT, band energy, anomaly score và hiệu chuẩn ngưỡng.
- [ ] P4/P5 hoàn thiện MQTT ingestion, SQLite, API và dashboard thật.

Các thư mục có README hoặc `.gitkeep` để Git lưu lại; Git không lưu thư mục rỗng.
Không đưa mật khẩu Wi-Fi, token, `.env`, database hoặc dataset lớn vào Git.
Chưa chọn giấy phép phát hành; nhóm thống nhất trước khi công khai/tái sử dụng.

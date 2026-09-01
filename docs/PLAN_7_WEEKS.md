# Kế hoạch 7 tuần

| Tuần | P1 | P2 | P3 | P4 | P5 | Mốc nghiệm thu |
|---|---|---|---|---|---|---|
| 1 | Git, kiến trúc | Chốt phần cứng | Chốt đặc trưng | Contract | Wireframe, test plan | Cả nhóm chạy mock, thống nhất dữ liệu |
| 2 | Skeleton, Wi-Fi | Driver, sampling | CSV, FFT thử | MQTT/backend | Dashboard mock | Từng module có demo riêng |
| 3 | Ghép firmware | Cửa sổ mẫu | RMS/P2P/crest/FFT | Lưu DB thật | API thật | ESP32 → MQTT → DB → UI |
| 4 | Cảnh báo cục bộ | Đo thử | Baseline | Event/status | Cảnh báo UI | Demo baseline |
| 5 | Offline queue | Dataset thật | Score, đánh giá | Dedup/reconnect | E2E | So sánh hai phương pháp bước đầu |
| 6 | Sửa lỗi tích hợp | Độ ổn định | Metrics cuối | Latency/offline | Báo cáo | Demo mất mạng và phục hồi |
| 7 | Freeze, demo | Hardware report | Algorithm report | Backend report | Slide/video | Nộp code, báo cáo, tổng duyệt |

Ưu tiên anomaly score đa đặc trưng; TinyML là mở rộng nếu đáp ứng tiến độ/yêu cầu đề.
Chia train/test theo run_id, không trộn cửa sổ cùng một lần đo giữa train/test.

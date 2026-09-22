# Contract v1 — đề xuất cần chốt tuần 1

Một bản tin features chứa đặc trưng + kết quả phân loại cùng cửa sổ đo.
`payload-schema.json` áp dụng riêng bản tin features, không áp dụng status/events.

| Trường | Ý nghĩa |
|---|---|
| schema_version | 1; thay đổi không tương thích phải tăng phiên bản |
| device_id | ID ổn định, không chứa dấu / |
| boot_id | ID duy nhất cho mỗi lần khởi động |
| sequence | Bộ đếm cửa sổ trong boot, không thay đổi khi gửi lại |
| timestamp | Unix UTC giây lúc đo; null nếu chưa đồng bộ giờ |
| uptime_ms | Thời gian từ lúc boot đến lúc đo; dùng bộ đếm 64-bit |
| sample_rate_hz | Tần số lấy mẫu thực tế của cửa sổ |
| sample_count | Số mẫu hợp lệ trong cửa sổ |
| rms, peak_to_peak | Gia tốc đơn vị g, đã bỏ DC trên trục z đề xuất |
| crest_factor | peak(abs(x))/RMS; 0 nếu RMS bằng 0 |
| dominant_frequency | Hz; bỏ thành phần DC |
| band_energy | Tích phân PSD dải 10–200 Hz, đơn vị g², cần chốt chuẩn hóa/window |
| anomaly_score | Điểm không âm; null khi P3 chưa nạp model hiệu chuẩn từ dữ liệu thật |
| health_state | OFF / NORMAL / WARNING / FAULT |
| temperature_c | °C hoặc null nếu không có cảm biến |

Không dùng OFFLINE làm health_state: mất mạng là trạng thái kết nối riêng,
thiết bị vẫn có thể phát hiện FAULT tại chỗ. Dữ liệu giả không định nghĩa ngưỡng thật.
Không xuất NaN/Infinity vào JSON; cửa sổ lỗi phải tạo sự kiện, không giả đặc trưng = 0.
P2/P3 chốt ODR cảm biến và bộ lọc chống aliasing; 800 Hz/512 chỉ là cấu hình ban đầu.

# Checklist E2E — P5 điều phối

Tất cả đang CHƯA CHẠY. Ghi commit, cấu hình, ngày, người chạy, log/ảnh và pass/fail.

| ID | Kịch bản | Kết quả mong đợi | Trạng thái |
|---|---|---|---|
| T01 | Hoạt động bình thường | Lấy mẫu, lưu, hiển thị và health phù hợp | Chưa chạy |
| T02 | Dữ liệu bất thường có nhãn | Cảnh báo theo thuật toán đã hiệu chuẩn | Chưa chạy |
| T03 | Mất Wi-Fi | ESP32 vẫn tính đặc trưng/phát hiện cục bộ | Chưa chạy |
| T04 | Phục hồi Wi-Fi | Gửi bù đúng ID/thời gian, không trùng | Chưa chạy |
| T05 | Backend restart | DB giữ lịch sử, subscriber kết nối lại | Chưa chạy |
| T06 | Thiết bị mất heartbeat | Status offline, UI không hiển thị như dữ liệu mới | Chưa chạy |
| T07 | Online nhưng không có bản đo mới | UI báo stale | Chưa chạy |
| T08 | Gửi cùng feature hai lần | Chỉ một bản ghi trong DB | Chưa chạy |
| T09 | Replay bản đo cũ | Latest không bị thay bằng bản cũ | Chưa chạy |
| T10 | Demo liên tục 15 phút | Có log độ ổn định và lỗi nếu xuất hiện | Chưa chạy |

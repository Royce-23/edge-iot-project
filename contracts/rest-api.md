# REST API đề xuất

Prefix `/api`, JSON UTF-8. Dưới đây là API đích, mock chỉ triển khai hai route đầu.

| Method/path | Kết quả |
|---|---|
| GET /api/devices | Array [{device_id, online, last_seen}] |
| GET /api/devices/{id}/latest | Một FeatureRecord; 404 nếu chưa có |
| GET /api/devices/{id}/history?limit=100 | Array FeatureRecord, tăng theo thời gian đo; limit 1..1000 |
| GET /api/devices/{id}/events?limit=100 | Array event; P4 chốt schema trước tích hợp |
| GET /api/devices/{id}/status | {device_id, online, last_seen, stale} |

Thời gian dùng UTC Unix giây hoặc null. Lỗi: {"detail":"..."} với HTTP status phù hợp.
Latest lấy theo thời gian đo đã đồng bộ; không để replay cũ ghi đè dữ liệu mới.
Khi timestamp null không suy diễn thứ tự giữa các boot; chỉ thứ tự sequence trong boot.
`last_seen` lấy từ kết nối/heartbeat sống; `stale` dựa độ tuổi bản đo,
không đồng nhất với online. P1/P4/P5 thống nhất timeout và chu kỳ heartbeat.
Dashboard/backend nên cùng origin; nếu tách origin thì cấu hình CORS rõ ràng.

# MQTT đề xuất

| Topic | Nội dung | QoS | Retain |
|---|---|---|---|
| machine/{device_id}/features | Theo payload-schema.json | 1 | false |
| machine/{device_id}/health | Kết quả phân loại tùy chọn, tham chiếu cùng boot_id/sequence | 1 | false |
| machine/{device_id}/events | event_id, device_id, timestamp, type, message | 1 | false |
| machine/{device_id}/status | `{ "device_id": string, "online": boolean, "timestamp": integer|null }` | 1 | true |

Topic chính ban đầu là features. Health tùy chọn; backend không lưu thêm một
feature_record khi nhận health. P4 chốt schema riêng cho health/events/status trước khi tích hợp.
Status dùng heartbeat định kỳ và Last Will online=false; backend cũng kiểm tra timeout.
Firmware gửi `timestamp=null` cho status cho tới khi đồng bộ được giờ UTC.
Payload device_id phải khớp topic. Giới hạn kích thước, kiểm tra schema trước khi lưu.
QoS 1 có thể gửi trùng: dedup bằng (device_id, boot_id, sequence), không bằng timestamp.
Gửi bù giữ nguyên ID và timestamp lúc đo; server thêm received_at lúc nhận.
Không coi một bản ghi lịch sử gửi bù là bằng chứng thiết bị đang online.

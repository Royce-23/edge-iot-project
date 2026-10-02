# Dữ liệu thô
P2/P3 ghi đường dẫn chia sẻ và mô tả từng run tại đây. Dữ liệu trong thư mục này được gitignore.

P2 lưu dữ liệu đo thật theo tên:

```text
<condition>_<run_id>_<yyyymmdd-hhmmss>.csv
```

Mỗi run phải có condition, RPM/tải, vị trí/hướng cảm biến, range, ODR, target
fs, actual fs và ghi chú rig. Không trộn cửa sổ từ cùng một `run_id` vào cả
train và test. Schema minh họa nằm tại
`test-data/p2_sampling_example.synthetic.csv`; file đó không phải dữ liệu đo.

## Quy ước trạng thái P3

Không đưa trạng thái quạt tắt (`OFF`) vào tập `normal`; firmware xử lý `OFF`
riêng. Mỗi mô hình chỉ nên áp dụng cho cùng một chế độ vận hành mục tiêu
(tốc độ, tải, vị trí và hướng cảm biến).

- `normal/healthy/severity=0`: quạt khỏe, lắp đúng, đã chạy ổn định ở tốc độ
  và tải mục tiêu. Thu nhiều run độc lập, có cả lúc nguội và sau khi máy ấm.
- `abnormal/imbalance/severity=1..3`: gắn khối lượng lệch tâm nhỏ, cố định chắc
  chắn và tăng mức độ từng bước trong giới hạn an toàn.
- `abnormal/looseness/severity=1..3`: mô phỏng độ lỏng gá có kiểm soát; không
  tháo chi tiết bảo vệ hoặc để cụm quay có thể văng ra.
- `abnormal/rubbing/severity=1..3`: chỉ dùng rig có che chắn và cơ cấu tạo cọ
  xát nhẹ, lặp lại được. Dừng ngay nếu nhiệt hoặc dòng tăng bất thường.
- `abnormal/bearing_fault`: chỉ dùng ổ bi lỗi có sẵn; không cố tình phá ổ bi
  đang vận hành.

Diagnostic chờ một lệnh metadata trước khi in dữ liệu CSV:

```text
START,normal,healthy,0,normal01,100,no_load
START,abnormal,imbalance,1,imbalance01,100,no_load
STOP
```

Lưu từng lệnh `START` thành một file riêng. Tên file phải khớp label và run ID,
ví dụ `normal_normal01_20260930-100000.csv` hoặc
`abnormal_imbalance01_20260930-110000.csv`. Không đổi cấu hình cơ khí trong
giữa một run.

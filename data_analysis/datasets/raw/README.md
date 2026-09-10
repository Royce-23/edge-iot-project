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

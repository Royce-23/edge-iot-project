# Phân tích — P3 phối hợp P2
notebooks/: khám phá tín hiệu. scripts/: xử lý tái lập được.
datasets/raw/: dữ liệu lớn giữ ngoài Git; ghi đường dẫn chia sẻ trong README.
datasets/samples/: chỉ commit mẫu nhỏ, không thông tin cá nhân.
results/: metrics, biểu đồ nhỏ và cấu hình thực nghiệm.
Mỗi lần đo cần run_id, timestamp, condition, cấu hình tốc độ, sample rate, label.
Chia train/test theo run_id; mọi ngưỡng/chuẩn hóa học từ train, đánh giá trên test độc lập.
Không dùng số giả của mock để tuyên bố độ chính xác thực tế.

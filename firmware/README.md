# Firmware — P1/P2/P3

Mở folder này bằng PlatformIO. `platformio.ini` dùng esp32-s3-devkitc-1 làm
mặc định; P2 xác nhận đúng board thực tế, pin và dung lượng trước khi build/upload.
Đây là skeleton: main chỉ in thông báo, chưa lấy mẫu/phát hiện/kết nối mạng.
Chạy `pio run -d firmware` từ root khi đã cài PlatformIO. Chưa xác minh build phần cứng.

- P1: main.cpp, mqtt_client, device_state, offline_queue, config_manager.
- P2: sensors/, sampling.cpp, sampling.h.
- P3: features.cpp, fft_processor.cpp, classifier.cpp, features.h.
- `types.h` dùng chung, đổi phải trao đổi cả ba người.

P2 cung cấp cửa sổ ax/ay/az; P3 hiện nhận một trục qua extractFeatures.
P1 chọn trục đã thống nhất và gọi tuần tự; không trộn main của nhiều project.
Không thêm hàm giả luôn trả NORMAL vào luồng đo thật.

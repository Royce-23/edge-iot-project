# Dashboard dữ liệu thật

Deploy thư mục `dashboard` dưới dạng static site. `index.html` tải `js/app.js`; mã này lấy REST và WebSocket từ `https://edge-iot-project.onrender.com` theo mặc định. Nếu backend của bạn có URL khác, khai báo `window.EDGE_IOT_API_BASE = "https://backend-cua-ban.onrender.com"` trong một script trước `js/app.js`, hoặc sửa hằng `API_BASE` trong `js/app.js`.

Dashboard mặc định hiển thị `motor_01`. Mở `/?device=ten_thiet_bi` để xem ID khác. ID phải trùng `config_manager.cpp` và MQTT topic. Khi chưa có dữ liệu, số đo là dấu `-`; khi thiết bị ngừng gửi, trạng thái OFFLINE và thời gian dữ liệu cũ vẫn được hiển thị. Nhãn "MQTT CONNECTED" báo backend đã nối broker; trạng thái máy phụ thuộc heartbeat và số đo của ESP32.

Nếu dashboard và backend ở hai domain Render, đặt `DASHBOARD_ORIGINS` của backend bằng domain dashboard. Mở Developer Tools → Network để xem lỗi CORS/API nếu màn hình không cập nhật.

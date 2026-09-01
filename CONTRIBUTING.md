# Làm việc chung

1. Tạo Issue mô tả việc làm, người phụ trách, đầu vào/đầu ra và cách nghiệm thu.
2. Tạo nhánh từ main mới nhất. Nhánh `feature/pN-...` dành cho chức năng;
   `fix/pN-...` dành cho sửa lỗi.
3. Sửa module thuộc phạm vi của mình. Cần sửa phần người khác thì trao đổi qua Issue/PR.
4. Đổi contract phải cập nhật schema, ví dụ và người sử dụng trong cùng PR hoặc kế hoạch chuyển đổi.
5. Chạy thử phần liên quan, mô tả kết quả thật trong PR. Không ghi "đã test" nếu chưa chạy.
6. Một bạn khác review, sau đó merge vào main. Không để tích hợp đến tuần cuối.
7. Mỗi người viết nội dung báo cáo kỹ thuật của mình; P5 tổng hợp.

Commit gợi ý: `feat: add ...`, `fix: handle ...`, `docs: explain ...`, `chore: configure ...`.
Mỗi PR nên hoàn thành một việc nhỏ. Không commit mật khẩu, token, `.env`, `.venv`, `.pio`.
Tên nhánh là quy ước làm việc, không phải cơ chế hạn chế quyền sửa file.

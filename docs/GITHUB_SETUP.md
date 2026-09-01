# Đưa bộ khung lên GitHub — Windows + VS Code

## 1. Trưởng nhóm: chuẩn bị
Giải nén ZIP vào một thư mục riêng, ví dụ `D:\Projects\edge-iot-project`.
VS Code → File → Open Folder → chọn thư mục có `README.md`, `firmware`, `backend`.
Không mở riêng một file. Terminal → New Terminal, chạy `git --version`.

Nếu Git chưa có tên/email, thay thông tin ví dụ và chạy:
```bash
git config --global user.name "TEN_CUA_BAN"
git config --global user.email "EMAIL_GITHUB_HOAC_NOREPLY_CUA_BAN"
```
Dùng địa chỉ noreply ở GitHub Settings → Emails nếu muốn giữ kín email.

## 2. Tạo repo online
Mở https://github.com/new bằng tài khoản trưởng nhóm.
- Repository name: `edge-iot-project`.
- Description: `ESP32-S3 predictive maintenance - IoT project - 5 members - 7 weeks`.
- Visibility: Private.
- Không khởi tạo README, .gitignore hoặc license vì bộ khung đã có file.
- Bấm Create repository và sao chép URL HTTPS của repo.

## 3. Đưa code lên lần đầu
Trong terminal ở đúng thư mục bộ khung:
```bash
git init -b main
git add .
git status
git commit -m "chore: initialize IoT team starter"
git remote add origin https://github.com/USERNAME/edge-iot-project.git
git push -u origin main
```
Thay USERNAME bằng tài khoản sở hữu repo; dùng URL thực tế GitHub cung cấp.
Hoàn tất đăng nhập GitHub trong trình duyệt khi Git/VS Code yêu cầu.
Không dùng mật khẩu tài khoản để nhập vào yêu cầu mật khẩu HTTPS của Git.

Nếu đã có repo/code trước đó: đừng chạy lại cả khối một cách máy móc.
Commit công việc hiện tại trước, chỉ chép các file còn thiếu; không ghi đè code.
Kiểm tra `git remote -v` trước khi thêm origin. Nếu origin đã đúng thì bỏ bước thêm.
Nếu repo online có README/commit: clone repo online trước, chép bộ khung vào bản clone,
kiểm tra khác biệt, commit rồi push. Không dùng force push để vượt lỗi lịch sử.

## 4. Mời thành viên
Repo → Settings → Collaborators (hoặc Collaborators and teams) → Add people.
Mời bằng username GitHub chính xác của 4 bạn; mỗi bạn chấp nhận lời mời.
Không chia sẻ tài khoản hoặc mật khẩu của trưởng nhóm.

## 5. Mỗi thành viên clone
```bash
git clone https://github.com/USERNAME/edge-iot-project.git
cd edge-iot-project
code .
```
Hoặc VS Code → Ctrl+Shift+P → Git: Clone → dán URL → chọn thư mục lưu → Open.
Repo Private yêu cầu đăng nhập đúng tài khoản đã nhận lời mời.
Mỗi người clone đủ repo; chỉ tập trung sửa module được giao.

## 6. Tạo nhánh riêng
Mỗi người chọn đúng MỘT lệnh tương ứng trong bảng, chạy từ `main` đã cập nhật:

| Người | Lệnh |
|---|---|
| P1 | `git switch -c feature/p1-firmware-integration` |
| P2 | `git switch -c feature/p2-sensor-sampling` |
| P3 | `git switch -c feature/p3-feature-detection` |
| P4 | `git switch -c feature/p4-backend-api` |
| P5 | `git switch -c feature/p5-dashboard` |

Nhánh ở máy chỉ hiện trên GitHub sau khi push:
```bash
git push -u origin HEAD
```
Nhánh chưa có thay đổi sẽ chưa có gì để tạo pull request; hãy code và commit trước.
Trưởng nhóm không cần tạo sẵn cả 5 nhánh.

## 7. Sau khi code một chức năng
Ví dụ P2 chỉ stage phần mình:
```bash
git status
git add firmware/src/sensors firmware/src/sampling.cpp firmware/include/sampling.h
git commit -m "feat: add sensor sampling"
git push -u origin HEAD
```
GitHub → Pull requests → New pull request → base `main`, compare nhánh của bạn.
Điền nội dung theo mẫu, nhờ một bạn review, sửa nếu cần rồi mới merge.
Sau PR được merge: về main, pull và tạo nhánh mới cho việc tiếp theo.
Ví dụ `feature/p2-sampling-timing`; nhánh trong bảng là nhánh khởi đầu.

## 8. Cập nhật khi đang làm dở
Commit công việc trên nhánh riêng trước, rồi:
```bash
git fetch origin
git merge origin/main
```
Nếu conflict: đọc hai thay đổi, trao đổi với chủ module, sửa file, `git add` file đó,
`git commit`, chạy thử rồi push. Không chọn Accept All một cách tự động.

## 9. Quy ước main
Ưu tiên PR + ít nhất một bạn review; không push thẳng main sau lần khởi tạo.
Nếu gói GitHub/repo hỗ trợ, vào Settings → Rules → Rulesets hoặc Branches để
yêu cầu pull request/review và chặn force push cho main. Nếu không hỗ trợ,
nhóm vẫn thực hiện quy trình review thủ công; bộ khung không tự bật bảo vệ nhánh.

## 10. Lỗi thường gặp
- Không thấy folder online: Git không lưu folder rỗng; thêm README hoặc .gitkeep rồi commit/push.
- Không thấy folder trong VS Code: File → Open Folder chọn đúng thư mục gốc.
- Explorer sắp xếp chữ cái: không cần đổi tên folder để khớp thứ tự trong bảng phân công.
- `nothing to commit`: chưa có thay đổi mới hoặc chưa lưu file.
- `Author identity unknown`: cấu hình user.name và user.email ở bước 1.
- `remote origin already exists`: kiểm tra `git remote -v`; không thêm origin lần nữa.
- `rejected / non-fast-forward`: lấy và hợp nhất lịch sử; không force push.

## Kiểm tra hoàn tất
- [ ] Trên GitHub thấy README và đủ 7 thư mục chính.
- [ ] 4 bạn đã nhận lời mời và clone thành công.
- [ ] Mỗi bạn đang trên nhánh riêng, không phải main.
- [ ] Cả nhóm đã chạy mock và đọc contracts.
- [ ] Có ít nhất một PR thử được review và merge.

# PBL2 — kho học liệu tiếng Nhật

Backend C++17 cho quản lý và tra cứu từ vựng, ngữ pháp, kanji, bảng chữ cái, câu hỏi JLPT và dữ liệu học tập cá nhân. Giao diện terminal sẽ được làm sau.

## Cấu trúc

- `include/domain/`, `src/domain/`: các thực thể và phiên ôn.
- `include/dsa/`, `src/dsa/`: danh sách liên kết đôi dùng cho lịch sử và thẻ ôn.
- `include/algorithms/`, `src/algorithms/`: KMP và Merge Sort.
- `include/managers/`, `src/managers/`: nghiệp vụ theo nhóm dữ liệu.
- `include/storage/`, `src/storage/`: đọc, kiểm tra, ghi 11 file TXT.
- `Application` giữ chung một `DataStore` và tạo các manager; `ApplicationService` điều phối thao tác nhiều bảng.

Header chỉ khai báo; định nghĩa class nằm trong `src`. Riêng template danh sách đôi, Merge Sort và sinh ID đặt ở `.h` trong `src` và được include để trình biên dịch có thể tạo các phiên bản template.

## Build và kiểm thử

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

`pbl2_app` hiện chỉ là điểm khởi động backend. Dữ liệu cũ trong `database/` chưa được chỉnh hoặc nạp tự động. Khi dùng `FileStorage::loadAll`, thư mục dữ liệu phải có đủ 11 file theo thiết kế 3.0; file rỗng được chấp nhận khi các quan hệ còn lại hợp lệ. Mọi thử nghiệm ghi trong test dùng thư mục tạm riêng.

Sau khi manager thay đổi dữ liệu, `ApplicationService::dirty()` cho biết RAM còn dữ liệu chưa lưu. `save()` ghi các file `.tmp` trước khi thay file đích; thay nhiều file không phải một giao dịch nguyên tử. Gói import/export chỉ gồm năm file học liệu, không gồm tài khoản hoặc kết quả cá nhân.

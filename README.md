# PBL2_JPN — backend C++17

Thư viện quản lý học liệu tiếng Nhật theo backend 3.0: tài khoản, Vocabulary/Grammar/Kanji, Level, tìm kiếm, yêu thích, lịch sử, review, alphabet, ngân hàng câu hỏi, quiz, thống kê và import/export TXT. Backend độc lập UI, không cần raylib; một tiến trình cục bộ gọi API tuần tự.

## Build và test

Windows với MinGW trong PATH:

```powershell
cmake -S . -B build/debug -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j 4
ctest --test-dir build/debug --output-on-failure

cmake -S . -B build/release -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j 4
ctest --test-dir build/release --output-on-failure
```

Visual Studio 2022:

```powershell
cmake -S . -B build/msvc -G "Visual Studio 17 2022" -A x64
cmake --build build/msvc --config Debug -j 4
ctest --test-dir build/msvc -C Debug --output-on-failure
```

Chỉ build thư viện:

```powershell
cmake -S . -B build/library-only -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DPBL2_BUILD_EXAMPLE=OFF
cmake --build build/library-only -j 4
```

Ví dụ consumer tùy chọn, chỉ ghi vào thư mục tạm riêng rồi dọn sau khi thành công:

```powershell
cmake -S . -B build/debug -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DPBL2_BUILD_EXAMPLE=ON
cmake --build build/debug -j 4
.\build\debug\backend_example.exe
```

Mọi test ghi file dùng thư mục tạm. `database/` giữ nguyên. Đã bỏ main cũ vốn thêm/sửa/save trực tiếp database.

## Nối frontend

```cmake
add_subdirectory(path/to/PBL2_JPN pbl2)
target_link_libraries(my_frontend PRIVATE pbl2_backend)
```

Frontend include `Application.h`, tạo một Application, `load(folder)` hoặc `initialize(emptyFolder, admin, password)`, rồi login. Backend cấp ID, kiểm quyền/quan hệ và chấm bài; kết quả là snapshot theo giá trị.

Mỗi thao tác bền vững tự thử save. `Status::ok()` cho biết lệnh được chấp nhận; `save == PENDING` nghĩa là đã áp dụng trong RAM nhưng chưa lưu đủ. Gọi `retrySave()`, giữ backend đang mở, không lặp CRUD/submit. Trước khi hủy backend, gọi `close()` và xử lý UNSAVED_CHANGES; chỉ dùng `discardAndClose()` khi người dùng chủ động bỏ dữ liệu chưa lưu.

## Đọc code và tài liệu

| Vị trí | Trách nhiệm |
|---|---|
| Application.h/.cpp | API duy nhất cho frontend, snapshot, phiên và chính sách lưu |
| domain/ | Entity, ID, validation, ReviewSession/QuizSession |
| dsa/LinkedList.h và .tpp | Danh sách đôi tự cài, dùng thật cho history/review |
| algorithms/ | KMP và Merge Sort tự cài |
| DataStore, managers/, ApplicationService, storage/ | Implementation nội bộ |
| tests/ | Test hành vi/invariant, kiểm tra có hiệu lực cả Release |
| UI/backend_example.cpp | Consumer không chứa luật nghiệp vụ |

- [API và vòng đời](docs/backend_api.md)
- [Kiến trúc/UML/ownership/độ phức tạp](docs/backend_architecture.md)
- [Hướng dẫn học và bảo vệ 5–7 phút](docs/backend_learning_guide.md)
- [Ma trận yêu cầu → API → test, môi trường đã kiểm](docs/backend_completion.md)
- [Hợp đồng 11 file TXT](docs/data_format.md)
- [Thiết kế 3.0 và bổ sung tích hợp](docs/backend_design_3.md)
- [Audit ban đầu](docs/backend_audit.md), [review diff](docs/backend_review.md)

Giới hạn: mật khẩu demo lưu TXT theo schema; không dùng tài khoản thật. Save chưa nguyên tử cho 11 file. Phiên học và chi tiết đáp án không tồn tại qua restart; lịch sử quiz chỉ lưu điểm tổng hợp. Không hứa thread-safe hoặc phục hồi mất điện.

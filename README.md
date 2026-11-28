# PBL2_JPN — backend C++17

Thư viện quản lý học liệu tiếng Nhật theo backend 3.0: tài khoản, Vocabulary/Grammar/Kanji, Level, tìm kiếm, yêu thích, lịch sử, review, alphabet, ngân hàng câu hỏi, quiz, thống kê và import/export TXT. Backend độc lập UI, không cần raylib; một tiến trình cục bộ gọi API tuần tự.

## Build và test

Backend và công cụ migrate cần libsodium từ 1.0.18. Giao diện Raylib là tùy chọn, bật bằng `PBL2_BUILD_RAYLIB`.

Debian hoặc Ubuntu

```sh
sudo apt install build-essential cmake pkg-config libsodium-dev
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DPBL2_BUILD_EXAMPLE=ON -DPBL2_BUILD_RAYLIB=OFF
cmake --build build/debug -j 4
ctest --test-dir build/debug --output-on-failure
```

Trong terminal MSYS2 UCRT64, cài dependency

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-libsodium mingw-w64-ucrt-x86_64-pkgconf
```

Sau đó dùng PowerShell với `C:\msys64\ucrt64\bin` trong PATH để CMake và chương trình tìm đúng thư viện UCRT64.

```powershell
cmake -S . -B build/debug -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DPBL2_BUILD_EXAMPLE=ON -DPBL2_BUILD_RAYLIB=OFF
cmake --build build/debug -j 4
ctest --test-dir build/debug --output-on-failure

cmake -S . -B build/release -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DPBL2_BUILD_EXAMPLE=ON -DPBL2_BUILD_RAYLIB=OFF
cmake --build build/release -j 4
ctest --test-dir build/release --output-on-failure
```

### Build ứng dụng Raylib (tùy chọn)

Trong MSYS2 UCRT64, cài Raylib bằng `pacman -S --needed mingw-w64-ucrt-x86_64-raylib`. Sau đó mở PowerShell với thư mục `ucrt64/bin` trên PATH.

```powershell
cmake -S . -B build-raylib -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON `
  -DPBL2_BUILD_RAYLIB=ON -DPBL2_BUILD_EXAMPLE=ON
cmake --build build-raylib -j 4
ctest --test-dir build-raylib --output-on-failure
```

Chạy `.\build-raylib\pbl2_app.exe` từ thư mục gốc để mở `database/`. Dùng `pbl2_app --data-dir <path>` với đường dẫn tuyệt đối để mở đúng kho từ bất kỳ thư mục làm việc nào. CMake đặt font và giấy phép dưới `assets/fonts/` cạnh executable. Trên Linux, nếu Raylib đã được cài cùng gói CMake, cấu hình với các cờ trên và dùng generator mặc định.

Để chạy demo trên kho tự sinh trong PowerShell

```powershell
$demoFixture = Join-Path ([IO.Path]::GetTempPath()) ('pbl2-demo-' + [guid]::NewGuid())
.\build-raylib\backend_example.exe --fixture "$demoFixture"
.\build-raylib\pbl2_app.exe --data-dir "$demoFixture"
```

Trên Linux, `backend_example --fixture "$(mktemp -d)"` cần một thư mục rỗng. Dùng lại đường dẫn fixture đó với `pbl2_app --data-dir` nếu đã build Raylib. [Kịch bản demo 5–7 phút](docs/backend_learning_guide.md#demo-57-phút)

Visual Studio 2022 với vcpkg. Đặt `VCPKG_ROOT` tới thư mục cài vcpkg.

```powershell
& "$env:VCPKG_ROOT/vcpkg.exe" install libsodium:x64-windows
cmake -S . -B build/msvc -G "Visual Studio 17 2022" -A x64 `
  "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
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

Mọi test ghi vào thư mục tạm với fixture tự sinh.

## Migrate tài khoản

Đóng ứng dụng và mọi tiến trình đang dùng kho trước khi chạy công cụ. Từ thư mục gốc, dùng lệnh tương ứng với bản build ở trên.

```powershell
.\build\debug\pbl2_migrate_users.exe .\database\user.txt
```

```sh
./build/debug/pbl2_migrate_users ./database/user.txt
```

Công cụ chỉ xử lý `user.txt`. Nó kiểm các bản ghi, tạo hash có salt riêng rồi thay file bằng một thao tác trên cùng filesystem. V2 hợp lệ chạy lại chỉ báo số tài khoản. Nếu lịch sử Git từng chứa mật khẩu thật, coi các mật khẩu đó là đã lộ và thay ở mọi nơi từng sử dụng chúng. Công cụ giữ nguyên lịch sử Git.

## Nối frontend

```cmake
add_subdirectory(path/to/PBL2_JPN pbl2)
target_link_libraries(my_frontend PRIVATE pbl2_backend)
```

Frontend include `<pbl2/Application.h>`, tạo một Application, gọi `load(folder)` hoặc `initialize(emptyFolder, admin, password)`, rồi login. Backend cấp ID, kiểm quyền và quan hệ, chấm bài và trả snapshot theo giá trị. `<Application.h>` chuyển tiếp đến header mới cho consumer hiện có.

Mỗi thao tác bền vững tự thử save. `Status::ok()` cho biết lệnh được chấp nhận; `save == PENDING` nghĩa là đã áp dụng trong RAM nhưng chưa lưu đủ. Gọi `retrySave()`, giữ backend đang mở, không lặp CRUD/submit. Trước khi hủy backend, gọi `close()` và xử lý UNSAVED_CHANGES; chỉ dùng `discardAndClose()` khi người dùng chủ động bỏ dữ liệu chưa lưu.

## Đọc code và tài liệu

| Vị trí | Trách nhiệm |
|---|---|
| `include/pbl2/` | Facade, request và snapshot theo giá trị |
| `src/application/`, `src/domain/` | Điều phối nghiệp vụ, manager, entity và phiên nội bộ |
| `src/storage/`, `src/search/` | Kho dùng chung, chỉ mục ID, TXT và xếp hạng học liệu |
| `src/dsa/`, `src/algorithms/` | Danh sách đôi cho history/review và Merge Sort ổn định |
| `apps/raylib/`, `examples/backend_example.cpp` | Giao diện Raylib và consumer console độc lập |
| `tests/` | Hành vi, invariant và fixture tạm, chạy cả Debug và Release |

- [API và vòng đời](docs/backend_api.md)
- [Benchmark tra cứu trước và sau bước ②](docs/search_benchmark.md)
- [Kiến trúc/UML/ownership/độ phức tạp](docs/backend_architecture.md)
- [Hướng dẫn học và bảo vệ 5–7 phút](docs/backend_learning_guide.md)
- [Ma trận nghiệm thu backend tại mốc thiết kế cũ](docs/backend_completion.md)
- [Hợp đồng 11 file TXT](docs/data_format.md)
- [Thiết kế 3.0 và bổ sung tích hợp](docs/backend_design_3.md)
- [Audit ban đầu](docs/backend_audit.md), [review diff](docs/backend_review.md)

Quyền ứng dụng áp dụng qua API. Hệ điều hành quản lý quyền đọc và sửa TXT trực tiếp. Lưu cả 11 file vẫn có thể dở dang giữa các lần replace. Phiên học và chi tiết đáp án kết thúc khi restart, còn lịch sử quiz lưu điểm tổng hợp. Kho dùng một tiến trình gọi tuần tự.

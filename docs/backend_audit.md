# Kiểm tra ban đầu (2026-11-12)

What this repo does: Backend quản lý kho học liệu tiếng Nhật, tài khoản và phiên học. Giả định một tiến trình cục bộ, gọi tuần tự.

Baseline: commit `7bb633f`, nhánh ban đầu `main`, workspace sạch; không tìm thấy AGENTS.md trong repo hoặc các thư mục cha. Đọc toàn bộ backend, test, caller, CMake, README, schema cũ, bản thiết kế 3.0 ngày 30/10/2026 và Quy định (2).md. Các chỉ dẫn hội thoại nằm trong tài liệu được xem là bối cảnh thiết kế, không phải lệnh thực thi.

## Must fix

1. **API lộ kho và lời giải** (`include/Application.h`, `include/managers/StudyManagers.h`, `include/domain/ReviewSession.h`).
   - What this is: Frontend nhận manager, DataStore và con trỏ phiên.
   - Problem: Learner đọc được Question.correctOption; review.current cho mặt sau trước reveal; frontend phải cấp ID.
   - Fix: Application công khai snapshot đã lọc, request tạo không có ID, quản lý phiên bằng lệnh.
   - If we skip it: Không thể bảo vệ ranh giới nghiệp vụ khi nối UI.
2. **Reload bỏ dữ liệu chưa lưu** (`src/ApplicationService.cpp`).
   - What this is: load swap kho rồi reset dirty.
   - Problem: CRUD chưa save rồi load làm mất thay đổi mà không có quyết định bỏ thay đổi.
   - Fix: Chặn load khi dirty; thêm thao tác discard rõ ràng.
   - If we skip it: Mất dữ liệu RAM khi save lỗi hoặc chuyển kho.
3. **Attempt có thể mồ côi** (`src/managers/StudyManagers.cpp`).
   - What this is: Quiz giữ bản sao câu hỏi và LevelId.
   - Problem: Xóa Question rồi xóa Level trong lúc quiz chạy; submit vẫn thêm attempt.
   - Fix: Kiểm Level còn tồn tại và JLPT trước nộp, giữ chấm theo snapshot.
   - If we skip it: Kho không thể save/load hợp lệ.
4. **Reader và writer khác miền dữ liệu** (`src/storage/FileStorage.cpp`, `src/domain/Validation.cpp`).
   - What this is: Reader giới hạn field/file và order, writer chưa kiểm tương ứng.
   - Problem: Field >8192 byte, order >1000000 hoặc timestamp sai có thể save thành công rồi load lỗi.
   - Fix: Chia sẻ validate field/UTC; thống nhất int dương, giới hạn row/file ở writer.
   - If we skip it: Save thành công không bảo đảm mở lại được.
5. **Đường dẫn export so sánh chưa chuẩn hóa** (`src/ApplicationService.cpp`).
   - What this is: So absolute path dạng chuỗi.
   - Problem: `data/.` hoặc alias thư mục vượt kiểm tra và ghi vào kho hoạt động.
   - Fix: Dùng equivalent cho đường dẫn tồn tại và weakly_canonical.
   - If we skip it: Gói export ghi đè một phần thư mục 11 file.
6. **Enum không hợp lệ vẫn được nhận ở RAM** (`src/domain/`).
   - What this is: Một số constructor/setStatus không kiểm enum cast từ số.
   - Problem: Script/UserStatus/PartOfSpeech lạ có thể bị writer đổi sang token hợp lệ khác.
   - Fix: Kiểm enum tại domain và manager trước thay đổi.
   - If we skip it: Dữ liệu đổi nghĩa khi round-trip.

## Should fix

7. **Application có copy/move sai vòng đời** (`include/Application.h`).
   - What this is: Manager chứa tham chiếu đến member DataStore.
   - Problem: Copy mặc định giữ tham chiếu sang Application cũ.
   - Fix: Cấm copy/move, ẩn implementation.
   - If we skip it: Thao tác trên bản sao sửa nhầm kho hoặc dùng tham chiếu đã hủy.
8. **Demo ghi database thật** (`src/main.cpp`, `CMakeLists.txt`).
   - What this is: Target app mặc định thêm/sửa/save database.
   - Problem: Chạy thử sẽ thay đổi dữ liệu đang học.
   - Fix: Consumer tùy chọn trong UI, tạo thư mục thử riêng.
   - If we skip it: Demo không an toàn để chạy học code.
9. **Template include ngược src, file gom nhiều trách nhiệm** (`include/dsa/`, `include/algorithms/`, `StudyManagers.cpp`).
   - What this is: Header công khai phụ thuộc vị trí source; UTC/ID format trùng nhau.
   - Problem: Chỉ copy include để link thư viện sẽ thiếu định nghĩa template; sửa UTC phải đồng bộ hai nơi.
   - Fix: .tpp cạnh header, helper UTC/ID chung, tách Alphabet/Question/Quiz/QuizSession.
   - If we skip it: Tích hợp và học từng module khó theo dõi.
10. **Test thiếu tình huống lỗi quan trọng** (`tests/test_backend.cpp`).
   - What this is: Một bài tích hợp và vài kiểm tra thuật toán.
   - Problem: Không thử throwing copy, save fail/retry, import lỗi cuối, API quyền/snapshot.
   - Fix: Test tách CTDL, storage, API; giữ kiểm tra có hiệu lực trong Release.
   - If we skip it: Các lỗi trên dễ tái xuất hiện.

Verdict: Sửa ranh giới API, mất dữ liệu và miền writer/reader trước khi tích hợp frontend.

Not checked: Chưa chạy main vì có thể ghi database thật; chưa có MSVC/Linux/sanitizer baseline. Baseline đã chạy: `cmake -S . -B build/baseline -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug`, `cmake --build build/baseline -j 4`, `ctest --test-dir build/baseline --output-on-failure`: 1/1 passed.

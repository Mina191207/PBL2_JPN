# Review diff hoàn thiện backend

What this change does: Hoàn thiện API Application cho frontend, giữ các thuật toán/CTDL tự cài và bổ sung bảo vệ phiên, quyền, quan hệ, TXT và trạng thái lưu. Tải dự kiến là một tiến trình cục bộ, gọi tuần tự.

Looks good. Ship.

Đã đọc diff và caller liên quan ở backend, tests, consumer, CMake và README; truy vết create/update/delete → manager → dirty/save, load/import staged, search/history, review và quiz/submit/retry. Không còn finding bắt buộc sửa được xác nhận trong phạm vi này sau các sửa và test hồi quy dưới đây.

Các vấn đề phát hiện khi review đã được sửa:

- Writer từng chỉ validate quan hệ bằng chỉ mục có sẵn: DataStore nội bộ thêm trùng ID mà chưa rebuild có thể ghi dataset không mở lại được. SaveAll/SavePackage nay copy, rebuild và validate trước serialize; test_storage thử duplicate User/Resource và kiểm chưa tạo thư mục đích.
- Cặp category/name ghép bằng ':' có thể va chạm giữa (A:B,C) và (A,B:C). Dùng delimiter '|' bị domain cấm trong dữ liệu; test_search tạo cả hai cặp và load lại thành công.
- Vocabulary NA_ADJ dài hơn token dùng kiểm record ban đầu. Domain và writer nay dùng chung partOfSpeechToken; Question dự trù token INACTIVE dài hơn để toggle không làm record vượt giới hạn.
- Load không được lexical-normalize đường dẫn trước khi mở vì symlink/.. có thể đổi nghĩa. Giữ absolute path và để filesystem phân giải; export dùng cùng helper kiểm danh tính/ancestor thực.
- MSVC: tránh macro max của windows.h; tránh weakly_canonical trực tiếp trên thư mục export chưa tồn tại trong môi trường này. Các test API chạy thật trên MSVC, gồm alias '.', '..' và khác hoa/thường.

Đã giữ các phần đúng: DLL/KMP/Merge Sort không bị thay bằng thư viện, baseline test không giảm assertion/expected; các thay đổi chủ yếu là ranh giới Application, validation chung và tách file theo trách nhiệm. Header template không include ngược src; source nghiệp vụ không cin/cout/sleep/system; không thêm dependency UI/mạng/SQL. Không thay dữ liệu thật hoặc sửa main.

Kiểm chứng cuối, lệnh và giới hạn được ghi ở backend_completion.md. Test bao gồm Release với CHECK vẫn hoạt động, ownership throwing-copy và ASan, API learner bị chặn, snapshot riêng user, load/import all-or-nothing trong RAM, save lỗi và retry không nhân đôi attempt.

Verdict: Ship.

Not checked: Linux, crash/power-loss recovery và lỗi vật lý flush/close/ổ đĩa đầy; không hứa atomic cho cả 11 file. Frontend phải gọi và xử lý close trước khi hủy Application.

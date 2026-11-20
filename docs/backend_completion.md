# Ma trận hoàn thiện backend 3.0

Căn cứ: bản thiết kế ngày 30/10/2026 được giữ ở backend_design_3.md và yêu cầu tích hợp ngày 12/11/2026. Workspace ban đầu sạch tại 7bb633f trên main; làm việc trên refactor/backend-3-completion. Không thay database thật. Baseline MinGW Debug build được, test cũ 1/1 pass; xem backend_audit.md cho các lỗi chưa được baseline phát hiện.

“Đạt” dưới đây dựa trên hành vi đã kiểm, không dựa vào tên class. Một file test chứa nhiều ca CHECK luôn hoạt động trong Release. Test API hoàn thành các luồng qua Application mà không dùng DataStore; các test storage/index cố ý dùng thành phần nội bộ để gây lỗi.

## Yêu cầu → implementation → kiểm chứng

| Nhóm/mục thiết kế và yêu cầu | API hoặc file thực hiện | Test hành vi | Trạng thái |
|---|---|---|---|
| A: bảo vệ workspace, nhánh riêng, baseline | Git; backend_audit.md | status sạch trước sửa, baseline 1/1 | đạt |
| B: backend C++17 độc lập UI | CMake pbl2_backend; Application Impl | library-only, consumer target riêng | đạt |
| B: chia trách nhiệm | AlphabetManager/QuestionManager/QuizManager, domain/QuizSession | test_backend, test_api | đạt |
| B: template cạnh header, include đầy đủ | LinkedList.tpp, MergeSort.tpp, IdGenerator.h | từng header compile riêng; kiểm case path | đạt |
| B/E: copy/move/destructor/ownership | Resource protected copy, DLL deep copy, Application noncopyable/nonmovable | test_algorithms throwing copy/alive; test_api static_assert | đạt |
| C: API duy nhất, snapshot, không lộ kho/password | Application.h/.cpp, AccountSummary, ResourceDetail | test_api apiFlow; UI/backend_example chỉ Application | đạt |
| C/F: error code và trạng thái lưu riêng | Status, Result, Impl::change/persist, retrySave | test_api save lỗi, PENDING, retry, attempt không nhân đôi | đạt |
| C/F: loaded, load lỗi, dirty/reload/close/discard | load, initialize, close, discardAndClose | test_api load lỗi giữ phiên; dirty chặn; discard thành công/thất bại | đạt |
| D1/12.1: login/logout, role/status, epoch | UserManager, Application.login/logout | test_api đổi user/login sai/logout và invalidate review/quiz | đạt |
| D1: bootstrap admin, username/quyền/tài khoản | initialize, createAccount, setAccountStatus, resetPassword | test_api duplicate case, username sai, role sai, self-lock, reset; storage no active admin | đạt |
| D2/12.2: CRUD 3 Resource, ID chung, detail/type bất biến | create/update theo loại, resource, removeResource | test_api create/update/delete, type mismatch, detail snapshot | đạt |
| D2: liên kết Resource/Question/Favorite | ResourceManager::canChange, ApplicationService::removeResource | test_api inactive Question chặn xóa, đổi Level bị chặn, dọn Favorite | đạt |
| D2: Level CRUD, unique/category JLPT, tham chiếu Attempt | LevelManager, levels/create/update/removeLevel | test_api, test_search cặp category/name chứa ':'; storage FK | đạt |
| D3/11: ID hash lookup, KMP một matcher, trường tìm/rank 4/3/2/1 | ResourceManager::search, Resource::matchScore overrides | test_search searchTests: rank tối đa, trường phụ, loại trừ example/note | đạt |
| D3: filter AND, stable sort/tie theo số ID | SearchOptions, MergeSort | test_search mọi sort/filter và ID dài; test_algorithms stability | đạt |
| D3: query rỗng, history đúng một lần, lỗi history không mất kết quả | Application::search, HistoryManager | test_search empty cả exactId, invalid options, max HistoryId; test_api save lỗi | đạt |
| D4: Favorite không trùng và riêng user | FavoriteManager; favorites | test_api duplicate/missing resource/bỏ của người khác | đạt |
| D4: history newest, tìm lại/xóa của mình | HistoryManager, history/searchAgain/clearHistory | test_api tìm lại ghi một lần, user khác không xem/xóa được | đạt |
| D4/13.1: review snapshot, reveal/rate/requeue/stop/count | ReviewSession, ReviewView, Application review commands | test_api snapshot sau sửa nguồn, back ẩn, lặp thẻ, count current, stop | đạt |
| D5: Alphabet CRUD/script/glyph/filter/order | AlphabetManager, alphabet API | test_api create/update/remove/filter/tie; storage duplicate script/glyph | đạt |
| D6: Question CRUD/list/detail/status/options/FK/JLPT | QuestionManager, QuestionInput, questions/question | test_api options trùng trim, level mismatch, learner forbidden; storage negative | đạt |
| D6/13.2: 1–50 ACTIVE, đủ câu, shuffle engine và seed | QuizManager::start, Impl.engine | test_api count 0/51/thiếu câu; test_search cùng seed, không trùng và loại inactive | đạt |
| D6: snapshot/đổi đáp án/bỏ trống/chấm/nộp một lần | QuizSession, submitQuiz | test_api thay Question khi đang làm, bỏ trống=0, retry đạt100%, nộp hai lần | đạt |
| D6: Question/Level đổi trong lúc làm | QuizManager::submit kiểm Level, copy Question | test_api deletedQuestionSnapshot và Level xóa/đổi category | đạt |
| D6: retryWrong và lịch sử tổng hợp sau restart | retryWrong, quizAnswers, attempts | test_api không tạo phiên rỗng, reset lời giải, load mất chi tiết nhưng còn summary | đạt |
| D7: thống kê theo loại/cấp độ | statistics(optional LevelId) | test_api counters/filter, test_search import counters | đạt |
| D7/14.5: import 5 file chỉ thêm, Level reuse/conflict, staged | ApplicationService::importPackage | test_search indexAndImportTests: xung đột cuối gói, không thêm một phần, cùng Level reuse | đạt |
| D7: export đủ Level và cấm thư mục hoạt động tương đương | FileStorage::fillPackageRows, sameDirectory | test_api '.', '..', alias hoa/thường Windows; storage package round-trip | đạt |
| E1/10: DLL hai đầu/hai chiều/eraseIf/size/copy/self assign/no default T | LinkedList.h/.tpp | test_algorithms listTests; test_backend dùng thật history/review | đạt |
| E2/E3: KMP lặp/biên/Unicode, Merge stable/no default T | KmpMatcher.cpp, MergeSort.tpp | test_algorithms so expected/find và Item không default constructor | đạt |
| E4/9: index vị trí/rebuild sau realloc/erase/username | DataStore::rebuildIndexes | test_search thêm100 resource, xóa giữa, đổi username rồi rebuild | đạt |
| E/F: ID uint64_t dài/overflow, max chung, UTC helper | EntityId, IdGenerator, Time | test_algorithms 999/99999/999999/overflow; storage R1000000 round-trip | đạt |
| F/14.1–3: 11 TXT/BOM/LF/CRLF/empty trailing/Unicode/header-detail1:1 | FileStorage::readRows/loadAll/loadPackage | test_storage malformedTests/roundTripTests; test_backend | đạt |
| F: parse hết số/overflow/enum/timestamp/FK/trùng, load all-or-nothing | domain Validation/Time; DataStore checks | test_storage bảng malformed và kho cũ giữ nguyên | đạt |
| F: reader/domain/writer cùng miền field/row/file/int | validateTxtRecord, append, checked.rebuildIndexes | test_storage >8192, row>16384, file>32MiB, INT_MAX, UTC sai, duplicate trước write | đạt |
| F: open/write/flush/close/replace kiểm lỗi, dirty/retry | writeRows, Impl.persist/retrySave | test_storage open/replace lỗi và retry; test_api attempt giữ RAM | đạt |
| G: Release không mất assertion, không dùng database thật | CHECK và assert override cũ, TempDirectory | Debug/Release suites, database diff rỗng | đạt |
| H: API/UML/guide/design/consumer khớp code | README, backend_api/architecture/learning_guide/design_3, UI | consumer compile/chạy; review diff và đối chiếu caller | đạt |

## Kiểm tra môi trường

Kết quả cuối ngày 12/11/2026: toàn bộ năm nhóm test qua trên các cấu hình Windows dưới đây. Không suy diễn kiểm trên Windows thành đã kiểm Linux.

| Cấu hình | Lệnh | Kết quả |
|---|---|---|
| Baseline Windows MinGW GCC15.2 Debug | cmake -S . -B build/baseline -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug; build; ctest | 1/1 pass trước sửa |
| Windows MinGW Debug | cmake --build build/debug -j 4; ctest --test-dir build/debug --output-on-failure | 5/5 pass trên mã cuối |
| Windows MinGW Release | cmake --build build/release -j 4; ctest --test-dir build/release --output-on-failure | 5/5 pass trên mã cuối |
| Windows MSVC19.44 x64 Debug | cmake --build build/msvc --config Debug -j4; ctest --test-dir build/msvc -C Debug --output-on-failure | 5/5 pass trên mã cuối |
| Windows MSVC19.44 x64 Release | build --config Release và ctest -C Release | 5/5 pass |
| Windows MSVC19.44 x86 ASan Release | xem lệnh dưới | 5/5 pass |
| Không tests/UI | build/library-only, BUILD_TESTING=OFF, PBL2_BUILD_EXAMPLE=OFF | build thành công trên mã cuối, không target test/UI |
| Consumer | build/debug/backend_example.exe | chạy thành công với MinGW Debug và MSVC x64 Release; dữ liệu tạm được dọn |
| Header/case | g++ -std=c++17 -Werror -pedantic-errors -fsyntax-only từng .h; kiểm path lowercase | 36 header pass, không trùng case |
| Linux | không có lượt kiểm trong đợt này | chưa kiểm chứng |

ASan MinGW không link được vì toolchain thiếu libasan (`cannot find -lasan`). Máy có MSVC ASan x86, dùng cấu hình riêng (không gắn cờ sanitizer vào API mặc định):

```powershell
cmake -S . -B build/msvc-asan -G "Visual Studio 17 2022" -A Win32 -DCMAKE_TRY_COMPILE_CONFIGURATION=Release "-DCMAKE_CXX_FLAGS=/EHsc /fsanitize=address /Zi" "-DCMAKE_EXE_LINKER_FLAGS=/DEBUG"
cmake --build build/msvc-asan --config Release -j 4
# PATH chứa VC/Tools/MSVC/14.44.35207/bin/Hostx64/x86 để tìm runtime ASan.
ctest --test-dir build/msvc-asan -C Release --output-on-failure
```

Lỗi mới được phát hiện và sửa trong quá trình kiểm: macro max của windows.h xung đột numeric_limits trên MSVC; weakly_canonical của đường dẫn export chưa tồn tại trả Access denied trên MSVC trong môi trường này. Kiểm theo danh tính thư mục và phân giải ancestor đã tồn tại thay thế phần so absolute chuỗi cũ.

## Giới hạn có chủ ý

- Chưa có UI; consumer chỉ minh họa API. Frontend còn form, input/render, điều hướng và thông báo lỗi/PENDING, không còn luật nghiệp vụ phải tự viết.
- Không atomic/recovery cho 11 file hoặc nhiều tiến trình. Nếu replace lỗi giữa chừng, giữ backend RAM và retrySave; không tự load lại dataset đĩa đang chưa lưu đủ.
- Close có kiểm dirty, nhưng destructor không thể cấm caller/OS kết thúc tiến trình. Frontend phải xử lý close; crash/tắt máy/bỏ qua hợp đồng có thể mất RAM chưa lưu.
- Password demo plaintext theo schema. Không thiết kế auth mạng/production.
- Không lưu quiz/review dở hoặc chi tiết đáp án qua restart. History chỉ lưu query; searchAgain dùng tìm nội dung mặc định, không phục dựng filter/mode cũ.
- Giữ byte UTF-8 nguyên vẹn, không chuẩn hóa Nhật/romaji/bỏ dấu hoặc kiểm ngôn ngữ. ID có thể tái dùng sau xóa max theo thiết kế3.0.
- Không mô phỏng lỗi vật lý ổ đĩa đầy/flush/close thật; code có kiểm trạng thái stream, test chủ động gây lỗi mở/replace. Không coi kiểm ASan là chứng minh tuyệt đối không có lỗi bộ nhớ.

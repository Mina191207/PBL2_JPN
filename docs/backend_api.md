# Hợp đồng tích hợp frontend

## Điểm vào và lỗi

Frontend dùng `Application` qua `#include <pbl2/Application.h>` và link target `pbl2_backend`. `<Application.h>` là header chuyển tiếp cho consumer đang dùng đường dẫn cũ. DataStore, manager và FileStorage là header riêng của backend và test nội bộ.

Backend chạy một tiến trình, gọi tuần tự, không thread-safe. Application cấm copy/move vì manager mượn cùng kho. Mọi response sở hữu nội dung theo giá trị, giữ được qua CRUD/load sau đó nhưng là snapshot cũ. Sửa bản sao không sửa kho.

Status gồm `BackendError error`, `SaveState save`, `message`; Result<T> có thêm `optional<T> value`. Kiểm ok() trước khi đọc value. Message để hiển thị/chẩn đoán, không parse nó để điều khiển luồng.

| Mã lỗi | Ý nghĩa |
|---|---|
| NONE | Chấp nhận lệnh/truy vấn; vẫn phải kiểm trạng thái lưu |
| NOT_LOADED | Cần load hoặc initialize |
| LOGIN_REQUIRED / FORBIDDEN | Chưa đăng nhập / thiếu quyền ADMIN |
| INVALID_INPUT | Sai giá trị cục bộ, enum hoặc search options |
| NOT_FOUND | Query detail/history không tìm thấy |
| REJECTED | Luật nghiệp vụ từ chối: trùng, đang tham chiếu, thiếu câu, credentials sai… |
| INVALID_STATE | Sai bước phiên, ví dụ rate trước reveal hoặc xem lời giải trước submit |
| ID_EXHAUSTED | Không thể cấp ID uint64_t kế tiếp |
| IO_ERROR | Load/export/retry save lỗi I/O hoặc dữ liệu TXT sai |
| UNSAVED_CHANGES | Load/close bị chặn vì dirty |

Command quản trị bị manager từ chối thường trả REJECTED, kể cả ID đích không tồn tại; query detail dùng NOT_FOUND. Mã lỗi đủ để UI chọn đăng nhập, giữ form, báo lỗi hoặc retry mà không cần phân tích message. Lỗi hệ thống ngoài dự kiến như hết bộ nhớ có thể ném exception chuẩn; caller bắt `const std::exception&` ở ranh giới ứng dụng.

| SaveState | Ngữ nghĩa |
|---|---|
| UNCHANGED | Không cần lưu, hoặc lệnh bị từ chối; thao tác bị từ chối không thay đổi dữ liệu |
| SAVED | Đã áp dụng và lưu đủ phần đang dirty, dirty=false |
| PENDING | Đã áp dụng trong RAM, save lỗi, dirty=true; giữ backend và retrySave |

Command chấp nhận nhưng save lỗi vẫn trả `error=NONE`, `save=PENDING` và giữ ID/attempt vừa tạo. RetrySave chỉ ghi các bảng còn dirty từ RAM. Retry lỗi trả IO_ERROR/PENDING. Submit tìm kiếm vẫn trả kết quả nếu record history hoặc save lỗi, kèm cảnh báo trong message.

## Vòng đời

- `initialize(emptyFolder, adminName, password)` tạo admin ACTIVE đầu tiên và đủ 11 file khi backend chưa loaded và thư mục chưa tồn tại hoặc rỗng. Phiên bắt đầu ở trạng thái khách. Nếu save lỗi, backend vẫn loaded và dirty để retry.
- `load(folder, UnsavedPolicy::REJECT)`: nạp đủ 11 file vào staged, chỉ đổi kho khi hợp lệ. Lỗi giữ kho, dirty, tài khoản và phiên cũ. Thành công logout/hủy phiên và chuyển thư mục hoạt động.
- `load(folder, UnsavedPolicy::DISCARD)`: cho phép bỏ dirty **chỉ sau khi kho mới đã nạp thành công**.
- `loaded()`, `dirty()` báo trạng thái. `retrySave()` hoạt động cả sau logout và trả UNCHANGED khi kho đã sạch.
- `login(name,password)` tìm username theo lowercase ASCII và xác thực nguyên byte mật khẩu. Tài khoản LOCKED, username sai và mật khẩu sai nhận cùng lỗi credentials. Mọi lần login hủy tài khoản, review và quiz cũ. Đăng nhập đúng có thể trả SAVED hoặc PENDING nếu hash cần nâng cấp. Logout giữ lại thay đổi đã áp dụng trong RAM.
- `close()` từ chối dirty; thành công đưa về chưa loaded. `discardAndClose()` bỏ RAM có chủ ý.

Frontend phải gọi close trước khi hủy Application và xử lý kết quả. Destructor giải phóng bộ nhớ, không hiện dialog và không thể ngăn OS kết thúc tiến trình. Library không bảo vệ được khi caller bỏ qua close, crash hoặc tắt máy; không có journal/recovery trong phạm vi này.

ID dùng kiểu mạnh ResourceId, LevelId…; UI dùng isValid(string) hoặc bắt invalid_argument khi dựng ID từ input. Không tự sinh ID. Create trả ID; request update không có ID/type/score, ID đích là tham số riêng và Resource phải giữ loại cũ.

## API theo chức năng

Mọi query nghiệp vụ cần phiên ACTIVE. Mọi command quản trị kiểm ADMIN ở Application và manager.

| Nhóm | API thực tế | Quyền/kết quả |
|---|---|---|
| Tài khoản | currentAccount, accounts | AccountSummary chỉ id/username/role/status; accounts ADMIN |
| Quản trị user | createAccount, setAccountStatus, resetPassword | ADMIN; không xóa user; không khóa người hiện hành/admin ACTIVE cuối |
| Level | levels, createLevel, updateLevel, removeLevel | List theo order rồi số ID; CRUD ADMIN, kiểm Resource/Question/Attempt |
| Resource | create/updateVocabulary, create/updateGrammar, create/updateKanji, removeResource | ADMIN, request theo loại; ID chung; xóa dọn Favorite, chặn cả Question INACTIVE |
| Detail/search | resource, search, submitSearch, statistics | ResourceDetail là variant 3 entity. SearchRow theo giá trị có ID, term, reading nếu có, meaning và score |
| Cá nhân | addFavorite, removeFavorite, favorites, history, searchAgain, clearHistory | Chủ sở hữu luôn từ phiên; favorites trả ResourceDetail, history mới nhất trước |
| Review | startReview, review, revealReview, rateReview, stopReview | ReviewState đếm cả current; back optional chỉ có sau reveal |
| Alphabet | alphabet(script?,group?), create/update/removeAlphabet | List theo displayOrder rồi số ID; CRUD ADMIN |
| Question | questions(level?,status?), question, create/update/removeQuestion, setQuestionStatus | ADMIN, vì entity chứa đáp án; list/detail theo giá trị |
| Quiz | startQuiz, quiz, selectAnswer, submitQuiz, quizAnswers, retryWrong, stopQuiz, attempts, activeQuestionCount | Mỗi câu trong QuizState có prompt và options. QuizState giữ lựa chọn của chính actor theo cùng thứ tự. quizAnswers trả đáp án và lời giải theo thứ tự đề sau khi nộp |
| Gói | importPackage, exportPackage | ADMIN, gói 5 file học liệu, không phải backup hệ thống |

Các input struct và kiểu trả cụ thể nằm trong Application.h. Username ASCII 3–32 ký tự, bắt đầu bằng chữ, phần còn lại chữ/số/`_.-`. Tạo và reset dùng cùng quy tắc mật khẩu 1–128 byte, theo ký tự TXT và không có space/tab ở hai đầu. Hash lưu trên đĩa dùng quy tắc riêng trong data_format.md.

| API | Guest | Learner A | Learner B | Admin ACTIVE | LOCKED |
|---|---|---|---|---|---|
| initialize, load, loaded, dirty, close, discardAndClose, retrySave | Điều kiện vòng đời | Điều kiện vòng đời | Điều kiện vòng đời | Điều kiện vòng đời | Điều kiện vòng đời |
| login, logout | Xác thực, hủy phiên | Xác thực, hủy phiên | Xác thực, hủy phiên | Xác thực, hủy phiên | Login bị từ chối, logout an toàn |
| currentAccount, levels, resource, search, statistics, alphabet, activeQuestionCount | LOGIN_REQUIRED | Được gọi | Được gọi | Được gọi | LOGIN_REQUIRED |
| Favorite, history, submitSearch, searchAgain, review, quiz, attempts | LOGIN_REQUIRED | Chỉ của A | Chỉ của B | Chỉ của admin | LOGIN_REQUIRED |
| accounts, quản trị user, CRUD level/học liệu/alphabet/câu hỏi, questions, question, importPackage, exportPackage | LOGIN_REQUIRED | FORBIDDEN | FORBIDDEN | Được gọi, theo invariant | LOGIN_REQUIRED |

`stopReview` và `stopQuiz` là no-op khi không có phiên hiện hành. Quyền đọc/sửa file TXT trực tiếp do hệ điều hành quản lý.

### Tra cứu và lịch sử

- `search(options)` chỉ đọc. UI dùng đường này để mở hoặc refresh màn và đổi mode, loại, level, sort. Phân trang và scroll dùng snapshot hiện có.
- `submitSearch(options)` thêm đúng một history với query hợp lệ và không rỗng, kể cả khi không có kết quả. Khi chỉ history thay đổi, lượt lưu chỉ replace `search_history.txt`. Query rỗng liệt kê theo bộ lọc với cả hai mode. Query sai trả INVALID_INPUT trước khi ghi.
- Backend kiểm UTF-8, ký tự TXT và giới hạn 8192 byte trên đầu vào thô, rồi bỏ space, tab, U+00A0 và U+3000 ở hai đầu. History giữ cách viết sau trim. Tìm nội dung fold A–Z ASCII ở cả hai phía và giữ dấu, khoảng trắng bên trong và ký tự Nhật. Phạm vi đối chiếu là UTF-8 theo byte, với ID theo grammar EntityId.
- `searchAgain(id)` chỉ tìm mục của actor, rồi chạy như submit và trả options cùng rows theo giá trị. Schema cũ lưu mỗi query, nên replay dùng filter và sort mặc định. Nếu query là ID chuẩn đang tồn tại, replay chọn exact ID. Các trường hợp còn lại dùng mode nội dung.
- Điểm là 4 cho nguyên term, 3 cho tiền tố term, 2 cho chuỗi con term hoặc trường chuyên biệt, 1 cho nghĩa. Query có nhiều chữ Hán có thể trả từng Kanji tương ứng với điểm 3. Type và level kết hợp AND. Relevance theo điểm giảm, levelOrder tăng, ID số tăng. Năm sort còn lại dùng ID số tăng khi trùng tiêu chí chính.

Quiz chọn 1–50 câu ACTIVE, không lặp. Một engine tồn tại trong Application; constructor nhận seed tùy chọn để test. Phiên giữ bản sao: sửa/xóa/inactive Question không đổi đề đang làm. Submit kiểm Level snapshot còn tồn tại và JLPT; nếu mất/đổi category, không thêm attempt. Bỏ trống `'-'` tính sai. Submit một lần, không sửa đáp án sau nộp. RetryWrong dùng câu sai của snapshot, tạo phiên mới, không tạo phiên rỗng. Sau load/restart chỉ xem attempts tổng hợp, không bịa chi tiết bài cũ.

## Ví dụ

Giả định `require(Status)` kiểm ok và hiển thị PENDING, còn `take(Result<T>)` kiểm rồi lấy value. Consumer độc lập ở `examples/backend_example.cpp`.

```cpp
Application app;
require(app.load(testFolder));
require(app.login("learner", "demo456"));
SearchOptions options;
options.query = "学校";
auto preview = take(app.search(options));
auto rows = take(app.submitSearch(options));
require(app.addFavorite(rows.front().id));
require(app.startReview());
auto front = take(app.review());         // chưa có current->back
require(app.revealReview());
auto revealed = take(app.review());      // có back
require(app.rateReview(false));          // tự requeue
app.stopReview();
```

```cpp
require(app.startQuiz(levelId, 10));
auto quiz = take(app.quiz());
require(app.selectAnswer(0, 'B'));
auto submitted = app.submitQuiz();       // backend chấm và tạo một attempt
if (submitted.ok() && submitted.status.save == SaveState::PENDING) {
    auto saved = app.retrySave();        // giữ submitted.value, không submit lần hai
    // Nếu vẫn lỗi: giữ backend đang mở, hiển thị cảnh báo và cho retry tiếp.
}
auto explanations = take(app.quizAnswers());
require(app.retryWrong());               // chỉ khi còn câu sai
```

```cpp
require(app.login("admin", "demo123"));
auto level = take(app.createLevel({"JLPT", "N5", 5, ""}));
auto id = take(app.createVocabulary({level, "本", "ほん", "sách", PartOfSpeech::NOUN, ""}));
require(app.updateVocabulary(id, {level, "本", "ほん", "quyển sách", PartOfSpeech::NOUN, ""}));
require(app.exportPackage(separateFolder));
require(otherBackend.importPackage(separateFolder)); // kho khác chưa có ResourceId đó
```

UI phụ trách form, màn hình, render, input/focus và thông báo. Backend tự xử lý quyền, ID, validation, quan hệ xóa, tìm/sort, review queue, shuffle/chấm bài, staged import/xóa và TXT. UI không truy cập DataStore, chấm lại điểm, đọc lời giải trước bước cho phép hoặc ghép/parse file.

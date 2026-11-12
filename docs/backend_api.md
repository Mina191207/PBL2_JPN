# Hợp đồng tích hợp frontend

## Điểm vào và lỗi

API duy nhất là `Application` trong `include/Application.h`. Header DataStore/manager/FileStorage phục vụ implementation/test nội bộ, không phải API cạnh tranh. Không còn app.data(), app.users() hoặc app.service() trong API frontend.

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
| SAVED | Đã áp dụng và toàn bộ lượt save thành công, dirty=false |
| PENDING | Đã áp dụng trong RAM, save lỗi, dirty=true; giữ backend và retrySave |

Command chấp nhận nhưng save lỗi vẫn `error=NONE`, `save=PENDING`, trả ID/attempt vừa tạo. RetrySave lỗi trả IO_ERROR/PENDING. Retry chỉ ghi RAM, không chạy lại command. Search vẫn trả kết quả nếu record history hoặc save lỗi; message chứa cảnh báo. Không có rollback giả nếu thay file lỗi giữa chừng.

## Vòng đời

- `initialize(emptyFolder, adminName, demoPassword)`: chỉ khi chưa loaded và thư mục chưa tồn tại/rỗng. Tạo admin ACTIVE đầu tiên, không tự login. Nếu save lỗi vẫn giữ loaded/dirty để retry; không ghi đè dataset hiện hữu.
- `load(folder, UnsavedPolicy::REJECT)`: nạp đủ 11 file vào staged, chỉ đổi kho khi hợp lệ. Lỗi giữ kho, dirty, tài khoản và phiên cũ. Thành công logout/hủy phiên và chuyển thư mục hoạt động.
- `load(folder, UnsavedPolicy::DISCARD)`: cho phép bỏ dirty **chỉ sau khi kho mới đã nạp thành công**.
- `loaded()`, `dirty()` báo trạng thái; `retrySave()` hoạt động cả sau logout.
- `login(name,password)` không phân biệt hoa ASCII của username. Mọi lần login, kể cả thất bại, hủy tài khoản/review/quiz cũ. Logout không xóa các thay đổi RAM đã áp dụng.
- `close()` từ chối dirty; thành công đưa về chưa loaded. `discardAndClose()` bỏ RAM có chủ ý.

Frontend phải gọi close trước khi hủy Application và xử lý kết quả. Destructor giải phóng bộ nhớ, không hiện dialog và không thể ngăn OS kết thúc tiến trình. Library không bảo vệ được khi caller bỏ qua close, crash hoặc tắt máy; không có journal/recovery trong phạm vi này.

ID dùng kiểu mạnh ResourceId, LevelId…; UI dùng isValid(string) hoặc bắt invalid_argument khi dựng ID từ input. Không tự sinh ID. Create trả ID; request update không có ID/type/score, ID đích là tham số riêng và Resource phải giữ loại cũ.

## API theo chức năng

Mọi query nghiệp vụ cần login. Mọi command quản trị kiểm ADMIN ở Application và manager.

| Nhóm | API thực tế | Quyền/kết quả |
|---|---|---|
| Tài khoản | currentAccount, accounts | AccountSummary chỉ id/username/role/status; accounts ADMIN |
| Quản trị user | createAccount, setAccountStatus, resetPassword | ADMIN; không xóa user; không khóa người hiện hành/admin ACTIVE cuối |
| Level | levels, createLevel, updateLevel, removeLevel | List theo order rồi số ID; CRUD ADMIN, kiểm Resource/Question/Attempt |
| Resource | create/updateVocabulary, create/updateGrammar, create/updateKanji, removeResource | ADMIN, request theo loại; ID chung; xóa dọn Favorite, chặn cả Question INACTIVE |
| Detail/search | resource, search, statistics | ResourceDetail là variant 3 entity; SearchRow/ResourceStats theo giá trị |
| Cá nhân | addFavorite, removeFavorite, favorites, history, searchAgain, clearHistory | Chủ sở hữu luôn từ phiên; favorites trả ResourceDetail, history mới nhất trước |
| Review | startReview, review, revealReview, rateReview, stopReview | ReviewState đếm cả current; back optional chỉ có sau reveal |
| Alphabet | alphabet(script?,group?), create/update/removeAlphabet | List theo displayOrder rồi số ID; CRUD ADMIN |
| Question | questions(level?,status?), question, create/update/removeQuestion, setQuestionStatus | ADMIN, vì entity chứa đáp án; list/detail theo giá trị |
| Quiz | startQuiz, quiz, selectAnswer, submitQuiz, quizAnswers, retryWrong, stopQuiz, attempts | QuizState không chứa đáp án đúng/lời giải; quizAnswers chỉ sau nộp |
| Gói | importPackage, exportPackage | ADMIN, gói 5 file học liệu, không phải backup hệ thống |

Các input struct và kiểu trả cụ thể nằm trong Application.h. Username ASCII 3–32 ký tự, bắt đầu bằng chữ, phần còn lại chữ/số/`_.-`. Password demo không rỗng/toàn space-tab, tối đa 128 byte, không space/tab hai đầu và không phá TXT. Xem data_format.md cho validation còn lại.

Search dựng một matcher cho query không rỗng, lấy ranking tối đa 4/3/2/1. Filter type/level là AND. Query rỗng liệt kê theo filter kể cả exactId mode, không ghi history. Query hợp lệ không rỗng ghi đúng một history kể cả kết quả rỗng; query sai bị từ chối trước record. Search phân biệt hoa theo dữ liệu lưu, không normalize tiếng Nhật. SearchAgain lấy query của đúng user rồi tìm nội dung với filter/sort mặc định và ghi một lần mới: schema history không lưu mode/filter/sort cũ.

Quiz chọn 1–50 câu ACTIVE, không lặp. Một engine tồn tại trong Application; constructor nhận seed tùy chọn để test. Phiên giữ bản sao: sửa/xóa/inactive Question không đổi đề đang làm. Submit kiểm Level snapshot còn tồn tại và JLPT; nếu mất/đổi category, không thêm attempt. Bỏ trống `'-'` tính sai. Submit một lần, không sửa đáp án sau nộp. RetryWrong dùng câu sai của snapshot, tạo phiên mới, không tạo phiên rỗng. Sau load/restart chỉ xem attempts tổng hợp, không bịa chi tiết bài cũ.

## Ví dụ

Giả định `require(Status)` kiểm ok và hiển thị PENDING; `take(Result<T>)` kiểm rồi lấy value. Consumer build được đầy đủ ở UI/backend_example.cpp.

```cpp
Application app;
require(app.load(testFolder));
require(app.login("learner", "demo456"));
SearchOptions options;
options.query = "学校";
auto rows = take(app.search(options));
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

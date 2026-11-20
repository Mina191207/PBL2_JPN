# Học và bảo vệ backend

## Thứ tự đọc

1. `docs/data_format.md`, `domain/EntityId.h` và `src/domain/EntityId.cpp`: danh tính khác chuỗi hiển thị; uint64_t và kiểm overflow trước phép nhân.
2. `domain/resource.h`, `vocabulary.cpp`, `grammar.cpp`, `kanji.cpp`: constructor cha, protected copy, virtual destructor, override hành vi.
3. `dsa/LinkedList.h/.tpp`, `tests/test_algorithms.cpp`: sở hữu node, invariant, copy lỗi và copy-and-swap.
4. `KmpMatcher.cpp`, `MergeSort.tpp`: thuật toán độc lập nghiệp vụ, test expected nhỏ và kiểu không có default constructor.
5. `DataStore.cpp`, `ResourceManager.cpp`: chỉ mục vị trí, view đa hình và ranking/sort.
6. `UserManager.cpp`, `PersonalManagers.cpp`, `LevelManager.cpp`, `QuestionManager.cpp`: quyền, dữ liệu riêng và ràng buộc tham chiếu.
7. `ReviewSession.cpp`, `QuizSession.cpp`, `QuizManager.cpp`: state machine, snapshot và nộp đúng một lần.
8. `FileStorage.cpp`, `ApplicationService.cpp`: ghép header/detail, staged/swap và phân biệt RAM với đĩa.
9. `Application.h/.cpp`, `test_api.cpp`, `UI/backend_example.cpp`: API thực tế cho frontend, cảnh báo save và vòng đời.

## Kiến thức gắn với code thật

| Môn/kiến thức | File/hàm và điều cần tự giải thích |
|---|---|
| KT lập trình: phân rã, điều kiện, vòng lặp | FileStorage::readRows/split/integer; tại sao parse phải dùng hết chuỗi và giữ cột rỗng |
| Chuỗi và biên số | EntityId::isValid, idAfter; kiểm `(max-digit)/10` trước nhân và max trước +1 |
| Đệ quy/chia để trị | mergeSort: sort hai nửa và merge; log k tầng, mỗi tầng k phần tử |
| File/exception/RAII | FileStorage::writeRows kiểm open/write/flush/close/replace; lỗi không được nuốt |
| CTDL tuyến tính | vector chứa entity, DLL history và pending review; lợi ích theo thao tác cần dùng |
| Con trỏ/cấp phát | DLL::pushFront/pushBack/unlink: cấp node trước, nối prev/next, hủy đúng một lần |
| Deep copy/strong guarantee | DLL copy constructor catch-clear-rethrow và operator= dùng copy-and-swap |
| Bảng băm | DataStore::rebuildIndexes/findResource; collision, vị trí số và vector erase |
| Queue FIFO | ReviewSession::rate(false) pushBack, advance lấy front rồi popFront |
| KMP | constructor dựng lps; findFirst nhảy lps[j-1], tái dùng matcher giữa các trường |
| Stable sort | MergeSort.tpp lấy trái khi `!less(right,left)`, giữ thứ tự tương đương |
| OOP đóng gói/const | ID/type không có setter, Resource query const, entity trả snapshot |
| Kế thừa/đa hình | ResourceManager::search gọi matchScore qua Resource*, startReview gọi makeReviewCard |
| Association/composition/aggregation/dependency | Bảng ownership trong backend_architecture.md; giải thích theo vòng đời, không dựa riêng ký hiệu * |
| Template/callable/operator | DLL<T>, EntityId<prefix>, mergeSort<T,Compare>, forEach lambda, ==/!= và operator= tự nhiên |
| Điều phối | ApplicationService::removeResource/importPackage staged/swap; Application::Impl::change áp dụng một command rồi thử save |

## Ví dụ invariant nhỏ

DLL ban đầu `1 <-> 2 <-> 3`. eraseIf xóa 1 và 2 phải giữ `head=tail=3`, prev/next của 3 null, size=1. PopBack sau đó phải đưa cả head/tail về null. Test còn copy list có Item không default constructor; Item ném khi copy thứ hai để kiểm không rò node và assignment đích vẫn còn dữ liệu cũ.

KMP pattern `ababac`, text `ababababac`: kết quả vị trí byte 4. Khi gặp sai ký tự, lps giữ phần tiền tố đã biết thay vì quay text lại đầu. So với std::string::find chỉ dùng trong test để đối chiếu expected, nghiệp vụ dùng KMP tự cài.

Merge Sort dữ liệu `(2,0),(1,1),(2,2),(1,3)`, chỉ so khóa đầu: kết quả `(1,1),(1,3),(2,0),(2,2)`. Số thứ hai chứng minh stability, không chỉ chứng minh các khóa đã tăng.

Review một thẻ: remaining=1; rate trước reveal bị chặn; reveal rồi rate(false) vẫn remaining=1 và mặt sau lại ẩn; reveal/rate(true) đưa remaining=0. Quiz một câu bỏ trống: correct=0, percentage=0, submit lần hai không thêm attempt.

Chạy kiểm chứng mà không cần UI:

```powershell
ctest --test-dir build/debug -R algorithms --output-on-failure
ctest --test-dir build/debug -R "api|search|storage" --output-on-failure
.\build\debug\backend_example.exe
```

## Câu hỏi bảo vệ và hướng trả lời

1. **Vì sao không dùng DLL cho mọi bảng?** Vector phù hợp duyệt/copy/truy cập vị trí. DLL tự cài dùng thật nơi hai đầu/duyệt ngược có ích; không tạo thêm cấu trúc chỉ để kể tên.
2. **Có prev thì xóa theo ID O(1) không?** Chỉ unlink node đã biết O(1); tìm theo điều kiện vẫn O(n). Node private nên API không cho giữ pointer node lâu dài.
3. **Copy entity có ID mới không?** Không. Copy giữ danh tính để staged/snapshot; create API mới cấp max+1.
4. **Nếu copy T ném giữa constructor list?** Destructor list chưa chạy khi constructor thất bại; catch phải clear các node đã dựng rồi rethrow. Test alive đếm đủ object sau lỗi.
5. **Vì sao index lưu số vẫn rebuild?** Realloc giữ chỉ số nhưng erase dịch phần tử; rebuild làm Location khớp vector mới. Không dùng index để quảng cáo toàn bộ CRUD O(1).
6. **Đa hình thật ở đâu?** Mở vòng `item->matchScore` trong ResourceManager và `item->makeReviewCard` trong ApplicationService; ba override đọc trường khác nhau.
7. **Header/detail có phải association Resource–Vocabulary 1:1 không?** 1:1 là hợp đồng TXT. Trong RAM Vocabulary là Resource qua kế thừa; không có CatalogEntry thứ hai dễ lệch dữ liệu.
8. **Tại sao Application cấm copy/move?** Manager chứa tham chiếu tới DataStore; copy mặc định có thể trỏ sang kho object cũ. Impl tạo/hủy chung vòng đời.
9. **Giấu nút admin có đủ không?** Không; guard ở Application và kiểm lại trong manager. Test learner gọi trực tiếp mọi API quản trị.
10. **Save lỗi thì rollback chưa?** Chưa. RAM đã thay đổi, dirty=true, trả PENDING. Retry chỉ save. Replace nhiều TXT không phải transaction nguyên tử.
11. **Question bị xóa giữa quiz thì sao?** Nội dung vẫn là snapshot. Submit chỉ cần Level snapshot còn JLPT; không lưu QuestionId trong attempt nên xóa Question không làm mồ côi attempt.
12. **Sau restart luyện lại bài cũ được không?** Không có chi tiết đáp án trong schema, chỉ có summary. RetryWrong thuộc phiên kết quả hiện tại.
13. **Thêm manager cho mọi entity có giúp OOP không?** Không. Favorite/History/Attempt có luật riêng, không đủ CRUD máy móc; không tạo singleton/BaseManager.
14. **UI có thể bỏ qua reveal bằng review snapshot không?** Snapshot trước reveal không chứa back; rate cũng kiểm revealed ở domain. Học liệu chi tiết vẫn tra cứu được độc lập, đây là luồng tự học, không phải chống gian lận toàn hệ thống.

## Demo 5–7 phút

| Thời gian | Nội dung và bằng chứng |
|---|---|
| 0:00–0:45 | Giới thiệu thư viện độc lập UI, UML và hợp đồng 11 TXT |
| 0:45–1:45 | Consumer login/search/favorite; mở ranking và lời gọi virtual |
| 1:45–2:45 | Review trước/sau reveal, chưa nhớ requeue; chỉ node ownership trong DLL |
| 2:45–3:45 | Quiz snapshot, chọn đáp án, submit một lần, summary/lời giải |
| 3:45–4:45 | Test learner bị chặn; xóa Resource còn Question và level không mồ côi |
| 4:45–5:45 | Test save lỗi/retry không nhân đôi attempt, import lỗi không thêm một phần |
| 5:45–6:30 | KMP/LPS, Merge Sort stable, index rebuild; nêu giới hạn multi-file save/restart |

Hai thành viên đều nên tự chạy test, giải thích được một invariant, một lời gọi đa hình và một thao tác nhiều bảng, kể cả phần người kia viết.

# Học và bảo vệ backend

## Thứ tự đọc

1. Đọc `docs/data_format.md`, `include/pbl2/domain/EntityId.h` và `src/domain/EntityId.cpp` để theo dõi ID, uint64_t và kiểm overflow trước phép nhân.
2. Đọc `include/pbl2/domain/resource.h` rồi các override `makeReviewCard` trong `src/domain/`.
3. Đọc `src/dsa/LinkedList.h` cùng `LinkedList.tpp` và `tests/test_algorithms.cpp` để xem ownership của node, copy lỗi và copy-and-swap.
4. Theo dõi `src/search/ResourceSearch.cpp` và `src/algorithms/MergeSort.tpp` để xem scoring và sắp xếp ổn định. Test search đi qua API với mẫu lặp và UTF-8.
5. Xem `src/storage/DataStore.cpp` để theo dõi chỉ mục vị trí và duyệt đúng loại học liệu.
6. `UserManager.cpp`, `PersonalManagers.cpp`, `LevelManager.cpp`, `QuestionManager.cpp`: quyền, dữ liệu riêng và ràng buộc tham chiếu.
7. `ReviewSession.cpp`, `QuizSession.cpp`, `QuizManager.cpp`: state machine, snapshot và nộp đúng một lần.
8. `FileStorage.cpp`, `ApplicationService.cpp`: ghép header/detail, staged/swap và phân biệt RAM với đĩa.
9. Đọc `include/pbl2/Application.h`, `tests/test_api.cpp` và `examples/backend_example.cpp` để theo dõi hợp đồng, cảnh báo lưu và vòng đời.

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
| Tra cứu | Chuẩn bị query một lần, dùng vị trí từ `string_view::find` để xác định nguyên term, prefix hoặc substring |
| Stable sort | MergeSort.tpp lấy trái khi `!less(right,left)`, giữ thứ tự tương đương |
| OOP đóng gói/const | ID/type không có setter, Resource query const, entity trả snapshot |
| Kế thừa/đa hình | ApplicationService::startReview gọi `makeReviewCard` qua `Resource*`, từng loại dựng thẻ riêng |
| Association/composition/aggregation/dependency | Bảng ownership trong backend_architecture.md; giải thích theo vòng đời, không dựa riêng ký hiệu * |
| Template/callable/operator | DLL<T>, EntityId<prefix>, mergeSort<T,Compare>, forEach lambda, ==/!= và operator= tự nhiên |
| Điều phối | ApplicationService::removeResource/importPackage staged/swap; Application::Impl::change áp dụng một command rồi thử save |

## Ví dụ invariant nhỏ

DLL ban đầu `1 <-> 2 <-> 3`. eraseIf xóa 1 và 2 phải giữ `head=tail=3`, prev/next của 3 null, size=1. PopBack sau đó phải đưa cả head/tail về null. Test còn copy list có Item không default constructor; Item ném khi copy thứ hai để kiểm không rò node và assignment đích vẫn còn dữ liệu cũ.

Query `ababac` trong term `ababababac` khớp ở byte 4 và nhận điểm 2. `test_search` kiểm kết quả, thứ tự và điểm qua Application.

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
6. **Đa hình ở đâu?** ApplicationService gọi `resource->makeReviewCard()` khi tạo thẻ. Ba override dựng nội dung theo từng loại học liệu.
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
| 0:00–0:50 | Tạo fixture và mở `pbl2_app --data-dir` theo README. Vào bằng `admin` / `demo123`, tạo một tài khoản và học liệu, thử lọc role ở Accounts. |
| 0:50–2:15 | Đăng xuất, vào bằng `learner` / `demo456`. Dán `　学校 `, mở chi tiết, thêm yêu thích. Thử lọc level N4 rỗng, đổi về N5 và đổi sort. Xóa query để thử cuộn danh sách và mở một hàng khác. |
| 2:15–3:00 | Vào History và chạy lại query đã lưu. Kiểm hàng kết quả mở đúng chi tiết. |
| 3:00–4:10 | Bắt đầu Review, mở đáp án rồi đánh giá nhớ. Mở Quiz ở N5, trả lời sai và nộp, xem đáp án rồi chọn Retry wrong. |
| 4:10–5:20 | Đóng, mở lại cùng fixture, kiểm favorites, history và attempt. Vào bằng `partner` / `demo456` để thấy dữ liệu cá nhân tách riêng. |
| 5:20–6:30 | Chạy nhóm `api`, `storage` hoặc `workflow` từ CTest. Chỉ ra quyền của actor, partial replace/retry và chi phí Merge Sort trên sơ đồ. |

Hai thành viên đều nên tự chạy test, giải thích được một invariant, một lời gọi đa hình và một thao tác nhiều bảng, kể cả phần người kia viết.

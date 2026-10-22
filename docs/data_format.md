# DATA FORMAT CONTRACT — PBL2 Japanese Learning Resource Manager

> **Phiên bản schema:** `1.0.0`  
> **Trạng thái:** Chuẩn đích của Gate 02-A; chỉ coi là đã áp dụng sau khi dữ liệu, Domain và các bài kiểm thử được đồng bộ.  
> **Áp dụng cho:** Ứng dụng C++17, lưu trữ bằng TXT UTF-8, 8 file dữ liệu trong thư mục `database/`.  
> **Phạm vi:** Định dạng vật lý, ý nghĩa trường, quy tắc hợp lệ, ràng buộc liên file, nạp dữ liệu và tính tương thích. Không mô tả chi tiết thuật toán hoặc giao diện.

## 1. Mục tiêu và nguyên tắc thiết kế

1. **Một nguồn sự thật cho mỗi thông tin:** `learning_resource.txt` quản lý danh tính, loại học liệu và cấp độ; file chi tiết quản lý nội dung riêng của loại đó. Không lưu `title` lặp lại trong Catalog.
2. **Contract trước code:** parser, Domain, Repository, Manager và test cùng tuân thủ một tài liệu, không tự đặt quy tắc ở từng nơi.
3. **Load all-or-nothing:** dữ liệu chỉ được công bố cho ứng dụng khi tất cả file và quan hệ giữa chúng hợp lệ. Bất kỳ lỗi nghiêm trọng nào cũng hủy trạng thái nạp tạm và giữ nguyên trạng thái đang dùng.
4. **Không mất dữ liệu:** bảo toàn nội dung tiếng Nhật, tiếng Việt, dấu câu và dấu `;` trong các trường văn bản; không âm thầm sửa lỗi, bỏ bản ghi sai hoặc thay giá trị không hợp lệ bằng mặc định.
5. **Định dạng đơn giản nhưng chặt chẽ:** v1 dùng một dòng/một bản ghi, dấu `|` phân tách trường, chưa hỗ trợ escaping/quoting/multiline; đổi quy ước là thay đổi phiên bản schema.
6. **ID ổn định:** ID đại diện cho một thực thể, không được tự đổi trong thao tác update. ID `R...` là khóa chung của Catalog và toàn bộ học liệu chi tiết.
7. **Phạm vi loại học liệu v1:** chỉ gồm Vocabulary, Grammar, Kanji như các file hiện có. Bảng chữ cái, quiz và câu hỏi JLPT trong mô tả sản phẩm **chưa có schema trong 8 file này**; muốn lưu riêng phải lập contract và kế hoạch migration tương ứng, không âm thầm nhét vào một loại khác.

### 1.1. Danh mục 8 file — chuẩn đích

| File | Thứ tự trường trong một bản ghi | Số trường |
|---|---|---:|
| `learning_resource.txt` | `resource_id\|resource_type\|level_id` | 3 |
| `vocabulary.txt` | `id\|word\|reading\|meaning\|type\|example` | 6 |
| `grammar.txt` | `id\|pattern\|meaning\|structure\|use\|example\|note` | 7 |
| `kanji.txt` | `id\|character\|meaning\|stroke_count\|onyomi\|kunyomi\|examples` | 7 |
| `resource_levels.txt` | `id\|category\|name\|order\|description` | 5 |
| `user.txt` | `user_id\|username\|password\|role\|status` | 5 |
| `favorites.txt` | `favorite_id\|user_id\|resource_id` | 3 |
| `search_history.txt` | `history_id\|user_id\|query\|searched_at` | 4 |

**Quy tắc header:** file TXT **không có dòng header**; các chuỗi trên chỉ là mô tả schema, không phải một dòng dữ liệu hợp lệ. Tên file là cố định, phân biệt hoa/thường về mặt contract, kể cả khi một hệ điều hành cho phép truy cập không phân biệt.

## 2. Global Physical File Contract (áp dụng đồng nhất)

| Mã | Chủ đề | Quy tắc bắt buộc |
|---|---|---|
| PHY-01 | Encoding | UTF-8. Từ chối byte sequence sai, overlong encoding, UTF-8 biểu diễn surrogate và code point > `U+10FFFF`. Không áp dụng Unicode normalization ngầm. |
| PHY-02 | BOM | Cho phép **tối đa một** UTF-8 BOM `EF BB BF` tại byte offset 0 của file; bỏ BOM trước khi đọc dòng đầu. BOM xuất hiện ở vị trí khác là lỗi. |
| PHY-03 | Dòng | Mỗi bản ghi đúng một dòng vật lý. Chấp nhận LF (`\n`) và CRLF (`\r\n`); bỏ duy nhất `\r` đứng ngay trước `\n`. Từ chối mọi `\r` còn lại, kể cả `\r` ở cuối file không có `\n`. |
| PHY-04 | Delimiter | Byte ASCII `|` (`0x7C`) phân tách các trường. Không có escape, quote hoặc nối dòng ở v1. Dấu `|` bên trong dữ liệu bị cấm. |
| PHY-05 | Số trường | Chính xác như bảng tại §1.1, kể cả trường cuối rỗng. Split phải giữ các trường rỗng ở giữa/cuối (`A|B|` có 3 trường). |
| PHY-06 | Dòng trống | Chỉ bỏ qua dòng **0 byte nội dung** (sau khi xử lý newline, không tính BOM đầu file). Dòng chỉ có dấu cách/tab được xem là bản ghi lỗi, không tự bỏ qua. |
| PHY-07 | Khoảng trắng | Với trường văn bản thường: trim **đầu/cuối** ASCII Space `U+0020`, Tab `U+0009` và khoảng trắng toàn chiều rộng `U+3000`; không xóa khoảng trắng nằm giữa và không biến đổi cách viết/hoa thường. Các trường ID, enum, số và timestamp phải ở dạng chuẩn sẵn; nếu có khoảng trắng thừa thì từ chối. Trường `password` không trim, xem §3.6. |
| PHY-08 | Ký tự cấm | Cấm `|`, NUL, CR, LF, ký tự điều khiển C0 (ngoại trừ Tab chỉ được phép ở đầu/cuối trường văn bản để trim), `U+0085`, `U+2028`, `U+2029`; không được có BOM/`U+FEFF` trong trường. |
| PHY-09 | Dấu `;` | Là ký tự dữ liệu hợp lệ, **không** phải dấu tách trường ở tầng Storage. Không làm mất `xem;nhìn` hoặc `学生;学校;学ぶ`. |
| PHY-10 | Giới hạn | Tối đa **16.384 byte/dòng** (không tính CR/LF), **8.192 byte/trường** trước trim, **32 MiB/file**. Giới hạn tính theo byte; tuyệt đối không cắt `std::string` ở giới hạn byte để “sửa” bản ghi. Vượt giới hạn => lỗi. |
| PHY-11 | File thiếu/lỗi | Cả 8 file là thành phần bắt buộc của dataset: file thiếu/không đọc được => nạp thất bại. File tồn tại nhưng **0 byte** hợp lệ và tạo collection rỗng; các quan hệ còn lại vẫn phải được kiểm tra. |
| PHY-12 | Đường dẫn | Root `database/` do tầng ứng dụng truyền vào Loader (tham số cấu hình/CLI); không phụ thuộc working directory ngẫu nhiên, không hardcode đường dẫn tuyệt đối. |
| PHY-13 | Dữ liệu lạ | Không chấp nhận comment (`#`), header, trường bổ sung, token enum lạ hoặc định dạng của phiên bản khác. Không tự bỏ qua record hỏng. |
| PHY-14 | Ghi file | Writer (gate sau) phải xuất UTF-8 **không BOM**, newline `\n`, thứ tự trường đúng schema, token chuẩn. Encoder phải từ chối dữ liệu không thể biểu diễn theo v1; không tự thay thế bằng dấu `?`. |

**Lưu ý Unicode:** kiểm tra UTF-8 chỉ chứng minh chuỗi byte hợp lệ, **không** chứng minh đã chuẩn hóa NFC/NFKC, không chứng minh ngữ nghĩa tiếng Nhật, và `std::string::size()` là kích thước **byte**, không phải số chữ/hình tự. Với điều kiện bắt buộc không được toàn khoảng trắng, dùng tập whitespace đã nêu trong PHY-07; không dùng `std::isspace(char)` trực tiếp trên byte UTF-8.

**Giới hạn độ dài riêng:** bảng chi tiết bên dưới có thể quy định ngưỡng nhỏ hơn PHY-10; áp dụng ngưỡng chặt hơn. Các số 16 KiB/8 KiB/32 MiB là chính sách tài nguyên của v1, không phải thông số bắt buộc bởi giảng viên.

## 3. Field-Level & Domain Contracts

**Ký hiệu:** `Required` = sau kiểm tra/trim không được rỗng; `Optional` = được biểu diễn bằng chuỗi rỗng giữa hai dấu `|` hoặc ở cuối dòng; `FK` = khóa tham chiếu; `Unique` = duy nhất trong phạm vi ghi rõ. Cột *C++ type* mô tả kiểu sau khi parse, không phải kiểu vật lý của TXT (mọi trường ban đầu đều là UTF-8 text).

### 3.1. `learning_resource.txt` — Resource Catalog

**Schema:** `resource_id|resource_type|level_id`  
**Ví dụ:** `R001|VOCABULARY|L005`

| Trường | C++ type đề xuất | Bắt buộc | Quy tắc |
|---|---|---|---|
| `resource_id` | `std::string` | Yes | ID loại `R`, chuẩn §4; **Unique** trong Catalog; bất biến sau khi tạo. |
| `resource_type` | `ResourceType` (`enum class`) | Yes | Chính xác một trong `VOCABULARY`, `GRAMMAR`, `KANJI` (phân biệt hoa/thường). |
| `level_id` | `std::string` | Yes | ID loại `L`; **FK** sang `resource_levels.id`. |

**Invariant CAT:**
- `resource_id` là khóa toàn cục của **một** học liệu, dù thuộc loại nào; không có hai hàng Catalog cùng ID.
- Mỗi hàng Catalog có **đúng một** hàng Detail trong file tương ứng `resource_type`, và **không** có hàng của loại khác mang cùng ID.
- Mỗi hàng Detail phải có đúng một hàng Catalog; không tồn tại “học liệu mồ côi”.
- `level_id` phải tồn tại, không được tự tạo level giả khi không tìm thấy.
- **Không có trường `title`**: tên hiển thị suy ra từ `Vocabulary.word`, `Grammar.pattern` hoặc `Kanji.character`. Class `resource` đang chứa `title` trên GitHub phải được điều chỉnh ở bước đồng bộ Domain.

### 3.2. `vocabulary.txt` — Vocabulary Detail

**Schema:** `id|word|reading|meaning|type|example`  
**Ví dụ:** `R001|食べる|たべる|ăn|VERB|毎日朝ご飯を食べます。`

| Trường | C++ type đề xuất | Bắt buộc | Quy tắc |
|---|---|---|---|
| `id` | `std::string` | Yes | ID `R`, **Unique** trong `vocabulary.txt`; FK đến Catalog loại `VOCABULARY`. |
| `word` | `std::string` | Yes | Từ/cụm từ cần học; không toàn khoảng trắng; tối đa **512 byte** UTF-8. |
| `reading` | `std::string` | No | Cách đọc (thường kana); tối đa **512 byte**; không bắt buộc chữ kana vì không triển khai bộ phân loại Unicode/NLP. |
| `meaning` | `std::string` | Yes | Nghĩa; tối đa **4.096 byte**; được chứa dấu `;` như `xem;nhìn`. |
| `type` | `PartOfSpeech` (`enum class`) | Yes | Chỉ nhận đúng `NOUN`, `VERB`, `I_ADJ`, `NA_ADJ`, `ADV`, `OTHER`. |
| `example` | `std::string` | No | Ví dụ sử dụng; tối đa **8.192 byte** (còn bị giới hạn bởi tổng dòng). |

**Ánh xạ enum chuẩn:** `NOUN→Noun`, `VERB→Verb`, `I_ADJ→IAdjective`, `NA_ADJ→NaAdjective`, `ADV→Adverb`, `OTHER→Other`. Encode phải đảo ánh xạ chính xác; không lưu chuỗi `Verb`/`Noun` theo tên enum C++.

**Invariant VOC:** constructor của Domain bảo vệ `id`, `word`, `meaning` và enum hợp lệ; độ dài/định dạng TXT do Codec hoặc validator ở biên dữ liệu kiểm tra. **Không** đặt điều kiện `word` phải unique: cùng cách viết có thể là học liệu khác nếu nghiệp vụ cho phép; uniqueness dựa trên ID.

### 3.3. `grammar.txt` — Grammar Detail

**Schema:** `id|pattern|meaning|structure|use|example|note`  
**Ví dụ:** `R002|～ながら|vừa...vừa...|Vます bỏ ます + ながら|Hai hành động xảy ra đồng thời|音楽を聞きながら勉強します。|Hai hành động thường có cùng chủ thể`

| Trường | C++ type đề xuất | Bắt buộc | Quy tắc |
|---|---|---|---|
| `id` | `std::string` | Yes | ID `R`, Unique trong file, FK đến Catalog loại `GRAMMAR`. |
| `pattern` | `std::string` | Yes | Mẫu ngữ pháp; không toàn khoảng trắng; tối đa **512 byte**. |
| `meaning` | `std::string` | Yes | Ý nghĩa/cách diễn đạt; tối đa **4.096 byte**. |
| `structure` | `std::string` | No | Cấu trúc kết hợp; tối đa **4.096 byte**. |
| `use` | `std::string` | No | Mô tả ngữ cảnh/cách sử dụng; tối đa **4.096 byte**. |
| `example` | `std::string` | No | Câu ví dụ; tối đa **8.192 byte**. |
| `note` | `std::string` | No | Ghi chú; có thể rỗng ở cuối dòng; tối đa **4.096 byte**. |

**Invariant GRA:** luôn giữ đủ **7 trường**, kể cả khi `note` rỗng (`...|câu ví dụ|`). Không trộn `use` với `structure`: một bên mô tả cấu trúc, một bên mô tả cách dùng. Không buộc dữ liệu ví dụ phải tuân theo parser ngữ pháp tiếng Nhật — nằm ngoài phạm vi PBL2.

### 3.4. `kanji.txt` — Kanji Detail

**Schema:** `id|character|meaning|stroke_count|onyomi|kunyomi|examples`  
**Ví dụ:** `R003|学|học|8|ガク|まな.ぶ|学生;学校;学ぶ`

| Trường | C++ type đề xuất | Bắt buộc | Quy tắc |
|---|---|---|---|
| `id` | `std::string` | Yes | ID `R`, Unique trong file, FK đến Catalog loại `KANJI`. |
| `character` | `std::string` | Yes | **Đúng 1 Unicode scalar value** sau decode UTF-8; tối đa 4 byte. Giới hạn một scalar, không tuyên bố đã xác thực thuộc tập chữ Hán. |
| `meaning` | `std::string` | Yes | Nghĩa chữ Hán; tối đa **4.096 byte**. |
| `stroke_count` | `std::uint16_t` | Yes | TXT chứa số thập phân ASCII chuẩn; **1–255**, không dấu `+/-`, không số 0 đầu (`08` sai), không chữ/khoảng trắng. |
| `onyomi` | `std::string` | No | Âm On; tối đa **512 byte**. |
| `kunyomi` | `std::string` | No | Âm Kun; tối đa **512 byte**. |
| `examples` | `std::string` | No | Chuỗi ví dụ thô, tối đa **8.192 byte**. Dấu `;` được giữ nguyên; tầng Storage không tự chia thành danh sách. |

**Invariant KAN:** ít nhất **một** trong `onyomi` hoặc `kunyomi` không rỗng. `character` là một Unicode scalar, không đồng nghĩa một *grapheme cluster* (ví dụ kí tự có variation selector cần contract khác). `meaning` đã có trong TXT nhưng **thiếu trong `kanji.h` đã push**; kiểu `stroke_count` hiện là `string` cũng chưa đạt schema mục tiêu. Không âm thầm đảo vị trí các trường để “đọc tạm” file cũ.

### 3.5. `resource_levels.txt` — Level Catalog

**Schema:** `id|category|name|order|description`  
**Ví dụ:** `L005|JLPT|JLPT N5|5|Beginner`

| Trường | C++ type đề xuất | Bắt buộc | Quy tắc |
|---|---|---|---|
| `id` | `std::string` | Yes | ID loại `L`; Unique; có thể được nhiều Resource tham chiếu. |
| `category` | `std::string` | Yes | Mã nhóm cấp độ, ASCII uppercase theo mẫu `[A-Z][A-Z0-9_]{0,31}`; `JLPT`, `GENERAL` là các giá trị ví dụ. Không hardcode chỉ hai giá trị này. |
| `name` | `std::string` | Yes | Nhãn hiển thị, tối đa **256 byte**; không toàn khoảng trắng. |
| `order` | `std::uint16_t` | Yes | Số thứ tự **1–1000**, thập phân chuẩn không số 0 đầu. Số nhỏ xếp trước khi sort tăng dần. |
| `description` | `std::string` | No | Mô tả, tối đa **2.048 byte**. |

**Invariant LEV:** không có hai Level trùng `id`; cặp (`category`, `name`) không trùng chính xác byte sau trim. Có thể trùng `order` và phải có tie-breaker ổn định theo `id` khi sắp xếp. Với quy ước JLPT, `JLPT N1` đến `JLPT N5` tương ứng `order` 1 đến 5; `L005` chỉ là ID, **không** tự suy diễn cấp độ từ ID. `category = General` trong dữ liệu hiện tại phải đổi sang token chuẩn nếu sử dụng hợp đồng mới.

### 3.6. `user.txt` — Accounts

**Schema:** `user_id|username|password|role|status`  
**Ví dụ học thuật:** `U001|demo_admin|demo_only_password|ADMIN|ACTIVE`

| Trường | C++ type đề xuất | Bắt buộc | Quy tắc |
|---|---|---|---|
| `user_id` | `std::string` | Yes | ID loại `U`, Unique; bất biến. |
| `username` | `std::string` | Yes | ASCII 3–32 ký tự, mẫu `[A-Za-z][A-Za-z0-9_.-]{2,31}`. Unique **không phân biệt hoa/thường ASCII**; so khớp khi đăng nhập theo cùng quy tắc. |
| `password` | `std::string` | Yes | Demo plaintext (xem cảnh báo); 1–128 byte; so khớp chính xác; **không trim**, từ chối SP/TAB/U+3000 ở hai đầu để tránh lỗi nhập âm thầm. |
| `role` | `UserRole` (`enum class`) | Yes | Chỉ `ADMIN` hoặc `LEARNER`. |
| `status` | `UserStatus` (`enum class`) | Yes | Chỉ `ACTIVE` hoặc `LOCKED`. |

**Invariant USER:** account `LOCKED` vẫn có thể được tham chiếu bởi lịch sử/yêu thích đã lưu, nhưng **không thể đăng nhập**. Kiểm tra quyền thực hiện tại Service/Manager, không phải Parser. Hai username `Admin` và `admin` được xem là trùng theo contract.

**Giới hạn bảo mật bắt buộc ghi vào báo cáo:** đây là **mô phỏng học thuật** dựa trên TXT, `password` đang lưu plaintext, **không an toàn cho tài khoản thật hoặc production**. Không gọi `std::hash` là thuật toán băm mật khẩu an toàn, không tự viết thuật toán mã hóa. Các ví dụ/demo chỉ dùng tài khoản giả.

### 3.7. `favorites.txt` — Favorite Relation

**Schema:** `favorite_id|user_id|resource_id`  
**Ví dụ:** `F001|U002|R001`

| Trường | C++ type đề xuất | Bắt buộc | Quy tắc |
|---|---|---|---|
| `favorite_id` | `std::string` | Yes | ID loại `F`, Unique trong Favorite. |
| `user_id` | `std::string` | Yes | FK đến `user.user_id`, không cần `ACTIVE` khi load lịch sử quan hệ. |
| `resource_id` | `std::string` | Yes | FK đến `learning_resource.resource_id`. |

**Invariant FAV:** cặp (`user_id`, `resource_id`) **Unique** toàn file, dù `favorite_id` khác nhau; không thêm cùng một yêu thích hai lần. **Không** hạn chế favorite theo `LEARNER`, vì dữ liệu hiện có bao gồm liên kết của tài khoản `ADMIN`; nếu muốn đổi đây là business rule mới cần quyết định riêng. Khi xóa Resource sau này, các Favorite trỏ tới nó phải được xóa **trong cùng thay đổi trạng thái nhất quán** (xem §6).

### 3.8. `search_history.txt` — Search History

**Schema:** `history_id|user_id|query|searched_at`  
**Ví dụ mới:** `H001|U002|食べる|2026-09-14T14:30:00Z`

| Trường | C++ type đề xuất | Bắt buộc | Quy tắc |
|---|---|---|---|
| `history_id` | `std::string` | Yes | ID loại `H`, Unique. |
| `user_id` | `std::string` | Yes | FK đến `user.user_id`, bao gồm tài khoản `LOCKED` có dữ liệu cũ. |
| `query` | `std::string` | Yes | Nội dung truy vấn sau trim không rỗng, tối đa **1.024 byte**; giữ nguyên chữ Nhật/Việt, không normalize Unicode ngầm. |
| `searched_at` | `UtcTimestamp` (kiểu domain tự định nghĩa) | Yes | Chính xác **`YYYY-MM-DDTHH:MM:SSZ`** theo **UTC**; phải kiểm tra lịch Gregorian, ngày hợp lệ, giờ `00..23`, phút/giây `00..59`; không nhận leap second `:60` v1. |

**Invariant HIS:** thời điểm là thời gian UTC, không phụ thuộc múi giờ máy chạy; encode theo đúng định dạng canonical 20 ký tự ASCII. Thứ tự lịch sử có thể sort theo (`searched_at`, `history_id`) để giải quyết trùng giây. Query rỗng/không hợp lệ không được ghi vào lịch sử.

**Thay đổi schema cần migration:** dữ liệu đang push chứa timestamp **không có múi giờ** (`2026-09-14T21:30:00`). Không được tự suy ra UTC từ chuỗi đó. Nếu **xác nhận** các timestamp cũ được nhập theo giờ Việt Nam UTC+07:00, thì chuyển `21:30:00` thành `14:30:00Z` cùng ngày; nếu chưa xác nhận nguồn giờ, phải gắn lỗi migration cần giải quyết. Xem §8.

## 4. ID Contract — cấu trúc, cấp phát và phạm vi duy nhất

ID là **opaque identifier** đối với nghiệp vụ: ngoài lớp tạo/kiểm tra ID, không dùng phần số để suy ra loại học liệu, level hay ngày tạo. Tất cả ID phân biệt hoa/thường và phải ở dạng canonical.

**Quy tắc tổng quát:** `PREFIX` + số thập phân dương, **đệm tối thiểu 3 chữ số**. Không thêm số 0 thừa khi trị số từ 1000 trở lên. Thuật toán biểu diễn chuẩn là `PREFIX + zero_pad_to_at_least_3(decimal(n))`.

| Không gian ID | Prefix | Khoảng hợp lệ | Ví dụ hợp lệ | Ví dụ không hợp lệ |
|---|---|---:|---|---|
| Learning Resource | `R` | 1–1000 | `R001`, `R999`, `R1000`, `R1001` | `R000`, `R0001` |
| Resource Level | `L` | 1–999 | `L004`, `L005`, `L999` | `L000`, `L1000` |
| User | `U` | 1–9999 | `U001`, `U999`, `U1000` | `U01`, `U0001` |
| Favorite | `F` | 1–999999 | `F001`, `F1000` | `F000`, `F0001` |
| Search History | `H` | 1–999999 | `H001`, `H1000` | `H000`, `H0001` |

**Tính duy nhất:** `R` duy nhất trong Catalog và chỉ được tái xuất hiện *đúng vai trò tham chiếu hoặc Detail cùng thực thể*; `U`, `L`, `F`, `H` duy nhất trong tập thực thể của riêng chúng. ID `R005` ở `vocabulary.txt` và `learning_resource.txt` là **liên kết hợp lệ**, không phải hai Resource trùng nhau.

**Cấp phát ID khi có CRUD:** hệ thống phải giữ **high-water mark (giá trị lớn nhất từng cấp phát)** cho từng namespace trong metadata bền vững của cơ chế lưu snapshot ở Gate ghi dữ liệu. Không được đơn giản lấy `max(ID hiện còn) + 1` sau khi đã cho xóa vật lý, vì có thể tái sử dụng ID đã xóa. Tám file TXT hiện tại **không đủ thông tin để chứng minh “không bao giờ tái sử dụng ID” qua nhiều lần xóa/khởi động**; đây là điều kiện kỹ thuật cho Gate CRUD, không phải khả năng đã tồn tại. Khi vượt giới hạn namespace, từ chối cấp phát (`ID_EXHAUSTED`), không wrap hoặc tái sử dụng ID.

## 5. Cross-File Integrity & Validation

### 5.1. Quan hệ tham chiếu

```text
resource_levels.id <-------- learning_resource.level_id
                                   |
               +-------------------+-------------------+
               |                   |                   |
       VOCABULARY detail    GRAMMAR detail       KANJI detail
              (1:1)              (1:1)              (1:1)
                                   |
learning_resource.resource_id <-- favorites.resource_id
             ^
             |
user.user_id <-- favorites.user_id
     |
     +-------- search_history.user_id
```

**Cardinality:** mỗi Level có thể gắn 0..n Resource; một Catalog Resource có đúng 1 Detail; mỗi User có thể có 0..n Favorite và History; một Resource có thể được 0..n User yêu thích.

### 5.2. Bất biến kiểm tra toàn bộ dataset

1. Mỗi file đúng schema, UTF-8, giới hạn byte, token, kiểu dữ liệu và bắt buộc/rỗng.
2. Trong từng tập: các khóa chính Unique; với User còn Unique username theo ASCII case-fold; với Favorite còn Unique cặp (User, Resource); với Level còn Unique cặp (category, name).
3. Catalog chỉ chứa các loại `VOCABULARY`, `GRAMMAR`, `KANJI` và mọi level tham chiếu tồn tại.
4. Tập ID Catalog đúng bằng **hợp** các tập ID Vocabulary/Grammar/Kanji; ba tập Detail **đôi một rời nhau**.
5. Với mỗi `resource_id`, `resource_type` trong Catalog phải trùng file Detail chứa ID đó.
6. Mỗi Favorite tham chiếu một User và một Catalog Resource tồn tại, kể cả khi User đã `LOCKED`.
7. Mỗi History tham chiếu một User tồn tại; timestamp chuẩn UTC hợp lệ.
8. Dataset có thể hoàn toàn rỗng (**tám file tồn tại nhưng rỗng**); đây là trạng thái khởi tạo hợp lệ. Nhưng một Catalog có Resource và thiếu Detail/Level sẽ làm toàn bộ load thất bại.

### 5.3. Thứ tự kiểm tra có tính quyết định

1. Kiểm tra sự hiện diện và khả năng mở/đọc toàn bộ 8 file, kích thước file.
2. Đọc dữ liệu thành trạng thái **tạm**, kiểm tra BOM, newline, UTF-8, kích thước dòng, số trường và ký tự cấm.
3. Decode kiểu mạnh (`enum`, số, datetime), kiểm tra required, độ dài riêng và invariant Domain.
4. Kiểm tra duplicate/unique **nội bộ từng collection**.
5. Kiểm tra FK, quan hệ 1:1 Catalog–Detail và sự rời nhau giữa Detail.
6. Chỉ khi **mọi bước đạt** mới commit trạng thái tạm vào trạng thái chính. Không dùng dữ liệu đang parse để tạo Search Index sống.

Các bước nhằm quy trách nhiệm và tạo diagnostics có thể lặp lại, không đòi mỗi lần phải dừng ở lỗi đầu tiên; hệ thống có thể tổng hợp nhiều lỗi nếu vẫn bảo toàn dữ liệu tạm an toàn.

## 6. Failure Semantics, Diagnostics và Save Consistency

### 6.1. Load — all-or-nothing trong RAM

- `load(dataset_root)` nhận đường dẫn từ ứng dụng, tạo `Dataset`/collection tạm, không xóa hay ghi lên collection hiện tại trong khi load.
- Parser/Repository/Validator có thể trả lỗi hoặc ném exception theo API sau này, nhưng **bất kỳ lỗi nào cũng không làm thay đổi trạng thái đang sử dụng**; object tạm được giải phóng an toàn theo RAII.
- Các trường hợp file thiếu, file không đọc được, UTF-8 lỗi, field count sai, enum/số/timestamp sai, ID trùng, FK mồ côi và catalog/detail không khớp đều là **fatal load errors**.
- Không có cơ chế “skip bad row”, tự sửa token, tự tạo Level hoặc tự đổi ID.
- Lỗi cấp phát bộ nhớ/I/O phát sinh giữa load cũng không được để hệ thống ở trạng thái mới nửa chừng (strong exception guarantee ở ranh giới công bố state).

### 6.2. Diagnostic contract

Thông báo phải hỗ trợ ít nhất:

```text
Severity: ERROR
Code: FK_LEVEL_NOT_FOUND
File: database/learning_resource.txt
Line: 2
Field: level_id
Value: L004
Detail: level_id does not exist in resource_levels.txt
```

**Nhóm mã lỗi đề xuất cố định:**

| Nhóm | Mã tiêu biểu |
|---|---|
| I/O | `FILE_NOT_FOUND`, `FILE_OPEN_FAILED`, `FILE_READ_FAILED`, `FILE_TOO_LARGE` |
| Encoding/format | `INVALID_UTF8`, `INVALID_BOM`, `INVALID_NEWLINE`, `LINE_TOO_LONG`, `FIELD_COUNT_MISMATCH`, `FORBIDDEN_CHARACTER` |
| Value | `REQUIRED_FIELD_EMPTY`, `VALUE_TOO_LONG`, `INVALID_ID`, `INVALID_ENUM`, `INVALID_INTEGER`, `INTEGER_OUT_OF_RANGE`, `INVALID_TIMESTAMP`, `INVALID_USERNAME` |
| Uniqueness | `DUPLICATE_PRIMARY_ID`, `DUPLICATE_USERNAME`, `DUPLICATE_FAVORITE`, `DUPLICATE_LEVEL_NAME` |
| Integrity | `FK_LEVEL_NOT_FOUND`, `FK_USER_NOT_FOUND`, `FK_RESOURCE_NOT_FOUND`, `DETAIL_NOT_FOUND`, `ORPHAN_DETAIL`, `RESOURCE_TYPE_MISMATCH`, `DUPLICATE_DETAIL_ID` |
| State/write (gate sau) | `ID_EXHAUSTED`, `SNAPSHOT_WRITE_FAILED`, `SNAPSHOT_COMMIT_FAILED` |

Lỗi parse phải có `file` + `line` (1-based) + `field` nếu xác định được; lỗi liên file phải xác định **ít nhất** bản ghi nguồn và khóa gây lỗi. Không đưa plaintext `password` vào log; có thể hiển thị `<redacted>` cho các giá trị nhạy cảm. Nếu không xác định được dòng (ví dụ không mở được file) thì ghi `line: n/a`.

### 6.3. Ghi dữ liệu — quyết định kiến trúc, **chưa cài trong Gate 02 đọc**

- Việc thay một collection trong RAM **không có nghĩa** ghi nhiều TXT lên đĩa là nguyên tử.
- Khi có CRUD/persistence, phải ghi **toàn bộ dataset phiên bản mới** vào thư mục snapshot tạm, flush/đóng và kiểm tra, rồi chuyển **điểm tham chiếu phiên bản đang hoạt động** bằng bước commit duy nhất có quy trình recovery.
- Metadata snapshot sẽ chứa ít nhất `schema_version`, ID high-water marks và danh tính snapshot. Cách cập nhật con trỏ/manifest an toàn trên Windows cần được thiết kế và kiểm thử thực tế ở Gate ghi; **không được mặc định `rename` của nhiều file là transaction**.
- Quy tắc xóa được chốt: User bị `LOCKED` thay vì xóa vật lý; xóa Resource đồng thời loại toàn bộ Favorite tham chiếu, và loại Detail tương ứng trong cùng transaction. Search History chỉ tham chiếu User nên không có FK trực tiếp đến Resource.
- Search Index nếu có lưu ra TXT chỉ là dữ liệu dẫn xuất/cache, **không** là nguồn sự thật và có thể rebuild sau load hợp lệ.

## 7. Canonical Dataset Example — đủ 8 file, kiểm tra được integrity

Các khối bên dưới là **nội dung file**, không chứa header. Đây là **fixture mục tiêu** để test; không ngầm khẳng định data đã được sửa trên repository.

**`database/resource_levels.txt`**

```text
L004|JLPT|JLPT N4|4|Elementary
L005|JLPT|JLPT N5|5|Beginner
```

**`database/learning_resource.txt`**

```text
R001|VOCABULARY|L005
R002|GRAMMAR|L004
R003|KANJI|L005
R004|VOCABULARY|L005
R005|VOCABULARY|L005
R006|GRAMMAR|L004
R007|KANJI|L005
```

**`database/vocabulary.txt`**

```text
R001|食べる|たべる|ăn|VERB|毎日朝ご飯を食べます。
R004|学校|がっこう|trường học|NOUN|私は学校へ行きます。
R005|見る|みる|xem;nhìn|VERB|テレビを見ます。;空を見ます。
```

**`database/grammar.txt`**

```text
R002|～ながら|vừa...vừa...|Vます bỏ ます + ながら|Hai hành động xảy ra đồng thời|音楽を聞きながら勉強します。|Hai hành động thường có cùng chủ thể
R006|～たい|muốn làm...|Vます bỏ ます + たい|Biểu thị mong muốn của người nói|日本へ行きたいです。|
```

**`database/kanji.txt`**

```text
R003|学|học|8|ガク|まな.ぶ|学生;学校;学ぶ
R007|日|ngày;mặt trời|4|ニチ;ジツ|ひ;か|日本;日曜日
```

**`database/user.txt`** — chỉ dùng account giả trong demo:

```text
U001|admin|admin123|ADMIN|ACTIVE
U002|learner01|123456|LEARNER|ACTIVE
U003|learner02|123456|LEARNER|LOCKED
```

**`database/favorites.txt`**

```text
F001|U002|R001
F002|U002|R003
F003|U001|R001
```

**`database/search_history.txt`** — giả định đã **xác nhận** timestamp cũ được tạo tại Việt Nam (UTC+07:00):

```text
H001|U002|食べる|2026-09-14T14:30:00Z
H002|U002|học|2026-09-14T14:35:00Z
H003|U002|ながら|2026-09-14T14:40:00Z
```

**Kết quả dự kiến:** 2 Level, 7 Catalog Resource, 3 Vocabulary, 2 Grammar, 2 Kanji, 3 User, 3 Favorite, 3 History; **0** ID trùng, FK hỏng hoặc Detail sai loại.

## 8. Migration Map — khác biệt giữa repository hiện tại và schema đích

| File / Domain | Dữ liệu hoặc code đã quan sát trên nhánh `database` | Việc cần làm **trước** khi chạy Gate 02 loader |
|---|---|---|
| `learning_resource.txt` | 4 trường: `R001\|VOCABULARY\|食べる\|L005`, mới có R001–R003 | Bỏ cột title dư; đăng ký đúng R004–R007 (3 trường). |
| `vocabulary.txt` | Token `Verb`, `Noun` | Chuyển sang `VERB`, `NOUN`; giữ dấu `;` trong nội dung. |
| `resource_levels.txt` | Chỉ `L005\|General\|JLPT N5\|5\|Beginner` | Bổ sung L004, chuẩn hóa `category` theo token uppercase (ví dụ `JLPT`). |
| `kanji.txt` / `kanji.h` | TXT 7 trường nhưng Domain đã push thiếu `meaning` và giữ `stroke_count` là `string` | Đồng bộ Domain đủ 7 trường và kiểu số; bổ sung test. |
| `resource.h` | Có `title`, constructor khai báo không khớp số thuộc tính | Điều chỉnh theo Catalog ba trường; không giữ title dư hoặc constructor sai. |
| `level.h` | `order` là `int` nhưng constructor nhận toàn `string` | Chốt kiểu số và kiểm tra chữ số ở Codec; không phụ thuộc constructor cũ. |
| `search_history.txt` | Datetime dạng local không offset | Xác nhận timezone nguồn, chuyển sang UTC `...Z`; không tự gắn `Z` vào giờ cũ. |
| `src/main.cpp` | Bản GitHub đã quan sát khởi tạo `vocabulary` với ID rỗng | Thay bài chạy thử cũ bằng test hợp lệ, tách test khỏi app; không phải yêu cầu sửa ngay trong tài liệu này. |
| `vocabulary.cpp` | ID validator có thể chấp nhận giá trị vượt R1000 | Sửa theo §4, có test ranh giới `R000`, `R001`, `R999`, `R1000`, `R1001`. |

**Không sửa trực tiếp những file đầu vào cũ khi migration chưa được duyệt.** Commit schema/document trước; tạo bản backup/fixture tương thích; migration phải là thay đổi có review và test, không được làm bằng parser tự “đoán” version theo số cột.

## 9. Round-trip, Negative Tests và Exit Criteria cho A1

### 9.1. Bất biến encode/decode

- Với mọi object hợp lệ có thể biểu diễn trong v1: `decode(encode(object))` phải tạo object tương đương về giá trị dữ liệu có ý nghĩa; nếu encoder chuẩn hóa khoảng trắng, so sánh với dạng canonical đã quy định, không so sánh bytes của file đầu vào chưa chuẩn hóa.
- Với mọi record canonical hợp lệ: `encode(decode(record)) == record` (không bao gồm BOM hoặc ký tự xuống dòng).
- Không tự trim `password`, không đổi kana/kanji, không tách nghĩa theo `;`, không làm mất trường cuối rỗng.

### 9.2. Test matrix tối thiểu

| Test ID | Dữ liệu/tình huống | Kết quả mong đợi |
|---|---|---|
| `FMT-001` | 8 file fixture tại §7 | Load hợp lệ, đúng số object. |
| `FMT-002` | LF và CRLF, file có/không có BOM đầu file | Kết quả như nhau. |
| `FMT-003` | BOM nằm ở dòng 2, UTF-8 overlong/surrogate, byte NUL | Từ chối rõ mã lỗi và vị trí. |
| `FMT-004` | `R006\|...\|` (trường cuối rỗng) | Parse đúng đủ 7 trường Grammar. |
| `FMT-005` | Thiếu/thừa dấu `|`, dòng chỉ chứa khoảng trắng | Từ chối bản ghi. |
| `FMT-006` | `xem;nhìn`, `学生;学校;学ぶ`, tiếng Việt có dấu | Giữ nguyên sau encode/decode. |
| `FMT-007` | PartOfSpeech `Verb` hoặc `VERB ` | Từ chối; không tự sửa thành `VERB`. |
| `FMT-008` | ID `R000`, `R0001`, `R1001`, `R1000` | Ba ID đầu sai, `R1000` hợp lệ. |
| `FMT-009` | `stroke_count` `0`, `08`, `abc`, `256` | Đều bị từ chối. |
| `FMT-010` | Detail trùng ID, Detail khác loại với Catalog | Load thất bại; state cũ không đổi. |
| `FMT-011` | Thiếu `L004`; Favorite trỏ User/Resource không tồn tại | Load thất bại; thông báo đúng FK. |
| `FMT-012` | Hai favorite khác ID cùng (`U002`, `R001`) | Bị từ chối duplicate cặp. |
| `FMT-013` | Username `Admin` và `admin` trong cùng file | Bị từ chối duplicate username. |
| `FMT-014` | Ngày `2026-02-29T10:00:00Z`, không có `Z` | Bị từ chối; 2026 không phải năm nhuận. |
| `FMT-015` | File thiếu/file 0 byte/file vượt 32 MiB/dòng vượt 16 KiB | Hành vi đúng chính sách và diagnostics. |
| `FMT-016` | File hỏng được nạp khi state đang hợp lệ | Không thay đổi một phần state hiện tại. |
| `FMT-017` | `kanji.character` có hai Unicode scalar | Bị từ chối. |
| `FMT-018` | Mỗi trong 8 file chỉ có nội dung rỗng | Chấp nhận dataset hoàn toàn rỗng. |

### 9.3. Definition of Done (Gate 02-A)

- [ ] Đặc tả tám schema và thứ tự trường được duyệt.
- [ ] Quy tắc ID, giới hạn, enum và timestamp được thống nhất với nhóm.
- [ ] Chính sách UTF-8/BOM/CRLF/`|`/empty field/whitespace và file thiếu không mâu thuẫn.
- [ ] Các quan hệ FK và 1:1 được thể hiện và có negative test dự kiến.
- [ ] Đã xác nhận **timezone nguồn** của lịch sử cũ trước khi migration.
- [ ] Dữ liệu mẫu được đồng bộ từ §7 (hoặc migration có chứng cứ tương đương).
- [ ] Các khác biệt Domain/Database ở §8 được đưa vào danh sách sửa; không tuyên bố đã test nếu chưa chạy.
- [ ] Các quyết định mới được ghi vào `docs/decisions.md` và commit cùng schema.

## 10. Decisions Log — tóm tắt để đối chiếu `docs/decisions.md`

| ID | Quyết định | Ghi chú đánh đổi |
|---|---|---|
| `ADR-01` | Global immutable Resource ID, canonical R001–R1000 | Giới hạn tối đa 1000 ID vòng đời; cần high-water mark nếu xóa vật lý. |
| `ADR-02` | Catalog 3 trường + typed Detail | Không lặp title; cần toàn vẹn giữa nhiều file. |
| `ADR-03` | 1:1 Catalog–Detail; không mồ côi | Load có kiểm tra liên file. |
| `ADR-04` | Không chứa escape hoặc multiline ở schema v1 | Đơn giản, nhưng nội dung không được chứa `|` hoặc newline. |
| `ADR-05` | UTF-8 hợp lệ, trim có giới hạn, không Unicode normalization | Kiểm tra encoding nhưng không xử lý NLP/biến thể Unicode. |
| `ADR-06` | Đọc all-or-nothing, diagnostics có vị trí | Không có chế độ sửa lỗi tự động khi load. |
| `ADR-07` | Enum/token nghiêm ngặt, kiểu số có range | Dữ liệu legacy cần migration. |
| `ADR-08` | `searched_at` UTC ISO 8601 `Z` | Cần xác nhận timezone và đổi timestamp cũ. |
| `ADR-09` | Favorite unique pair; xóa Resource xử lý Favorite đồng transaction | CRUD đa file đòi cơ chế save snapshot. |
| `ADR-10` | Plaintext password chỉ cho demo học thuật | Không dùng account thật; chưa bảo mật production. |
| `ADR-11` | Storage format giới hạn 16 KiB/line và 32 MiB/file | Tránh input không kiểm soát; cần test biên. |
| `ADR-12` | ID high-water mark lưu trong snapshot metadata (Gate ghi) | Tám TXT hiện tại chưa thể đảm bảo không tái sử dụng ID khi xóa. |

---

**Ranh giới Gate:** tài liệu này là **hợp đồng mục tiêu**. `A1 hoàn thành` nghĩa là kiến trúc/schema được duyệt và các khác biệt được ghi nhận; `Gate 02 hoàn thành` chỉ khi code, dữ liệu đã migration, automated tests và validation liên file thực sự đạt. Không coi trạng thái đã push hiện tại là phù hợp với toàn bộ contract này.

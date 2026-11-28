# Hợp đồng TXT — backend 3.0

Hợp đồng gồm 11 file. Phiên bản V2 áp dụng riêng cho bảng tài khoản.

## 11 file bắt buộc

Mỗi dòng là một bản ghi, phân cách bằng `|` và giữ cột rỗng cuối. Dòng đầu của `user.txt` là marker `PBL2_USERS_V2`. Các bảng còn lại bắt đầu ngay bằng bản ghi.

| File | Thứ tự trường | Cột |
|---|---|---:|
| learning_resource.txt | resource_id, resource_type, level_id | 3 |
| vocabulary.txt | id, word, reading, meaning, type, example | 6 |
| grammar.txt | id, pattern, meaning, structure, use, example, note | 7 |
| kanji.txt | id, character, meaning, stroke_count, onyomi, kunyomi, examples | 7 |
| resource_levels.txt | id, category, name, order, description | 5 |
| user.txt | user_id, username, password_hash, role, status | 5 |
| favorites.txt | favorite_id, user_id, resource_id | 3 |
| search_history.txt | history_id, user_id, query, searched_at | 4 |
| alphabet.txt | alphabet_id, script, glyph, romaji, group_name, display_order, note | 7 |
| question.txt | question_id, level_id, resource_id, prompt, option_a, option_b, option_c, option_d, correct_option, explanation, status | 11 |
| quiz_attempt.txt | attempt_id, user_id, level_id, total_questions, correct_answers, submitted_at | 6 |

UTF-8 hợp lệ; không normalize hoặc cắt chuỗi Nhật theo byte. Chấp nhận một BOM ở đầu, LF/CRLF, dòng cuối không newline; CR đơn không thuộc CRLF bị từ chối. Writer xuất UTF-8 không BOM, LF. Cấm `|`, CR, LF, NUL trong trường, không escape/quote/multiline. Dấu `;` là nội dung. Chỉ dòng 0 byte được bỏ qua; dòng toàn khoảng trắng vẫn phải parse và sẽ lỗi.

Giới hạn chung reader/domain/writer: 8192 byte/trường, 16384 byte/dòng không tính newline, 32 MiB/file. Domain kiểm field và tổng record trước khi nhận CRUD; writer kiểm lại tổng file trước mở/thay file đích. Văn bản giữ nguyên khoảng trắng; required không được rỗng/toàn space-tab ASCII. Riêng options so trùng sau trim space/tab; không normalize Unicode. Password không trim. Số/ID/enum/timestamp không nhận khoảng trắng dư. Category Level đổi a-z ASCII thành A-Z.

## Miền dữ liệu

- ID: prefix R/U/L/F/H/A/Q/T và số dương uint64_t, tối thiểu 3 chữ số, không thêm 0 thừa từ 1000. R999, R1000, R1000000 hợp lệ; R000, R0001, R18446744073709551616 sai. Cấp max hiện có +1, Resource dùng max chung ba loại. Xóa ID lớn nhất có thể làm số đó được tái dùng; không có metadata/high-water mark mới.
- Số nguyên parse hết chuỗi, không dấu hoặc 0 thừa. Level.order và displayOrder trong 1..INT_MAX; strokeCount 1..255; quiz total 1..50, correct 0..total.
- ResourceType: VOCABULARY/GRAMMAR/KANJI. PartOfSpeech: NOUN/VERB/I_ADJ/NA_ADJ/ADV/OTHER. Role ADMIN/LEARNER; user status ACTIVE/LOCKED; question status ACTIVE/INACTIVE; script HIRAGANA/KATAKANA. Không nhận token lạ hoặc enum cast sai trong RAM.
- Vocabulary bắt buộc word/meaning; Grammar pattern/meaning; Kanji character/meaning và ít nhất một On/Kun. Không kiểm một chữ Nhật bằng string.size()==1.
- Level category/name bắt buộc, cặp category/name duy nhất. Question/Attempt chỉ tham chiếu Level category JLPT.
- User có username ASCII 3–32 ký tự, bắt đầu bằng chữ, còn lại là chữ, số hoặc `_.-`, duy nhất sau lowercase ASCII. Mật khẩu nhập khi tạo/reset dài 1–128 byte, tuân thủ giới hạn ký tự TXT và không có space/tab ở hai đầu. Backend xác thực đúng các byte đầu vào.
- Alphabet glyph/romaji/group_name bắt buộc, unique cặp script/glyph.
- Question prompt và bốn options bắt buộc, options không trùng sau trim; đáp án A–D. ResourceId rỗng nghĩa là không liên kết; nếu có phải tồn tại và cùng level.
- History query không rỗng. Timestamp History/Attempt là UTC Gregorian `YYYY-MM-DDTHH:MM:SSZ`, kiểm năm/ngày nhuận/giờ 0–23/phút giây 0–59. Không tự gắn Z vào timestamp cũ không rõ timezone.

## Quan hệ và lưu

`password_hash` là chuỗi mã hóa Argon2id v1.3 của libsodium, chứa bộ tham số, salt 16 byte và hash 32 byte. Tham số ghi mới là `m=65536,t=2,p=1`. Reader hỗ trợ `m=19456..65536` KiB với `t=2,p=1` và kiểm định dạng trước khi verify. Sau đăng nhập đúng, backend kiểm need-to-rehash và nâng hash cũ nếu cần. API tài khoản chỉ trả ID, username, role và status.

V1 thiếu marker. Load yêu cầu chạy `pbl2_migrate_users`. Chỉ công cụ migrate giải nghĩa trường thứ ba ở V1 là mật khẩu gốc, kể cả khi giá trị bắt đầu bằng tiền tố Argon2id. Công cụ bảo toàn thứ tự bản ghi, ID, username, role và status. V2 hợp lệ được giữ nguyên bytes khi chạy lại. Công cụ ghi một file tạm tạo độc quyền cùng thư mục, với mode 0600 trên Linux và ACL giới hạn cho chủ file, quản trị viên máy và SYSTEM trên Windows. Sau flush/close, công cụ thay `user.txt` bằng rename/replace của hệ điều hành. Nếu bị ngắt trước replace, nguồn vẫn ở V1 và lần chạy sau dùng tên tạm mới. File tạm của lần bị ngắt chỉ chứa bảng V2.

Mỗi header Resource có đúng một detail đúng loại, không header/detail mồ côi hoặc ID trùng giữa loại. LevelId phải tồn tại. Favorite có User/Resource tồn tại và cặp duy nhất. History/Attempt có User tồn tại. Kho hoạt động phải có ít nhất một admin ACTIVE; file collection có thể rỗng khi các quan hệ còn lại hợp lệ. Bootstrap kho mới bằng Application.initialize; load 11 file hoàn toàn rỗng bị từ chối vì thiếu admin.

Load staged rồi swap khi toàn bộ hợp lệ. Lỗi parse có file, dòng và nguyên nhân. Manager kiểm quan hệ trước khi nhận mutation và đánh dấu bảng bị ảnh hưởng. Bootstrap đánh dấu đủ 11 bảng. Writer serialize các bảng dirty, ghi và đóng file tạm trước khi replace từng file. Search chỉ đọc ghi 0 file. Submit từ trạng thái sạch chỉ ghi history. Tạo, reset, đổi trạng thái và rehash từ trạng thái sạch chỉ ghi user. Lượt ghi nhiều bảng có thể để lại phiên bản cũ và mới xen kẽ nếu replace lỗi. Retry chỉ ghi lại các bảng chưa replace thành công và không chạy lại command.

Gói học liệu gồm learning_resource, vocabulary, grammar, kanji và resource_levels, với đủ Level cho mọi Resource. Import chỉ thêm mới và giữ ID. ResourceId trùng bị từ chối. LevelId trùng chỉ dùng lại khi toàn bộ nội dung khớp. Kiểm cả gói trước swap. Export xuất toàn bộ levels và từ chối thư mục dữ liệu hoạt động kể cả alias tương đương. File tạm có sẵn trong thư mục gói làm export báo lỗi để tránh ghi qua liên kết. Gói chỉ chứa học liệu.

Ví dụ giữ trường cuối rỗng:

```text
learning_resource.txt: R001|VOCABULARY|L001
vocabulary.txt: R001|学校|がっこう|trường;học|NOUN|
resource_levels.txt: L001|JLPT|N5|5|
question.txt: Q001|L001||意味は何ですか|a|b|c|d|A||ACTIVE
```

Tên trước dấu `: ` chú thích tên file. Fixture đủ 11 file được dựng ở `tests/support/Fixture.h`. Round-trip và ca lỗi nằm ở `tests/test_storage.cpp`.

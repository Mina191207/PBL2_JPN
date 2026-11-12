# Hợp đồng TXT — backend 3.0

Tài liệu này thay thế hợp đồng Gate 02 cũ còn nói 8 file, ID giới hạn, high-water mark và snapshot engine. Schema cột giữ nguyên thiết kế 3.0; không migration hoặc sửa dữ liệu thật trong lần refactor này.

## 11 file bắt buộc

Không header/comment. Mỗi dòng một bản ghi, phân cách `|`, giữ cột rỗng cuối.

| File | Thứ tự trường | Cột |
|---|---|---:|
| learning_resource.txt | resource_id, resource_type, level_id | 3 |
| vocabulary.txt | id, word, reading, meaning, type, example | 6 |
| grammar.txt | id, pattern, meaning, structure, use, example, note | 7 |
| kanji.txt | id, character, meaning, stroke_count, onyomi, kunyomi, examples | 7 |
| resource_levels.txt | id, category, name, order, description | 5 |
| user.txt | user_id, username, password, role, status | 5 |
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
- User: username ASCII 3–32 ký tự, bắt đầu bằng chữ, còn lại chữ/số/`_.-`; unique khi lowercase ASCII. Password bắt buộc, <=128 byte, không space/tab hai đầu. Chỉ tài khoản demo cục bộ, plaintext theo schema; không trả User/password qua API frontend.
- Alphabet glyph/romaji/group_name bắt buộc, unique cặp script/glyph.
- Question prompt và bốn options bắt buộc, options không trùng sau trim; đáp án A–D. ResourceId rỗng nghĩa là không liên kết; nếu có phải tồn tại và cùng level.
- History query không rỗng. Timestamp History/Attempt là UTC Gregorian `YYYY-MM-DDTHH:MM:SSZ`, kiểm năm/ngày nhuận/giờ 0–23/phút giây 0–59. Không tự gắn Z vào timestamp cũ không rõ timezone.

## Quan hệ và lưu

Mỗi header Resource có đúng một detail đúng loại, không header/detail mồ côi hoặc ID trùng giữa loại. LevelId phải tồn tại. Favorite có User/Resource tồn tại và cặp duy nhất. History/Attempt có User tồn tại. Kho hoạt động phải có ít nhất một admin ACTIVE; file collection có thể rỗng khi các quan hệ còn lại hợp lệ. Bootstrap kho mới bằng Application.initialize; load 11 file hoàn toàn rỗng bị từ chối vì thiếu admin.

Load staged rồi swap chỉ khi toàn bộ hợp lệ. Lỗi parse có file/dòng/nguyên nhân, không skip bản ghi hỏng hoặc tự sửa ID. Writer rebuild/kiểm quan hệ trên bản copy trước serialization; ghi đủ .tmp, kiểm mở/ghi/flush/close, rồi replace từng file. Lỗi giữa replace có thể làm đĩa pha trạng thái cũ/mới; RAM còn dirty để retry toàn bộ. Không có atomic commit 11 file, journal hoặc khóa nhiều tiến trình.

Gói học liệu gồm đúng năm tên file đầu liên quan Resource và Level: learning_resource, vocabulary, grammar, kanji, resource_levels. Gói không cần user/admin nhưng phải đủ Level cho Resource. Import chỉ thêm mới, không đổi ID: ResourceId trùng từ chối; LevelId trùng cùng toàn bộ nội dung dùng lại, khác nội dung từ chối. Kiểm cả gói trước swap. Export chứa đủ Level được tham chiếu (hiện xuất toàn bộ levels); cấm ghi vào thư mục dữ liệu hoạt động kể cả alias tương đương. Gói không phải backup toàn hệ thống.

Ví dụ giữ trường cuối rỗng:

```text
learning_resource.txt: R001|VOCABULARY|L001
vocabulary.txt: R001|学校|がっこう|trường;học|NOUN|
resource_levels.txt: L001|JLPT|N5|5|
question.txt: Q001|L001||意味は何ですか|a|b|c|d|A||ACTIVE
```

Tên trước dấu `: ` chỉ chú thích file, không nằm trong nội dung TXT. Fixture đủ 11 file được dựng ở tests/Fixture.h; round-trip và negative tests ở tests/test_storage.cpp.

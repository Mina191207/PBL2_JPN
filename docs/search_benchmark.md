# Benchmark tra cứu bước ②

Hai bản chạy cùng harness `tests/search_benchmark.cpp`, GCC 15.2.0 MSYS2 UCRT64, CMake Release trên Intel Core i5-12450HX. Bản trước là checkout ngay sau bước ①. Bản sau là bước ②. Mỗi cỡ có 1.000 hoặc 10.000 học liệu, hai level và `std::mt19937` seed 42. Mỗi query/mode có 5 lượt warm-up và 32 mẫu. p95 lấy theo nearest rank.

Query chung cho hai bản gồm `学校`, `not-present`, `prefix-12`, `学校学`, exact ID `R001` và query rỗng. Hồi quy whitespace và thứ tự ID/score nằm ở `tests/test_search.cpp`.

- **Chỉ đọc** gọi `ResourceManager::search` trên cả hai bản để cô lập scan, so khớp, sort và dựng snapshot theo giá trị. Baseline `Application::search` tự lưu khi query có nội dung.
- **Submit** gọi API công khai. Harness dùng `Application::search` ở bản trước và `Application::submitSearch` ở bản sau. Lịch sử được xóa ngoài vùng đo trước từng nhóm query. Mỗi mẫu bao gồm lượt lưu đồng bộ nếu có.
- **File thay** đo ngoài vùng tính giờ bằng hard link giữ định danh của các file TXT fixture. So `filesystem::equivalent` phát hiện replace cả khi nội dung không đổi. Fixture nằm trong thư mục tạm. Phần render và nạp atlas của Raylib chưa được đo riêng.

## Kết quả

Thời gian tính bằng µs. Số file là số file TXT đích thực sự bị replace trong một lượt từ trạng thái sạch.

| Học liệu | Query | Mode | Median trước | p95 trước | Median sau | p95 sau | File thay trước → sau |
|---:|---|---|---:|---:|---:|---:|---:|
| 1000 | Hit | Chỉ đọc | 43.20 | 46.70 | 24.95 | 27.50 | 0 → 0 |
| 1000 | Hit | Submit | 9956.25 | 14885.90 | 1057.50 | 1349.40 | 11 → 1 |
| 1000 | Miss | Chỉ đọc | 68.90 | 94.10 | 38.10 | 41.30 | 0 → 0 |
| 1000 | Miss | Submit | 10691.65 | 13602.70 | 964.00 | 1196.70 | 11 → 1 |
| 1000 | Prefix | Chỉ đọc | 61.15 | 73.10 | 48.10 | 50.90 | 0 → 0 |
| 1000 | Prefix | Submit | 9900.30 | 10911.70 | 914.25 | 1150.20 | 11 → 1 |
| 1000 | Kanji | Chỉ đọc | 46.20 | 47.40 | 19.20 | 21.30 | 0 → 0 |
| 1000 | Kanji | Submit | 10840.55 | 12423.20 | 934.65 | 1198.90 | 11 → 1 |
| 1000 | ID | Chỉ đọc | 0.10 | 0.20 | 0.10 | 0.20 | 0 → 0 |
| 1000 | ID | Submit | 10002.80 | 13401.40 | 857.10 | 1518.20 | 11 → 1 |
| 1000 | Rỗng | Chỉ đọc | 672.40 | 725.50 | 770.30 | 951.40 | 0 → 0 |
| 1000 | Rỗng | Submit | 708.85 | 840.80 | 789.50 | 1063.80 | 0 → 0 |
| 10000 | Hit | Chỉ đọc | 374.00 | 420.30 | 317.35 | 464.20 | 0 → 0 |
| 10000 | Hit | Submit | 82519.45 | 155358.70 | 1232.95 | 1551.90 | 11 → 1 |
| 10000 | Miss | Chỉ đọc | 1804.80 | 1926.30 | 548.70 | 601.10 | 0 → 0 |
| 10000 | Miss | Submit | 81156.80 | 90730.50 | 1509.10 | 1795.10 | 11 → 1 |
| 10000 | Prefix | Chỉ đọc | 2067.70 | 2215.30 | 743.05 | 1068.70 | 0 → 0 |
| 10000 | Prefix | Submit | 78652.25 | 126627.30 | 1729.70 | 2062.70 | 11 → 1 |
| 10000 | Kanji | Chỉ đọc | 1431.85 | 4125.20 | 185.90 | 200.50 | 0 → 0 |
| 10000 | Kanji | Submit | 81916.10 | 128738.60 | 1191.90 | 1486.50 | 11 → 1 |
| 10000 | ID | Chỉ đọc | 0.30 | 0.30 | 0.10 | 0.20 | 0 → 0 |
| 10000 | ID | Submit | 78632.95 | 145873.40 | 861.50 | 1058.20 | 11 → 1 |
| 10000 | Rỗng | Chỉ đọc | 30060.30 | 40103.10 | 10536.90 | 14715.00 | 0 → 0 |
| 10000 | Rỗng | Submit | 29717.30 | 39187.40 | 10545.15 | 11398.50 | 0 → 0 |

Median chỉ đọc giảm cho các truy vấn nội dung trong fixture này. Browse rỗng ở cỡ 1.000 tăng từ 672,40 lên 770,30 µs. p95 của hit chỉ đọc ở cỡ 10.000 tăng từ 420,30 lên 464,20 µs. Số đo mô tả workload này. Lượt submit hợp lệ có nội dung thay 1 file ở cả hai cỡ.

## Chạy lại

Dùng compiler UCRT64 và libsodium như hướng dẫn build trong README. Tùy chọn tạo benchmark executable độc lập với Raylib và CTest. Dùng cùng harness ở checkout bước ① hoặc dùng executable baseline còn trong thư mục tạm của lượt đo.

```powershell
$bench = Join-Path ([IO.Path]::GetTempPath()) 'pbl2-search-release'
cmake -S . -B $bench -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release -DPBL2_BUILD_SEARCH_BENCHMARK=ON
cmake --build $bench --target search_benchmark -j 6
& (Join-Path $bench 'search_benchmark.exe') | Set-Content -Encoding ascii (Join-Path $bench 'results.csv')
```

Ở Linux, dùng cùng tùy chọn CMake Release và chạy `search_benchmark` từ thư mục build tạm. Harness tự tạo và dọn fixture.


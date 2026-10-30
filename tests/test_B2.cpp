
#include "storage/FileStorage.h"

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <cstdio>

// Thong bao ket qua mot dieu kien
bool check(bool condition, const std::string& name) {
    if (condition) {
        std::cout << "[PASS] " << name << '\n';
    } else {
        std::cerr << "[FAIL] " << name << '\n';
    }
    return condition;
}

int main() {
    // TEST 1: Parse mot dong hop le
    vocabulary vocab = storage::parseVocabularyLine(
        "R001|食べる|たべる|ăn|VERB|毎日朝ご飯を食べます。"
    );

    if (!check(
        vocab.getId().getId() == "R001" &&
        vocab.getWord() == "食べる" &&
        vocab.getMeaning() == "ăn",
        "Parse Vocabulary hop le"
    )) return 1;

    // TEST 2: Tu loai sai
    bool rejected = false;

    try {
        storage::parseVocabularyLine(
            "R008|見る|みる|xem|INVALID|Vi du"
        );
    }
    catch (const std::invalid_argument&) {
        rejected = true;
    }

    if (!check(rejected, "Phat hien tu loai sai"))
        return 1;

    // TEST 3: Tao file mau co BOM va CRLF
    const std::string path = "b2_test_data.txt";

    {
        std::ofstream file(path, std::ios::binary);
        if (!check(bool(file), "Tao file test"))
            return 1;

        file << "\xEF\xBB\xBF";
        file << "R001|食べる|たべる|ăn|VERB|Ví dụ 1\r\n";
        file << "R005|見る|みる|xem;nhìn|VERB|Ví dụ 2\r\n";
    }

    LinkedList<vocabulary> words;
    std::string error;

    bool loaded = storage::loadVocabularyFile(
        path, words, error
    );

    if (!loaded)
        std::cerr << error << '\n';

    if (!check(loaded, "Doc file BOM va CRLF"))
        return 1;

    if (!check(words.size() == 2, "So ban ghi = 2"))
        return 1;

    if (!check(
        words.at(1).getMeaning() == "xem;nhìn",
        "Giu nguyen dau cham phay"
    )) return 1;

    // TEST 4: File sai o dong thu hai
    {
        std::ofstream file(path, std::ios::binary);
        file << "R009|読む|よむ|đọc|VERB|Vi du\n";
        file << "R010|好き|すき|thích|INVALID|Vi du\n";
    }

    bool failed = !storage::loadVocabularyFile(
        path, words, error
    );

    if (!check(
        failed &&
        words.size() == 2 &&
        words.at(0).getId().getId() == "R001",
        "File loi khong thay doi danh sach cu"
    )) return 1;

    if (!check(
        error.find("2") != std::string::npos,
        "Thong bao dung dong loi"
    )) return 1;

    std::remove(path.c_str());

    std::cout << "ALL B2 TESTS PASSED\n";
    return 0;
}

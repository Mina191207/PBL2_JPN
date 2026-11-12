#include "Fixture.h"
#include "domain/Time.h"
#include "domain/Validation.h"
#include <limits>
#include <vector>

void malformedTests() {
    TempDirectory temp;
    const auto original = fixture();
    DataStore loaded;
    std::string error;
    saveFixture(temp.path, original);
    CHECK(FileStorage{}.loadAll(temp.path.string(), loaded, error));
    auto invalid = original;
    invalid.vocabularies.push_back(invalid.vocabularies.front());
    CHECK(!FileStorage{}.saveAll((temp.path / "duplicates").string(), invalid, error));
    CHECK(!std::filesystem::exists(temp.path / "duplicates"));
    invalid = original;
    invalid.users.push_back(invalid.users.front());
    CHECK(!FileStorage{}.saveAll((temp.path / "duplicates").string(), invalid, error));
    const std::vector<std::pair<std::string,std::string>> cases = {
        {"learning_resource.txt", "R001|VOCABULARY|L001|extra\n"},
        {"learning_resource.txt", "R001|VOCABULARY|L001\nR001|GRAMMAR|L001\n"},
        {"learning_resource.txt", "R001|GRAMMAR|L001\n"},
        {"learning_resource.txt", "R001|VOCABULARY|L001\nR999|KANJI|L001\n"},
        {"resource_levels.txt", "L001|JLPT|N5|1x|\n"},
        {"resource_levels.txt", "L001|JLPT|N5|2147483648|\n"},
        {"resource_levels.txt", "L001|JLPT|N5|-1|\n"},
        {"resource_levels.txt", "L001|JLPT|N5|01|\n"},
        {"resource_levels.txt", "L001|JLPT|N5|1|\r"},
        {"vocabulary.txt", "R001|word||meaning|Verb|\n"},
        {"vocabulary.txt", "R001|word||meaning|NOUN|\nR001|duplicate||m|NOUN|\n"},
        {"vocabulary.txt", "R999|word||meaning|NOUN|\n"},
        {"vocabulary.txt", "R001|\xC0\xAF||meaning|NOUN|\n"},
        {"vocabulary.txt", "R001|word||meaning|NOUN|\n\xEF\xBB\xBF"},
        {"user.txt", "U001|Admin|pass|ADMIN|ACTIVE\nU002|ADMIN|pass|LEARNER|ACTIVE\n"},
        {"user.txt", "U001|Admin|pass|ADMIN|UNKNOWN\n"},
        {"user.txt", "U001|Admin|pass|ADMIN|LOCKED\n"},
        {"favorites.txt", "F001|U002|R999\n"},
        {"favorites.txt", "F001|U999|R001\n"},
        {"favorites.txt", "F001|U002|R001\nF002|U002|R001\n"},
        {"search_history.txt", "H001|U002|query|2026-02-29T01:00:00Z\n"},
        {"search_history.txt", "H001|U002|query|2026-11-12T01:00:00\n"},
        {"alphabet.txt", "A001|HIRAGANA|a|a|group|1|\nA002|HIRAGANA|a|a|group|2|\n"},
        {"alphabet.txt", "A001|OTHER|a|a|group|1|\n"},
        {"question.txt", "Q001|L002|R001|p|a|b|c|d|A||ACTIVE\n"},
        {"question.txt", "Q001|L001||p|one| one |c|d|A||ACTIVE\n"},
        {"quiz_attempt.txt", "T001|U002|L001|2|3|2026-11-12T01:00:00Z\n"},
        {"quiz_attempt.txt", "T001|U002|L999|2|1|2026-11-12T01:00:00Z\n"},
        {"grammar.txt", " \n"},
    };
    for (const auto& entry : cases) {
        saveFixture(temp.path, original);
        writeFile(temp.path / entry.first, entry.second);
        CHECK(!FileStorage{}.loadAll(temp.path.string(), loaded, error));
        CHECK(!error.empty() && loaded.users.size() == 3 && loaded.questions.size() == 4);
        CHECK(loaded.findResource(ResourceId("R001"))->getTerm() == "学校");
    }
    saveFixture(temp.path, original);
    std::filesystem::remove(temp.path / "alphabet.txt");
    CHECK(!FileStorage{}.loadAll(temp.path.string(), loaded, error));
    CHECK(loaded.alphabet.size() == 1);
}

void roundTripTests() {
    TempDirectory temp;
    auto data = fixture();
    data.levels.emplace_back(LevelId("L003"), "CUSTOM", "large order", std::numeric_limits<int>::max(), "");
    data.alphabet.emplace_back(AlphabetId("A002"), Script::KATAKANA, "ア", "a", "vowel", std::numeric_limits<int>::max(), "");
    data.vocabularies.emplace_back(ResourceId("R1000000"), LevelId("L003"), "本", "", "sách", PartOfSpeech::NOUN, "");
    data.rebuildIndexes();
    saveFixture(temp.path, data);
    DataStore loaded;
    std::string error;
    CHECK(FileStorage{}.loadAll(temp.path.string(), loaded, error));
    CHECK(loaded.levels.back().getOrder() == std::numeric_limits<int>::max());
    CHECK(loaded.alphabet.back().getDisplayOrder() == std::numeric_limits<int>::max());
    CHECK(loaded.findResource(ResourceId("R1000000"))->getMeaning() == "sách");
    CHECK(loaded.kanjis[0].getExamples() == "学生;学校");
    CHECK(loaded.questions[1].getResourceId() == std::nullopt);
    CHECK(loaded.histories.size() == 1 && loaded.attempts.size() == 1);
    for (const auto& entry : std::filesystem::directory_iterator(temp.path)) {
        std::ifstream in(entry.path(), std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(in)), {});
        std::string crlf = "\xEF\xBB\xBF";
        for (char c : text) { if (c == '\n') crlf += '\r'; crlf += c; }
        writeFile(entry.path(), crlf);
    }
    CHECK(FileStorage{}.loadAll(temp.path.string(), loaded, error));
    CHECK(loaded.vocabularies.front().getMeaning() == "trường;học");
    const auto package = temp.path / "package";
    CHECK(FileStorage{}.savePackage(package.string(), data, error));
    DataStore imported;
    CHECK(FileStorage{}.loadPackage(package.string(), imported, error));
    CHECK(imported.resourceById.size() == 4 && imported.levels.size() == 3);
    CHECK(imported.users.empty() && imported.questions.empty());
    writeFile(package / "vocabulary.txt", "R001|" + std::string(8193,'a') + "||m|NOUN|\n");
    CHECK(!FileStorage{}.loadPackage(package.string(), imported, error));
    CHECK(imported.resourceById.size() == 4);
    CHECK(validUtcTimestamp("2024-02-29T23:59:59Z"));
    CHECK(!validUtcTimestamp("1900-02-29T00:00:00Z"));
    CHECK(!validUtcTimestamp("2026-11-12T24:00:00Z"));
    CHECK(validUtcTimestamp(utcNow()));
    throws([] { Vocabulary(ResourceId("R001"), LevelId("L001"), std::string(8193,'w'), "", "m", PartOfSpeech::NOUN, ""); });
    throws([] { Grammar(ResourceId("R001"), LevelId("L001"), "p", std::string(8192,'m'), std::string(8192,'s'), "", "", ""); });
    throws([] { QuizAttempt(AttemptId("T001"), UserId("U001"), LevelId("L001"), 1, 0, "bad UTC"); });
    throws([] { Vocabulary(ResourceId("R001"), LevelId("L001"), "word", "", "meaning", static_cast<PartOfSpeech>(90), ""); });
    throws([] { AlphabetEntry(AlphabetId("A001"), static_cast<Script>(90), "a", "a", "g", 1, ""); });
    // Invalid UTF-8 must be rejected equally through a domain request and a TXT row.
    throws([] { validateTxtField("\xED\xA0\x80", "surrogate"); });
    throws([] { validateTxtField("\xF4\x90\x80\x80", "out of range"); });
}

void writerFailureTests() {
    TempDirectory temp;
    auto data = fixture();
    std::string error;
    std::filesystem::create_directory(temp.path / "user.txt.tmp");
    CHECK(!FileStorage{}.saveAll(temp.path.string(), data, error));
    CHECK(!std::filesystem::exists(temp.path / "user.txt"));
    std::filesystem::remove(temp.path / "user.txt.tmp");
    std::filesystem::create_directory(temp.path / "question.txt");
    CHECK(!FileStorage{}.saveAll(temp.path.string(), data, error));
    CHECK(error.find("replace") != std::string::npos);
    std::filesystem::remove(temp.path / "question.txt");
    CHECK(FileStorage{}.saveAll(temp.path.string(), data, error));
    DataStore loaded;
    CHECK(FileStorage{}.loadAll(temp.path.string(), loaded, error));
    // Writer checks the same 32 MiB limit as reader before replacing any file.
    DataStore big;
    big.levels.emplace_back(LevelId("L001"), "JLPT", "N5", 1, "");
    for (int i = 0; i < 4100; ++i)
        big.vocabularies.emplace_back(idAfter<ResourceId>(i), LevelId("L001"), "w", "", std::string(8192,'m'), PartOfSpeech::NOUN, "");
    big.rebuildIndexes();
    const auto out = temp.path / "too_big";
    CHECK(!FileStorage{}.savePackage(out.string(), big, error));
    CHECK(!std::filesystem::exists(out));
}
int main() {
    try { malformedTests(); roundTripTests(); writerFailureTests(); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

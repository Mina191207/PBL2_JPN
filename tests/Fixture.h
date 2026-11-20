#pragma once
#include "DataStore.h"
#include "storage/FileStorage.h"
#include "TestSupport.h"

inline DataStore fixture() {
    DataStore data;
    data.users.emplace_back(UserId("U001"), "Admin", "demo123", UserRole::ADMIN, UserStatus::ACTIVE);
    data.users.emplace_back(UserId("U002"), "learner1", "demo123", UserRole::LEARNER, UserStatus::ACTIVE);
    data.users.emplace_back(UserId("U003"), "learner2", "demo123", UserRole::LEARNER, UserStatus::ACTIVE);
    data.levels.emplace_back(LevelId("L001"), "JLPT", "N5", 5, "");
    data.levels.emplace_back(LevelId("L002"), "JLPT", "N4", 4, "");
    data.vocabularies.emplace_back(ResourceId("R001"), LevelId("L001"), "学校", "がっこう", "trường;học", PartOfSpeech::NOUN, "学校へ行く");
    data.grammars.emplace_back(ResourceId("R002"), LevelId("L001"), "～たい", "muốn", "Vます", "mong muốn", "", "");
    data.kanjis.emplace_back(ResourceId("R003"), LevelId("L002"), "学", "học", 8, "ガク", "まなぶ", "学生;学校");
    data.alphabet.emplace_back(AlphabetId("A001"), Script::HIRAGANA, "あ", "a", "vowel", 1, "");
    data.favorites.emplace_back(FavoriteId("F001"), UserId("U002"), ResourceId("R001"));
    data.histories.pushBack(SearchHistory(HistoryId("H001"), UserId("U002"), "学校", "2026-11-12T01:00:00Z"));
    for (int i = 1; i <= 4; ++i)
        data.questions.emplace_back(QuestionId("Q00" + std::to_string(i)), LevelId("L001"),
            i == 1 ? std::optional<ResourceId>(ResourceId("R001")) : std::nullopt,
            "prompt " + std::to_string(i), std::array<std::string,4>{"one", "two", "three", "four"},
            'A', "explanation", i == 4 ? QuestionStatus::INACTIVE : QuestionStatus::ACTIVE);
    data.attempts.emplace_back(AttemptId("T001"), UserId("U002"), LevelId("L001"), 3, 2, "2026-11-12T01:00:00Z");
    data.rebuildIndexes();
    data.validateRelations(true);
    return data;
}
inline void saveFixture(const std::filesystem::path& folder, const DataStore& data) {
    std::string error;
    CHECK(FileStorage{}.saveAll(folder.string(), data, error));
}

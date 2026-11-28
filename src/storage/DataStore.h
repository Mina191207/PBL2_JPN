#pragma once
#include "pbl2/domain/AlphabetEntry.h"
#include "pbl2/domain/Question.h"
#include "pbl2/domain/QuizAttempt.h"
#include "domain/favorite.h"
#include "pbl2/domain/grammar.h"
#include "pbl2/domain/kanji.h"
#include "pbl2/domain/level.h"
#include "pbl2/domain/search_his.h"
#include "domain/user.h"
#include "pbl2/domain/vocabulary.h"
#include "dsa/LinkedList.h"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

struct ResourceLocation {
    ResourceType type;
    std::size_t index;
};

enum class StoreTable : unsigned {
    NONE = 0, ALPHABET = 1 << 0, FAVORITES = 1 << 1, GRAMMAR = 1 << 2, KANJI = 1 << 3,
    RESOURCES = 1 << 4, QUESTIONS = 1 << 5, ATTEMPTS = 1 << 6, LEVELS = 1 << 7,
    HISTORY = 1 << 8, USERS = 1 << 9, VOCABULARY = 1 << 10,
    PACKAGE = GRAMMAR | KANJI | RESOURCES | LEVELS | VOCABULARY,
    ALL = (1 << 11) - 1
};
constexpr StoreTable operator|(StoreTable a, StoreTable b) noexcept {
    return static_cast<StoreTable>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
constexpr StoreTable operator&(StoreTable a, StoreTable b) noexcept {
    return static_cast<StoreTable>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}
constexpr StoreTable operator~(StoreTable tables) noexcept {
    return static_cast<StoreTable>(~static_cast<unsigned>(tables));
}
inline StoreTable& operator|=(StoreTable& a, StoreTable b) noexcept { return a = a | b; }

class DataStore {
  public:
    std::vector<Vocabulary> vocabularies;
    std::vector<Grammar> grammars;
    std::vector<Kanji> kanjis;
    std::vector<Level> levels;
    std::vector<User> users;
    std::vector<Favorite> favorites;
    DoublyLinkedList<SearchHistory> histories;
    std::vector<AlphabetEntry> alphabet;
    std::vector<Question> questions;
    std::vector<QuizAttempt> attempts;
    StoreTable dirtyTables = StoreTable::NONE;
    bool dirty() const noexcept { return dirtyTables != StoreTable::NONE; }

    std::unordered_map<std::string, ResourceLocation> resourceById;
    std::unordered_map<std::string, std::size_t> userById, userByName, levelById, questionById;

    void rebuildIndexes();
    void validateRelations(bool requireAdmin = false) const;
    void swap(DataStore& other) noexcept;
    const Resource* findResource(const ResourceId& id) const noexcept;
    const Level* findLevel(const LevelId& id) const noexcept;
    const User* findUser(const UserId& id) const noexcept;
    static std::string lowerAscii(std::string value);
};

#include "IdGenerator.h"

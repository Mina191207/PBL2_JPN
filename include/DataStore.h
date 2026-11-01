#pragma once
#include "domain/AlphabetEntry.h"
#include "domain/Question.h"
#include "domain/QuizAttempt.h"
#include "domain/favorite.h"
#include "domain/grammar.h"
#include "domain/kanji.h"
#include "domain/level.h"
#include "domain/search_his.h"
#include "domain/user.h"
#include "domain/vocabulary.h"
#include "dsa/LinkedList.h"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

struct ResourceLocation {
    ResourceType type;
    std::size_t index;
};

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
    bool dirty = false;

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

template <class Id, class Container> Id nextId(const Container& values);

#include "../src/IdGenerator.h"

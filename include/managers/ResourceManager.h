#pragma once
#include "DataStore.h"
#include "managers/UserManager.h"
#include <optional>
#include <string>
#include <vector>

enum class SearchSort {
    RELEVANCE,
    ID,
    LEVEL_ASC,
    LEVEL_DESC,
    TERM_ASC,
    TERM_DESC
};
struct SearchOptions {
    std::string query;
    bool exactId = false;
    std::optional<ResourceType> type;
    std::optional<LevelId> levelId;
    SearchSort sort = SearchSort::RELEVANCE;
};
struct SearchRow {
    ResourceId id;
    ResourceType type;
    std::string term, meaning;
    LevelId levelId;
    int levelOrder;
    int score;
};
struct ResourceStats {
    std::size_t vocabulary = 0, grammar = 0, kanji = 0;
};

class ResourceManager {
    DataStore& data_;
    const UserManager& users_;
    bool canChange(const Resource& item, bool update, std::string& error) const;

  public:
    ResourceManager(DataStore& data, const UserManager& users);
    ResourceId nextResourceId() const;
    bool addVocabulary(const Vocabulary& item, std::string& error);
    bool addGrammar(const Grammar& item, std::string& error);
    bool addKanji(const Kanji& item, std::string& error);
    bool updateVocabulary(const Vocabulary& item, std::string& error);
    bool updateGrammar(const Grammar& item, std::string& error);
    bool updateKanji(const Kanji& item, std::string& error);
    const Resource* find(const ResourceId& id) const noexcept;
    std::vector<SearchRow> search(const SearchOptions& options) const;
    ResourceStats statistics(std::optional<LevelId> level = {}) const;
};

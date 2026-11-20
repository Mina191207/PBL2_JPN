#pragma once
#include "DataStore.h"
#include "managers/UserManager.h"
#include <optional>
#include <string>
#include <vector>

#include "Search.h"

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

#include "application/managers/ResourceManager.h"
#include <limits>

ResourceManager::ResourceManager(DataStore& data, const UserManager& users)
    : data_(data),
      users_(users) {
}

const Resource* ResourceManager::find(const ResourceId& id) const noexcept {
    return data_.findResource(id);
}

bool ResourceManager::canChange(const Resource& item, bool update, std::string& error) const {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    if (!data_.findLevel(item.getLevelId())) {
        error = "level not found";
        return false;
    }
    const Resource* old = data_.findResource(item.getId());
    if (!update && old) {
        error = "resource id exists";
        return false;
    }
    if (update && (!old || old->getType() != item.getType())) {
        error = "resource not found or type changed";
        return false;
    }
    if (update && old->getLevelId() != item.getLevelId())
        for (const auto& question : data_.questions)
            if (question.getResourceId() && *question.getResourceId() == item.getId() &&
                question.getLevelId() != item.getLevelId()) {
                error = "question uses old level";
                return false;
            }
    return true;
}
ResourceId ResourceManager::nextResourceId() const {
    std::uint64_t maxValue = 0;
    for (const auto& item : data_.vocabularies)
        if (item.getId().number() > maxValue)
            maxValue = item.getId().number();
    for (const auto& item : data_.grammars)
        if (item.getId().number() > maxValue)
            maxValue = item.getId().number();
    for (const auto& item : data_.kanjis)
        if (item.getId().number() > maxValue)
            maxValue = item.getId().number();
    return idAfter<ResourceId>(maxValue);
}
bool ResourceManager::addVocabulary(const Vocabulary& item, std::string& error) {
    if (!canChange(item, false, error))
        return false;
    data_.vocabularies.push_back(item);
    data_.rebuildIndexes();
    data_.dirtyTables |= StoreTable::RESOURCES | StoreTable::VOCABULARY;
    return true;
}
bool ResourceManager::addGrammar(const Grammar& item, std::string& error) {
    if (!canChange(item, false, error))
        return false;
    data_.grammars.push_back(item);
    data_.rebuildIndexes();
    data_.dirtyTables |= StoreTable::RESOURCES | StoreTable::GRAMMAR;
    return true;
}
bool ResourceManager::addKanji(const Kanji& item, std::string& error) {
    if (!canChange(item, false, error))
        return false;
    data_.kanjis.push_back(item);
    data_.rebuildIndexes();
    data_.dirtyTables |= StoreTable::RESOURCES | StoreTable::KANJI;
    return true;
}
bool ResourceManager::updateVocabulary(const Vocabulary& item, std::string& error) {
    if (!canChange(item, true, error))
        return false;
    auto& previous = data_.vocabularies[data_.resourceById.at(item.getId().getId()).index];
    const bool moved = previous.getLevelId() != item.getLevelId();
    previous = item;
    data_.dirtyTables |= StoreTable::VOCABULARY;
    if (moved) data_.dirtyTables |= StoreTable::RESOURCES;
    return true;
}
bool ResourceManager::updateGrammar(const Grammar& item, std::string& error) {
    if (!canChange(item, true, error))
        return false;
    auto& previous = data_.grammars[data_.resourceById.at(item.getId().getId()).index];
    const bool moved = previous.getLevelId() != item.getLevelId();
    previous = item;
    data_.dirtyTables |= StoreTable::GRAMMAR;
    if (moved) data_.dirtyTables |= StoreTable::RESOURCES;
    return true;
}
bool ResourceManager::updateKanji(const Kanji& item, std::string& error) {
    if (!canChange(item, true, error))
        return false;
    auto& previous = data_.kanjis[data_.resourceById.at(item.getId().getId()).index];
    const bool moved = previous.getLevelId() != item.getLevelId();
    previous = item;
    data_.dirtyTables |= StoreTable::KANJI;
    if (moved) data_.dirtyTables |= StoreTable::RESOURCES;
    return true;
}
ResourceStats ResourceManager::statistics(std::optional<LevelId> level) const {
    ResourceStats result;
    for (const auto& item : data_.vocabularies)
        if (!level || item.getLevelId() == *level)
            ++result.vocabulary;
    for (const auto& item : data_.grammars)
        if (!level || item.getLevelId() == *level)
            ++result.grammar;
    for (const auto& item : data_.kanjis)
        if (!level || item.getLevelId() == *level)
            ++result.kanji;
    return result;
}

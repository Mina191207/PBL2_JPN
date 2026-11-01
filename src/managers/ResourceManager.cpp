#include "managers/ResourceManager.h"
#include "algorithms/KmpMatcher.h"
#include "algorithms/MergeSort.h"
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
    if (maxValue == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("id space exhausted");
    std::string digits = std::to_string(maxValue + 1);
    if (digits.size() < 3)
        digits.insert(0, 3 - digits.size(), '0');
    return ResourceId("R" + digits);
}
bool ResourceManager::addVocabulary(const Vocabulary& item, std::string& error) {
    if (!canChange(item, false, error))
        return false;
    data_.vocabularies.push_back(item);
    data_.rebuildIndexes();
    data_.dirty = true;
    return true;
}
bool ResourceManager::addGrammar(const Grammar& item, std::string& error) {
    if (!canChange(item, false, error))
        return false;
    data_.grammars.push_back(item);
    data_.rebuildIndexes();
    data_.dirty = true;
    return true;
}
bool ResourceManager::addKanji(const Kanji& item, std::string& error) {
    if (!canChange(item, false, error))
        return false;
    data_.kanjis.push_back(item);
    data_.rebuildIndexes();
    data_.dirty = true;
    return true;
}
bool ResourceManager::updateVocabulary(const Vocabulary& item, std::string& error) {
    if (!canChange(item, true, error))
        return false;
    data_.vocabularies[data_.resourceById.at(item.getId().getId()).index] = item;
    data_.dirty = true;
    return true;
}
bool ResourceManager::updateGrammar(const Grammar& item, std::string& error) {
    if (!canChange(item, true, error))
        return false;
    data_.grammars[data_.resourceById.at(item.getId().getId()).index] = item;
    data_.dirty = true;
    return true;
}
bool ResourceManager::updateKanji(const Kanji& item, std::string& error) {
    if (!canChange(item, true, error))
        return false;
    data_.kanjis[data_.resourceById.at(item.getId().getId()).index] = item;
    data_.dirty = true;
    return true;
}
std::vector<SearchRow> ResourceManager::search(const SearchOptions& options) const {
    std::vector<SearchRow> rows;
    auto include = [&](const Resource& item, int score) {
        if (options.type && item.getType() != *options.type)
            return;
        if (options.levelId && item.getLevelId() != *options.levelId)
            return;
        const Level* level = data_.findLevel(item.getLevelId());
        rows.push_back({item.getId(),
                        item.getType(),
                        item.getTerm(),
                        item.getMeaning(),
                        item.getLevelId(),
                        level ? level->getOrder() : 0,
                        score});
    };
    if (options.exactId) {
        if (ResourceId::isValid(options.query)) {
            const Resource* item = data_.findResource(ResourceId(options.query));
            if (item)
                include(*item, 4);
        }
    } else {
        KmpMatcher matcher(options.query);
        std::vector<const Resource*> view;
        for (const auto& item : data_.vocabularies)
            view.push_back(&item);
        for (const auto& item : data_.grammars)
            view.push_back(&item);
        for (const auto& item : data_.kanjis)
            view.push_back(&item);
        for (const Resource* item : view) {
            if (options.type && item->getType() != *options.type)
                continue;
            if (options.levelId && item->getLevelId() != *options.levelId)
                continue;
            int score = options.query.empty() ? 0 : item->matchScore(matcher);
            if (options.query.empty() || score > 0)
                include(*item, score);
        }
    }
    mergeSort(rows, [&](const SearchRow& a, const SearchRow& b) {
        switch (options.sort) {
        case SearchSort::RELEVANCE:
            if (a.score != b.score)
                return a.score > b.score;
            if (a.levelOrder != b.levelOrder)
                return a.levelOrder < b.levelOrder;
            break;
        case SearchSort::LEVEL_ASC:
            if (a.levelOrder != b.levelOrder)
                return a.levelOrder < b.levelOrder;
            break;
        case SearchSort::LEVEL_DESC:
            if (a.levelOrder != b.levelOrder)
                return a.levelOrder > b.levelOrder;
            break;
        case SearchSort::TERM_ASC:
            if (a.term != b.term)
                return a.term < b.term;
            break;
        case SearchSort::TERM_DESC:
            if (a.term != b.term)
                return a.term > b.term;
            break;
        case SearchSort::ID:
            break;
        }
        return a.id.number() < b.id.number();
    });
    return rows;
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

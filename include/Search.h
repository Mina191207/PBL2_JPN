#pragma once
#include "domain/EntityId.h"
#include "domain/ResourceType.h"
#include <optional>
#include <cstddef>
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

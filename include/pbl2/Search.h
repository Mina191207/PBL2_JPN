#pragma once
#include "pbl2/domain/EntityId.h"
#include "pbl2/domain/ResourceType.h"
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
    std::string reading;
};
struct SearchReplay {
    SearchOptions options;
    std::vector<SearchRow> rows;
};
struct ResourceStats {
    std::size_t vocabulary = 0, grammar = 0, kanji = 0;
};

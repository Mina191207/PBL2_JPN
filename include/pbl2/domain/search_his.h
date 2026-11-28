#pragma once
#include "pbl2/Search.h"
#include <string>

class SearchHistory {
    HistoryId id_;
    UserId userId_;
    SearchOptions options_;
    std::string searchedAt_;
    bool hasOptions_ = true;

  public:
    SearchHistory(HistoryId id, UserId userId, std::string query, std::string searchedAt);
    SearchHistory(HistoryId id, UserId userId, SearchOptions options, std::string searchedAt);
    const HistoryId& getId() const noexcept;
    const UserId& getUserId() const noexcept;
    const std::string& getQuery() const noexcept;
    const SearchOptions& getOptions() const noexcept;
    bool hasSearchOptions() const noexcept;
    const std::string& getSearchedAt() const noexcept;
};

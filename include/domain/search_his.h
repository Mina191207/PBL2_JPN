#pragma once
#include "EntityId.h"
#include <string>

class SearchHistory {
    HistoryId id_;
    UserId userId_;
    std::string query_;
    std::string searchedAt_;

  public:
    SearchHistory(HistoryId id, UserId userId, std::string query, std::string searchedAt);
    const HistoryId& getId() const noexcept;
    const UserId& getUserId() const noexcept;
    const std::string& getQuery() const noexcept;
    const std::string& getSearchedAt() const noexcept;
};

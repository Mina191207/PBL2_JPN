#include <stdexcept>
#include "domain/Time.h"
#include "domain/search_his.h"
#include "domain/Validation.h"
#include <utility>

SearchHistory::SearchHistory(HistoryId id, UserId userId, std::string query, std::string searchedAt)
    : id_(std::move(id)),
      userId_(std::move(userId)),
      query_(std::move(query)),
      searchedAt_(std::move(searchedAt)) {
    validateTxtField(query_, "query");
    validateTxtField(searchedAt_, "searched_at");
    validateTxtRecord({id_.getId(), userId_.getId(), query_, searchedAt_});
    if (!validUtcTimestamp(searchedAt_))
        throw std::invalid_argument("invalid UTC timestamp");
}

const HistoryId& SearchHistory::getId() const noexcept {
    return id_;
}
const UserId& SearchHistory::getUserId() const noexcept {
    return userId_;
}
const std::string& SearchHistory::getQuery() const noexcept {
    return query_;
}
const std::string& SearchHistory::getSearchedAt() const noexcept {
    return searchedAt_;
}

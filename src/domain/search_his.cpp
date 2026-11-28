#include <stdexcept>
#include "domain/Time.h"
#include "pbl2/domain/search_his.h"
#include "domain/Validation.h"
#include <utility>

SearchHistory::SearchHistory(HistoryId id, UserId userId, std::string query, std::string searchedAt)
    : SearchHistory(std::move(id), std::move(userId), SearchOptions{std::move(query), false, {}, {}},
                    std::move(searchedAt)) {
    hasOptions_ = false;
}

SearchHistory::SearchHistory(HistoryId id, UserId userId, SearchOptions options, std::string searchedAt)
    : id_(std::move(id)),
      userId_(std::move(userId)),
      options_(std::move(options)),
      searchedAt_(std::move(searchedAt)) {
    validateTxtField(options_.query, "query");
    validateTxtField(searchedAt_, "searched_at");
    if (!validUtcTimestamp(searchedAt_))
        throw std::invalid_argument("invalid UTC timestamp");
    if ((options_.exactId && !ResourceId::isValid(options_.query)) ||
        (options_.type && (*options_.type < ResourceType::VOCABULARY || *options_.type > ResourceType::KANJI)) ||
        options_.sort < SearchSort::RELEVANCE || options_.sort > SearchSort::TERM_DESC)
        throw std::invalid_argument("invalid history search options");
}

const HistoryId& SearchHistory::getId() const noexcept {
    return id_;
}
const UserId& SearchHistory::getUserId() const noexcept {
    return userId_;
}
const std::string& SearchHistory::getQuery() const noexcept {
    return options_.query;
}
const SearchOptions& SearchHistory::getOptions() const noexcept {
    return options_;
}
bool SearchHistory::hasSearchOptions() const noexcept {
    return hasOptions_;
}
const std::string& SearchHistory::getSearchedAt() const noexcept {
    return searchedAt_;
}

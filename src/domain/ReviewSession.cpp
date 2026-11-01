#include "domain/ReviewSession.h"

ReviewSession::ReviewSession(const std::vector<ReviewCard>& cards)
    : initial_(cards.size()) {
    for (const auto& card : cards) {
        pending_.pushBack(card);
    }
    advance();
}

void ReviewSession::advance() {
    current_.reset();
    revealed_ = false;
    if (!pending_.empty()) {
        current_ = pending_.front();
        pending_.popFront();
    }
}

const ReviewCard* ReviewSession::current() const noexcept {
    return current_ ? &*current_ : nullptr;
}

bool ReviewSession::reveal() noexcept {
    if (!current_) {
        return false;
    }
    revealed_ = true;
    return true;
}

bool ReviewSession::rate(bool remembered) {
    if (!current_ || !revealed_) {
        return false;
    }
    if (!remembered) {
        pending_.pushBack(*current_);
    }
    ++ratings_;
    advance();
    return true;
}

bool ReviewSession::finished() const noexcept {
    return !current_ && pending_.empty();
}

std::size_t ReviewSession::remaining() const noexcept {
    return pending_.size() + (current_ ? 1 : 0);
}

std::size_t ReviewSession::initialCount() const noexcept {
    return initial_;
}

std::size_t ReviewSession::ratingCount() const noexcept {
    return ratings_;
}

void ReviewSession::stop() noexcept {
    pending_.clear();
    current_.reset();
    revealed_ = false;
}

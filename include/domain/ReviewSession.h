#pragma once
#include "ReviewCard.h"
#include "dsa/LinkedList.h"
#include <cstddef>
#include <optional>
#include <vector>

class ReviewSession {
    DoublyLinkedList<ReviewCard> pending_;
    std::optional<ReviewCard> current_;
    bool revealed_ = false;
    std::size_t initial_ = 0;
    std::size_t ratings_ = 0;

    void advance();

  public:
    explicit ReviewSession(const std::vector<ReviewCard>& cards);
    const ReviewCard* current() const noexcept;
    bool reveal() noexcept;
    bool revealed() const noexcept;
    bool rate(bool remembered);
    bool finished() const noexcept;
    std::size_t remaining() const noexcept;
    std::size_t initialCount() const noexcept;
    std::size_t ratingCount() const noexcept;
    void stop() noexcept;
};

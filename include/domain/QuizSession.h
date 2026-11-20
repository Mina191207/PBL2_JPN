#pragma once
#include "domain/Question.h"
#include <cstddef>
#include <vector>

class QuizSession {
    std::vector<Question> questions_;
    std::vector<char> answers_;
    bool submitted_ = false;

  public:
    explicit QuizSession(std::vector<Question> questions);
    const std::vector<Question>& questions() const noexcept;
    const std::vector<char>& answers() const noexcept;
    bool select(std::size_t index, char answer);
    int correctCount() const noexcept;
    std::vector<Question> wrongQuestions() const;
    bool submitted() const noexcept;
    void markSubmitted() noexcept;
};

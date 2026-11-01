#include "domain/QuizAttempt.h"
#include "domain/Validation.h"
#include <stdexcept>
#include <utility>

QuizAttempt::QuizAttempt(
    AttemptId id, UserId userId, LevelId levelId, int total, int correct, std::string submittedAt)
    : id_(std::move(id)),
      userId_(std::move(userId)),
      levelId_(std::move(levelId)),
      totalQuestions_(total),
      correctAnswers_(correct),
      submittedAt_(std::move(submittedAt)) {
    validatePositive(totalQuestions_, "total_questions");
    if (totalQuestions_ > 50) {
        throw std::invalid_argument("too many questions");
    }
    if (correctAnswers_ < 0 || correctAnswers_ > totalQuestions_) {
        throw std::invalid_argument("invalid score");
    }
    validateTxtField(submittedAt_, "submitted_at");
}

const AttemptId& QuizAttempt::getId() const noexcept {
    return id_;
}
const UserId& QuizAttempt::getUserId() const noexcept {
    return userId_;
}
const LevelId& QuizAttempt::getLevelId() const noexcept {
    return levelId_;
}
int QuizAttempt::getTotalQuestions() const noexcept {
    return totalQuestions_;
}
int QuizAttempt::getCorrectAnswers() const noexcept {
    return correctAnswers_;
}
const std::string& QuizAttempt::getSubmittedAt() const noexcept {
    return submittedAt_;
}
double QuizAttempt::percentage() const noexcept {
    return 100.0 * correctAnswers_ / totalQuestions_;
}

#pragma once
#include "EntityId.h"
#include <string>

class QuizAttempt {
    AttemptId id_;
    UserId userId_;
    LevelId levelId_;
    int totalQuestions_;
    int correctAnswers_;
    std::string submittedAt_;

  public:
    QuizAttempt(AttemptId id,
                UserId userId,
                LevelId levelId,
                int total,
                int correct,
                std::string submittedAt);
    const AttemptId& getId() const noexcept;
    const UserId& getUserId() const noexcept;
    const LevelId& getLevelId() const noexcept;
    int getTotalQuestions() const noexcept;
    int getCorrectAnswers() const noexcept;
    const std::string& getSubmittedAt() const noexcept;
    double percentage() const noexcept;
};

#pragma once
#include "EntityId.h"
#include <array>
#include <optional>
#include <string>

enum class QuestionStatus {
    ACTIVE,
    INACTIVE
};

class Question {
    QuestionId id_;
    LevelId levelId_;
    std::optional<ResourceId> resourceId_;
    std::string prompt_;
    std::array<std::string, 4> options_;
    char correctOption_;
    std::string explanation_;
    QuestionStatus status_;

  public:
    Question(QuestionId id,
             LevelId levelId,
             std::optional<ResourceId> resourceId,
             std::string prompt,
             std::array<std::string, 4> options,
             char correctOption,
             std::string explanation,
             QuestionStatus status);
    const QuestionId& getId() const noexcept;
    const LevelId& getLevelId() const noexcept;
    const std::optional<ResourceId>& getResourceId() const noexcept;
    const std::string& getPrompt() const noexcept;
    const std::array<std::string, 4>& getOptions() const noexcept;
    char getCorrectOption() const noexcept;
    const std::string& getExplanation() const noexcept;
    QuestionStatus getStatus() const noexcept;
    bool isCorrect(char answer) const noexcept;
    void setStatus(QuestionStatus status);
};

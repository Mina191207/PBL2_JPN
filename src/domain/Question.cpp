#include "domain/Question.h"
#include "domain/Validation.h"
#include <set>
#include <stdexcept>
#include <utility>

Question::Question(QuestionId id,
                   LevelId levelId,
                   std::optional<ResourceId> resourceId,
                   std::string prompt,
                   std::array<std::string, 4> options,
                   char correctOption,
                   std::string explanation,
                   QuestionStatus status)
    : id_(std::move(id)),
      levelId_(std::move(levelId)),
      resourceId_(std::move(resourceId)),
      prompt_(std::move(prompt)),
      options_(std::move(options)),
      correctOption_(correctOption),
      explanation_(std::move(explanation)),
      status_(status) {
    validateTxtField(prompt_, "prompt");
    for (const auto& option : options_) {
        validateTxtField(option, "option");
    }
    std::set<std::string> uniqueOptions;
    for (const auto& option : options_) {
        uniqueOptions.insert(trimBasic(option));
    }
    if (uniqueOptions.size() != options_.size()) {
        throw std::invalid_argument("duplicate option");
    }
    validateTxtField(explanation_, "explanation", false);
    if (correctOption_ < 'A' || correctOption_ > 'D') {
        throw std::invalid_argument("invalid correct option");
    }
    // Reserve the longer status token so a later ACTIVE -> INACTIVE remains writable.
    validateTxtRecord({id_.getId(),
                       levelId_.getId(),
                       resourceId_ ? resourceId_->getId() : "",
                       prompt_, options_[0], options_[1], options_[2], options_[3],
                       std::string(1, correctOption_), explanation_, "INACTIVE"});
    if (status_ != QuestionStatus::ACTIVE && status_ != QuestionStatus::INACTIVE)
        throw std::invalid_argument("invalid question status");
}

const QuestionId& Question::getId() const noexcept {
    return id_;
}
const LevelId& Question::getLevelId() const noexcept {
    return levelId_;
}
const std::optional<ResourceId>& Question::getResourceId() const noexcept {
    return resourceId_;
}
const std::string& Question::getPrompt() const noexcept {
    return prompt_;
}
const std::array<std::string, 4>& Question::getOptions() const noexcept {
    return options_;
}
char Question::getCorrectOption() const noexcept {
    return correctOption_;
}
const std::string& Question::getExplanation() const noexcept {
    return explanation_;
}
QuestionStatus Question::getStatus() const noexcept {
    return status_;
}
bool Question::isCorrect(char answer) const noexcept {
    return answer == correctOption_;
}
void Question::setStatus(QuestionStatus status) {
    if (status != QuestionStatus::ACTIVE && status != QuestionStatus::INACTIVE)
        throw std::invalid_argument("invalid status");
    status_ = status;
}

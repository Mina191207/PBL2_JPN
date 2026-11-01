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
        const auto first = option.find_first_not_of(" \t");
        const auto last = option.find_last_not_of(" \t");
        uniqueOptions.insert(option.substr(first, last - first + 1));
    }
    if (uniqueOptions.size() != options_.size()) {
        throw std::invalid_argument("duplicate option");
    }
    validateTxtField(explanation_, "explanation", false);
    if (correctOption_ < 'A' || correctOption_ > 'D') {
        throw std::invalid_argument("invalid correct option");
    }
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
void Question::setStatus(QuestionStatus status) noexcept {
    status_ = status;
}

#include "domain/QuizSession.h"
#include <algorithm>
#include <utility>
#include <stdexcept>

QuizSession::QuizSession(std::vector<Question> questions)
    : questions_(std::move(questions)),
      answers_(questions_.size(), '-') {
    if (questions_.empty() || questions_.size() > 50)
        throw std::invalid_argument("quiz session requires 1..50 questions");
    for (const auto& question : questions_)
        if (question.getLevelId() != questions_.front().getLevelId())
            throw std::invalid_argument("quiz questions must share a level");
}

const std::vector<Question>& QuizSession::questions() const noexcept {
    return questions_;
}

const std::vector<char>& QuizSession::answers() const noexcept {
    return answers_;
}

bool QuizSession::submitted() const noexcept {
    return submitted_;
}

void QuizSession::markSubmitted() noexcept {
    submitted_ = true;
}

bool QuizSession::select(std::size_t index, char answer) {
    if (submitted_ || index >= answers_.size() || (answer != '-' && (answer < 'A' || answer > 'D')))
        return false;
    answers_[index] = answer;
    return true;
}

int QuizSession::correctCount() const noexcept {
    int correct = 0;
    for (std::size_t i = 0; i < questions_.size(); ++i)
        if (questions_[i].isCorrect(answers_[i]))
            ++correct;
    return correct;
}

std::vector<Question> QuizSession::wrongQuestions() const {
    std::vector<Question> wrong;
    for (std::size_t i = 0; i < questions_.size(); ++i)
        if (!questions_[i].isCorrect(answers_[i]))
            wrong.push_back(questions_[i]);
    return wrong;
}

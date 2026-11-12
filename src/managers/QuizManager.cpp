#include "managers/QuizManager.h"
#include "domain/Time.h"
#include <algorithm>
#include <utility>

QuizManager::QuizManager(DataStore& data, const UserManager& users)
    : data_(data),
      users_(users) {
}

const QuizSession* QuizManager::session() const noexcept {
    return session_ && sessionEpoch_ == users_.sessionEpoch() ? &*session_ : nullptr;
}

void QuizManager::stop() noexcept {
    session_.reset();
}

bool QuizManager::start(const LevelId& level,
                        std::size_t count,
                        std::mt19937& engine,
                        std::string& error) {
    if (!users_.currentUser()) {
        error = "login required";
        return false;
    }
    const Level* found = data_.findLevel(level);
    if (!found || found->getCategory() != "JLPT") {
        error = "JLPT level required";
        return false;
    }
    if (count < 1 || count > 50) {
        error = "question count must be 1..50";
        return false;
    }
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < data_.questions.size(); ++i)
        if (data_.questions[i].getLevelId() == level &&
            data_.questions[i].getStatus() == QuestionStatus::ACTIVE)
            indices.push_back(i);
    if (indices.size() < count) {
        error = "not enough questions";
        return false;
    }
    std::shuffle(indices.begin(), indices.end(), engine);
    std::vector<Question> selected;
    for (std::size_t i = 0; i < count; ++i)
        selected.push_back(data_.questions[indices[i]]);
    session_.emplace(std::move(selected));
    sessionEpoch_ = users_.sessionEpoch();
    return true;
}

bool QuizManager::selectAnswer(std::size_t index, char answer) {
    return session() && session_->select(index, answer);
}

bool QuizManager::submit(std::optional<QuizAttempt>& result, std::string& error) {
    const User* user = users_.currentUser();
    if (!user || !session() || session_->submitted()) {
        error = "no active quiz";
        return false;
    }
    const auto& questions = session_->questions();
    const Level* level = data_.findLevel(questions.front().getLevelId());
    if (!level || level->getCategory() != "JLPT") {
        error = "quiz level no longer available";
        return false;
    }
    try {
        QuizAttempt attempt(nextId<AttemptId>(data_.attempts),
                            user->getId(),
                            questions.front().getLevelId(),
                            static_cast<int>(questions.size()),
                            session_->correctCount(),
                            utcNow());
        result = attempt;
        data_.attempts.push_back(attempt);
        session_->markSubmitted();
        data_.dirty = true;
        return true;
    } catch (const std::exception& e) {
        result.reset();
        error = e.what();
        return false;
    }
}

bool QuizManager::retryWrong(std::string& error) {
    if (!users_.currentUser() || !session() || !session_->submitted()) {
        error = "submit quiz first";
        return false;
    }
    auto wrong = session_->wrongQuestions();
    if (wrong.empty()) {
        error = "no wrong answers";
        return false;
    }
    session_.emplace(std::move(wrong));
    return true;
}

std::vector<QuizAttempt> QuizManager::history() const {
    std::vector<QuizAttempt> result;
    const User* user = users_.currentUser();
    if (user)
        for (const auto& attempt : data_.attempts)
            if (attempt.getUserId() == user->getId())
                result.push_back(attempt);
    return result;
}

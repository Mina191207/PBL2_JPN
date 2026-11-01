#include "managers/StudyManagers.h"
#include "algorithms/MergeSort.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>

namespace {
std::string utcNow() {
    const std::time_t now = std::time(nullptr);
    std::tm value{};
#ifdef _WIN32
    gmtime_s(&value, &now);
#else
    gmtime_r(&now, &value);
#endif
    std::ostringstream out;
    out << std::put_time(&value, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}
}

AlphabetManager::AlphabetManager(DataStore& data, const UserManager& users)
    : data_(data),
      users_(users) {
}

QuestionManager::QuestionManager(DataStore& data, const UserManager& users)
    : data_(data),
      users_(users) {
}

QuizSession::QuizSession(std::vector<Question> questions)
    : questions_(std::move(questions)),
      answers_(questions_.size(), '-') {
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
bool AlphabetManager::add(const AlphabetEntry& item, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    for (const auto& old : data_.alphabet)
        if (old.getId() == item.getId()) {
            error = "alphabet id exists";
            return false;
        }
    for (const auto& old : data_.alphabet) {
        if (old.getScript() == item.getScript() && old.getGlyph() == item.getGlyph()) {
            error = "alphabet glyph exists";
            return false;
        }
    }
    data_.alphabet.push_back(item);
    data_.dirty = true;
    return true;
}
bool AlphabetManager::update(const AlphabetEntry& item, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    for (auto& old : data_.alphabet)
        if (old.getId() == item.getId()) {
            for (const auto& other : data_.alphabet) {
                if (other.getId() != item.getId() && other.getScript() == item.getScript() &&
                    other.getGlyph() == item.getGlyph()) {
                    error = "alphabet glyph exists";
                    return false;
                }
            }
            old = item;
            data_.dirty = true;
            return true;
        }
    error = "alphabet entry not found";
    return false;
}
bool AlphabetManager::remove(const AlphabetId& id, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    for (auto it = data_.alphabet.begin(); it != data_.alphabet.end(); ++it)
        if (it->getId() == id) {
            data_.alphabet.erase(it);
            data_.dirty = true;
            return true;
        }
    error = "alphabet entry not found";
    return false;
}
std::vector<AlphabetEntry> AlphabetManager::list(std::optional<Script> script,
                                                 const std::string& group) const {
    std::vector<AlphabetEntry> result;
    for (const auto& item : data_.alphabet)
        if ((!script || item.getScript() == *script) &&
            (group.empty() || item.getGroupName() == group))
            result.push_back(item);
    mergeSort(result, [](const AlphabetEntry& a, const AlphabetEntry& b) {
        if (a.getDisplayOrder() != b.getDisplayOrder())
            return a.getDisplayOrder() < b.getDisplayOrder();
        return a.getId().number() < b.getId().number();
    });
    return result;
}
bool QuestionManager::valid(const Question& item, std::string& error) const {
    const Level* level = data_.findLevel(item.getLevelId());
    if (!level || level->getCategory() != "JLPT") {
        error = "JLPT level required";
        return false;
    }
    if (item.getResourceId()) {
        const Resource* resource = data_.findResource(*item.getResourceId());
        if (!resource || resource->getLevelId() != item.getLevelId()) {
            error = "question resource invalid";
            return false;
        }
    }
    return true;
}
bool QuestionManager::add(const Question& item, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    if (data_.questionById.count(item.getId().getId())) {
        error = "question id exists";
        return false;
    }
    if (!valid(item, error))
        return false;
    data_.questions.push_back(item);
    data_.rebuildIndexes();
    data_.dirty = true;
    return true;
}
bool QuestionManager::update(const Question& item, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    auto it = data_.questionById.find(item.getId().getId());
    if (it == data_.questionById.end()) {
        error = "question not found";
        return false;
    }
    if (!valid(item, error))
        return false;
    data_.questions[it->second] = item;
    data_.dirty = true;
    return true;
}
bool QuestionManager::remove(const QuestionId& id, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    auto it = data_.questionById.find(id.getId());
    if (it == data_.questionById.end()) {
        error = "question not found";
        return false;
    }
    data_.questions.erase(data_.questions.begin() + static_cast<std::ptrdiff_t>(it->second));
    data_.rebuildIndexes();
    data_.dirty = true;
    return true;
}
bool QuestionManager::setStatus(const QuestionId& id, QuestionStatus status, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    auto it = data_.questionById.find(id.getId());
    if (it == data_.questionById.end()) {
        error = "question not found";
        return false;
    }
    data_.questions[it->second].setStatus(status);
    data_.dirty = true;
    return true;
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

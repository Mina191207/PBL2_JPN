#pragma once
#include "DataStore.h"
#include "managers/UserManager.h"
#include <optional>
#include <random>
#include <string>
#include <vector>

class AlphabetManager {
    DataStore& data_;
    const UserManager& users_;

  public:
    AlphabetManager(DataStore& data, const UserManager& users);
    bool add(const AlphabetEntry& item, std::string& error);
    bool update(const AlphabetEntry& item, std::string& error);
    bool remove(const AlphabetId& id, std::string& error);
    std::vector<AlphabetEntry> list(std::optional<Script> script = {},
                                    const std::string& group = {}) const;
};

class QuestionManager {
    DataStore& data_;
    const UserManager& users_;
    bool valid(const Question& item, std::string& error) const;

  public:
    QuestionManager(DataStore& data, const UserManager& users);
    bool add(const Question& item, std::string& error);
    bool update(const Question& item, std::string& error);
    bool remove(const QuestionId& id, std::string& error);
    bool setStatus(const QuestionId& id, QuestionStatus status, std::string& error);
};

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

class QuizManager {
    DataStore& data_;
    const UserManager& users_;
    std::optional<QuizSession> session_;
    std::size_t sessionEpoch_ = 0;

  public:
    QuizManager(DataStore& data, const UserManager& users);
    bool start(const LevelId& level, std::size_t count, std::mt19937& engine, std::string& error);
    bool selectAnswer(std::size_t index, char answer);
    bool submit(std::optional<QuizAttempt>& result, std::string& error);
    bool retryWrong(std::string& error);
    const QuizSession* session() const noexcept;
    std::vector<QuizAttempt> history() const;
    void stop() noexcept;
};

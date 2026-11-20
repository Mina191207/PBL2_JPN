#pragma once
#include "DataStore.h"
#include "managers/UserManager.h"
#include "domain/QuizSession.h"
#include <optional>
#include <random>
#include <string>
#include <vector>

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

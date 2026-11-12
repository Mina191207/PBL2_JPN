#pragma once
#include "DataStore.h"
#include "managers/UserManager.h"
#include <optional>
#include <string>
#include <vector>

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

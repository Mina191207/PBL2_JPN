#pragma once
#include "DataStore.h"
#include "managers/UserManager.h"
#include <string>

class LevelManager {
    DataStore& data_;
    const UserManager& users_;

  public:
    LevelManager(DataStore& data, const UserManager& users);
    bool add(const Level& level, std::string& error);
    bool update(const Level& level, std::string& error);
    bool remove(const LevelId& id, std::string& error);
};

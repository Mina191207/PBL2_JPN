#pragma once
#include "DataStore.h"
#include "managers/UserManager.h"
#include <optional>
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

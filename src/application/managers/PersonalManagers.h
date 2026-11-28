#pragma once
#include "storage/DataStore.h"
#include "application/managers/UserManager.h"
#include <string>
#include <vector>

class FavoriteManager {
    DataStore& data_;
    const UserManager& users_;

  public:
    FavoriteManager(DataStore& data, const UserManager& users);
    bool add(const ResourceId& resourceId, std::string& error);
    bool remove(const ResourceId& resourceId, std::string& error);
    std::vector<Favorite> list() const;
};

class HistoryManager {
    DataStore& data_;
    const UserManager& users_;

  public:
    HistoryManager(DataStore& data, const UserManager& users);
    bool record(const SearchOptions& options, std::string& error);
    std::vector<SearchHistory> listNewest() const;
    const SearchHistory* find(const HistoryId& id) const;
    bool clear(std::string& error);
};

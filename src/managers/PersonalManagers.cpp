#include "managers/PersonalManagers.h"
#include "domain/Validation.h"
#include "domain/Time.h"

FavoriteManager::FavoriteManager(DataStore& data, const UserManager& users)
    : data_(data),
      users_(users) {
}

HistoryManager::HistoryManager(DataStore& data, const UserManager& users)
    : data_(data),
      users_(users) {
}

bool FavoriteManager::add(const ResourceId& resourceId, std::string& error) {
    const User* user = users_.currentUser();
    if (!user) {
        error = "login required";
        return false;
    }
    if (!data_.findResource(resourceId)) {
        error = "resource not found";
        return false;
    }
    for (const auto& item : data_.favorites)
        if (item.getUserId() == user->getId() && item.getResourceId() == resourceId) {
            error = "already favorite";
            return false;
        }
    data_.favorites.emplace_back(nextId<FavoriteId>(data_.favorites), user->getId(), resourceId);
    data_.dirty = true;
    return true;
}
bool FavoriteManager::remove(const ResourceId& resourceId, std::string& error) {
    const User* user = users_.currentUser();
    if (!user) {
        error = "login required";
        return false;
    }
    for (auto it = data_.favorites.begin(); it != data_.favorites.end(); ++it)
        if (it->getUserId() == user->getId() && it->getResourceId() == resourceId) {
            data_.favorites.erase(it);
            data_.dirty = true;
            return true;
        }
    error = "favorite not found";
    return false;
}
std::vector<Favorite> FavoriteManager::list() const {
    std::vector<Favorite> result;
    const User* user = users_.currentUser();
    if (user)
        for (const auto& item : data_.favorites)
            if (item.getUserId() == user->getId())
                result.push_back(item);
    return result;
}
bool HistoryManager::record(const std::string& query, std::string& error) {
    const User* user = users_.currentUser();
    if (!user) {
        error = "login required";
        return false;
    }
    if (isBlank(query))
        return true;
    std::uint64_t maxValue = 0;
    data_.histories.forEach([&](const SearchHistory& item) {
        if (item.getId().number() > maxValue)
            maxValue = item.getId().number();
    });
    try {
        data_.histories.pushBack(
            SearchHistory(idAfter<HistoryId>(maxValue), user->getId(), query, utcNow()));
        data_.dirty = true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    return true;
}
std::vector<SearchHistory> HistoryManager::listNewest() const {
    std::vector<SearchHistory> result;
    const User* user = users_.currentUser();
    if (user)
        data_.histories.forEachReverse([&](const SearchHistory& item) {
            if (item.getUserId() == user->getId())
                result.push_back(item);
        });
    return result;
}
bool HistoryManager::clear(std::string& error) {
    const User* user = users_.currentUser();
    if (!user) {
        error = "login required";
        return false;
    }
    data_.histories.eraseIf([&](const SearchHistory& item) {
        return item.getUserId() == user->getId();
    });
    data_.dirty = true;
    return true;
}

#include "managers/PersonalManagers.h"
#include "domain/Validation.h"
#include <ctime>
#include <iomanip>
#include <limits>
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
    if (maxValue == std::numeric_limits<std::uint64_t>::max()) {
        error = "id space exhausted";
        return false;
    }
    std::string digits = std::to_string(maxValue + 1);
    if (digits.size() < 3)
        digits.insert(0, 3 - digits.size(), '0');
    try {
        data_.histories.pushBack(
            SearchHistory(HistoryId("H" + digits), user->getId(), query, utcNow()));
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

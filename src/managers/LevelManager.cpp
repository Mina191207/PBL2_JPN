#include "managers/LevelManager.h"

LevelManager::LevelManager(DataStore& data, const UserManager& users)
    : data_(data),
      users_(users) {
}

bool LevelManager::add(const Level& level, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    if (data_.levelById.count(level.getId().getId())) {
        error = "level id exists";
        return false;
    }
    for (const auto& item : data_.levels)
        if (item.getCategory() == level.getCategory() && item.getName() == level.getName()) {
            error = "level name exists";
            return false;
        }
    data_.levels.push_back(level);
    data_.rebuildIndexes();
    data_.dirty = true;
    return true;
}
bool LevelManager::update(const Level& level, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    auto it = data_.levelById.find(level.getId().getId());
    if (it == data_.levelById.end()) {
        error = "level not found";
        return false;
    }
    for (const auto& item : data_.levels)
        if (item.getId() != level.getId() && item.getCategory() == level.getCategory() &&
            item.getName() == level.getName()) {
            error = "level name exists";
            return false;
        }
    if (level.getCategory() != "JLPT") {
        for (const auto& item : data_.questions)
            if (item.getLevelId() == level.getId()) {
                error = "question uses level";
                return false;
            }
        for (const auto& item : data_.attempts)
            if (item.getLevelId() == level.getId()) {
                error = "attempt uses level";
                return false;
            }
    }
    data_.levels[it->second] = level;
    data_.dirty = true;
    return true;
}
bool LevelManager::remove(const LevelId& id, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    auto it = data_.levelById.find(id.getId());
    if (it == data_.levelById.end()) {
        error = "level not found";
        return false;
    }
    for (const auto& item : data_.vocabularies)
        if (item.getLevelId() == id) {
            error = "resource uses level";
            return false;
        }
    for (const auto& item : data_.grammars)
        if (item.getLevelId() == id) {
            error = "resource uses level";
            return false;
        }
    for (const auto& item : data_.kanjis)
        if (item.getLevelId() == id) {
            error = "resource uses level";
            return false;
        }
    for (const auto& item : data_.questions)
        if (item.getLevelId() == id) {
            error = "question uses level";
            return false;
        }
    for (const auto& item : data_.attempts)
        if (item.getLevelId() == id) {
            error = "attempt uses level";
            return false;
        }
    data_.levels.erase(data_.levels.begin() + static_cast<std::ptrdiff_t>(it->second));
    data_.rebuildIndexes();
    data_.dirty = true;
    return true;
}

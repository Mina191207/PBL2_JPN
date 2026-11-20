#include "managers/AlphabetManager.h"
#include "algorithms/MergeSort.h"
#include <algorithm>
#include <utility>

AlphabetManager::AlphabetManager(DataStore& data, const UserManager& users)
    : data_(data),
      users_(users) {
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

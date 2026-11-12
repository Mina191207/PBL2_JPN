#include "managers/QuestionManager.h"
#include <algorithm>
#include <utility>

QuestionManager::QuestionManager(DataStore& data, const UserManager& users)
    : data_(data),
      users_(users) {
}

bool QuestionManager::valid(const Question& item, std::string& error) const {
    const Level* level = data_.findLevel(item.getLevelId());
    if (!level || level->getCategory() != "JLPT") {
        error = "JLPT level required";
        return false;
    }
    if (item.getResourceId()) {
        const Resource* resource = data_.findResource(*item.getResourceId());
        if (!resource || resource->getLevelId() != item.getLevelId()) {
            error = "question resource invalid";
            return false;
        }
    }
    return true;
}

bool QuestionManager::add(const Question& item, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    if (data_.questionById.count(item.getId().getId())) {
        error = "question id exists";
        return false;
    }
    if (!valid(item, error))
        return false;
    data_.questions.push_back(item);
    data_.rebuildIndexes();
    data_.dirty = true;
    return true;
}

bool QuestionManager::update(const Question& item, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    auto it = data_.questionById.find(item.getId().getId());
    if (it == data_.questionById.end()) {
        error = "question not found";
        return false;
    }
    if (!valid(item, error))
        return false;
    data_.questions[it->second] = item;
    data_.dirty = true;
    return true;
}

bool QuestionManager::remove(const QuestionId& id, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    auto it = data_.questionById.find(id.getId());
    if (it == data_.questionById.end()) {
        error = "question not found";
        return false;
    }
    data_.questions.erase(data_.questions.begin() + static_cast<std::ptrdiff_t>(it->second));
    data_.rebuildIndexes();
    data_.dirty = true;
    return true;
}

bool QuestionManager::setStatus(const QuestionId& id, QuestionStatus status, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    auto it = data_.questionById.find(id.getId());
    if (it == data_.questionById.end()) {
        error = "question not found";
        return false;
    }
    if (status != QuestionStatus::ACTIVE && status != QuestionStatus::INACTIVE) {
        error = "invalid status";
        return false;
    }
    data_.questions[it->second].setStatus(status);
    data_.dirty = true;
    return true;
}

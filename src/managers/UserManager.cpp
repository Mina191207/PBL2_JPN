#include "managers/UserManager.h"
#include <algorithm>

UserManager::UserManager(DataStore& data)
    : data_(data) {
}

void UserManager::logout() noexcept {
    current_.reset();
    ++epoch_;
}

const User* UserManager::currentUser() const noexcept {
    return current_ ? data_.findUser(*current_) : nullptr;
}

std::size_t UserManager::sessionEpoch() const noexcept {
    return epoch_;
}

bool UserManager::login(const std::string& username, const std::string& password) {
    current_.reset();
    ++epoch_;
    auto it = data_.userByName.find(DataStore::lowerAscii(username));
    if (it == data_.userByName.end())
        return false;
    const User& user = data_.users[it->second];
    if (user.getStatus() != UserStatus::ACTIVE || !user.checkPassword(password))
        return false;
    current_ = user.getId();
    return true;
}
bool UserManager::isAdmin() const noexcept {
    const User* user = currentUser();
    return user && user->getRole() == UserRole::ADMIN && user->getStatus() == UserStatus::ACTIVE;
}
bool UserManager::create(const User& user, std::string& error) {
    if (!isAdmin() && !(data_.users.empty() && user.getRole() == UserRole::ADMIN &&
                        user.getStatus() == UserStatus::ACTIVE)) {
        error = "admin required";
        return false;
    }
    if (data_.userById.count(user.getId().getId()) ||
        data_.userByName.count(DataStore::lowerAscii(user.getUsername()))) {
        error = "user id or username already exists";
        return false;
    }
    data_.users.push_back(user);
    data_.rebuildIndexes();
    data_.dirty = true;
    return true;
}
bool UserManager::setStatus(const UserId& id, UserStatus status, std::string& error) {
    if (!isAdmin()) {
        error = "admin required";
        return false;
    }
    auto it = data_.userById.find(id.getId());
    if (it == data_.userById.end()) {
        error = "user not found";
        return false;
    }
    if (status != UserStatus::ACTIVE && status != UserStatus::LOCKED) {
        error = "invalid status";
        return false;
    }
    User& user = data_.users[it->second];
    if (status == UserStatus::LOCKED) {
        if (currentUser()->getId() == id) {
            error = "cannot lock current user";
            return false;
        }
        if (user.getRole() == UserRole::ADMIN && user.getStatus() == UserStatus::ACTIVE) {
            int active = 0;
            for (const auto& entry : data_.users)
                if (entry.getRole() == UserRole::ADMIN && entry.getStatus() == UserStatus::ACTIVE)
                    ++active;
            if (active == 1) {
                error = "cannot lock last admin";
                return false;
            }
        }
    }
    user.setStatus(status);
    data_.dirty = true;
    return true;
}
bool UserManager::resetPassword(const UserId& id, const std::string& password, std::string& error) {
    if (!isAdmin()) {
        error = "admin required";
        return false;
    }
    auto it = data_.userById.find(id.getId());
    if (it == data_.userById.end()) {
        error = "user not found";
        return false;
    }
    try {
        data_.users[it->second].resetPassword(password);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    data_.dirty = true;
    return true;
}

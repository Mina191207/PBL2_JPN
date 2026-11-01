#include "domain/user.h"
#include "domain/Validation.h"
#include <stdexcept>
#include <utility>

User::User(UserId id, std::string username, std::string password, UserRole role, UserStatus status)
    : id_(std::move(id)),
      username_(std::move(username)),
      password_(std::move(password)),
      role_(role),
      status_(status) {
    validateTxtField(username_, "username");
    validateTxtField(password_, "password");
    if (username_.size() < 3 || username_.size() > 32 ||
        !((username_[0] >= 'A' && username_[0] <= 'Z') ||
          (username_[0] >= 'a' && username_[0] <= 'z'))) {
        throw std::invalid_argument("invalid username");
    }
    for (char c : username_) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '.' || c == '-')) {
            throw std::invalid_argument("invalid username");
        }
    }
    if (password_.size() > 128 || password_.front() == ' ' || password_.front() == '\t' ||
        password_.back() == ' ' || password_.back() == '\t') {
        throw std::invalid_argument("invalid password");
    }
}

const UserId& User::getId() const noexcept {
    return id_;
}
const std::string& User::getUsername() const noexcept {
    return username_;
}
UserRole User::getRole() const noexcept {
    return role_;
}
UserStatus User::getStatus() const noexcept {
    return status_;
}

bool User::checkPassword(const std::string& password) const noexcept {
    return password_ == password;
}

void User::setStatus(UserStatus status) noexcept {
    status_ = status;
}

void User::resetPassword(std::string password) {
    validateTxtField(password, "password");
    if (password.size() > 128 || password.front() == ' ' || password.front() == '\t' ||
        password.back() == ' ' || password.back() == '\t') {
        throw std::invalid_argument("invalid password");
    }
    password_ = std::move(password);
}

#pragma once
#include "EntityId.h"
#include <string>

enum class UserRole {
    ADMIN,
    LEARNER
};
enum class UserStatus {
    ACTIVE,
    LOCKED
};

class User {
    UserId id_;
    std::string username_;
    std::string password_;
    UserRole role_;
    UserStatus status_;

  public:
    User(UserId id, std::string username, std::string password, UserRole role, UserStatus status);
    const UserId& getId() const noexcept;
    const std::string& getUsername() const noexcept;
    UserRole getRole() const noexcept;
    UserStatus getStatus() const noexcept;
    bool checkPassword(const std::string& password) const noexcept;
    void setStatus(UserStatus status) noexcept;
    void resetPassword(std::string password);
    friend class FileStorage;
};

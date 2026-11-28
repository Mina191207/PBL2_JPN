#pragma once
#include "pbl2/domain/EntityId.h"
#include "pbl2/UserRole.h"
#include <stdexcept>
#include <string>

class CredentialError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class User {
    UserId id_;
    std::string username_;
    std::string passwordHash_;
    UserRole role_;
    UserStatus status_;

    struct StoredCredential {};
    User(UserId id, std::string username, std::string hash, UserRole role, UserStatus status, StoredCredential);
    static User restore(UserId id, std::string username, const std::string& hash, UserRole role, UserStatus status);
    bool rehashPassword(const std::string& verifiedPassword);
    friend class FileStorage;
    friend class UserManager;

  public:
    User(UserId id, std::string username, const std::string& password, UserRole role, UserStatus status);
    const UserId& getId() const noexcept;
    const std::string& getUsername() const noexcept;
    UserRole getRole() const noexcept;
    UserStatus getStatus() const noexcept;
    bool checkPassword(const std::string& password) const;
    void setStatus(UserStatus status);
    void resetPassword(const std::string& password);
};

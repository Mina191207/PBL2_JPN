#pragma once
#include "DataStore.h"
#include <optional>
#include <string>

class UserManager {
    DataStore& data_;
    std::optional<UserId> current_;
    std::size_t epoch_ = 0;

  public:
    explicit UserManager(DataStore& data);
    bool login(const std::string& username, const std::string& password);
    void logout() noexcept;
    const User* currentUser() const noexcept;
    std::size_t sessionEpoch() const noexcept;
    bool isAdmin() const noexcept;
    bool create(const User& user, std::string& error);
    bool setStatus(const UserId& id, UserStatus status, std::string& error);
    bool resetPassword(const UserId& id, const std::string& password, std::string& error);
};

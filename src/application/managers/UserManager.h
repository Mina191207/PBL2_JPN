#pragma once
#include "storage/DataStore.h"
#include <optional>
#include <string>

class UserManager {
    DataStore& data_;
    std::optional<UserId> current_;
    std::size_t epoch_ = 0;
    bool replacePassword(User& user, const std::string& password, std::string& error);

  public:
    explicit UserManager(DataStore& data);
    bool login(const std::string& username, const std::string& password, bool* rehashed = nullptr);
    void logout() noexcept;
    const User* currentUser() const noexcept;
    std::size_t sessionEpoch() const noexcept;
    bool isAdmin() const noexcept;
    bool create(const User& user, std::string& error);
    bool setStatus(const UserId& id, UserStatus status, std::string& error);
    bool resetPassword(const UserId& id, const std::string& password, std::string& error);
    bool changePassword(const std::string& currentPassword, const std::string& newPassword, std::string& error);
};

#pragma once
#include "storage/DataStore.h"
#include <filesystem>
#include <string>

struct UserMigration {
    std::size_t accounts = 0;
    bool migrated = false;
};

class FileStorage {
    static bool readUsers(const std::filesystem::path& file, DataStore& data, bool allowLegacy);
    static std::string userRows(const std::vector<User>& users);
    static bool saveTables(const std::string& folder, const DataStore& data, StoreTable& pending, std::string& error);

  public:
    bool loadAll(const std::string& folder, DataStore& out, std::string& error) const;
    bool saveAll(const std::string& folder, const DataStore& data, std::string& error) const;
    bool saveDirty(const std::string& folder, DataStore& data, std::string& error) const;
    bool saveUsers(const std::string& folder, const std::vector<User>& users, std::string& error) const;
    bool migrateUsers(const std::filesystem::path& file, UserMigration& result, std::string& error) const;
    bool loadPackage(const std::string& folder, DataStore& out, std::string& error) const;
    bool savePackage(const std::string& folder, const DataStore& data, std::string& error) const;
};

#pragma once
#include "storage/DataStore.h"
#include <cstdint>
#include <filesystem>
#include <string>

struct UserMigration {
    std::size_t accounts = 0;
    bool migrated = false;
};

class StoreLock {
    std::intptr_t handle_;

  public:
    static constexpr char filename[] = ".pbl2.lock";
    const std::filesystem::path root;

    explicit StoreLock(const std::filesystem::path& folder, bool create = false);
    ~StoreLock();
    StoreLock(const StoreLock&) = delete;
    StoreLock& operator=(const StoreLock&) = delete;
};

class FileStorage {
    static bool readUsers(const std::filesystem::path& file, DataStore& data, bool allowLegacy);
    static std::string userRows(const std::vector<User>& users);
    static bool saveTables(const StoreLock& lock, const DataStore& data, StoreTable tables, std::string& error);
    static void readPackage(const std::filesystem::path& root, DataStore& out);

  public:
    bool loadAll(const std::string& folder, DataStore& out, std::string& error) const;
    bool loadAll(const StoreLock& lock, DataStore& out, std::string& error, bool* empty = nullptr) const;
    bool saveAll(const std::string& folder, const DataStore& data, std::string& error) const;
    bool saveDirty(const StoreLock& lock, DataStore& data, std::string& error) const;
    bool isEmpty(const StoreLock& lock) const;
    bool migrateUsers(const std::filesystem::path& file, UserMigration& result, std::string& error) const;
    bool loadPackage(const std::string& folder, DataStore& out, std::string& error) const;
    bool loadPackage(const StoreLock& lock, DataStore& out, std::string& error) const;
    std::vector<std::string> existingPackageFiles(const std::filesystem::path& folder) const;
    bool savePackage(const std::string& folder, const DataStore& data, std::string& error,
                     const std::vector<std::string>* approvedFiles = nullptr) const;
};

#ifdef PBL2_STORAGE_TESTING
enum class StorageTestPoint { REPLACED, COMMITTED, RESTORED };
extern void (*storageTestHook)(StorageTestPoint);
#endif

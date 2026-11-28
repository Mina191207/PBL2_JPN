#include "storage/FileStorage.h"
#include <iostream>

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    if (argc != 2) {
        std::cerr << "Usage: pbl2_migrate_users <path-to-user.txt> (close the app first)\n";
        return 2;
    }
    UserMigration result;
    std::string error;
    try {
        if (FileStorage{}.migrateUsers(std::filesystem::path(argv[1]), result, error)) {
            std::cout << (result.migrated ? "Migrated to V2: " : "Already V2: ")
                      << result.accounts << " accounts\n";
            return 0;
        }
    } catch (const std::exception&) {
        error = "user.txt: migration failed";
    }
    std::cerr << error << '\n';
    return 1;
}

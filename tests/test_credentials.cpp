#include "pbl2/Application.h"
#include "Fixture.h"
#include "storage/Paths.h"
#include "PasswordInput.h"
#include <sodium.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <iterator>
#include <map>
#include <sstream>
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#include <sddl.h>
#else
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
constexpr char marker[] = "PBL2_USERS_V2\n";
constexpr char secret[] = "Fixture-Aa\t\xe6\x97\xa5";
constexpr char replacement[] = "Fixture-reset-secret";

std::string bytes(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    CHECK(stream);
    return {std::istreambuf_iterator<char>(stream), {}};
}

auto otherFiles(const fs::path& folder) {
    std::map<fs::path, std::pair<std::string, fs::file_time_type>> result;
    for (const auto& entry : fs::directory_iterator(folder)) {
        if (entry.path().filename() == "user.txt" || entry.path().filename() == StoreLock::filename ||
            !entry.is_regular_file()) continue;
        result.emplace(entry.path().filename(), std::make_pair(bytes(entry.path()), entry.last_write_time()));
    }
    return result;
}

std::string rows(std::string_view first, std::string_view second, std::string_view third) {
    return "U001|Admin|" + std::string(first) + "|ADMIN|ACTIVE\n" +
           "U002|learner1|" + std::string(second) + "|LEARNER|ACTIVE\n" +
           "U003|learner2|" + std::string(third) + "|LEARNER|LOCKED\n";
}

std::string encodedFromRow(const std::string& contents, const std::string& prefix) {
    auto start = contents.find(prefix);
    CHECK(start != std::string::npos);
    start += prefix.size();
    const auto end = contents.find('|', start);
    CHECK(end != std::string::npos);
    return contents.substr(start, end - start);
}

std::string earlierHash(std::string_view password) {
    // fixture() or Application::initialize has already initialized the backend library.
    char encoded[crypto_pwhash_argon2id_STRBYTES];
    CHECK(crypto_pwhash_argon2id_str(encoded, password.data(), password.size(), 2, 19456 * 1024) == 0);
    return encoded;
}

void passwordField() {
    PasswordInput field;
    field.append('A');
    field.append(0x65E5);
    CHECK(field.value().size() == 4 && field.masked() == "**");
    field.backspace();
    CHECK(field.value() == "A");
    for (std::size_t i = field.value().size(); i < maxPasswordBytes; ++i) field.append('b');
    CHECK(!field.tooLong() && field.value().size() == maxPasswordBytes);
    field.append('c');
    CHECK(field.tooLong());
    field.backspace();
    CHECK(!field.tooLong() && field.value().size() == maxPasswordBytes - 1);
    field.clear();
    CHECK(field.value().empty() && field.masked().empty());
}

void accountFlow() {
    TempDirectory temp;
    const fs::path file = temp.path / "user.txt";
    const auto folder = pathToUtf8(temp.path);
    Application app;
    CHECK(app.initialize(folder, "fixture-admin", secret).save == SaveState::SAVED);
    CHECK(app.login("fixture-admin", secret).ok());
    const auto initial = otherFiles(temp.path);
    CHECK(initial.size() == 10);
    const auto blocker = temp.path / "alphabet.txt.tmp";
    CHECK(fs::create_directory(blocker));

    const auto one = app.createAccount("fixture-learner", secret, UserRole::LEARNER);
    CHECK(one.ok() && one.status.save == SaveState::SAVED && !app.dirty());
    const auto contents = bytes(file);
    const auto hashOne = encodedFromRow(contents, "U001|fixture-admin|");
    const auto hashTwo = encodedFromRow(contents, "U002|fixture-learner|");
    CHECK(hashOne.find("$argon2id$v=19$m=65536,t=2,p=1$") == 0 && hashOne != hashTwo);
    CHECK(contents.find(secret) == std::string::npos);
    CHECK(app.login("fixture-learner", secret).ok());
    CHECK(!app.login("fixture-learner", std::string(secret) + " ").ok());
    CHECK(!app.login("fixture-learner", std::string(secret) + '\0').ok());
    auto wrong = std::string(secret);
    wrong[0] = 'f';
    CHECK(!app.login("fixture-learner", wrong).ok());
    CHECK(!app.currentAccount().ok());
    CHECK(app.login("fixture-admin", secret).ok());

    const auto bad = std::string(replacement) + ' ';
    CHECK(!app.createAccount("fixture-invalid", bad, UserRole::LEARNER).ok());
    CHECK(!app.resetPassword(*one.value, bad).ok());
    CHECK(bytes(file) == contents);
    CHECK(app.resetPassword(*one.value, replacement).save == SaveState::SAVED);
    CHECK(app.setAccountStatus(*one.value, UserStatus::LOCKED).save == SaveState::SAVED);
    auto locked = app.login("fixture-learner", replacement);
    auto unknown = app.login("fixture-unknown", replacement);
    auto invalid = app.login("fixture-admin", replacement);
    CHECK(locked.error == BackendError::REJECTED && locked.error == unknown.error && locked.error == invalid.error);
    CHECK(locked.message == unknown.message && locked.message == invalid.message && !app.currentAccount().ok());
    CHECK(locked.message.find(replacement) == std::string::npos);
    CHECK(app.login("fixture-admin", secret).ok());
    CHECK(app.setAccountStatus(*one.value, UserStatus::ACTIVE).save == SaveState::SAVED);
    CHECK(otherFiles(temp.path) == initial);
    CHECK(app.close().ok());

    Application reopened;
    CHECK(reopened.load(folder).ok());
    CHECK(!reopened.login("fixture-learner", secret).ok());
    CHECK(reopened.login("fixture-learner", replacement).ok());
    CHECK(reopened.currentAccount().value->id == *one.value);
    CHECK(reopened.close().ok());

    // A failed level save is still pending after a subsequent account command.
    CHECK(app.load(folder).ok() && app.login("fixture-admin", secret).ok());
    CHECK(fs::remove(blocker));
    const auto levelBlocker = temp.path / ".pbl2-stage";
    writeFile(levelBlocker, "occupied");
    auto level = app.createLevel({"JLPT", "N5", 1, ""});
    CHECK(level.ok() && level.status.save == SaveState::PENDING);
    CHECK(app.resetPassword(*one.value, secret).save == SaveState::PENDING);
    CHECK(app.dirty() && app.close().error == BackendError::UNSAVED_CHANGES);
    CHECK(fs::remove(levelBlocker));
    CHECK(app.retrySave().save == SaveState::SAVED && !app.dirty());
    CHECK(app.load(folder).ok() && app.login("fixture-learner", secret).ok());
    CHECK(app.levels().value->size() == 1);
}

void ownPasswordFlow() {
    TempDirectory temp;
    saveFixture(temp.path, fixture());
    const auto folder = temp.path.u8string();
    const auto file = temp.path / "user.txt";
    const auto before = bytes(file);
    const auto other = otherFiles(temp.path);
    Application app;
    CHECK(app.changePassword("demo123", secret).error == BackendError::NOT_LOADED);
    CHECK(app.load(folder).ok());
    CHECK(app.changePassword("demo123", secret).error == BackendError::LOGIN_REQUIRED);
    CHECK(app.login("learner1", "demo123").ok());
    CHECK(app.resetPassword(UserId("U003"), secret).error == BackendError::FORBIDDEN);
    CHECK(!app.changePassword("wrong", secret).ok());
    CHECK(!app.changePassword("demo123", "bad ").ok());
    CHECK(!app.changePassword("demo123", std::string(maxPasswordBytes + 1, 'a')).ok());
    CHECK(bytes(file) == before && !app.dirty());
    const auto blocker = temp.path / ".pbl2-stage";
    writeFile(blocker, "occupied");
    const auto changed = app.changePassword("demo123", secret);
    CHECK(changed.ok() && changed.save == SaveState::PENDING && app.dirty());
    CHECK(bytes(file) == before && app.retrySave().save == SaveState::PENDING);
    CHECK(app.currentAccount().value->id == UserId("U002"));
    CHECK(app.close().error == BackendError::UNSAVED_CHANGES);
    CHECK(fs::remove(blocker));
    CHECK(app.retrySave().save == SaveState::SAVED && !app.dirty());
    const auto after = bytes(file);
    CHECK(after.find(secret) == std::string::npos);
    for (const auto* row : {"U001|Admin|", "U003|learner2|"})
        CHECK(encodedFromRow(after, row) == encodedFromRow(before, row));
    CHECK(otherFiles(temp.path) == other);
    app.logout();
    CHECK(app.changePassword(secret, replacement).error == BackendError::LOGIN_REQUIRED);
    CHECK(app.close().ok() && app.load(folder).ok());
    CHECK(!app.login("learner1", "demo123").ok() && app.login("learner1", secret).ok());
    CHECK(app.login("learner2", "demo123").ok() && !app.login("learner2", secret).ok());
    CHECK(app.login("Admin", "demo123").ok());
    CHECK(app.setAccountStatus(UserId("U002"), UserStatus::LOCKED).save == SaveState::SAVED);
    CHECK(app.resetPassword(UserId("U002"), replacement).save == SaveState::SAVED);
    CHECK(!app.login("learner1", replacement).ok());
    CHECK(app.changePassword(replacement, secret).error == BackendError::LOGIN_REQUIRED);
    CHECK(app.login("Admin", "demo123").ok());
    CHECK(app.setAccountStatus(UserId("U002"), UserStatus::ACTIVE).save == SaveState::SAVED);
    CHECK(!app.login("learner1", secret).ok() && app.login("learner1", replacement).ok());
}

#ifdef _WIN32
struct BlockReplace {
    HANDLE handle;
    explicit BlockReplace(const fs::path& path)
        : handle(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) {
        CHECK(handle != INVALID_HANDLE_VALUE);
    }
    ~BlockReplace() { CloseHandle(handle); }
};

void checkWriteDenied(const fs::path& folder, const fs::path& file, const std::string& source) {
    DWORD required = 0;
    CHECK(!GetFileSecurityW(folder.c_str(), DACL_SECURITY_INFORMATION, nullptr, 0, &required));
    CHECK(required != 0);
    std::vector<unsigned char> original(required);
    CHECK(GetFileSecurityW(folder.c_str(), DACL_SECURITY_INFORMATION, original.data(), required, &required));
    PSECURITY_DESCRIPTOR deny = nullptr;
    CHECK(ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:(D;;0x2;;;WD)(A;;FA;;;WD)", SDDL_REVISION_1, &deny, nullptr));
    const bool applied = SetFileSecurityW(folder.c_str(), DACL_SECURITY_INFORMATION, deny);
    LocalFree(deny);
    CHECK(applied);
    std::string error;
    UserMigration result;
    const bool ok = FileStorage{}.migrateUsers(file, result, error);
    CHECK(SetFileSecurityW(folder.c_str(), DACL_SECURITY_INFORMATION, original.data()));
    CHECK(!ok && error.find("cannot open temp") != std::string::npos);
    CHECK(bytes(file) == source);
}
#else
void checkWriteDenied(const fs::path& folder, const fs::path& file, const std::string& source) {
    if (geteuid() == 0) return; // Root can create files in a read-only directory.
    const auto before = fs::status(folder).permissions();
    fs::permissions(folder, fs::perms::owner_write, fs::perm_options::remove);
    std::string error;
    UserMigration result;
    const bool ok = FileStorage{}.migrateUsers(file, result, error);
    fs::permissions(folder, before);
    CHECK(!ok && error.find("cannot open temp") != std::string::npos);
    CHECK(bytes(file) == source);
}
#endif

void migrationFlow() {
    TempDirectory temp;
    const auto folder = temp.path / pathFromUtf8("fixture-\xe6\x97\xa5");
    const auto file = folder / "user.txt";
    const auto dataPath = pathToUtf8(folder);
    std::string error;
    FileStorage storage;
    CHECK(storage.saveAll(dataPath, fixture(), error));
    const auto disguised = earlierHash("synthetic legacy input");
    const auto legacy = rows(secret, disguised, replacement);
    const auto originalV2 = bytes(file);
    writeFile(folder / "user.txt.tmp-stale", originalV2);
    const auto others = otherFiles(folder);
    writeFile(file, legacy);
    Application app;
    auto v1 = app.load(dataPath);
    CHECK(v1.error == BackendError::IO_ERROR && v1.message.find("migration required") != std::string::npos);
    CHECK(!app.loaded() && app.login("admin", secret).error == BackendError::NOT_LOADED);
    CHECK(!app.initialize(dataPath, "fixture-admin", secret).ok() && bytes(file) == legacy);
    checkWriteDenied(folder, file, legacy);
    CHECK(otherFiles(folder) == others);
    UserMigration result;
#ifdef _WIN32
    {
        const BlockReplace block(file);
        CHECK(!storage.migrateUsers(file, result, error));
        CHECK(error == "cannot replace user.txt" && bytes(file) == legacy);
        CHECK(otherFiles(folder) == others);
    }
#endif
    const auto malformed = legacy + "U004|ADMIN|fixture-pass|ADMIN|ACTIVE\n";
    writeFile(file, malformed);
    CHECK(!storage.migrateUsers(file, result, error));
    CHECK(error.find("line 4") != std::string::npos && error.find("duplicate username") != std::string::npos);
    CHECK(bytes(file) == malformed && otherFiles(folder) == others);
    for (const auto& bad : {"U000|Admin|fixture-pass|ADMIN|ACTIVE\n",
                            "U001|Admin|fixture-pass|ADMIN|LOCKED\n",
                            "U001|Admin|fixture-pass|UNKNOWN|ACTIVE\n",
                            "U001|Admin|fixture-pass|ADMIN|UNKNOWN\n",
                            "U001|Admin|fixture-pass|ADMIN|ACTIVE\nU001|Other|fixture-pass|LEARNER|ACTIVE\n"}) {
        writeFile(file, bad);
        CHECK(!storage.migrateUsers(file, result, error));
        CHECK(bytes(file) == bad && otherFiles(folder) == others);
    }
    writeFile(file, legacy);
    CHECK(storage.migrateUsers(file, result, error) && result.migrated && result.accounts == 3);
    const auto v2 = bytes(file);
    CHECK(v2.find(marker) == 0 && v2.find(disguised) == std::string::npos && v2.find(secret) == std::string::npos);
    CHECK(otherFiles(folder) == others);
    CHECK(storage.migrateUsers(file, result, error) && !result.migrated && result.accounts == 3);
    CHECK(bytes(file) == v2 && otherFiles(folder) == others);
    CHECK(app.load(dataPath).ok() && app.login("ADMIN", secret).ok());
    const auto accounts = app.accounts();
    CHECK(accounts.ok() && accounts.value->size() == 3);
    CHECK(accounts.value->at(0).id == UserId("U001") && accounts.value->at(0).username == "Admin");
    CHECK(accounts.value->at(0).role == UserRole::ADMIN && accounts.value->at(0).status == UserStatus::ACTIVE);
    CHECK(accounts.value->at(1).id == UserId("U002") && accounts.value->at(1).username == "learner1");
    CHECK(accounts.value->at(1).role == UserRole::LEARNER && accounts.value->at(1).status == UserStatus::ACTIVE);
    CHECK(accounts.value->at(2).id == UserId("U003") && accounts.value->at(2).username == "learner2");
    CHECK(accounts.value->at(2).role == UserRole::LEARNER && accounts.value->at(2).status == UserStatus::LOCKED);
    CHECK(!app.login("learner2", replacement).ok() && !app.currentAccount().ok());
    CHECK(app.login("learner1", disguised).ok());

    const auto hash = encodedFromRow(v2, "U001|Admin|");
    auto memory = hash; memory.replace(memory.find("65536"), 5, "999999");
    auto parallel = hash; parallel.replace(parallel.find("p=1"), 3, "p=2");
    auto version = hash; version.replace(version.find("v=19"), 4, "v=16");
    auto iterations = hash; iterations.replace(iterations.find("t=2"), 3, "t=9");
    auto algorithm = hash; algorithm.replace(1, 8, "argon2i");
    auto alphabet = hash; alphabet.back() = '!';
    auto truncated = hash; truncated.pop_back();
    for (const auto& bad : {std::string(secret), memory, parallel, version, iterations, algorithm, alphabet, truncated}) {
        const auto corrupt = marker + rows(bad, hash, hash);
        writeFile(file, corrupt);
        const auto load = app.load(dataPath);
        CHECK(!load.ok() && load.message.find("user.txt, line 2: invalid or unsupported password hash") == 0);
        CHECK(app.currentAccount().value->id == UserId("U002"));
        CHECK(!storage.migrateUsers(file, result, error) && error.find("already in use") != std::string::npos);
        CHECK(bytes(file) == corrupt && otherFiles(folder) == others);
    }
    CHECK(app.close().ok());
    CHECK(!storage.migrateUsers(file, result, error) && error.find("invalid or unsupported password hash") != std::string::npos);

    // Only user.txt is needed to migrate, regardless of the learning store.
    const auto separate = temp.path / "user-only";
    CHECK(fs::create_directory(separate));
    writeFile(separate / "user.txt", legacy);
    CHECK(storage.migrateUsers(separate / "user.txt", result, error) && result.migrated);
    CHECK(otherFiles(separate).empty());
}

void rehashFlow() {
    TempDirectory temp;
    const auto file = temp.path / "user.txt";
    const auto folder = pathToUtf8(temp.path);
    Application app;
    CHECK(app.initialize(folder, "fixture-admin", secret).save == SaveState::SAVED);
    CHECK(app.close().ok());
    const auto previous = earlierHash(secret);
    const std::string oldFile = std::string(marker) + "U001|fixture-admin|" + previous + "|ADMIN|ACTIVE\n";
    writeFile(file, oldFile);
    const auto other = otherFiles(temp.path);
    CHECK(app.load(folder).ok());
    CHECK(!app.login("fixture-admin", replacement).ok() && bytes(file) == oldFile);
    const auto blocker = temp.path / "alphabet.txt.tmp";
    CHECK(fs::create_directory(blocker));
#ifdef _WIN32
    {
        const BlockReplace block(file);
        const auto status = app.login("fixture-admin", secret);
        CHECK(status.ok() && status.save == SaveState::PENDING && app.dirty());
        CHECK(app.currentAccount().ok() && bytes(file) == oldFile);
        app.logout();
        CHECK(app.close().error == BackendError::UNSAVED_CHANGES);
        CHECK(app.retrySave().error == BackendError::IO_ERROR);
    }
    CHECK(app.retrySave().save == SaveState::SAVED && !app.dirty());
#else
    CHECK(app.login("fixture-admin", secret).save == SaveState::SAVED);
#endif
    const auto upgraded = bytes(file);
    CHECK(upgraded != oldFile && upgraded.find("$argon2id$v=19$m=65536,t=2,p=1$") != std::string::npos);
    CHECK(upgraded.find(secret) == std::string::npos && otherFiles(temp.path) == other);
    CHECK(app.load(folder).ok());
    CHECK(app.login("fixture-admin", secret).save == SaveState::UNCHANGED);
    CHECK(bytes(file) == upgraded);
}

void benchmark() {
    std::array<double, 5> hashTimes{}, verifyTimes{};
    for (std::size_t i = 0; i < hashTimes.size(); ++i) {
        const auto start = Clock::now();
        const User user(UserId("U001"), "fixture-admin", secret, UserRole::ADMIN, UserStatus::ACTIVE);
        const auto hashed = Clock::now();
        CHECK(user.checkPassword(secret));
        const auto verified = Clock::now();
        hashTimes[i] = std::chrono::duration<double, std::milli>(hashed - start).count();
        verifyTimes[i] = std::chrono::duration<double, std::milli>(verified - hashed).count();
    }
    std::sort(hashTimes.begin(), hashTimes.end());
    std::sort(verifyTimes.begin(), verifyTimes.end());
    std::cout << "Argon2id median of 5: hash " << hashTimes[2] << " ms, verify " << verifyTimes[2] << " ms\n";
}
}

int main() {
    try {
        std::ostringstream output, errors;
        auto* previousOut = std::cout.rdbuf(output.rdbuf());
        auto* previousErr = std::cerr.rdbuf(errors.rdbuf());
        try {
            passwordField();
            accountFlow();
            ownPasswordFlow();
            migrationFlow();
            rehashFlow();
        } catch (...) {
            std::cout.rdbuf(previousOut);
            std::cerr.rdbuf(previousErr);
            throw;
        }
        std::cout.rdbuf(previousOut);
        std::cerr.rdbuf(previousErr);
        CHECK(output.str().empty() && errors.str().empty());
        benchmark();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

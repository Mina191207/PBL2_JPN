#include "storage/FileStorage.h"
#include "storage/Paths.h"
#include "domain/Validation.h"
#include "domain/Time.h"
#include <sodium.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <sddl.h>
#include <random>
#else
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <cstdlib>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef PBL2_STORAGE_TESTING
void (*storageTestHook)(StorageTestPoint) = nullptr;
#endif

namespace {
namespace fs = std::filesystem;
using Fields = std::vector<std::string>;
using Rows = std::map<StoreTable, std::string>;
constexpr char userFile[] = "user.txt";
constexpr char userMarker[] = "PBL2_USERS_V2";
constexpr char stageName[] = ".pbl2-stage";
constexpr char rollbackName[] = ".pbl2-rollback";

fs::path storeRoot(const fs::path& folder, bool create) {
    if (create) fs::create_directories(folder);
    if (!fs::exists(folder)) throw std::runtime_error("data directory does not exist");
    const auto root = fs::canonical(folder);
    if (!fs::is_directory(root)) throw std::runtime_error("data path is not a directory");
    return root;
}

bool emptyStore(const fs::path& root) {
    for (const auto& entry : fs::directory_iterator(root))
        if (entry.path().filename() != StoreLock::filename) return false;
    return true;
}

constexpr std::array tableFiles{
    std::pair{StoreTable::ALPHABET, "alphabet.txt"},
    std::pair{StoreTable::FAVORITES, "favorites.txt"},
    std::pair{StoreTable::GRAMMAR, "grammar.txt"},
    std::pair{StoreTable::KANJI, "kanji.txt"},
    std::pair{StoreTable::RESOURCES, "learning_resource.txt"},
    std::pair{StoreTable::QUESTIONS, "question.txt"},
    std::pair{StoreTable::ATTEMPTS, "quiz_attempt.txt"},
    std::pair{StoreTable::LEVELS, "resource_levels.txt"},
    std::pair{StoreTable::HISTORY, "search_history.txt"},
    std::pair{StoreTable::USERS, userFile},
    std::pair{StoreTable::VOCABULARY, "vocabulary.txt"}
};

const char* fileName(StoreTable key) {
    for (const auto& [table, name] : tableFiles)
        if (table == key) return name;
    throw std::logic_error("unknown storage table");
}

Rows emptyRows(StoreTable tables) {
    Rows rows;
    for (const auto& [table, name] : tableFiles)
        if ((tables & table) != StoreTable::NONE) rows.try_emplace(table);
    return rows;
}

struct WipeText {
    std::string& text;
    bool enabled = true;
    ~WipeText() {
        if (enabled) sodium_memzero(text.data(), text.size());
    }
};

Fields split(const std::string& line) {
    Fields fields;
    std::size_t begin = 0;
    while (true) {
        auto end = line.find('|', begin);
        if (end == std::string::npos) {
            fields.push_back(line.substr(begin));
            break;
        }
        fields.push_back(line.substr(begin, end - begin));
        begin = end + 1;
    }
    return fields;
}
int integer(const std::string& text, int min, int max) {
    if (text.empty() || (text.size() > 1 && text.front() == '0'))
        throw std::invalid_argument("invalid integer");
    int value = 0;
    for (char c : text) {
        if (c < '0' || c > '9' || value > (max - (c - '0')) / 10)
            throw std::invalid_argument("invalid integer");
        value = value * 10 + (c - '0');
    }
    if (value < min)
        throw std::invalid_argument("integer out of range");
    return value;
}
template <class F>
void readLines(const fs::path& path, const char* name, F consume, bool secret = false) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error(std::string(name) + ": cannot open file");
    file.seekg(0, std::ios::end);
    if (file.tellg() > 32 * 1024 * 1024)
        throw std::runtime_error(std::string(name) + ": file too large");
    file.seekg(0);
    std::size_t number = 0;
    std::string line;
    if (secret) line.reserve(16384 + 4); // Record, initial BOM and optional CR.
    for (;;) {
        WipeText wipe{line, secret};
        if (!std::getline(file, line)) break;
        ++number;
        try {
            if (number == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
                line.erase(0, 3);
            if (!file.eof() && !line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.size() > 16384 || !validUtf8(line) ||
                line.find_first_of("\r\0", 0, 2) != std::string::npos)
                throw std::invalid_argument("invalid line encoding or length");
            consume(line, number);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string(name) + ", line " + std::to_string(number) + ": " +
                                     e.what());
        }
    }
    if (file.bad() || !file.eof())
        throw std::runtime_error(std::string(name) + ": read failed");
}
void validateColumns(const Fields& fields, std::size_t columns) {
    if (fields.size() != columns) throw std::invalid_argument("wrong column count");
    for (const auto& field : fields)
        if (field.size() > maxTxtFieldBytes) throw std::invalid_argument("field too long");
}
template <class F>
void readRows(const fs::path& folder, const char* name, std::size_t columns, F consume) {
    readLines(folder / name, name, [&](const std::string& line, std::size_t) {
        if (line.empty()) return;
        const auto fields = split(line);
        validateColumns(fields, columns);
        consume(fields);
    });
}
ResourceType resourceType(const std::string& value) {
    if (value == "VOCABULARY")
        return ResourceType::VOCABULARY;
    if (value == "GRAMMAR")
        return ResourceType::GRAMMAR;
    if (value == "KANJI")
        return ResourceType::KANJI;
    throw std::invalid_argument("invalid resource type");
}
constexpr std::array searchSortTokens{"RELEVANCE", "ID", "LEVEL_ASC", "LEVEL_DESC", "TERM_ASC", "TERM_DESC"};

SearchSort searchSort(const std::string& value) {
    const auto found = std::find(searchSortTokens.begin(), searchSortTokens.end(), value);
    if (found == searchSortTokens.end()) throw std::invalid_argument("invalid search sort");
    return static_cast<SearchSort>(found - searchSortTokens.begin());
}
const char* token(ResourceType value) {
    switch (value) {
    case ResourceType::VOCABULARY:
        return "VOCABULARY";
    case ResourceType::GRAMMAR:
        return "GRAMMAR";
    case ResourceType::KANJI:
        return "KANJI";
    }
    return "";
}
PartOfSpeech partOfSpeech(const std::string& value) {
    if (value == "NOUN")
        return PartOfSpeech::NOUN;
    if (value == "VERB")
        return PartOfSpeech::VERB;
    if (value == "I_ADJ")
        return PartOfSpeech::IADJECTIVE;
    if (value == "NA_ADJ")
        return PartOfSpeech::NAADJECTIVE;
    if (value == "ADV")
        return PartOfSpeech::ADVERB;
    if (value == "OTHER")
        return PartOfSpeech::OTHER;
    throw std::invalid_argument("invalid part of speech");
}
void append(Rows& rows, StoreTable table, const Fields& fields) {
    std::string line;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].size() > maxTxtFieldBytes || fields[i].find_first_of("|\r\n") != std::string::npos ||
            fields[i].find('\0') != std::string::npos || !validUtf8(fields[i]))
            throw std::invalid_argument(std::string(fileName(table)) + ": invalid field");
        if (i)
            line += '|';
        line += fields[i];
    }
    if (line.size() > 16384)
        throw std::invalid_argument(std::string(fileName(table)) + ": line too long");
    auto& content = rows.at(table);
    if (content.size() + line.size() + 1 > 32 * 1024 * 1024)
        throw std::invalid_argument(std::string(fileName(table)) + ": file too large");
    content += line + '\n';
}
bool replaceFile(const fs::path& from, const fs::path& to) {
#ifdef _WIN32
    return MoveFileExW(
               from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code ec;
    fs::rename(from, to, ec);
    return !ec;
#endif
}

struct StagedFile {
    fs::path path;
    StagedFile() = default;
    StagedFile(const StagedFile&) = delete;
    StagedFile& operator=(const StagedFile&) = delete;

    ~StagedFile() {
        if (path.empty()) return;
        std::error_code ignored;
        fs::remove(path, ignored);
    }

    void write(const fs::path& folder, StoreTable table, const std::string& bytes) {
        const bool users = table == StoreTable::USERS;
        const std::string name = fileName(table);
        // Exclusive creation protects existing paths, including links to another store.
#ifdef _WIN32
        fs::path candidate = folder / (name + ".tmp");
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, FALSE};
        if (users) {
            std::random_device random;
            candidate = folder / (name + ".tmp-" + std::to_string(random()) + "-" + std::to_string(random()));
            if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
                    L"D:P(A;;GA;;;OW)(A;;GA;;;BA)(A;;GA;;;SY)", SDDL_REVISION_1, &descriptor, nullptr))
                throw std::runtime_error("user.txt: cannot set temp permissions");
            security.lpSecurityDescriptor = descriptor;
        }
        const HANDLE file = CreateFileW(candidate.c_str(), GENERIC_WRITE, 0, users ? &security : nullptr,
                                       CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (descriptor) LocalFree(descriptor);
        if (file == INVALID_HANDLE_VALUE) throw std::runtime_error(name + ": cannot open temp");
        path.swap(candidate);
        DWORD written = 0;
        const bool flushed = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
            written == bytes.size() && FlushFileBuffers(file);
        const bool closed = CloseHandle(file);
#else
        std::string candidate = (folder / (name + (users ? ".tmp-XXXXXX" : ".tmp"))).native();
        const int fd = users ? mkstemp(candidate.data()) : open(candidate.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
        if (fd < 0) throw std::runtime_error(name + ": cannot open temp");
        try {
            path = candidate;
        } catch (...) {
            close(fd);
            unlink(candidate.c_str());
            throw;
        }
        std::FILE* file = fdopen(fd, "wb");
        if (!file) {
            close(fd);
            throw std::runtime_error(name + ": cannot open temp stream");
        }
        const bool flushed = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size() &&
            std::fflush(file) == 0 && fsync(fd) == 0;
        const bool closed = std::fclose(file) == 0;
#endif
        if (!flushed || !closed) throw std::runtime_error(name + ": cannot write or close temp");
    }
};

bool journalExists(const fs::path& path) {
    const auto status = fs::symlink_status(path);
    if (!fs::exists(status)) return false;
    if (!fs::is_directory(status)
#ifdef _WIN32
        || (GetFileAttributesW(path.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0
#endif
    ) throw std::runtime_error(path.filename().string() + ": expected a journal directory");
    return true;
}

void syncDirectory(const fs::path& path) {
#ifndef _WIN32
    const int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) throw std::runtime_error("cannot open storage directory for flush");
    const bool synced = fsync(fd) == 0;
    close(fd);
    if (!synced) throw std::runtime_error("cannot flush storage directory");
#else
    (void)path; // Replacements use MOVEFILE_WRITE_THROUGH on Windows.
#endif
}

void cleanStage(const fs::path& root) {
    const auto stage = root / stageName;
    if (!journalExists(stage)) return;
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(stage)) {
        const auto name = pathToUtf8(entry.path().filename());
        bool known = name.rfind("user.txt.tmp-", 0) == 0;
        for (const auto& table : tableFiles) {
            const std::string base = table.second;
            known = known || name == base || name == base + ".old" ||
                    name == base + ".absent" || name == base + ".tmp";
        }
        if (!known) throw std::runtime_error("unexpected file in storage staging directory");
        files.push_back(entry.path());
    }
    syncDirectory(root);
    for (const auto& file : files) fs::remove(file);
    fs::remove(stage);
}

void tryCleanStage(const fs::path& root) {
    try { cleanStage(root); } catch (const std::exception&) { /* No rollback depends on this directory. */ }
}

void finishJournal(const fs::path& root) {
    // This rename is the commit point, or the end of a completed rollback.
    if (!replaceFile(root / rollbackName, root / stageName))
        throw std::runtime_error("cannot finalize storage journal");
}

void recover(const fs::path& root) {
    const auto journal = root / rollbackName;
    if (!journalExists(journal)) return;
    for (const auto& entry : tableFiles) {
        const std::string name = entry.second;
        // An unconsumed staged file means the original was never replaced.
        if (fs::exists(fs::symlink_status(journal / name))) continue;
        const auto old = journal / (name + ".old");
        const auto live = root / name;
        if (fs::exists(fs::symlink_status(old))) {
            if (!replaceFile(old, live)) throw std::runtime_error("cannot restore " + name);
        } else if (fs::exists(journal / (name + ".absent"))) {
            if (fs::is_directory(fs::symlink_status(live))) throw std::runtime_error("cannot restore " + name);
            fs::remove(live);
        } else continue;
#ifdef PBL2_STORAGE_TESTING
        if (storageTestHook) storageTestHook(StorageTestPoint::RESTORED);
#endif
    }
    syncDirectory(journal);
    syncDirectory(root);
    finishJournal(root);
}

void backupTable(const fs::path& root, const fs::path& stage, StoreTable table) {
    const std::string name = fileName(table);
    const auto live = root / name;
    const auto status = fs::symlink_status(live);
    if (!fs::exists(status)) {
        fs::create_directory(stage / (name + ".absent"));
        return;
    }
    const auto backup = stage / (name + ".old");
    if (fs::is_symlink(status)) {
        fs::copy_symlink(live, backup);
        return;
    }
    if (!fs::is_regular_file(status)) throw std::runtime_error("cannot replace " + name);
    std::error_code linkError;
    fs::create_hard_link(live, backup, linkError);
    if (!linkError) return;

    // Filesystems without hard links still get flushed backups and private user staging.
    std::ifstream input(live, std::ios::binary);
    if (!input) throw std::runtime_error("cannot back up " + name);
    const std::string bytes((std::istreambuf_iterator<char>(input)), {});
    StagedFile copy;
    copy.write(stage, table, bytes);
    if (!replaceFile(copy.path, backup)) throw std::runtime_error("cannot back up " + name);
}

bool writeRows(const fs::path& root, const Rows& rows, std::string& error, bool rollbackOnFailure = false) {
    bool journalActive = false;
    try {
        recover(root);
        cleanStage(root);
        const auto stage = root / stageName;
        const bool multi = rows.size() > 1;
        const auto prepared = multi ? root / rollbackName : stage;
        fs::create_directory(stage);
        for (const auto& [table, bytes] : rows) {
            if (multi) backupTable(root, stage, table);
            StagedFile temp;
            temp.write(stage, table, bytes);
            if (!replaceFile(temp.path, stage / fileName(table)))
                throw std::runtime_error("cannot stage " + std::string(fileName(table)));
        }
        syncDirectory(stage);
        if (multi) {
            if (!replaceFile(stage, prepared)) throw std::runtime_error("cannot activate storage journal");
            journalActive = true;
            syncDirectory(root);
        }
        for (const auto& row : rows) {
            const std::string name = fileName(row.first);
            if (!replaceFile(prepared / name, root / name)) throw std::runtime_error("cannot replace " + name);
#ifdef PBL2_STORAGE_TESTING
            if (multi && storageTestHook) storageTestHook(StorageTestPoint::REPLACED);
#endif
        }
        if (multi) {
            syncDirectory(prepared);
            syncDirectory(root);
            finishJournal(root);
        }
    } catch (const fs::filesystem_error& e) {
        error = rows.size() == 1 && rows.count(StoreTable::USERS) ? "user.txt: filesystem operation failed" : e.what();
    } catch (const std::exception& e) {
        error = e.what();
    }
    if (!error.empty()) {
        if (rollbackOnFailure && journalActive) {
            try { recover(root); }
            catch (const std::exception& e) {
                error += "; rollback pending: ";
                error += e.what();
            }
        }
        tryCleanStage(root);
        return false;
    }
#ifdef PBL2_STORAGE_TESTING
    if (storageTestHook) storageTestHook(StorageTestPoint::COMMITTED);
#endif
    tryCleanStage(root);
    return true;
}
void fillPackageRows(const DataStore& data, Rows& rows) {
    if (rows.count(StoreTable::RESOURCES)) {
        const auto headers = [&](const auto& items) {
            for (const auto& item : items)
                append(rows, StoreTable::RESOURCES,
                       {item.getId().getId(), token(item.getType()), item.getLevelId().getId()});
        };
        headers(data.vocabularies);
        headers(data.grammars);
        headers(data.kanjis);
    }
    if (rows.count(StoreTable::LEVELS)) {
        for (const auto& item : data.levels)
            append(rows, StoreTable::LEVELS, {item.getId().getId(), item.getCategory(),
                item.getName(), std::to_string(item.getOrder()), item.getDescription()});
    }
    if (rows.count(StoreTable::VOCABULARY)) {
        for (const auto& item : data.vocabularies)
            append(rows, StoreTable::VOCABULARY, {item.getId().getId(), item.getWord(), item.getReading(),
                item.getMeaning(), partOfSpeechToken(item.getPartOfSpeech()), item.getExample()});
    }
    if (rows.count(StoreTable::GRAMMAR)) {
        for (const auto& item : data.grammars)
            append(rows, StoreTable::GRAMMAR, {item.getId().getId(), item.getPattern(), item.getMeaning(),
                item.getStructure(), item.getUsage(), item.getExample(), item.getNote()});
    }
    if (rows.count(StoreTable::KANJI)) {
        for (const auto& item : data.kanjis)
            append(rows, StoreTable::KANJI, {item.getId().getId(), item.getCharacter(), item.getMeaning(),
                std::to_string(item.getStrokeCount()), item.getOnyomi(), item.getKunyomi(), item.getExamples()});
    }
}

}

StoreLock::StoreLock(const fs::path& folder, bool create) : root(storeRoot(folder, create)) {
    const auto file = root / filename;
#ifdef _WIN32
    const HANDLE handle = CreateFileW(file.c_str(), GENERIC_READ, 0, nullptr, OPEN_ALWAYS,
                                      FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const auto reason = GetLastError();
        if (reason == ERROR_SHARING_VIOLATION)
            throw std::runtime_error("data directory is already in use by another session");
        throw std::runtime_error("cannot lock data directory (Windows error " + std::to_string(reason) + ")");
    }
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(handle, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
        CloseHandle(handle);
        throw std::runtime_error("invalid data directory lock file");
    }
    handle_ = reinterpret_cast<std::intptr_t>(handle);
#else
    const int fd = open(file.c_str(), O_RDONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK, 0666);
    if (fd < 0) throw std::runtime_error("cannot open data directory lock file");
    struct stat info{};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) {
        close(fd);
        throw std::runtime_error("invalid data directory lock file");
    }
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        const int reason = errno;
        close(fd);
        if (reason == EWOULDBLOCK) throw std::runtime_error("data directory is already in use by another session");
        throw std::runtime_error("cannot lock data directory");
    }
    handle_ = fd;
#endif
}

StoreLock::~StoreLock() {
    // Keep the lock file in place so future opens use the same OS file identity.
#ifdef _WIN32
    CloseHandle(reinterpret_cast<HANDLE>(handle_));
#else
    close(static_cast<int>(handle_));
#endif
}

bool FileStorage::readUsers(const fs::path& file, DataStore& data, bool allowLegacy) {
    bool v2 = false;
    readLines(file, userFile, [&](const std::string& line, std::size_t number) {
        if (number == 1) {
            v2 = line == userMarker;
            if (v2) return;
            if (line.compare(0, 11, "PBL2_USERS_") == 0)
                throw std::invalid_argument("unsupported user file version");
            if (!allowLegacy)
                throw std::invalid_argument("migration required; run pbl2_migrate_users with the app closed");
        }
        if (line.empty()) return;
        auto fields = split(line);
        if (fields.size() != 5) {
            for (auto& field : fields) sodium_memzero(field.data(), field.size());
            throw std::invalid_argument("wrong column count");
        }
        WipeText wipe{fields[2], !v2};
        validateColumns(fields, 5);
        const auto& f = fields;
        if (f[3] != "ADMIN" && f[3] != "LEARNER") throw std::invalid_argument("invalid user role");
        if (f[4] != "ACTIVE" && f[4] != "LOCKED") throw std::invalid_argument("invalid user status");
        const auto role = f[3] == "ADMIN" ? UserRole::ADMIN : UserRole::LEARNER;
        const auto status = f[4] == "ACTIVE" ? UserStatus::ACTIVE : UserStatus::LOCKED;
        if (!data.userById.emplace(f[0], data.users.size()).second)
            throw std::invalid_argument("duplicate user id");
        if (!data.userByName.emplace(DataStore::lowerAscii(f[1]), data.users.size()).second)
            throw std::invalid_argument("duplicate username");
        data.users.push_back(v2 ? User::restore(UserId(f[0]), f[1], f[2], role, status) :
                                 User(UserId(f[0]), f[1], f[2], role, status));
    }, true);
    if (!v2 && !allowLegacy)
        throw std::invalid_argument("user.txt, line 1: migration required; run pbl2_migrate_users with the app closed");
    try {
        data.validateRelations(true);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument(std::string("user.txt: ") + e.what());
    }
    return v2;
}

std::string FileStorage::userRows(const std::vector<User>& users) {
    Rows rows;
    rows[StoreTable::USERS] = std::string(userMarker) + '\n';
    for (const auto& item : users)
        append(rows, StoreTable::USERS, {item.getId().getId(), item.getUsername(), item.passwordHash_,
            item.getRole() == UserRole::ADMIN ? "ADMIN" : "LEARNER",
            item.getStatus() == UserStatus::ACTIVE ? "ACTIVE" : "LOCKED"});
    return std::move(rows[StoreTable::USERS]);
}

bool FileStorage::isEmpty(const StoreLock& lock) const {
    recover(lock.root);
    cleanStage(lock.root);
    return emptyStore(lock.root);
}

bool FileStorage::migrateUsers(const fs::path& file, UserMigration& result, std::string& error) const {
    result = {};
    error.clear();
    try {
        if (file.filename() != userFile)
            throw std::invalid_argument("user.txt: expected an existing regular file");
        const StoreLock lock(fs::absolute(file).parent_path());
        recover(lock.root);
        const auto source = lock.root / userFile;
        if (!fs::is_regular_file(fs::symlink_status(source)))
            throw std::invalid_argument("user.txt: expected an existing regular file");
        DataStore staged;
        const bool v2 = readUsers(source, staged, true);
        if (!v2) {
            StagedFile temp;
            temp.write(lock.root, StoreTable::USERS, userRows(staged.users));
            if (!replaceFile(temp.path, source)) throw std::runtime_error("cannot replace user.txt");
        }
        result = {staged.users.size(), !v2};
        return true;
    } catch (const fs::filesystem_error&) {
        // File system errors can contain arbitrary path components.
        error = "user.txt: filesystem operation failed";
        return false;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

bool FileStorage::loadPackage(const std::string& folder, DataStore& out, std::string& error) const {
    try {
        const StoreLock lock(pathFromUtf8(folder));
        return loadPackage(lock, out, error);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

bool FileStorage::loadPackage(const StoreLock& lock, DataStore& out, std::string& error) const {
    error.clear();
    try {
        recover(lock.root);
        tryCleanStage(lock.root);
        readPackage(lock.root, out);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

void FileStorage::readPackage(const fs::path& root, DataStore& out) {
    DataStore staged;
    readRows(root, "resource_levels.txt", 5, [&](const Fields& f) {
        staged.levels.emplace_back(LevelId(f[0]), f[1], f[2], integer(f[3], 1, (std::numeric_limits<int>::max)()), f[4]);
    });
    struct Header {
        ResourceType type;
        LevelId level;
        bool seen = false;
    };
    std::map<std::string, Header> headers;
    readRows(root, "learning_resource.txt", 3, [&](const Fields& f) {
        ResourceId id(f[0]);
        if (!headers.emplace(f[0], Header{resourceType(f[1]), LevelId(f[2]), false}).second)
            throw std::invalid_argument("duplicate resource header");
    });
    auto header = [&](const std::string& id, ResourceType type) -> Header& {
        ResourceId parsed(id);
        auto it = headers.find(id);
        if (it == headers.end() || it->second.type != type || it->second.seen)
            throw std::invalid_argument("detail missing header, wrong type or duplicate");
        it->second.seen = true;
        return it->second;
    };
    readRows(root, "vocabulary.txt", 6, [&](const Fields& f) {
        auto& h = header(f[0], ResourceType::VOCABULARY);
        staged.vocabularies.emplace_back(
            ResourceId(f[0]), h.level, f[1], f[2], f[3], partOfSpeech(f[4]), f[5]);
    });
    readRows(root, "grammar.txt", 7, [&](const Fields& f) {
        auto& h = header(f[0], ResourceType::GRAMMAR);
        staged.grammars.emplace_back(
            ResourceId(f[0]), h.level, f[1], f[2], f[3], f[4], f[5], f[6]);
    });
    readRows(root, "kanji.txt", 7, [&](const Fields& f) {
        auto& h = header(f[0], ResourceType::KANJI);
        staged.kanjis.emplace_back(
            ResourceId(f[0]), h.level, f[1], f[2], integer(f[3], 1, 255), f[4], f[5], f[6]);
    });
    for (const auto& entry : headers)
        if (!entry.second.seen)
            throw std::invalid_argument("resource header missing detail: " + entry.first);
    staged.rebuildIndexes();
    staged.validateRelations();
    out.swap(staged);
}

bool FileStorage::loadAll(const std::string& folder, DataStore& out, std::string& error) const {
    try {
        const StoreLock lock(pathFromUtf8(folder));
        return loadAll(lock, out, error);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

bool FileStorage::loadAll(const StoreLock& lock, DataStore& out, std::string& error, bool* empty) const {
    error.clear();
    DataStore staged;
    try {
        const auto& root = lock.root;
        recover(root);
        tryCleanStage(root);
        if (emptyStore(root)) {
            if (empty) *empty = true;
            error = "Data directory is empty. Create a database and its first Admin to use it.";
            return false;
        }
        DataStore accounts;
        readUsers(root / userFile, accounts, false);
        readPackage(root, staged);
        staged.users.swap(accounts.users);
        readRows(root, "alphabet.txt", 7, [&](const Fields& f) {
            Script script;
            if (f[1] == "HIRAGANA")
                script = Script::HIRAGANA;
            else if (f[1] == "KATAKANA")
                script = Script::KATAKANA;
            else
                throw std::invalid_argument("invalid script");
            staged.alphabet.emplace_back(
                AlphabetId(f[0]), script, f[2], f[3], f[4], integer(f[5], 1, (std::numeric_limits<int>::max)()), f[6]);
        });
        readRows(root, "question.txt", 11, [&](const Fields& f) {
            std::optional<ResourceId> resource;
            if (!f[2].empty())
                resource.emplace(f[2]);
            QuestionStatus status;
            if (f[10] == "ACTIVE")
                status = QuestionStatus::ACTIVE;
            else if (f[10] == "INACTIVE")
                status = QuestionStatus::INACTIVE;
            else
                throw std::invalid_argument("invalid question status");
            if (f[8].size() != 1)
                throw std::invalid_argument("invalid correct option");
            staged.questions.emplace_back(QuestionId(f[0]),
                                          LevelId(f[1]),
                                          resource,
                                          f[3],
                                          std::array<std::string, 4>{f[4], f[5], f[6], f[7]},
                                          f[8][0],
                                          f[9],
                                          status);
        });
        readRows(root, "favorites.txt", 3, [&](const Fields& f) {
            staged.favorites.emplace_back(FavoriteId(f[0]), UserId(f[1]), ResourceId(f[2]));
        });
        readLines(root / "search_history.txt", "search_history.txt", [&](const std::string& line, std::size_t) {
            if (line.empty()) return;
            const auto fields = split(line);
            const bool legacy = fields.size() == 4;
            validateColumns(fields, legacy ? 4 : 8);
            const HistoryId id(fields[0]);
            const UserId user(fields[1]);
            if (legacy) {
                staged.histories.pushBack(SearchHistory(id, user, fields[2], fields[3]));
                return;
            }
            if (fields[4] != "CONTENT" && fields[4] != "ID")
                throw std::invalid_argument("invalid search mode");
            SearchOptions options{fields[2], fields[4] == "ID", {}, {}, searchSort(fields[7])};
            if (!fields[5].empty()) options.type = resourceType(fields[5]);
            if (!fields[6].empty()) options.levelId = LevelId(fields[6]);
            staged.histories.pushBack(SearchHistory(id, user, std::move(options), fields[3]));
        });
        readRows(root, "quiz_attempt.txt", 6, [&](const Fields& f) {
            if (!validUtcTimestamp(f[5]))
                throw std::invalid_argument("invalid timestamp");
            staged.attempts.emplace_back(AttemptId(f[0]),
                                         UserId(f[1]),
                                         LevelId(f[2]),
                                         integer(f[3], 1, 50),
                                         integer(f[4], 0, 50),
                                         f[5]);
        });
        staged.rebuildIndexes();
        staged.validateRelations(true);
        out.swap(staged);
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

std::vector<std::string> FileStorage::existingPackageFiles(const fs::path& folder) const {
    std::vector<std::string> files;
    for (const auto& [table, name] : tableFiles) {
        if ((StoreTable::PACKAGE & table) == StoreTable::NONE) continue;
        if (fs::exists(fs::symlink_status(folder / name))) files.emplace_back(name);
    }
    return files;
}

bool FileStorage::savePackage(const std::string& folder,
                              const DataStore& data,
                              std::string& error,
                              const std::vector<std::string>* approvedFiles) const {
    error.clear();
    try {
        DataStore checked(data);
        checked.rebuildIndexes();
        checked.validateRelations();
        auto rows = emptyRows(StoreTable::PACKAGE);
        fillPackageRows(data, rows);
        const StoreLock lock(pathFromUtf8(folder), true);
        if (approvedFiles) {
            for (const auto& file : existingPackageFiles(lock.root)) {
                if (std::find(approvedFiles->begin(), approvedFiles->end(), file) != approvedFiles->end()) continue;
                error = "Export destination changed. Review the files and export again.";
                return false;
            }
        }
        return writeRows(lock.root, rows, error, true);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

bool FileStorage::saveAll(const std::string& folder,
                          const DataStore& data,
                          std::string& error) const {
    error.clear();
    try {
        DataStore checked(data);
        checked.rebuildIndexes();
        checked.validateRelations(true);
        const StoreLock lock(pathFromUtf8(folder), true);
        return saveTables(lock, data, StoreTable::ALL, error);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

bool FileStorage::saveDirty(const StoreLock& lock, DataStore& data, std::string& error) const {
    if (!saveTables(lock, data, data.dirtyTables, error)) return false;
    data.dirtyTables = StoreTable::NONE;
    return true;
}

bool FileStorage::saveTables(const StoreLock& lock, const DataStore& data, StoreTable tables,
                             std::string& error) {
    error.clear();
    if (tables == StoreTable::NONE) return true;
    try {
        auto rows = emptyRows(tables);
        fillPackageRows(data, rows);
        if (rows.count(StoreTable::USERS)) rows[StoreTable::USERS] = userRows(data.users);
        if (rows.count(StoreTable::FAVORITES)) {
            for (const auto& item : data.favorites)
                append(rows, StoreTable::FAVORITES,
                       {item.getId().getId(), item.getUserId().getId(), item.getResourceId().getId()});
        }
        if (rows.count(StoreTable::HISTORY)) {
            data.histories.forEach([&](const SearchHistory& item) {
                Fields fields{item.getId().getId(), item.getUserId().getId(), item.getQuery(), item.getSearchedAt()};
                if (item.hasSearchOptions()) {
                    const auto& options = item.getOptions();
                    fields.push_back(options.exactId ? "ID" : "CONTENT");
                    fields.push_back(options.type ? token(*options.type) : "");
                    fields.push_back(options.levelId ? options.levelId->getId() : "");
                    fields.push_back(searchSortTokens[static_cast<std::size_t>(options.sort)]);
                }
                append(rows, StoreTable::HISTORY, fields);
            });
        }
        if (rows.count(StoreTable::ALPHABET)) {
            for (const auto& item : data.alphabet)
                append(rows, StoreTable::ALPHABET, {item.getId().getId(),
                    item.getScript() == Script::HIRAGANA ? "HIRAGANA" : "KATAKANA", item.getGlyph(),
                    item.getRomaji(), item.getGroupName(), std::to_string(item.getDisplayOrder()), item.getNote()});
        }
        if (rows.count(StoreTable::QUESTIONS)) {
            for (const auto& item : data.questions) {
                const auto& options = item.getOptions();
                append(rows, StoreTable::QUESTIONS, {item.getId().getId(), item.getLevelId().getId(),
                    item.getResourceId() ? item.getResourceId()->getId() : "", item.getPrompt(),
                    options[0], options[1], options[2], options[3], std::string(1, item.getCorrectOption()),
                    item.getExplanation(), item.getStatus() == QuestionStatus::ACTIVE ? "ACTIVE" : "INACTIVE"});
            }
        }
        if (rows.count(StoreTable::ATTEMPTS)) {
            for (const auto& item : data.attempts)
                append(rows, StoreTable::ATTEMPTS, {item.getId().getId(), item.getUserId().getId(),
                    item.getLevelId().getId(), std::to_string(item.getTotalQuestions()),
                    std::to_string(item.getCorrectAnswers()), item.getSubmittedAt()});
        }
        return writeRows(lock.root, rows, error);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

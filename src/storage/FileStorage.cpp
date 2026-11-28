#include "storage/FileStorage.h"
#include "storage/Paths.h"
#include "domain/Validation.h"
#include "domain/Time.h"
#include <sodium.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <sddl.h>
#include <random>
#else
#include <cstdio>
#include <fcntl.h>
#include <cstdlib>
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;
using Fields = std::vector<std::string>;
using Rows = std::map<StoreTable, std::string>;
constexpr char userFile[] = "user.txt";
constexpr char userMarker[] = "PBL2_USERS_V2";

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
            written == bytes.size() && (!users || FlushFileBuffers(file));
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
            std::fflush(file) == 0 && (!users || fsync(fd) == 0);
        const bool closed = std::fclose(file) == 0;
#endif
        if (!flushed || !closed) throw std::runtime_error(name + ": cannot write or close temp");
    }
};

bool writeRows(const fs::path& folder, const Rows& rows, std::string& error, StoreTable* pending = nullptr) {
    try {
        fs::create_directories(folder);
        std::map<StoreTable, StagedFile> staged;
        for (const auto& [table, bytes] : rows)
            staged.try_emplace(table).first->second.write(folder, table, bytes);

        for (auto& [table, temp] : staged) {
            const auto* name = fileName(table);
            if (!replaceFile(temp.path, folder / name))
                throw std::runtime_error("cannot replace " + std::string(name));
            temp.path.clear();
            if (pending) *pending = *pending & ~table;
        }
        return true;
    } catch (const fs::filesystem_error& e) {
        error = rows.size() == 1 && rows.count(StoreTable::USERS) ? "user.txt: filesystem operation failed" : e.what();
        return false;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
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

bool FileStorage::saveUsers(const std::string& folder, const std::vector<User>& users, std::string& error) const {
    error.clear();
    try {
        DataStore checked;
        checked.users = users;
        checked.rebuildIndexes();
        checked.validateRelations(true);
        return writeRows(pathFromUtf8(folder), {{StoreTable::USERS, userRows(users)}}, error);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

bool FileStorage::migrateUsers(const fs::path& file, UserMigration& result, std::string& error) const {
    result = {};
    error.clear();
    try {
        if (file.filename() != userFile || !fs::is_regular_file(fs::symlink_status(file)))
            throw std::invalid_argument("user.txt: expected an existing regular file");
        DataStore staged;
        const bool v2 = readUsers(file, staged, true);
        if (!v2 && !saveUsers(pathToUtf8(fs::absolute(file).parent_path()), staged.users, error))
            return false;
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
    error.clear();
    try {
        DataStore staged;
        const fs::path root = pathFromUtf8(folder);
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
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

bool FileStorage::loadAll(const std::string& folder, DataStore& out, std::string& error) const {
    error.clear();
    DataStore staged;
    try {
        const fs::path root = pathFromUtf8(folder);
        DataStore accounts;
        readUsers(root / userFile, accounts, false);
        if (!loadPackage(folder, staged, error)) return false;
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
        readRows(root, "search_history.txt", 4, [&](const Fields& f) {
            if (!validUtcTimestamp(f[3]))
                throw std::invalid_argument("invalid timestamp");
            staged.histories.pushBack(SearchHistory(HistoryId(f[0]), UserId(f[1]), f[2], f[3]));
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

bool FileStorage::savePackage(const std::string& folder,
                              const DataStore& data,
                              std::string& error) const {
    error.clear();
    try {
        DataStore checked(data);
        checked.rebuildIndexes();
        checked.validateRelations();
        auto rows = emptyRows(StoreTable::PACKAGE);
        fillPackageRows(data, rows);
        return writeRows(pathFromUtf8(folder), rows, error);
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
        auto pending = StoreTable::ALL;
        return saveTables(folder, data, pending, error);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

bool FileStorage::saveDirty(const std::string& folder, DataStore& data, std::string& error) const {
    return saveTables(folder, data, data.dirtyTables, error);
}

bool FileStorage::saveTables(const std::string& folder, const DataStore& data, StoreTable& pending,
                             std::string& error) {
    error.clear();
    if (pending == StoreTable::NONE) return true;
    try {
        auto rows = emptyRows(pending);
        fillPackageRows(data, rows);
        if (rows.count(StoreTable::USERS)) rows[StoreTable::USERS] = userRows(data.users);
        if (rows.count(StoreTable::FAVORITES)) {
            for (const auto& item : data.favorites)
                append(rows, StoreTable::FAVORITES,
                       {item.getId().getId(), item.getUserId().getId(), item.getResourceId().getId()});
        }
        if (rows.count(StoreTable::HISTORY)) {
            data.histories.forEach([&](const SearchHistory& item) {
                append(rows, StoreTable::HISTORY,
                       {item.getId().getId(), item.getUserId().getId(), item.getQuery(), item.getSearchedAt()});
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
        return writeRows(pathFromUtf8(folder), rows, error, &pending);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

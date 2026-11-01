#include "storage/FileStorage.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
namespace fs = std::filesystem;
using Fields = std::vector<std::string>;
using Rows = std::map<std::string, std::string>;

bool validUtf8(const std::string& text) {
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char first = static_cast<unsigned char>(text[i]);
        if (first < 0x80) {
            ++i;
            continue;
        }
        int extra = first >= 0xC2 && first <= 0xDF   ? 1
                    : first >= 0xE0 && first <= 0xEF ? 2
                    : first >= 0xF0 && first <= 0xF4 ? 3
                                                     : -1;
        if (extra < 0 || i + static_cast<std::size_t>(extra) >= text.size())
            return false;
        unsigned code = first & (extra == 1 ? 0x1F : extra == 2 ? 0x0F : 0x07);
        for (int j = 1; j <= extra; ++j) {
            unsigned char c = static_cast<unsigned char>(text[i + static_cast<std::size_t>(j)]);
            if ((c & 0xC0) != 0x80)
                return false;
            code = (code << 6) | (c & 0x3F);
        }
        if ((extra == 1 && code < 0x80) || (extra == 2 && code < 0x800) ||
            (extra == 3 && code < 0x10000) || (code >= 0xD800 && code <= 0xDFFF) ||
            code > 0x10FFFF || code == 0xFEFF)
            return false;
        i += static_cast<std::size_t>(extra + 1);
    }
    return true;
}
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
bool timestamp(const std::string& text) {
    if (text.size() != 20 || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' || text[19] != 'Z')
        return false;
    for (std::size_t i = 0; i < 19; ++i)
        if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16 && (text[i] < '0' || text[i] > '9'))
            return false;
    int year = std::stoi(text.substr(0, 4)), month = std::stoi(text.substr(5, 2)),
        day = std::stoi(text.substr(8, 2));
    if (year < 1 || month < 1 || month > 12)
        return false;
    int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
        days[1] = 29;
    return day >= 1 && day <= days[month - 1] && std::stoi(text.substr(11, 2)) < 24 &&
           std::stoi(text.substr(14, 2)) < 60 && std::stoi(text.substr(17, 2)) < 60;
}
template <class F>
void readRows(const fs::path& folder, const char* name, std::size_t columns, F consume) {
    const fs::path path = folder / name;
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error(std::string(name) + ": cannot open file");
    file.seekg(0, std::ios::end);
    if (file.tellg() > 32 * 1024 * 1024)
        throw std::runtime_error(std::string(name) + ": file too large");
    file.seekg(0);
    std::string line;
    std::size_t number = 0;
    while (std::getline(file, line)) {
        ++number;
        try {
            if (number == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
                line.erase(0, 3);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                continue;
            if (line.size() > 16384 || !validUtf8(line) ||
                line.find_first_of("\r\0", 0, 2) != std::string::npos)
                throw std::invalid_argument("invalid line encoding or length");
            Fields fields = split(line);
            if (fields.size() != columns)
                throw std::invalid_argument("wrong column count");
            for (const auto& field : fields)
                if (field.size() > 8192)
                    throw std::invalid_argument("field too long");
            consume(fields);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string(name) + ", line " + std::to_string(number) + ": " +
                                     e.what());
        }
    }
    if (file.bad())
        throw std::runtime_error(std::string(name) + ": read failed");
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
const char* token(PartOfSpeech value) {
    switch (value) {
    case PartOfSpeech::NOUN:
        return "NOUN";
    case PartOfSpeech::VERB:
        return "VERB";
    case PartOfSpeech::IADJECTIVE:
        return "I_ADJ";
    case PartOfSpeech::NAADJECTIVE:
        return "NA_ADJ";
    case PartOfSpeech::ADVERB:
        return "ADV";
    case PartOfSpeech::OTHER:
        return "OTHER";
    }
    return "";
}
void append(Rows& rows, const char* name, const Fields& fields) {
    std::string line;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].find_first_of("|\r\n") != std::string::npos ||
            fields[i].find('\0') != std::string::npos || !validUtf8(fields[i]))
            throw std::invalid_argument(std::string(name) + ": invalid field");
        if (i)
            line += '|';
        line += fields[i];
    }
    if (line.size() > 16384)
        throw std::invalid_argument(std::string(name) + ": line too long");
    rows[name] += line + '\n';
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
bool writeRows(const fs::path& folder, const Rows& rows, std::string& error) {
    try {
        fs::create_directories(folder);
        for (const auto& entry : rows) {
            const fs::path temp = folder / (entry.first + ".tmp");
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            if (!file)
                throw std::runtime_error("cannot open " + temp.string());
            file.write(entry.second.data(), static_cast<std::streamsize>(entry.second.size()));
            file.flush();
            if (!file)
                throw std::runtime_error("cannot write " + temp.string());
            file.close();
            if (!file)
                throw std::runtime_error("cannot close " + temp.string());
        }
        for (const auto& entry : rows) {
            const fs::path temp = folder / (entry.first + ".tmp");
            if (!replaceFile(temp, folder / entry.first))
                throw std::runtime_error("cannot replace " + entry.first);
        }
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
void fillPackageRows(const DataStore& data, Rows& rows) {
    for (const char* name : {"learning_resource.txt",
                             "vocabulary.txt",
                             "grammar.txt",
                             "kanji.txt",
                             "resource_levels.txt"})
        rows[name];
    for (const auto& item : data.levels)
        append(rows,
               "resource_levels.txt",
               {item.getId().getId(),
                item.getCategory(),
                item.getName(),
                std::to_string(item.getOrder()),
                item.getDescription()});
    for (const auto& item : data.vocabularies) {
        append(rows,
               "learning_resource.txt",
               {item.getId().getId(), token(item.getType()), item.getLevelId().getId()});
        append(rows,
               "vocabulary.txt",
               {item.getId().getId(),
                item.getWord(),
                item.getReading(),
                item.getMeaning(),
                token(item.getPartOfSpeech()),
                item.getExample()});
    }
    for (const auto& item : data.grammars) {
        append(rows,
               "learning_resource.txt",
               {item.getId().getId(), token(item.getType()), item.getLevelId().getId()});
        append(rows,
               "grammar.txt",
               {item.getId().getId(),
                item.getPattern(),
                item.getMeaning(),
                item.getStructure(),
                item.getUsage(),
                item.getExample(),
                item.getNote()});
    }
    for (const auto& item : data.kanjis) {
        append(rows,
               "learning_resource.txt",
               {item.getId().getId(), token(item.getType()), item.getLevelId().getId()});
        append(rows,
               "kanji.txt",
               {item.getId().getId(),
                item.getCharacter(),
                item.getMeaning(),
                std::to_string(item.getStrokeCount()),
                item.getOnyomi(),
                item.getKunyomi(),
                item.getExamples()});
    }
}
}

bool FileStorage::loadPackage(const std::string& folder, DataStore& out, std::string& error) const {
    error.clear();
    try {
        DataStore staged;
        const fs::path root(folder);
        readRows(root, "resource_levels.txt", 5, [&](const Fields& f) {
            staged.levels.emplace_back(LevelId(f[0]), f[1], f[2], integer(f[3], 1, 1000000), f[4]);
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
    if (!loadPackage(folder, staged, error))
        return false;
    try {
        const fs::path root(folder);
        readRows(root, "user.txt", 5, [&](const Fields& f) {
            UserRole role;
            UserStatus status;
            if (f[3] == "ADMIN")
                role = UserRole::ADMIN;
            else if (f[3] == "LEARNER")
                role = UserRole::LEARNER;
            else
                throw std::invalid_argument("invalid user role");
            if (f[4] == "ACTIVE")
                status = UserStatus::ACTIVE;
            else if (f[4] == "LOCKED")
                status = UserStatus::LOCKED;
            else
                throw std::invalid_argument("invalid user status");
            staged.users.emplace_back(UserId(f[0]), f[1], f[2], role, status);
        });
        readRows(root, "alphabet.txt", 7, [&](const Fields& f) {
            Script script;
            if (f[1] == "HIRAGANA")
                script = Script::HIRAGANA;
            else if (f[1] == "KATAKANA")
                script = Script::KATAKANA;
            else
                throw std::invalid_argument("invalid script");
            staged.alphabet.emplace_back(
                AlphabetId(f[0]), script, f[2], f[3], f[4], integer(f[5], 1, 1000000), f[6]);
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
            if (!timestamp(f[3]))
                throw std::invalid_argument("invalid timestamp");
            staged.histories.pushBack(SearchHistory(HistoryId(f[0]), UserId(f[1]), f[2], f[3]));
        });
        readRows(root, "quiz_attempt.txt", 6, [&](const Fields& f) {
            if (!timestamp(f[5]))
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
        Rows rows;
        fillPackageRows(data, rows);
        return writeRows(fs::path(folder), rows, error);
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
        data.validateRelations(true);
        Rows rows;
        fillPackageRows(data, rows);
        for (const char* name : {"user.txt",
                                 "favorites.txt",
                                 "search_history.txt",
                                 "alphabet.txt",
                                 "question.txt",
                                 "quiz_attempt.txt"})
            rows[name];
        for (const auto& item : data.users)
            append(rows,
                   "user.txt",
                   {item.getId().getId(),
                    item.getUsername(),
                    item.password_,
                    item.getRole() == UserRole::ADMIN ? "ADMIN" : "LEARNER",
                    item.getStatus() == UserStatus::ACTIVE ? "ACTIVE" : "LOCKED"});
        for (const auto& item : data.favorites)
            append(rows,
                   "favorites.txt",
                   {item.getId().getId(), item.getUserId().getId(), item.getResourceId().getId()});
        data.histories.forEach([&](const SearchHistory& item) {
            append(rows,
                   "search_history.txt",
                   {item.getId().getId(),
                    item.getUserId().getId(),
                    item.getQuery(),
                    item.getSearchedAt()});
        });
        for (const auto& item : data.alphabet)
            append(rows,
                   "alphabet.txt",
                   {item.getId().getId(),
                    item.getScript() == Script::HIRAGANA ? "HIRAGANA" : "KATAKANA",
                    item.getGlyph(),
                    item.getRomaji(),
                    item.getGroupName(),
                    std::to_string(item.getDisplayOrder()),
                    item.getNote()});
        for (const auto& item : data.questions) {
            const auto& options = item.getOptions();
            append(rows,
                   "question.txt",
                   {item.getId().getId(),
                    item.getLevelId().getId(),
                    item.getResourceId() ? item.getResourceId()->getId() : "",
                    item.getPrompt(),
                    options[0],
                    options[1],
                    options[2],
                    options[3],
                    std::string(1, item.getCorrectOption()),
                    item.getExplanation(),
                    item.getStatus() == QuestionStatus::ACTIVE ? "ACTIVE" : "INACTIVE"});
        }
        for (const auto& item : data.attempts)
            append(rows,
                   "quiz_attempt.txt",
                   {item.getId().getId(),
                    item.getUserId().getId(),
                    item.getLevelId().getId(),
                    std::to_string(item.getTotalQuestions()),
                    std::to_string(item.getCorrectAnswers()),
                    item.getSubmittedAt()});
        return writeRows(fs::path(folder), rows, error);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

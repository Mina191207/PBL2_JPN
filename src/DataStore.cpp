#include "DataStore.h"
#include <stdexcept>
#include <unordered_set>
#include <utility>

std::string DataStore::lowerAscii(std::string value) {
    for (char& c : value)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return value;
}
void DataStore::rebuildIndexes() {
    decltype(resourceById) resources;
    decltype(userById) usersById, usersByName, levelsById, questionsById;
    for (std::size_t i = 0; i < vocabularies.size(); ++i)
        if (!resources
                 .emplace(vocabularies[i].getId().getId(),
                          ResourceLocation{ResourceType::VOCABULARY, i})
                 .second)
            throw std::invalid_argument("duplicate resource id");
    for (std::size_t i = 0; i < grammars.size(); ++i)
        if (!resources
                 .emplace(grammars[i].getId().getId(), ResourceLocation{ResourceType::GRAMMAR, i})
                 .second)
            throw std::invalid_argument("duplicate resource id");
    for (std::size_t i = 0; i < kanjis.size(); ++i)
        if (!resources.emplace(kanjis[i].getId().getId(), ResourceLocation{ResourceType::KANJI, i})
                 .second)
            throw std::invalid_argument("duplicate resource id");
    for (std::size_t i = 0; i < users.size(); ++i) {
        if (!usersById.emplace(users[i].getId().getId(), i).second)
            throw std::invalid_argument("duplicate user id");
        if (!usersByName.emplace(lowerAscii(users[i].getUsername()), i).second)
            throw std::invalid_argument("duplicate username");
    }
    for (std::size_t i = 0; i < levels.size(); ++i)
        if (!levelsById.emplace(levels[i].getId().getId(), i).second)
            throw std::invalid_argument("duplicate level id");
    for (std::size_t i = 0; i < questions.size(); ++i)
        if (!questionsById.emplace(questions[i].getId().getId(), i).second)
            throw std::invalid_argument("duplicate question id");
    resourceById.swap(resources);
    userById.swap(usersById);
    userByName.swap(usersByName);
    levelById.swap(levelsById);
    questionById.swap(questionsById);
}
const Resource* DataStore::findResource(const ResourceId& id) const noexcept {
    auto it = resourceById.find(id.getId());
    if (it == resourceById.end())
        return nullptr;
    const auto& location = it->second;
    switch (location.type) {
    case ResourceType::VOCABULARY:
        return &vocabularies[location.index];
    case ResourceType::GRAMMAR:
        return &grammars[location.index];
    case ResourceType::KANJI:
        return &kanjis[location.index];
    }
    return nullptr;
}
const Level* DataStore::findLevel(const LevelId& id) const noexcept {
    auto it = levelById.find(id.getId());
    return it == levelById.end() ? nullptr : &levels[it->second];
}
const User* DataStore::findUser(const UserId& id) const noexcept {
    auto it = userById.find(id.getId());
    return it == userById.end() ? nullptr : &users[it->second];
}
void DataStore::validateRelations(bool requireAdmin) const {
    std::unordered_set<std::string> levelNames;
    for (const auto& level : levels) {
        if (!levelNames.insert(level.getCategory() + "|" + level.getName()).second) {
            throw std::invalid_argument("duplicate level name");
        }
    }
    int activeAdmins = 0;
    for (const auto& user : users)
        if (user.getRole() == UserRole::ADMIN && user.getStatus() == UserStatus::ACTIVE)
            ++activeAdmins;
    if (requireAdmin && !activeAdmins)
        throw std::invalid_argument("no active admin");
    for (const auto& item : vocabularies)
        if (!findLevel(item.getLevelId()))
            throw std::invalid_argument("resource level missing");
    for (const auto& item : grammars)
        if (!findLevel(item.getLevelId()))
            throw std::invalid_argument("resource level missing");
    for (const auto& item : kanjis)
        if (!findLevel(item.getLevelId()))
            throw std::invalid_argument("resource level missing");
    std::unordered_set<std::string> favoriteIds, pairs, historyIds, alphabetIds, alphabetPairs,
        attemptIds;
    for (const auto& item : favorites) {
        if (!favoriteIds.insert(item.getId().getId()).second)
            throw std::invalid_argument("duplicate favorite id");
        if (!findUser(item.getUserId()) || !findResource(item.getResourceId()))
            throw std::invalid_argument("favorite reference missing");
        if (!pairs.insert(item.getUserId().getId() + ":" + item.getResourceId().getId()).second)
            throw std::invalid_argument("duplicate favorite pair");
    }
    histories.forEach([&](const SearchHistory& item) {
        if (!historyIds.insert(item.getId().getId()).second)
            throw std::invalid_argument("duplicate history id");
        if (!findUser(item.getUserId()))
            throw std::invalid_argument("history user missing");
    });
    for (const auto& item : alphabet) {
        if (!alphabetIds.insert(item.getId().getId()).second) {
            throw std::invalid_argument("duplicate alphabet id");
        }
        const std::string key =
            std::to_string(static_cast<int>(item.getScript())) + ":" + item.getGlyph();
        if (!alphabetPairs.insert(key).second) {
            throw std::invalid_argument("duplicate alphabet glyph");
        }
    }
    for (const auto& item : questions) {
        const Level* level = findLevel(item.getLevelId());
        if (!level || level->getCategory() != "JLPT")
            throw std::invalid_argument("question level invalid");
        if (item.getResourceId()) {
            const Resource* resource = findResource(*item.getResourceId());
            if (!resource || resource->getLevelId() != item.getLevelId())
                throw std::invalid_argument("question resource invalid");
        }
    }
    for (const auto& item : attempts) {
        if (!attemptIds.insert(item.getId().getId()).second)
            throw std::invalid_argument("duplicate attempt id");
        const Level* level = findLevel(item.getLevelId());
        if (!findUser(item.getUserId()) || !level || level->getCategory() != "JLPT")
            throw std::invalid_argument("attempt reference invalid");
    }
}
void DataStore::swap(DataStore& other) noexcept {
    using std::swap;
    swap(vocabularies, other.vocabularies);
    swap(grammars, other.grammars);
    swap(kanjis, other.kanjis);
    swap(levels, other.levels);
    swap(users, other.users);
    swap(favorites, other.favorites);
    histories.swap(other.histories);
    swap(alphabet, other.alphabet);
    swap(questions, other.questions);
    swap(attempts, other.attempts);
    swap(dirty, other.dirty);
    swap(resourceById, other.resourceById);
    swap(userById, other.userById);
    swap(userByName, other.userByName);
    swap(levelById, other.levelById);
    swap(questionById, other.questionById);
}

#include "application/ApplicationService.h"
#include "storage/Paths.h"
#include <algorithm>
#include <filesystem>

ApplicationService::ApplicationService(DataStore& data,
                                       UserManager& users,
                                       FavoriteManager& favorites,
                                       QuizManager& quizzes,
                                       FileStorage& storage)
    : data_(data),
      users_(users),
      favorites_(favorites),
      quizzes_(quizzes),
      storage_(storage) {
}

ReviewSession* ApplicationService::review() noexcept {
    return review_ && reviewEpoch_ == users_.sessionEpoch() && users_.currentUser() ? &*review_ : nullptr;
}

void ApplicationService::logout() noexcept {
    review_.reset();
    quizzes_.stop();
    users_.logout();
}

bool ApplicationService::dirty() const noexcept {
    return data_.dirty();
}

bool ApplicationService::removeResource(const ResourceId& id, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    const Resource* resource = data_.findResource(id);
    if (!resource) {
        error = "resource not found";
        return false;
    }
    for (const auto& question : data_.questions)
        if (question.getResourceId() && *question.getResourceId() == id) {
            error = "question uses resource";
            return false;
        }
    DataStore staged(data_);
    const ResourceLocation location = staged.resourceById.at(id.getId());
    switch (location.type) {
    case ResourceType::VOCABULARY:
        staged.vocabularies.erase(staged.vocabularies.begin() +
                                  static_cast<std::ptrdiff_t>(location.index));
        staged.dirtyTables |= StoreTable::VOCABULARY;
        break;
    case ResourceType::GRAMMAR:
        staged.grammars.erase(staged.grammars.begin() +
                              static_cast<std::ptrdiff_t>(location.index));
        staged.dirtyTables |= StoreTable::GRAMMAR;
        break;
    case ResourceType::KANJI:
        staged.kanjis.erase(staged.kanjis.begin() + static_cast<std::ptrdiff_t>(location.index));
        staged.dirtyTables |= StoreTable::KANJI;
        break;
    }
    staged.favorites.erase(std::remove_if(staged.favorites.begin(),
                                          staged.favorites.end(),
                                          [&](const Favorite& favorite) {
                                              return favorite.getResourceId() == id;
                                          }),
                           staged.favorites.end());
    try {
        staged.rebuildIndexes();
        staged.validateRelations();
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    staged.dirtyTables |= StoreTable::RESOURCES;
    if (staged.favorites.size() != data_.favorites.size())
        staged.dirtyTables |= StoreTable::FAVORITES;
    data_.swap(staged);
    return true;
}
bool ApplicationService::startReview(std::string& error) {
    if (!users_.currentUser()) {
        error = "login required";
        return false;
    }
    std::vector<ReviewCard> cards;
    for (const auto& favorite : favorites_.list()) {
        const Resource* item = data_.findResource(favorite.getResourceId());
        if (!item) {
            error = "favorite resource missing";
            return false;
        }
        cards.push_back(item->makeReviewCard());
    }
    if (cards.empty()) {
        error = "no favorites";
        return false;
    }
    review_.emplace(cards);
    reviewEpoch_ = users_.sessionEpoch();
    return true;
}
bool ApplicationService::initialize(const std::string& folder, DataStore& staged) {
    auto next = std::make_unique<StoreLock>(pathFromUtf8(folder), true);
    if (!storage_.isEmpty(*next)) return false;
    staged.dirtyTables = StoreTable::ALL;
    data_.swap(staged);
    lock_ = std::move(next);
    return true;
}
bool ApplicationService::load(const std::string& folder, std::string& error, bool discardUnsaved, bool& empty) {
    if (data_.dirty() && !discardUnsaved) {
        error = "unsaved changes; save or explicitly discard before loading";
        return false;
    }
    try {
        const auto path = pathFromUtf8(folder);
        std::unique_ptr<StoreLock> next;
        if (!lock_ || !std::filesystem::equivalent(lock_->root, path))
            next = std::make_unique<StoreLock>(path);
        if (!storage_.loadAll(next ? *next : *lock_, data_, error, &empty)) return false;
        if (next) lock_ = std::move(next);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    logout();
    return true;
}
bool ApplicationService::save(std::string& error) {
    return storage_.saveDirty(*lock_, data_, error);
}
bool ApplicationService::importPackage(const std::string& folder, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    DataStore package;
    try {
        const bool same = lock_ && std::filesystem::equivalent(lock_->root, pathFromUtf8(folder));
        if (!(same ? storage_.loadPackage(*lock_, package, error)
                   : storage_.loadPackage(folder, package, error))) return false;
    } catch (const std::filesystem::filesystem_error& e) {
        error = e.what();
        return false;
    }
    DataStore staged(data_);
    for (const auto& level : package.levels) {
        const Level* old = staged.findLevel(level.getId());
        if (old) {
            if (old->getCategory() != level.getCategory() || old->getName() != level.getName() ||
                old->getOrder() != level.getOrder() ||
                old->getDescription() != level.getDescription()) {
                error = "level conflict: " + level.getId().getId();
                return false;
            }
        } else
            staged.levels.push_back(level);
    }
    auto check = [&](const ResourceId& id) {
        return staged.resourceById.count(id.getId()) == 0;
    };
    for (const auto& item : package.vocabularies)
        if (!check(item.getId())) {
            error = "resource id conflict";
            return false;
        }
    for (const auto& item : package.grammars)
        if (!check(item.getId())) {
            error = "resource id conflict";
            return false;
        }
    for (const auto& item : package.kanjis)
        if (!check(item.getId())) {
            error = "resource id conflict";
            return false;
        }
    staged.vocabularies.insert(
        staged.vocabularies.end(), package.vocabularies.begin(), package.vocabularies.end());
    staged.grammars.insert(staged.grammars.end(), package.grammars.begin(), package.grammars.end());
    staged.kanjis.insert(staged.kanjis.end(), package.kanjis.begin(), package.kanjis.end());
    try {
        staged.rebuildIndexes();
        staged.validateRelations(true);
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
    if (staged.levels.size() != data_.levels.size()) staged.dirtyTables |= StoreTable::LEVELS;
    if (!package.vocabularies.empty()) staged.dirtyTables |= StoreTable::VOCABULARY | StoreTable::RESOURCES;
    if (!package.grammars.empty()) staged.dirtyTables |= StoreTable::GRAMMAR | StoreTable::RESOURCES;
    if (!package.kanjis.empty()) staged.dirtyTables |= StoreTable::KANJI | StoreTable::RESOURCES;
    data_.swap(staged);
    return true;
}
bool ApplicationService::overlaps(const std::string& folder) const {
    return lock_ && pathsOverlap(folder, pathToUtf8(lock_->root));
}

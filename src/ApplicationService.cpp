#include "ApplicationService.h"
#include "domain/Validation.h"
#include <algorithm>
#include <filesystem>

ApplicationService::ApplicationService(DataStore& data,
                                       UserManager& users,
                                       ResourceManager& resources,
                                       FavoriteManager& favorites,
                                       HistoryManager& histories,
                                       QuizManager& quizzes,
                                       FileStorage& storage)
    : data_(data),
      users_(users),
      resources_(resources),
      favorites_(favorites),
      histories_(histories),
      quizzes_(quizzes),
      storage_(storage) {
}

ReviewSession* ApplicationService::review() noexcept {
    return review_ && reviewEpoch_ == users_.sessionEpoch() ? &*review_ : nullptr;
}

void ApplicationService::logout() noexcept {
    review_.reset();
    quizzes_.stop();
    users_.logout();
}

void ApplicationService::markDirty() noexcept {
    data_.dirty = true;
}

bool ApplicationService::dirty() const noexcept {
    return data_.dirty;
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
        break;
    case ResourceType::GRAMMAR:
        staged.grammars.erase(staged.grammars.begin() +
                              static_cast<std::ptrdiff_t>(location.index));
        break;
    case ResourceType::KANJI:
        staged.kanjis.erase(staged.kanjis.begin() + static_cast<std::ptrdiff_t>(location.index));
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
    data_.swap(staged);
    data_.dirty = true;
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
std::vector<SearchRow> ApplicationService::search(const SearchOptions& options,
                                                  std::string& historyError) {
    auto result = resources_.search(options);
    if (!isBlank(options.query) && users_.currentUser()) {
        if (histories_.record(options.query, historyError))
            data_.dirty = true;
    }
    return result;
}
bool ApplicationService::load(const std::string& folder, std::string& error) {
    if (!storage_.loadAll(folder, data_, error))
        return false;
    logout();
    activeFolder_ = folder;
    data_.dirty = false;
    return true;
}
bool ApplicationService::save(const std::string& folder, std::string& error) {
    if (!storage_.saveAll(folder, data_, error)) {
        data_.dirty = true;
        return false;
    }
    activeFolder_ = folder;
    data_.dirty = false;
    return true;
}
bool ApplicationService::importPackage(const std::string& folder, std::string& error) {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    DataStore package;
    if (!storage_.loadPackage(folder, package, error))
        return false;
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
    data_.swap(staged);
    data_.dirty = true;
    return true;
}
bool ApplicationService::exportPackage(const std::string& folder, std::string& error) const {
    if (!users_.isAdmin()) {
        error = "admin required";
        return false;
    }
    if (!activeFolder_.empty() &&
        std::filesystem::absolute(folder) == std::filesystem::absolute(activeFolder_)) {
        error = "export folder is active data folder";
        return false;
    }
    return storage_.savePackage(folder, data_, error);
}

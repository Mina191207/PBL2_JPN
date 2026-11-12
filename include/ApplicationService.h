#pragma once
#include "DataStore.h"
#include "domain/ReviewSession.h"
#include "managers/PersonalManagers.h"
#include "managers/ResourceManager.h"
#include "managers/AlphabetManager.h"
#include "managers/QuestionManager.h"
#include "managers/QuizManager.h"
#include "storage/FileStorage.h"
#include <optional>
#include <cstddef>
#include <string>
#include <vector>

class ApplicationService {
    DataStore& data_;
    UserManager& users_;
    ResourceManager& resources_;
    FavoriteManager& favorites_;
    HistoryManager& histories_;
    QuizManager& quizzes_;
    FileStorage& storage_;
    std::optional<ReviewSession> review_;
    std::size_t reviewEpoch_ = 0;
    std::string activeFolder_;

  public:
    ApplicationService(DataStore& data,
                       UserManager& users,
                       ResourceManager& resources,
                       FavoriteManager& favorites,
                       HistoryManager& histories,
                       QuizManager& quizzes,
                       FileStorage& storage);
    bool removeResource(const ResourceId& id, std::string& error);
    bool startReview(std::string& error);
    ReviewSession* review() noexcept;
    void logout() noexcept;
    std::vector<SearchRow> search(const SearchOptions& options, std::string& historyError);
    bool load(const std::string& folder, std::string& error, bool discardUnsaved = false);
    bool save(const std::string& folder, std::string& error);
    bool importPackage(const std::string& folder, std::string& error);
    bool exportPackage(const std::string& folder, std::string& error) const;
    void markDirty() noexcept;
    bool dirty() const noexcept;
};

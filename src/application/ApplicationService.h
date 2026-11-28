#pragma once
#include "storage/DataStore.h"
#include "domain/ReviewSession.h"
#include "application/managers/PersonalManagers.h"
#include "application/managers/AlphabetManager.h"
#include "application/managers/QuestionManager.h"
#include "application/managers/QuizManager.h"
#include "storage/FileStorage.h"
#include <optional>
#include <memory>
#include <cstddef>
#include <string>
#include <vector>

class ApplicationService {
    DataStore& data_;
    UserManager& users_;
    FavoriteManager& favorites_;
    QuizManager& quizzes_;
    FileStorage& storage_;
    std::optional<ReviewSession> review_;
    std::size_t reviewEpoch_ = 0;
    std::unique_ptr<StoreLock> lock_;

  public:
    ApplicationService(DataStore& data,
                       UserManager& users,
                       FavoriteManager& favorites,
                       QuizManager& quizzes,
                       FileStorage& storage);
    bool removeResource(const ResourceId& id, std::string& error);
    bool startReview(std::string& error);
    ReviewSession* review() noexcept;
    void logout() noexcept;
    bool initialize(const std::string& folder, DataStore& staged);
    bool load(const std::string& folder, std::string& error, bool discardUnsaved, bool& empty);
    bool save(std::string& error);
    bool importPackage(const std::string& folder, std::string& error);
    bool overlaps(const std::string& folder) const;
    bool dirty() const noexcept;
};

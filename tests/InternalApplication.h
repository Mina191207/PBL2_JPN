#pragma once
#include "ApplicationService.h"
#include "managers/LevelManager.h"

class InternalApplication {
    DataStore data_;
    UserManager users_;
    LevelManager levels_;
    ResourceManager resources_;
    FavoriteManager favorites_;
    HistoryManager histories_;
    AlphabetManager alphabet_;
    QuestionManager questions_;
    QuizManager quizzes_;
    FileStorage storage_;
    ApplicationService service_;

  public:
    InternalApplication();
    const DataStore& data() const noexcept;
    UserManager& users() noexcept;
    LevelManager& levels() noexcept;
    ResourceManager& resources() noexcept;
    FavoriteManager& favorites() noexcept;
    HistoryManager& histories() noexcept;
    AlphabetManager& alphabet() noexcept;
    QuestionManager& questions() noexcept;
    QuizManager& quizzes() noexcept;
    ApplicationService& service() noexcept;
};

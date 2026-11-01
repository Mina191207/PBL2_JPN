#include "Application.h"

Application::Application()
    : users_(data_),
      levels_(data_, users_),
      resources_(data_, users_),
      favorites_(data_, users_),
      histories_(data_, users_),
      alphabet_(data_, users_),
      questions_(data_, users_),
      quizzes_(data_, users_),
      service_(data_, users_, resources_, favorites_, histories_, quizzes_, storage_) {
}

const DataStore& Application::data() const noexcept {
    return data_;
}

UserManager& Application::users() noexcept {
    return users_;
}

LevelManager& Application::levels() noexcept {
    return levels_;
}

ResourceManager& Application::resources() noexcept {
    return resources_;
}

FavoriteManager& Application::favorites() noexcept {
    return favorites_;
}

HistoryManager& Application::histories() noexcept {
    return histories_;
}

AlphabetManager& Application::alphabet() noexcept {
    return alphabet_;
}

QuestionManager& Application::questions() noexcept {
    return questions_;
}

QuizManager& Application::quizzes() noexcept {
    return quizzes_;
}

ApplicationService& Application::service() noexcept {
    return service_;
}

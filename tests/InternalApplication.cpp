#include "InternalApplication.h"

InternalApplication::InternalApplication()
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

const DataStore& InternalApplication::data() const noexcept {
    return data_;
}

UserManager& InternalApplication::users() noexcept {
    return users_;
}

LevelManager& InternalApplication::levels() noexcept {
    return levels_;
}

ResourceManager& InternalApplication::resources() noexcept {
    return resources_;
}

FavoriteManager& InternalApplication::favorites() noexcept {
    return favorites_;
}

HistoryManager& InternalApplication::histories() noexcept {
    return histories_;
}

AlphabetManager& InternalApplication::alphabet() noexcept {
    return alphabet_;
}

QuestionManager& InternalApplication::questions() noexcept {
    return questions_;
}

QuizManager& InternalApplication::quizzes() noexcept {
    return quizzes_;
}

ApplicationService& InternalApplication::service() noexcept {
    return service_;
}

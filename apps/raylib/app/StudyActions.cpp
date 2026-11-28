#include "AppController.h"
#include <algorithm>
#include <type_traits>
#include <utility>

namespace {
std::string formatUtcTimestamp(const std::string& timestamp) {
    if (timestamp.size() != 20) return "Date unavailable";
    static constexpr const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    const int month = (timestamp[5] - '0') * 10 + (timestamp[6] - '0') - 1;
    if (month < 0 || month >= 12) return "Date unavailable";
    return timestamp.substr(8, 2) + " " + months[month] + " " + timestamp.substr(0, 4) +
           ", " + timestamp.substr(11, 8) + " UTC";
}}
void AppController::refreshLearnerBackendSection(Section section) {
    auto fail = [&](const Status& status) { if (!status.ok()) { backendMessage_ = status.message; backendMessageError_ = true; } };
    if (section == Section::Favorites) {
        const auto result = application_.favorites();
        fail(result.status);
        backendTable_.headers = {"ID", "Item", "Meaning", "Type", "Level"};
        if (result.value) for (const auto& detail : *result.value) std::visit([&](const auto& resource) {
            using T = std::decay_t<decltype(resource)>;
            const auto levelId = resource.getLevelId();
            std::string levelName = levelId.getId();
            for (const auto& level : resourceLevels_) if (level.getId() == levelId) { levelName = level.getName(); break; }
            const std::string type = std::is_same_v<T, Vocabulary> ? "Vocabulary" :
                                     std::is_same_v<T, Grammar> ? "Grammar" : "Kanji";
            backendTable_.rows.push_back({resource.getId(), {resource.getId().getId(), resource.getTerm(), resource.getMeaning(), type, levelName}});
        }, detail);
    } else if (section == Section::History) {
        const auto result = application_.history();
        fail(result.status);
        backendTable_.headers = {"ID", "Query", "Searched at (UTC)"};
        if (result.value) for (const auto& entry : *result.value)
            backendTable_.rows.push_back({entry.getId(), {entry.getId().getId(), entry.getQuery(), formatUtcTimestamp(entry.getSearchedAt())}});
    } else if (section == Section::Review) {
        backendActivity_.review.reset();
        backendActivity_.reviewResource.reset();
        auto result=application_.review();
        if(result.ok()&&result.value){
            backendActivity_.review=*result.value;
            if(backendActivity_.review->current && backendActivity_.review->current->back){
                auto resource=application_.resource(backendActivity_.review->current->resourceId);
                if(resource.ok()&&resource.value)backendActivity_.reviewResource=*resource.value;
            }
        }
        else if(result.status.error!=BackendError::INVALID_STATE)fail(result.status);
    } else if (section == Section::Quiz) {
        backendActivity_.quizLevels.clear();
        for(const auto& level:resourceLevels_)if(level.getCategory()=="JLPT")backendActivity_.quizLevels.push_back(level);
        auto selected=std::find_if(backendActivity_.quizLevels.begin(),backendActivity_.quizLevels.end(),[&](const Level& level){return backendActivity_.selectedQuizLevel&&level.getId()==*backendActivity_.selectedQuizLevel;});
        if(selected==backendActivity_.quizLevels.end()){
            backendActivity_.selectedQuizLevel.reset();
            for(const auto& level:backendActivity_.quizLevels){
                const auto available=application_.activeQuestionCount(level.getId());
                if(available.ok()&&available.value&&*available.value>0){backendActivity_.selectedQuizLevel=level.getId();break;}
            }
            if(!backendActivity_.selectedQuizLevel&&!backendActivity_.quizLevels.empty())backendActivity_.selectedQuizLevel=backendActivity_.quizLevels.front().getId();
        }
        backendActivity_.activeQuestionCount=0;
        if(backendActivity_.selectedQuizLevel){
            const auto available=application_.activeQuestionCount(*backendActivity_.selectedQuizLevel);
            if(available.ok()&&available.value)backendActivity_.activeQuestionCount=*available.value;
            else fail(available.status);
        }
        backendActivity_.quiz.reset();
        auto result=application_.quiz();
        if(result.ok()&&result.value)backendActivity_.quiz=*result.value;
        else if(result.status.error!=BackendError::INVALID_STATE)fail(result.status);
    } else if (section == Section::Statistics) {
        const auto levels = application_.levels();
        fail(levels.status);
        backendActivity_.resourceStats.reset();
        const auto stats = application_.statistics();
        fail(stats.status);
        if (stats.value) backendActivity_.resourceStats = *stats.value;
        const auto attempts = application_.attempts();
        fail(attempts.status);
        backendTable_.headers = {"Attempt ID", "Level", "Score", "Date (UTC)"};
        if (attempts.value) for (const auto& attempt : *attempts.value) {
            std::string levelName = "Level unavailable";
            for (const auto& level : resourceLevels_) if (level.getId() == attempt.getLevelId()) { levelName = level.getName(); break; }
            backendTable_.rows.push_back({{}, {attempt.getId().getId(), levelName,
                std::to_string(attempt.getCorrectAnswers()) + " / " + std::to_string(attempt.getTotalQuestions()),
                formatUtcTimestamp(attempt.getSubmittedAt())}});
        }
    }
}

bool AppController::handleBackendLearningEvent(const BackendScreenEvent& event) {

    switch (event.type) {
    case BackendScreenEvent::Type::REMOVE_FAVORITE: {
        const auto* id = std::get_if<ResourceId>(&event.id);
        if (!id) return true;
        const Status status = application_.removeFavorite(*id);
        refreshBackendSection();
        refreshSelectedFavoriteState();
        reportBackend(status, "Favorite removed.");
        return true;
    }
    case BackendScreenEvent::Type::CLEAR_HISTORY: {
        const Status status = application_.clearHistory();
        refreshBackendSection();
        reportBackend(status, "Search history cleared.");
        return true;
    }
    case BackendScreenEvent::Type::REPLAY_HISTORY: {
        const auto* id = std::get_if<HistoryId>(&event.id);
        if (!id) return true;
        auto result = application_.searchAgain(*id);
        if (!result.ok() || !result.value) { reportBackend(result.status, ""); return true; }
        resourceSearch_ = std::move(result.value->options);
        navigate(Section::Resources, false);
        resourcesScreen_.setSearch(resourceSearch_);
        showResourceResults({std::move(result.status), std::move(result.value->rows)});
        return true;
    }
    case BackendScreenEvent::Type::START_REVIEW: {
        const Status status = application_.startReview();
        if (status.ok()) backendActivity_.reviewStopped = false;
        refreshBackendSection();
        if (status.ok()) reportBackend(status, "Review started.");
        else backendMessage_ = status.message == "no favorites" ? "Add resources to Favorites before starting a review." : status.message;
        if (!status.ok()) backendMessageError_ = true;
        return true;
    }
    case BackendScreenEvent::Type::REVEAL_REVIEW: {
        const Status status = application_.revealReview(); refreshBackendSection(); reportBackend(status, "Answer revealed."); return true;
    }
    case BackendScreenEvent::Type::RATE_KNOWN: {
        const Status status = application_.rateReview(true); refreshBackendSection(); reportBackend(status, "Card rated as known."); return true;
    }
    case BackendScreenEvent::Type::RATE_UNKNOWN: {
        const Status status = application_.rateReview(false); refreshBackendSection(); reportBackend(status, "Card placed at the end of the review queue."); return true;
    }
    case BackendScreenEvent::Type::STOP_REVIEW:
        application_.stopReview(); backendActivity_.reviewStopped = true; refreshBackendSection();
        backendMessage_ = "Review stopped."; backendMessageError_ = false; return true;
    case BackendScreenEvent::Type::SELECT_QUIZ_LEVEL: {
        if (backendActivity_.quiz) return true;
        const auto* id = std::get_if<LevelId>(&event.id);
        if (!id) return true;
        const auto found = std::find_if(backendActivity_.quizLevels.begin(), backendActivity_.quizLevels.end(),
                                        [&](const Level& level) { return level.getId() == *id; });
        if (found == backendActivity_.quizLevels.end()) {
            backendMessage_ = "That JLPT level is no longer available."; backendMessageError_ = true; return true;
        }
        backendActivity_.selectedQuizLevel = *id; refreshBackendSection(); return true;
    }
    case BackendScreenEvent::Type::START_QUIZ: {
        if (backendActivity_.quiz) { backendMessage_ = "Stop or finish the current quiz before starting another."; backendMessageError_ = true; return true; }
        if (!backendActivity_.selectedQuizLevel) { backendMessage_ = "No JLPT level is available."; backendMessageError_ = true; return true; }
        if (backendActivity_.activeQuestionCount == 0) { backendMessage_ = "This level has no active questions."; backendMessageError_ = true; return true; }
        const std::size_t count = std::min<std::size_t>(10, backendActivity_.activeQuestionCount);
        const Status status = application_.startQuiz(*backendActivity_.selectedQuizLevel, count);
        if (status.ok()) { backendActivity_.quizResult.reset(); backendActivity_.quizAnswers.clear(); }
        refreshBackendSection(); reportBackend(status, "Quiz started."); return true;
    }
    case BackendScreenEvent::Type::ANSWER: {
        if (!backendActivity_.quiz || backendActivity_.quiz->submitted || event.questionIndex >= backendActivity_.quiz->questions.size()) return true;
        const Status status = application_.selectAnswer(event.questionIndex, event.answer);
        refreshBackendSection(); reportBackend(status, "Answer recorded."); return true;
    }
    case BackendScreenEvent::Type::SUBMIT_QUIZ: {
        if (!backendActivity_.quiz || backendActivity_.quiz->submitted) {
            backendMessage_ = "There is no active quiz to submit."; backendMessageError_ = true; return true;
        }
        auto attempt = application_.submitQuiz();
        if (attempt.ok() && attempt.value) {
            backendActivity_.quizResult = *attempt.value;
            const auto answers = application_.quizAnswers();
            if (answers.ok() && answers.value) backendActivity_.quizAnswers = *answers.value;
            else backendActivity_.quizAnswers.clear();
        }
        refreshBackendSection();
        if (!attempt.ok()) { backendMessage_ = attempt.status.message; backendMessageError_ = true; }
        else if (attempt.status.save == SaveState::PENDING) { backendMessage_ = "Quiz submitted; saving is pending: " + attempt.status.message; backendMessageError_ = false; }
        else { backendMessage_ = "Quiz submitted. Review the result and explanations."; backendMessageError_ = false; }
        return true;
    }
    case BackendScreenEvent::Type::RETRY_WRONG: {
        const Status status = application_.retryWrong();
        if (status.ok()) { backendActivity_.quizResult.reset(); backendActivity_.quizAnswers.clear(); }
        refreshBackendSection(); reportBackend(status, "Wrong answers loaded for another attempt."); return true;
    }
    case BackendScreenEvent::Type::STOP_QUIZ:
        if (!backendActivity_.quiz) { backendMessage_ = "There is no quiz session to stop."; backendMessageError_ = true; return true; }
        application_.stopQuiz(); backendActivity_.quizResult.reset(); backendActivity_.quizAnswers.clear();
        refreshBackendSection(); backendMessage_ = "Quiz stopped."; backendMessageError_ = false; return true;
    default:
        return false;
    }
}


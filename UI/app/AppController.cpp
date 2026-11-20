#include "AppController.h"

#include "raylib.h"
#include "../UiText.h"
#include "domain/Time.h"

#include <algorithm>
#include <ctime>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <set>
#include <type_traits>
#include <utility>

namespace {
constexpr const char* kWindowTitle = "PBL2 - Japanese Learning";
constexpr int kTargetFps = 60;

UserRole parseUserRole(const std::string& value) {
    if (value == "ADMIN") return UserRole::ADMIN;
    if (value == "LEARNER") return UserRole::LEARNER;
    throw std::invalid_argument("Role must be ADMIN or LEARNER.");
}

Script parseScript(const std::string& value) {
    if (value == "HIRAGANA") return Script::HIRAGANA;
    if (value == "KATAKANA") return Script::KATAKANA;
    throw std::invalid_argument("Script must be HIRAGANA or KATAKANA.");
}

QuestionStatus parseQuestionStatus(const std::string& value) {
    if (value == "ACTIVE") return QuestionStatus::ACTIVE;
    if (value == "INACTIVE") return QuestionStatus::INACTIVE;
    throw std::invalid_argument("Question status must be ACTIVE or INACTIVE.");
}

char parseCorrectOption(const std::string& value) {
    if (value.size() == 1 && value[0] >= 'A' && value[0] <= 'D') return value[0];
    throw std::invalid_argument("Correct option must be A, B, C, or D.");
}

int parseInteger(const std::string& value, const char* label) {
    std::size_t consumed = 0;
    const int parsed = std::stoi(value, &consumed);
    if (consumed != value.size()) throw std::invalid_argument(std::string(label) + " must be a whole number.");
    return parsed;
}

int dayOrdinal(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned adjustedMonth = month > 2 ? month - 3 : month + 9;
    const unsigned dayOfYear = (153 * adjustedMonth + 2) / 5 + day - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + static_cast<int>(dayOfEra);
}

bool parseIsoDate(const std::string& timestamp, int& year, unsigned& month, unsigned& day) {
    if (timestamp.size() < 10 || timestamp[4] != '-' || timestamp[7] != '-') return false;
    try {
        year = std::stoi(timestamp.substr(0, 4));
        month = static_cast<unsigned>(std::stoi(timestamp.substr(5, 2)));
        day = static_cast<unsigned>(std::stoi(timestamp.substr(8, 2)));
    } catch (...) { return false; }
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

int currentUtcDayOrdinal() {
    const std::time_t now = std::time(nullptr);
    const std::tm* utc = std::gmtime(&now);
    if (!utc) return 0;
    return dayOrdinal(utc->tm_year + 1900, static_cast<unsigned>(utc->tm_mon + 1),
                      static_cast<unsigned>(utc->tm_mday));
}

std::string formatUtcTimestamp(const std::string& timestamp) {
    if (!validUtcTimestamp(timestamp)) return "Date unavailable";
    static constexpr const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    const auto month = static_cast<std::size_t>(std::stoi(timestamp.substr(5, 2)) - 1);
    return timestamp.substr(8, 2) + " " + months[month] + " " + timestamp.substr(0, 4) +
           ", " + timestamp.substr(11, 8) + " UTC";
}
}

int AppController::run() {
    if (!initializeWindow()) return 1;
    initializeApplication();

    while (running_) {
        processFrame();
        if (!running_) break;
        drawFrame();

        // Render the busy state before making the synchronous Backend call.
        if (pendingLogin_) performPendingLogin();
    }

    shutdownWindow();
    return startupSucceeded_ ? 0 : 1;
}

bool AppController::initializeWindow() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(windowWidth_, windowHeight_, kWindowTitle);
    if (!IsWindowReady()) {
        std::cerr << "Raylib could not create the application window.\n";
        return false;
    }
    windowReady_ = true;
    SetWindowMinSize(800, 560);
    fontReady_ = UiText::initialize();
    const int monitor = GetCurrentMonitor();
    const int monitorWidth = GetMonitorWidth(monitor);
    const int monitorHeight = GetMonitorHeight(monitor);
    if (monitorWidth > 0 && monitorHeight > 0) {
        const int maxWidth = std::max(1, monitorWidth - 32);
        const int maxHeight = std::max(1, monitorHeight - 72);
        windowWidth_ = std::min(maxWidth, std::max(960, static_cast<int>(monitorWidth * 0.90f)));
        windowHeight_ = std::min(maxHeight, std::max(640, static_cast<int>(monitorHeight * 0.86f)));
        SetWindowSize(windowWidth_, windowHeight_);
        const Vector2 monitorPosition = GetMonitorPosition(monitor);
        windowX_ = static_cast<int>(monitorPosition.x) + (monitorWidth - windowWidth_) / 2;
        windowY_ = static_cast<int>(monitorPosition.y) + (monitorHeight - windowHeight_) / 2;
        SetWindowPosition(windowX_, windowY_);
    }
    SetTargetFPS(kTargetFps);
    SetExitKey(KEY_NULL);
    return true;
}

void AppController::initializeApplication() {
    const Status loadStatus = application_.load("database");
    backendLoaded_ = loadStatus.ok();
    startupSucceeded_ = backendLoaded_ && fontReady_;
    startupMessage_ = !fontReady_ ? UiText::error() : loadStatus.message;
    viewState_ = backendLoaded_ && fontReady_ ? ViewState::LOGIN : ViewState::STARTUP_ERROR;

}

void AppController::processFrame() {
    if (WindowShouldClose() && !exitDialog_) requestClose(true);

    const StartupAction closeAction = startupScreen_.poll(exitDialog_);
    if (exitDialog_) {
        switch (closeAction) {
        case StartupAction::RETRY_SAVE: retrySaveAndClose(); break;
        case StartupAction::DISCARD_CLOSE: discardAndClose(); break;
        case StartupAction::CANCEL_CLOSE: cancelClose(); break;
        case StartupAction::NONE:
        case StartupAction::REQUEST_CLOSE: break;
        }
    } else if (closeAction == StartupAction::REQUEST_CLOSE) {
        requestClose(false);
    } else if (viewState_ == ViewState::LOGIN) {
        handleLoginEvent(loginScreen_.update(loginInProgress_));
    } else if (viewState_ == ViewState::AUTHENTICATED && activeAccount_) {
        handleMainScreenEvent(mainScreen_.update(activeAccount_->role, selectedSection_));
    }
}

void AppController::handleMainScreenEvent(MainScreenEvent event) {
    if (event.type == MainScreenEvent::Type::SELECT_SECTION) {
        selectedSection_ = event.section;
        backendRows_.clear();
        backendTable_ = {};
        backendSelected_ = 0;
        backendScreen_.reset();
        if (selectedSection_ > 1) refreshBackendSection();
        else if (selectedSection_ == 0) refreshDashboard();
        return;
    }
    if (event.type == MainScreenEvent::Type::LOGOUT) {
        application_.logout();
        activeAccount_.reset();
        selectedSection_ = 0;
        loginScreen_.reset();
        loginMessage_.clear();
        viewState_ = ViewState::LOGIN;
        resourceRows_.clear();
        selectedResource_.reset();
        selectedResourceFavorited_.reset();
        backendTable_ = {};
        backendActivity_ = {};
        dashboardData_ = {};
        mainScreen_.resetDashboardMonth();
        resourcesScreen_.reset();
        return;
    }
    if (event.type == MainScreenEvent::Type::REQUEST_CLOSE) {
        requestClose(false);
        return;
    }
    if (selectedSection_ == 1) {
        handleResourcesEvent(resourcesScreen_.update(resourceRows_, resourceLevels_, *activeAccount_, selectedResourceFavorited_));
    } else if (selectedSection_ > 1) {
        handleBackendEvent(backendScreen_.update(selectedSection_, activeAccount_->role, backendRows_, backendTable_, backendActivity_));
    }
}

void AppController::drawFrame() {
    UiText::beginFrame();
    BeginDrawing();
    switch (viewState_) {
    case ViewState::STARTUP_ERROR:
        startupScreen_.draw(false, startupMessage_, false, {});
        break;
    case ViewState::LOGIN:
        loginScreen_.draw(loginInProgress_, loginMessage_);
        break;
    case ViewState::AUTHENTICATED:
        if (activeAccount_) {
            mainScreen_.draw(*activeAccount_, selectedSection_, dashboardData_);
            if (selectedSection_ == 1)
                resourcesScreen_.draw(resourceRows_, resourceLevels_, selectedResource_, *activeAccount_, resourcesMessage_, resourcesMessageError_, selectedResourceFavorited_);
            else if (selectedSection_ > 1)
                backendScreen_.draw(selectedSection_, activeAccount_->role, backendRows_, backendTable_, backendSelected_, backendMessage_, backendMessageError_, backendActivity_);
        }
        break;
    }
    if (exitDialog_) startupScreen_.drawExitDialog(exitMessage_);
    EndDrawing();
    UiText::endFrame();
}

void AppController::shutdownWindow() {
    if (!windowReady_) return;
    UiText::shutdown();
    CloseWindow();
    windowReady_ = false;
}

void AppController::handleLoginEvent(LoginEvent event) {
    switch (event.type) {
    case LoginEvent::Type::NONE:
        return;
    case LoginEvent::Type::EMPTY_FIELDS:
        loginMessage_ = "Enter both username and password.";
        return;
    case LoginEvent::Type::SUBMIT:
        if (loginInProgress_ || pendingLogin_) return;
        loginMessage_.clear();
        loginInProgress_ = true;
        pendingLogin_ = PendingLogin{std::move(event.username), std::move(event.password)};
        return;
    case LoginEvent::Type::REQUEST_CLOSE:
        requestClose(false);
        return;
    }
}

void AppController::performPendingLogin() {
    if (!pendingLogin_) return;

    Status loginStatus = application_.login(pendingLogin_->username, pendingLogin_->password);
    std::fill(pendingLogin_->password.begin(), pendingLogin_->password.end(), '\0');
    pendingLogin_.reset();
    loginInProgress_ = false;

    if (!loginStatus.ok()) {
        loginScreen_.clearPassword();
        loginMessage_ = loginStatus.message.empty() ? "Sign in failed." : loginStatus.message;
        return;
    }

    Result<AccountSummary> account = application_.currentAccount();
    if (!account.ok() || !account.value.has_value()) {
        application_.logout();
        loginScreen_.clearPassword();
        loginMessage_ = account.status.message.empty()
                            ? "The signed-in account could not be read."
                            : account.status.message;
        return;
    }

    activeAccount_ = *account.value;
    selectedSection_ = 0;
    mainScreen_.resetDashboardMonth();
    refreshDashboard();
    resourceLevels_.clear();
    const auto levels = application_.levels();
    if (levels.ok() && levels.value) resourceLevels_ = *levels.value;
    refreshResources();
    backendRows_.clear();
    backendTable_ = {};
    backendActivity_ = {};
    backendMessage_.clear();
    backendMessageError_ = false;
    loginScreen_.clearPassword();
    loginMessage_.clear();
    viewState_ = ViewState::AUTHENTICATED;
}

void AppController::refreshDashboard() {
    dashboardData_ = {};
    if (!activeAccount_) return;

    auto addError = [&](const std::string& message) {
        if (message.empty()) return;
        if (!dashboardData_.message.empty()) dashboardData_.message += " ";
        dashboardData_.message += message;
    };
    if (activeAccount_->role == UserRole::ADMIN) {
        const auto stats = application_.statistics();
        if (stats.ok() && stats.value) dashboardData_.resources = *stats.value;
        else addError(stats.status.message);
        const auto accounts = application_.accounts();
        if (accounts.ok() && accounts.value) dashboardData_.accountCount = accounts.value->size();
        else addError(accounts.status.message);
        const auto questions = application_.questions();
        if (questions.ok() && questions.value) dashboardData_.questionCount = questions.value->size();
        else addError(questions.status.message);
    } else {
        const auto favorites = application_.favorites();
        if (favorites.ok() && favorites.value) dashboardData_.favoriteCount = favorites.value->size();
        else addError(favorites.status.message);
        const auto attempts = application_.attempts();
        if (attempts.ok() && attempts.value) {
            std::set<int> activityDays;
            const int today = currentUtcDayOrdinal();
            for (const auto& attempt : *attempts.value) {
                const std::string& timestamp = attempt.getSubmittedAt();
                int year = 0; unsigned month = 0, day = 0;
                if (!parseIsoDate(timestamp, year, month, day)) continue;
                dashboardData_.quizActivityDatesUtc.push_back(timestamp.substr(0, 10));
                activityDays.insert(dayOrdinal(year, month, day));
            }
            dashboardData_.quizAttemptCount = attempts.value->size();
            if (!activityDays.empty()) {
                auto latest = *activityDays.rbegin();
                if (latest >= today - 1 && latest <= today) {
                    for (int day = latest; activityDays.count(day) != 0; --day)
                        ++dashboardData_.learningStreak;
                }
            }
        } else addError(attempts.status.message);
    }
    dashboardData_.loaded = dashboardData_.message.empty();
}

void AppController::refreshResources(const SearchOptions& options) {
    const auto rows = application_.search(options);
    if (!rows.ok() || !rows.value) {
        resourcesMessage_ = rows.status.message.empty() ? "Could not load resources." : rows.status.message;
        resourcesMessageError_ = true;
        resourceRows_.clear();
        return;
    }
    resourceRows_ = *rows.value;
    std::vector<std::string> visibleText;
    visibleText.reserve(resourceRows_.size() * 2 + 1);
    for (const auto& row : resourceRows_) { visibleText.push_back(row.term); visibleText.push_back(row.meaning); }
    UiText::preload(visibleText);
    resourcesMessage_ = rows.status.save == SaveState::PENDING
                            ? (rows.status.message.empty() ? "Results loaded; saving search history is pending." : "Results loaded; history save pending: " + rows.status.message)
                            : rows.status.message;
    resourcesMessageError_ = false;
}

void AppController::handleResourcesEvent(ResourcesEvent event) {
    auto report = [&](const Status& status, const char* success) {
        if (!status.ok()) { resourcesMessage_ = status.message.empty() ? "The operation failed." : status.message; resourcesMessageError_ = true; }
        else if (status.save == SaveState::PENDING)
            { resourcesMessage_ = status.message.empty() ? "Change applied; saving is pending." : "Change applied; save pending: " + status.message; resourcesMessageError_ = false; }
        else { resourcesMessage_ = success; resourcesMessageError_ = false; }
    };
    if (event.type != ResourcesEvent::Type::NONE && event.type != ResourcesEvent::Type::CANCEL_FORM) {
        resourcesMessage_.clear();
        resourcesMessageError_ = false;
    }
    switch (event.type) {
    case ResourcesEvent::Type::NONE: case ResourcesEvent::Type::CANCEL_FORM: return;
    case ResourcesEvent::Type::OPEN_EDIT: {
        const auto detail = application_.resource(event.id);
        if (detail.ok() && detail.value) {
            resourcesScreen_.setEditing(*detail.value, resourceLevels_);
            std::vector<std::string> fields;
            std::visit([&fields](const auto& item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, Vocabulary>) fields = {item.getWord(), item.getReading(), item.getMeaning(), item.getExample()};
                else if constexpr (std::is_same_v<T, Grammar>) fields = {item.getPattern(), item.getMeaning(), item.getStructure(), item.getUsage(), item.getExample(), item.getNote()};
                else fields = {item.getCharacter(), item.getMeaning(), item.getOnyomi(), item.getKunyomi(), item.getExamples()};
            }, *detail.value);
            UiText::preload(fields);
        }
        else { resourcesMessage_ = detail.status.message; resourcesMessageError_ = true; }
        return;
    }
    case ResourcesEvent::Type::SEARCH:
        selectedResource_.reset();
        selectedResourceFavorited_.reset();
        refreshResources(event.search);
        return;
    case ResourcesEvent::Type::SELECT: {
        selectedResource_ = std::nullopt;
        const auto detail = application_.resource(event.id);
        if (detail.ok() && detail.value) {
            selectedResource_ = *detail.value;
            refreshSelectedFavoriteState();
            std::vector<std::string> fields;
            std::visit([&fields](const auto& item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, Vocabulary>) fields = {item.getWord(), item.getReading(), item.getMeaning(), item.getExample()};
                else if constexpr (std::is_same_v<T, Grammar>) fields = {item.getPattern(), item.getMeaning(), item.getStructure(), item.getUsage(), item.getExample(), item.getNote()};
                else fields = {item.getCharacter(), item.getMeaning(), item.getOnyomi(), item.getKunyomi(), item.getExamples()};
            }, *detail.value);
            UiText::preload(fields);
        }
        else { resourcesMessage_ = detail.status.message; resourcesMessageError_ = true; }
        return;
    }
    case ResourcesEvent::Type::FAVORITE: {
        if (!selectedResourceFavorited_.has_value()) {
            resourcesMessage_ = "Favorite status is unavailable.";
            resourcesMessageError_ = true;
            return;
        }
        const bool wasFavorited = *selectedResourceFavorited_;
        const Status status = wasFavorited ? application_.removeFavorite(event.id) : application_.addFavorite(event.id);
        report(status, wasFavorited ? "Removed from favorites." : "Added to favorites.");
        if (status.ok()) refreshSelectedFavoriteState();
        return;
    }
    case ResourcesEvent::Type::REMOVE: {
        const Status status = application_.removeResource(event.id);
        report(status, "Resource removed.");
        if(status.ok()){
            selectedResource_.reset();selectedResourceFavorited_.reset();
            const std::string message=resourcesMessage_;const bool messageError=resourcesMessageError_;
            refreshResources();
            if(!resourcesMessageError_){resourcesMessage_=message;resourcesMessageError_=messageError;}
        }
        return;
    }
    case ResourcesEvent::Type::CREATE: case ResourcesEvent::Type::UPDATE: {
        const auto& f = event.form;
        Status status;
        std::optional<ResourceId> created;
        auto pos = [&]() {
            if (f.fields[3] == "VERB") return PartOfSpeech::VERB;
            if (f.fields[3] == "I_ADJ") return PartOfSpeech::IADJECTIVE;
            if (f.fields[3] == "NA_ADJ") return PartOfSpeech::NAADJECTIVE;
            if (f.fields[3] == "ADV") return PartOfSpeech::ADVERB;
            if (f.fields[3] == "OTHER") return PartOfSpeech::OTHER;
            if (f.fields[3] == "NOUN") return PartOfSpeech::NOUN;
            throw std::invalid_argument("Part of speech must be NOUN, VERB, I_ADJ, NA_ADJ, ADV, or OTHER.");
        };
        if (f.type == ResourceType::VOCABULARY) {
            PartOfSpeech partOfSpeech;
            try { partOfSpeech = pos(); }
            catch (const std::exception& e) { resourcesMessage_ = std::string("Invalid form: ") + e.what(); resourcesMessageError_ = true; return; }
            VocabularyInput input{f.levelId, f.fields[0], f.fields[1], f.fields[2], partOfSpeech, f.fields[4]};
            if (event.type == ResourcesEvent::Type::CREATE) { auto r=application_.createVocabulary(input); status=r.status; created=r.value; }
            else status=application_.updateVocabulary(event.id,input);
        } else if (f.type == ResourceType::GRAMMAR) {
            GrammarInput input{f.levelId, f.fields[0], f.fields[1], f.fields[2], f.fields[3], f.fields[4], f.fields[5]};
            if (event.type == ResourcesEvent::Type::CREATE) { auto r=application_.createGrammar(input); status=r.status; created=r.value; }
            else status=application_.updateGrammar(event.id,input);
        } else {
            int strokeCount=0;
            try { std::size_t parsed=0; strokeCount=std::stoi(f.fields[2],&parsed); if(parsed!=f.fields[2].size())throw std::invalid_argument("invalid stroke count"); }
            catch(const std::exception&){resourcesMessage_="Enter a whole number for stroke count.";resourcesMessageError_=true;return;}
            KanjiInput input{f.levelId, f.fields[0], f.fields[1], strokeCount, f.fields[3], f.fields[4], f.fields[5]};
            if (event.type == ResourcesEvent::Type::CREATE) { auto r=application_.createKanji(input); status=r.status; created=r.value; }
            else status=application_.updateKanji(event.id,input);
        }
        report(status, event.type == ResourcesEvent::Type::CREATE ? "Resource created." : "Resource updated.");
        if(status.ok()){
            const std::string message=resourcesMessage_;const bool messageError=resourcesMessageError_;
            refreshResources();
            if(!resourcesMessageError_){resourcesMessage_=message;resourcesMessageError_=messageError;}
        }
        if (created) {
            const auto detail = application_.resource(*created);
            if (detail.ok() && detail.value) selectedResource_ = *detail.value;
        }
        return;
    }
    }
}

void AppController::refreshSelectedFavoriteState() {
    selectedResourceFavorited_.reset();
    if (!activeAccount_ || !selectedResource_) return;
    const auto favoriteRows = application_.favorites();
    if (!favoriteRows.ok() || !favoriteRows.value) return;
    const ResourceId selectedId = std::visit([](const auto& item) { return item.getId(); }, *selectedResource_);
    selectedResourceFavorited_ = std::any_of(favoriteRows.value->begin(), favoriteRows.value->end(), [&](const auto& detail) {
        return std::visit([&](const auto& item) { return item.getId() == selectedId; }, detail);
    });
}

void AppController::refreshBackendSection() {
    backendRows_.clear();
    backendTable_ = {};
    backendMessage_.clear();
    backendMessageError_ = false;
    if (!activeAccount_) return;
    if (activeAccount_->role == UserRole::ADMIN) refreshAdminBackendSection(selectedSection_);
    else refreshLearnerBackendSection(selectedSection_);

    if (backendRows_.empty() && backendTable_.rows.empty() && backendMessage_.empty() &&
        !(activeAccount_->role == UserRole::LEARNER && (selectedSection_ == 4 || selectedSection_ == 5 || selectedSection_ == 7))) {
        backendMessage_ = "No records are available.";
        backendMessageError_ = false;
    }
    preloadBackendSectionText();
}

void AppController::refreshAdminBackendSection(std::size_t section) {
    auto fail = [&](const Status& status) { if (!status.ok()) { backendMessage_ = status.message; backendMessageError_ = true; } };
    if (section == 2) {
        const auto result = application_.levels();
        fail(result.status);
        backendTable_.headers = {"ID", "Category", "Level", "Order", "Description"};
        if (result.value) for (const auto& level : *result.value)
            backendTable_.rows.push_back({level.getId().getId(), level.getCategory(), level.getName(),
                                          std::to_string(level.getOrder()), level.getDescription()});
    } else if (section == 3) {
        const auto result = application_.accounts();
        fail(result.status);
        backendTable_.headers = {"ID", "Username", "Role", "Status"};
        if (result.value) for (const auto& account : *result.value)
            backendTable_.rows.push_back({account.id.getId(), account.username,
                account.role == UserRole::ADMIN ? "Admin" : "Learner",
                account.status == UserStatus::ACTIVE ? "Active" : "Locked"});
    } else if (section == 4) {
        const auto result = application_.alphabet();
        fail(result.status);
        if (result.value) for (const auto& item : *result.value)
            backendRows_.push_back(std::string(item.getScript() == Script::HIRAGANA ? "HIRAGANA|" : "KATAKANA|") +
                                   item.getGlyph() + "|" + item.getRomaji() + "|" + item.getGroupName() + "|" + item.getNote());
    } else if (section == 5) {
        const auto result = application_.questions();
        fail(result.status);
        backendTable_.headers = {"ID", "Prompt", "Level", "Status"};
        if (result.value) for (const auto& question : *result.value) {
            std::string levelName = question.getLevelId().getId();
            for (const auto& level : resourceLevels_) if (level.getId() == question.getLevelId()) { levelName = level.getName(); break; }
            backendTable_.rows.push_back({question.getId().getId(), question.getPrompt(), levelName,
                question.getStatus() == QuestionStatus::ACTIVE ? "Active" : "Inactive"});
        }
    }
}

void AppController::refreshLearnerBackendSection(std::size_t section) {
    auto fail = [&](const Status& status) { if (!status.ok()) { backendMessage_ = status.message; backendMessageError_ = true; } };
    if (section == 2) {
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
            backendTable_.rows.push_back({resource.getId().getId(), resource.getTerm(), resource.getMeaning(), type, levelName});
        }, detail);
    } else if (section == 3) {
        const auto result = application_.history();
        fail(result.status);
        backendTable_.headers = {"ID", "Query", "Searched at (UTC)"};
        if (result.value) for (const auto& entry : *result.value)
            backendTable_.rows.push_back({entry.getId().getId(), entry.getQuery(), formatUtcTimestamp(entry.getSearchedAt())});
    } else if (section == 4) {
        backendActivity_.review.reset();
        backendActivity_.reviewResource.reset();
        auto result=application_.review();
        if(result.ok()&&result.value){
            backendActivity_.review=*result.value;
            if(backendActivity_.review->current){
                auto resource=application_.resource(backendActivity_.review->current->resourceId);
                if(resource.ok()&&resource.value)backendActivity_.reviewResource=*resource.value;
            }
        }
        else if(result.status.error!=BackendError::INVALID_STATE)fail(result.status);
    } else if (section == 5) {
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
    } else if (section == 6) {
        const auto result = application_.alphabet();
        fail(result.status);
        if (result.value) for (const auto& item : *result.value)
            backendRows_.push_back(std::string(item.getScript() == Script::HIRAGANA ? "HIRAGANA|" : "KATAKANA|") +
                                   item.getGlyph() + "|" + item.getRomaji() + "|" + item.getGroupName() + "|" + item.getNote());
    } else if (section == 7) {
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
            backendTable_.rows.push_back({attempt.getId().getId(), levelName,
                std::to_string(attempt.getCorrectAnswers()) + " / " + std::to_string(attempt.getTotalQuestions()),
                formatUtcTimestamp(attempt.getSubmittedAt())});
        }
    }
}

void AppController::preloadBackendSectionText() {
    std::vector<std::string> preloadRows=backendRows_;
    preloadRows.insert(preloadRows.end(),backendTable_.headers.begin(),backendTable_.headers.end());
    for(const auto& row:backendTable_.rows)preloadRows.insert(preloadRows.end(),row.begin(),row.end());
    for(const auto& level:backendActivity_.quizLevels)preloadRows.push_back(level.getName());
    if(backendActivity_.quiz)for(const auto& question:backendActivity_.quiz->questions){
        preloadRows.push_back(question.prompt);
        for(const auto& option:question.options)preloadRows.push_back(option);
    }
    if(backendActivity_.quiz)for(const auto& answer:backendActivity_.quizAnswers)preloadRows.push_back(answer.explanation);
    if(backendActivity_.review&&backendActivity_.review->current){
        preloadRows.push_back(backendActivity_.review->current->front);
        if(backendActivity_.review->current->back)preloadRows.push_back(*backendActivity_.review->current->back);
    }
    if(backendActivity_.reviewResource)std::visit([&](const auto& resource){
        preloadRows.push_back(resource.getMeaning());
        using T=std::decay_t<decltype(resource)>;
        if constexpr(std::is_same_v<T,Vocabulary>){preloadRows.push_back(resource.getReading());preloadRows.push_back(resource.getExample());}
        else if constexpr(std::is_same_v<T,Grammar>){preloadRows.push_back(resource.getStructure());preloadRows.push_back(resource.getUsage());preloadRows.push_back(resource.getExample());}
        else {preloadRows.push_back(resource.getOnyomi());preloadRows.push_back(resource.getKunyomi());preloadRows.push_back(resource.getExamples());}
    },*backendActivity_.reviewResource);
    UiText::preload(preloadRows);
}

bool AppController::handleBackendLearningEvent(const BackendScreenEvent& event) {
    auto report = [&](const Status& status, const char* success) {
        if (!status.ok()) { backendMessage_ = status.message; backendMessageError_ = true; }
        else if (status.save == SaveState::PENDING) { backendMessage_ = "Action applied; save is pending: " + status.message; backendMessageError_ = false; }
        else { backendMessage_ = success; backendMessageError_ = false; }
    };

    switch (event.type) {
    case BackendScreenEvent::Type::START_REVIEW: {
        const Status status = application_.startReview();
        if (status.ok()) backendActivity_.reviewStopped = false;
        refreshBackendSection();
        if (status.ok()) report(status, "Review started.");
        else backendMessage_ = status.message == "no favorites" ? "Add resources to Favorites before starting a review." : status.message;
        if (!status.ok()) backendMessageError_ = true;
        return true;
    }
    case BackendScreenEvent::Type::REVEAL_REVIEW: {
        const Status status = application_.revealReview(); refreshBackendSection(); report(status, "Answer revealed."); return true;
    }
    case BackendScreenEvent::Type::RATE_KNOWN: {
        const Status status = application_.rateReview(true); refreshBackendSection(); report(status, "Card rated as known."); return true;
    }
    case BackendScreenEvent::Type::RATE_UNKNOWN: {
        const Status status = application_.rateReview(false); refreshBackendSection(); report(status, "Card placed at the end of the review queue."); return true;
    }
    case BackendScreenEvent::Type::STOP_REVIEW:
        application_.stopReview(); backendActivity_.reviewStopped = true; refreshBackendSection();
        backendMessage_ = "Review stopped."; backendMessageError_ = false; return true;
    case BackendScreenEvent::Type::SELECT_QUIZ_LEVEL: {
        if (backendActivity_.quiz) return true;
        const LevelId id(event.text);
        const auto found = std::find_if(backendActivity_.quizLevels.begin(), backendActivity_.quizLevels.end(),
                                        [&](const Level& level) { return level.getId() == id; });
        if (found == backendActivity_.quizLevels.end()) {
            backendMessage_ = "That JLPT level is no longer available."; backendMessageError_ = true; return true;
        }
        backendActivity_.selectedQuizLevel = id; refreshBackendSection(); return true;
    }
    case BackendScreenEvent::Type::PREVIOUS_QUESTION:
    case BackendScreenEvent::Type::NEXT_QUESTION:
        return true;
    case BackendScreenEvent::Type::START_QUIZ: {
        if (backendActivity_.quiz) { backendMessage_ = "Stop or finish the current quiz before starting another."; backendMessageError_ = true; return true; }
        if (!backendActivity_.selectedQuizLevel) { backendMessage_ = "No JLPT level is available."; backendMessageError_ = true; return true; }
        if (backendActivity_.activeQuestionCount == 0) { backendMessage_ = "This level has no active questions."; backendMessageError_ = true; return true; }
        const std::size_t count = std::min<std::size_t>(10, backendActivity_.activeQuestionCount);
        const Status status = application_.startQuiz(*backendActivity_.selectedQuizLevel, count);
        if (status.ok()) { backendActivity_.quizResult.reset(); backendActivity_.quizAnswers.clear(); }
        refreshBackendSection(); report(status, "Quiz started."); return true;
    }
    case BackendScreenEvent::Type::ANSWER: {
        if (!backendActivity_.quiz || backendActivity_.quiz->submitted || event.index >= backendActivity_.quiz->questions.size()) return true;
        const Status status = application_.selectAnswer(event.index, event.answer);
        refreshBackendSection(); report(status, "Answer recorded."); return true;
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
        refreshBackendSection(); report(status, "Wrong answers loaded for another attempt."); return true;
    }
    case BackendScreenEvent::Type::STOP_QUIZ:
        if (!backendActivity_.quiz) { backendMessage_ = "There is no quiz session to stop."; backendMessageError_ = true; return true; }
        application_.stopQuiz(); backendActivity_.quizResult.reset(); backendActivity_.quizAnswers.clear();
        refreshBackendSection(); backendMessage_ = "Quiz stopped."; backendMessageError_ = false; return true;
    default:
        return false;
    }
}

void AppController::handleBackendEvent(BackendScreenEvent event) {
    auto report=[&](const Status& s,const char* success){
        if(!s.ok()){backendMessage_=s.message;backendMessageError_=true;}
        else if(s.save==SaveState::PENDING){backendMessage_="Action applied; save is pending: "+s.message;backendMessageError_=false;}
        else {backendMessage_=success;backendMessageError_=false;}
    };
    auto refreshAndReport=[&](const Status& status,const char* success){
        refreshBackendSection();
        report(status,success);
    };
    if(event.type!=BackendScreenEvent::Type::NONE&&event.type!=BackendScreenEvent::Type::REFRESH){
        backendMessage_.clear();
        backendMessageError_=false;
    }
    if (handleBackendLearningEvent(event)) return;
    switch(event.type){
    case BackendScreenEvent::Type::NONE:return;
    case BackendScreenEvent::Type::START_REVIEW:
    case BackendScreenEvent::Type::REVEAL_REVIEW:
    case BackendScreenEvent::Type::RATE_KNOWN:
    case BackendScreenEvent::Type::RATE_UNKNOWN:
    case BackendScreenEvent::Type::STOP_REVIEW:
    case BackendScreenEvent::Type::SELECT_QUIZ_LEVEL:
    case BackendScreenEvent::Type::PREVIOUS_QUESTION:
    case BackendScreenEvent::Type::NEXT_QUESTION:
    case BackendScreenEvent::Type::START_QUIZ:
    case BackendScreenEvent::Type::ANSWER:
    case BackendScreenEvent::Type::SUBMIT_QUIZ:
    case BackendScreenEvent::Type::RETRY_WRONG:
    case BackendScreenEvent::Type::STOP_QUIZ:return;
    case BackendScreenEvent::Type::REFRESH:{
        const std::string previousMessage=backendMessage_;
        const bool previousMessageError=backendMessageError_;
        refreshBackendSection();
        if(!backendMessageError_&&!previousMessage.empty()){backendMessage_=previousMessage;backendMessageError_=previousMessageError;}
        return;
    }
    case BackendScreenEvent::Type::SELECT:backendSelected_=event.index;return;
    case BackendScreenEvent::Type::CLEAR_SELECTION:backendSelected_=static_cast<std::size_t>(-1);return;
    case BackendScreenEvent::Type::REMOVE_FAVORITE:{auto f=application_.favorites();if(f.value&&event.index<f.value->size()){const auto id=std::visit([](const auto& r){return r.getId();},(*f.value)[event.index]);const Status status=application_.removeFavorite(id);refreshBackendSection();refreshSelectedFavoriteState();report(status,"Favorite removed.");}return;}
    case BackendScreenEvent::Type::REPLAY_HISTORY:{auto h=application_.history();if(h.value&&event.index<h.value->size()){auto r=application_.searchAgain((*h.value)[event.index].getId());backendRows_.clear();if(r.value)for(const auto& row:*r.value)backendRows_.push_back(row.term+"  |  "+row.meaning+"  |  "+row.levelId.getId());backendMessage_=r.status.message;backendMessageError_=!r.status.ok();}return;}
    case BackendScreenEvent::Type::CLEAR_HISTORY:refreshAndReport(application_.clearHistory(),"Search history cleared.");return;
    case BackendScreenEvent::Type::TOGGLE_ACCOUNT:{auto a=application_.accounts();if(a.value&&event.index<a.value->size()){const auto& account=(*a.value)[event.index];if(account.id==activeAccount_->id){backendMessage_="You cannot lock the current account.";backendMessageError_=true;return;}const auto next=account.status==UserStatus::ACTIVE?UserStatus::LOCKED:UserStatus::ACTIVE;refreshAndReport(application_.setAccountStatus(account.id,next),"Account status updated.");}return;}
    case BackendScreenEvent::Type::TOGGLE_QUESTION:{auto q=application_.questions();if(q.value&&event.index<q.value->size()){const auto& item=(*q.value)[event.index];const auto next=item.getStatus()==QuestionStatus::ACTIVE?QuestionStatus::INACTIVE:QuestionStatus::ACTIVE;refreshAndReport(application_.setQuestionStatus(item.getId(),next),"Question status updated.");}return;}
    case BackendScreenEvent::Type::DELETE_ROW:{
        if(!activeAccount_||activeAccount_->role!=UserRole::ADMIN)return;
        Status status; const char* success="Record removed."; bool attempted=false;
        if(selectedSection_==2){auto r=application_.levels();if(r.value&&event.index<r.value->size()){status=application_.removeLevel((*r.value)[event.index].getId());success="Level removed.";attempted=true;}}
        else if(selectedSection_==4){auto r=application_.alphabet();if(r.value&&event.index<r.value->size()){status=application_.removeAlphabet((*r.value)[event.index].getId());success="Alphabet entry removed.";attempted=true;}}
        else if(selectedSection_==5){auto r=application_.questions();if(r.value&&event.index<r.value->size()){status=application_.removeQuestion((*r.value)[event.index].getId());success="Question removed.";attempted=true;}}
        refreshBackendSection();
        if(attempted)report(status,success);else {backendMessage_="The selected record is no longer available.";backendMessageError_=true;}
        return;
    }
    case BackendScreenEvent::Type::EXPORT_PACKAGE:{const auto s=application_.exportPackage(event.text);report(s,"Package exported.");return;}
    case BackendScreenEvent::Type::IMPORT_PACKAGE:{
        std::error_code pathError;
        const auto levelsFile=std::filesystem::path(event.text)/"resource_levels.txt";
        if(event.text.empty()||!std::filesystem::is_regular_file(levelsFile,pathError)){
            backendMessage_="The selected package is missing resource_levels.txt. Export a package to this folder first.";
            backendMessageError_=true;
            return;
        }
        refreshAndReport(application_.importPackage(event.text),"Package imported.");
        return;
    }
    case BackendScreenEvent::Type::CREATE_ROW:{
        const auto& f=event.fields;Status status;std::string error;
        try{
            if(event.index==2){LevelInput input{f[0],f[1],parseInteger(f[2],"Order"),f[3]};auto r=application_.createLevel(input);status=r.status;}
            else if(event.index==3){auto r=application_.createAccount(f[0],f[1],parseUserRole(f[2]));status=r.status;}
            else if(event.index==4){AlphabetInput input{parseScript(f[0]),f[1],f[2],f[3],parseInteger(f[4],"Display order"),f[5]};auto r=application_.createAlphabet(input);status=r.status;}
            else if(event.index==5){std::optional<ResourceId> resource;if(!f[1].empty())resource=ResourceId(f[1]);std::array<std::string,4> options{f[3],f[4],f[5],f[6]};QuestionInput input{LevelId(f[0]),resource,f[2],options,parseCorrectOption(f[7]),f[8],parseQuestionStatus(f[9])};auto r=application_.createQuestion(input);status=r.status;}
            else error="Unsupported admin form.";
        }catch(const std::exception& e){error=std::string("Invalid form: ")+e.what();}
        refreshBackendSection();
        if(!error.empty()){backendMessage_=error;backendMessageError_=true;}else report(status,"Record created.");
        return;
    }
    case BackendScreenEvent::Type::EDIT_ROW:{
        std::array<std::string,12> f{};
        if(selectedSection_==2){auto r=application_.levels();if(r.value&&event.index<r.value->size()){const auto& l=(*r.value)[event.index];f[0]=l.getCategory();f[1]=l.getName();f[2]=std::to_string(l.getOrder());f[3]=l.getDescription();backendScreen_.openEditForm(f,l.getId().getId());}}
        else if(selectedSection_==3){auto r=application_.accounts();if(r.value&&event.index<r.value->size())backendScreen_.openResetPasswordForm((*r.value)[event.index].id.getId());}
        else if(selectedSection_==4){auto r=application_.alphabet();if(r.value&&event.index<r.value->size()){const auto& a=(*r.value)[event.index];f[0]=a.getScript()==Script::HIRAGANA?"HIRAGANA":"KATAKANA";f[1]=a.getGlyph();f[2]=a.getRomaji();f[3]=a.getGroupName();f[4]=std::to_string(a.getDisplayOrder());f[5]=a.getNote();backendScreen_.openEditForm(f,a.getId().getId());}}
        else if(selectedSection_==5){auto r=application_.questions();if(r.value&&event.index<r.value->size()){const auto& q=(*r.value)[event.index];f[0]=q.getLevelId().getId();if(q.getResourceId())f[1]=q.getResourceId()->getId();f[2]=q.getPrompt();for(int i=0;i<4;++i)f[3+i]=q.getOptions()[i];f[7]=q.getCorrectOption();f[8]=q.getExplanation();f[9]=q.getStatus()==QuestionStatus::ACTIVE?"ACTIVE":"INACTIVE";backendScreen_.openEditForm(f,q.getId().getId());}}
        return;
    }
    case BackendScreenEvent::Type::UPDATE_ROW:{
        const auto& f=event.fields;Status status;std::string error;
        try{
            if(event.index==2){LevelInput input{f[0],f[1],parseInteger(f[2],"Order"),f[3]};status=application_.updateLevel(LevelId(event.text),input);}
            else if(event.index==4){AlphabetInput input{parseScript(f[0]),f[1],f[2],f[3],parseInteger(f[4],"Display order"),f[5]};status=application_.updateAlphabet(AlphabetId(event.text),input);}
            else if(event.index==5){std::optional<ResourceId> resource;if(!f[1].empty())resource=ResourceId(f[1]);std::array<std::string,4> options{f[3],f[4],f[5],f[6]};QuestionInput input{LevelId(f[0]),resource,f[2],options,parseCorrectOption(f[7]),f[8],parseQuestionStatus(f[9])};status=application_.updateQuestion(QuestionId(event.text),input);}
            else error="Unsupported admin form.";
        }catch(const std::exception& e){error=std::string("Invalid form: ")+e.what();}
        refreshBackendSection();
        if(!error.empty()){backendMessage_=error;backendMessageError_=true;}else report(status,"Record updated.");
        return;
    }
    case BackendScreenEvent::Type::RESET_PASSWORD:{report(application_.resetPassword(UserId(event.text),event.fields[0]),"Password reset.");return;}
    case BackendScreenEvent::Type::CANCEL_FORM:return;
    }
}

void AppController::requestClose(bool fromNativeWindow) {
    nativeClosePending_ = nativeClosePending_ || fromNativeWindow;

    const Status closeStatus = application_.close();
    if (closeStatus.ok() && !application_.loaded()) {
        backendLoaded_ = false;
        activeAccount_.reset();
        exitDialog_ = false;
        running_ = false;
        return;
    }

    exitDialog_ = true;
    exitMessage_ = closeStatus.message;
}

void AppController::retrySaveAndClose() {
    if (!application_.loaded()) {
        requestClose(nativeClosePending_);
        return;
    }

    const Status saveStatus = application_.retrySave();
    if (!saveStatus.ok() || saveStatus.save == SaveState::PENDING || application_.dirty()) {
        exitMessage_ = saveStatus.message.empty()
                           ? "Data is still waiting to be saved. Try again or cancel closing."
                           : saveStatus.message;
        return;
    }

    const Status closeStatus = application_.close();
    if (closeStatus.ok() && !application_.loaded()) {
        backendLoaded_ = false;
        activeAccount_.reset();
        exitDialog_ = false;
        running_ = false;
        return;
    }
    exitMessage_ = closeStatus.message.empty()
                       ? "The backend did not confirm that it closed."
                       : closeStatus.message;
}

void AppController::discardAndClose() {
    try {
        application_.discardAndClose();
        if (!application_.loaded() && !application_.dirty()) {
            backendLoaded_ = false;
            activeAccount_.reset();
            exitDialog_ = false;
            running_ = false;
            return;
        }
        exitMessage_ = "The backend did not confirm that it discarded pending data.";
    } catch (const std::exception& error) {
        exitMessage_ = error.what();
    }
}

void AppController::cancelClose() {
    exitDialog_ = false;
    exitMessage_.clear();

    if (!nativeClosePending_) return;

    // Raylib 6.2 has no API to clear a native close request. Recreate its
    // window after cancellation; the Application and its backend state survive.
    windowWidth_ = GetScreenWidth();
    windowHeight_ = GetScreenHeight();
    const Vector2 oldPosition = GetWindowPosition();
    windowX_ = static_cast<int>(oldPosition.x);
    windowY_ = static_cast<int>(oldPosition.y);
    UiText::suspend();
    CloseWindow();
    windowReady_ = false;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(windowWidth_, windowHeight_, kWindowTitle);
    if (!IsWindowReady()) {
        exitMessage_ = "The window could not be restored after cancelling close.";
        running_ = false;
        nativeClosePending_ = false;
        return;
    }
    windowReady_ = true;
    SetWindowMinSize(800, 560);
    if (!UiText::resume()) {
        exitMessage_ = UiText::error();
        running_ = false;
        nativeClosePending_ = false;
        return;
    }
    SetWindowPosition(windowX_, windowY_);
    SetTargetFPS(kTargetFps);
    SetExitKey(KEY_NULL);
    nativeClosePending_ = false;
}

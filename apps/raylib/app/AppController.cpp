#include "AppController.h"

#include "raylib.h"
#include "../UiText.h"
#include "../UITheme.h"
#include "../UiLayout.h"
#include "pbl2/InputLimits.h"

#include <algorithm>
#include <ctime>
#include <exception>
#include <iostream>
#include <set>
#include <utility>

namespace {
constexpr const char* kWindowTitle = "PBL2 - Japanese Learning";
constexpr int kTargetFps = 60;

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


}

AppController::~AppController() { shutdownWindow(); }

int AppController::run(const std::string& dataFolder) {
    if (!initializeWindow()) return 1;
    initializeApplication(dataFolder);

    while (running_) {
        processFrame();
        if (!running_) break;
        drawFrame();

        // Render the busy state before making the synchronous Backend call.
        if (loginInProgress_) performPendingLogin();
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

void AppController::initializeApplication(const std::string& dataFolder) {
    const Status loadStatus = application_.load(dataFolder);
    startupScreen_.setFolder(dataFolder, loadStatus.error == BackendError::EMPTY_DATA_DIRECTORY);
    backendLoaded_ = loadStatus.ok();
    startupSucceeded_ = backendLoaded_ && fontReady_;
    if (!startupSucceeded_) {
        startupMessage_ = "Data folder: " + (dataFolder.empty() ? std::string("(empty)") : dataFolder);
        if (!loadStatus.ok()) startupMessage_ += "\n" + loadStatus.message;
        if (!fontReady_) startupMessage_ += "\n" + UiText::error();
        std::cerr << startupMessage_ << '\n';
    }
    viewState_ = backendLoaded_ && fontReady_ ? ViewState::LOGIN : ViewState::STARTUP_ERROR;
}

void AppController::finishInitialization(const Status& status) {
    const auto folder = startupScreen_.folder();
    if (status.save == SaveState::PENDING) {
        startupScreen_.waitForSave();
        startupMessage_ = "Database save is pending. " + status.message;
    } else if (!status.ok()) {
        startupMessage_ = status.message;
    } else {
        const auto closed = application_.close();
        if (closed.ok()) initializeApplication(folder);
        else startupMessage_ = closed.message;
    }
}

void AppController::handleStartup() {
    switch (startupScreen_.poll(false)) {
    case StartupAction::REQUEST_CLOSE: requestClose(); break;
    case StartupAction::OPEN_FOLDER: initializeApplication(startupScreen_.folder()); break;
    case StartupAction::BEGIN_CREATE: startupMessage_.clear(); break;
    case StartupAction::CREATE_DATABASE: {
        const auto& password = startupScreen_.password();
        if (password.tooLong()) {
            startupMessage_ = "Password exceeds " + std::to_string(maxPasswordBytes) + " bytes.";
            startupScreen_.clearPassword();
            break;
        }
        const auto status = application_.initialize(startupScreen_.folder(), startupScreen_.admin(), password.value());
        startupScreen_.clearPassword();
        finishInitialization(status);
        break;
    }
    case StartupAction::RETRY_INITIALIZE: finishInitialization(application_.retrySave()); break;
    default: break;
    }
}

void AppController::processFrame() {
    if (WindowShouldClose() && !exitDialog_) { requestClose(); return; }

    if (exitDialog_) {
        switch (startupScreen_.poll(true, exitMessage_)) {
        case StartupAction::RETRY_SAVE: retrySaveAndClose(); break;
        case StartupAction::DISCARD_CLOSE: discardAndClose(); break;
        case StartupAction::CANCEL_CLOSE: cancelClose(); break;
        default: break;
        }
    } else if (viewState_ == ViewState::STARTUP_ERROR) {
        handleStartup();
    } else if (viewState_ == ViewState::LOGIN) {
        handleLoginEvent(loginScreen_.update(loginInProgress_));
    } else if (viewState_ == ViewState::AUTHENTICATED && activeAccount_) {
        if (changePasswordDialog_.isOpen()) {
            using Action = ChangePasswordDialog::Action;
            switch (changePasswordDialog_.update()) {
            case Action::SUBMIT:
                changePasswordDialog_.showResult(application_.changePassword(
                    changePasswordDialog_.currentPassword(), changePasswordDialog_.newPassword()));
                break;
            case Action::RETRY_SAVE: changePasswordDialog_.showResult(application_.retrySave()); break;
            case Action::REQUEST_CLOSE: requestClose(); break;
            case Action::NONE: break;
            }
            return;
        }
        if (contentModal()) { updateContent(); return; }
        if (UiInput::tab()) {
            const auto role = activeAccount_->role;
            if (mainScreen_.hasFocus()) {
                if (!mainScreen_.tab(role, selectedSection_) && !contentTab()) mainScreen_.tab(role, selectedSection_);
            } else if (!contentTab()) mainScreen_.tab(role, selectedSection_);
            return;
        }
        if (UiInput::click(UiLayout::content())) mainScreen_.clearFocus();
        handleMainScreenEvent(mainScreen_.update(activeAccount_->role, selectedSection_));
    }
}

bool AppController::contentModal() const {
    return selectedSection_ == Section::Resources ? resourcesScreen_.modalOpen() : backendScreen_.modalOpen();
}

bool AppController::contentTab() {
    if (selectedSection_ == Section::Resources)
        return resourcesScreen_.tab(activeAccount_->role, resourceRows_.size(), selectedResourceFavorited_, resourcesMessage_);
    if (selectedSection_ != Section::Dashboard)
        return backendScreen_.tab(selectedSection_, activeAccount_->role, backendTable_, backendAlphabet_,
                                  backendActivity_, backendMessage_);
    return false;
}

void AppController::handleMainScreenEvent(MainScreenEvent event) {
    if (event.type == MainScreenEvent::Type::CHANGE_PASSWORD) {
        changePasswordDialog_.open();
        return;
    }
    if (event.type == MainScreenEvent::Type::SELECT_SECTION) {
        resourcesScreen_.clearFocus();
        backendScreen_.clearFocus();
        navigate(event.section);
        return;
    }
    if (event.type == MainScreenEvent::Type::LOGOUT) {
        changePasswordDialog_.close();
        application_.logout();
        activeAccount_.reset();
        selectedSection_ = Section::Dashboard;
        loginInProgress_ = false;
        passwordUpgradePending_ = false;
        loginScreen_.reset();
        backendScreen_.reset();
        loginMessage_.clear();
        viewState_ = ViewState::LOGIN;
        resourceRows_.clear();
        resourceLevels_.clear();
        resourceSearch_ = {};
        selectedResource_.reset();
        selectedResourceFavorited_.reset();
        backendTable_ = {};
        backendAlphabet_.clear();
        backendActivity_ = {};
        dashboardData_ = {};
        mainScreen_.resetDashboardMonth();
        resourcesScreen_.reset();
        return;
    }
    if (event.type == MainScreenEvent::Type::REQUEST_CLOSE) {
        requestClose();
        return;
    }
    updateContent();
}

void AppController::updateContent() {
    if (selectedSection_ == Section::Resources) {
        handleResourcesEvent(resourcesScreen_.update(resourceRows_, resourceLevels_, *activeAccount_,
                                                     selectedResourceFavorited_, selectedResource_, resourcesMessage_));
    } else if (selectedSection_ != Section::Dashboard && selectedSection_ != Section::Resources) {
        handleBackendEvent(backendScreen_.update(selectedSection_, activeAccount_->role, backendAlphabet_,
                                                   backendTable_, backendActivity_, backendMessage_));
    }
}

void AppController::navigate(Section section, bool refresh) {
    if (section != selectedSection_) resourcesScreen_.closeForm();
    selectedSection_ = section;
    backendScreen_.navigate(section);
    if (!refresh) return;
    if (section == Section::Dashboard) refreshDashboard();
    else if (section == Section::Resources) refreshResources();
    else {
        auto levels = application_.levels();
        if (levels.value) resourceLevels_ = std::move(*levels.value);
        refreshBackendSection();
    }
}

void AppController::drawFrame() {
    BeginDrawing();
    drawScene();
    EndDrawing();
}

void AppController::drawScene() {
    switch (viewState_) {
    case ViewState::STARTUP_ERROR:
        startupScreen_.draw(startupMessage_);
        break;
    case ViewState::LOGIN:
        loginScreen_.draw(loginInProgress_, loginMessage_);
        break;
    case ViewState::AUTHENTICATED:
        if (activeAccount_) {
            mainScreen_.draw(*activeAccount_, selectedSection_, dashboardData_,
                             !contentModal() && !changePasswordDialog_.isOpen() && !exitDialog_);
            if (selectedSection_ == Section::Resources)
                resourcesScreen_.draw(resourceRows_, resourceLevels_, selectedResource_, *activeAccount_, resourcesMessage_, resourcesMessageError_, selectedResourceFavorited_);
            else if (selectedSection_ != Section::Dashboard && selectedSection_ != Section::Resources)
                backendScreen_.draw(selectedSection_, activeAccount_->role, backendAlphabet_, backendTable_, backendMessage_, backendMessageError_, backendActivity_);
            if (passwordUpgradePending_ && application_.dirty()) {
                const int y = GetScreenHeight() - 34;
                DrawRectangle(0, y, GetScreenWidth(), 34, UITheme::card);
                UITheme::drawText("Password upgrade is waiting to be saved.", 14, y + 8, 16, UITheme::text);
            }
            if (changePasswordDialog_.isOpen()) changePasswordDialog_.draw();
        }
        break;
    }
    if (exitDialog_) startupScreen_.drawExitDialog(exitMessage_);
}

void AppController::shutdownWindow() {
    changePasswordDialog_.close();
    if (!windowReady_) return;
    loginScreen_.clearPassword();
    UiText::shutdown();
    CloseWindow();
    windowReady_ = false;
}

void AppController::handleLoginEvent(LoginEvent event) {
    switch (event.type) {
    case LoginEvent::Type::NONE:
        return;
    case LoginEvent::Type::EDITED:
        loginMessage_.clear();
        return;
    case LoginEvent::Type::EMPTY_FIELDS:
        loginMessage_ = "Enter both username and password.";
        return;
    case LoginEvent::Type::PASSWORD_TOO_LONG:
        loginMessage_ = "Password exceeds " + std::to_string(maxPasswordBytes) + " bytes.";
        return;
    case LoginEvent::Type::SUBMIT:
        if (loginInProgress_) return;
        loginMessage_.clear();
        loginInProgress_ = true;
        return;
    case LoginEvent::Type::REQUEST_CLOSE:
        requestClose();
        return;
    }
}

void AppController::performPendingLogin() {
    loginInProgress_ = false;
    Status loginStatus;
    try {
        loginStatus = application_.login(loginScreen_.username(), loginScreen_.password());
    } catch (...) {
        loginScreen_.clearPassword();
        throw;
    }
    loginScreen_.clearPassword();

    if (!loginStatus.ok()) {
        loginMessage_ = loginStatus.message.empty() ? "Sign in failed." : loginStatus.message;
        return;
    }

    Result<AccountSummary> account = application_.currentAccount();
    if (!account.ok() || !account.value.has_value()) {
        application_.logout();
        loginMessage_ = account.status.message.empty()
                            ? "The signed-in account could not be read."
                            : account.status.message;
        return;
    }

    activeAccount_ = *account.value;
    selectedSection_ = Section::Dashboard;
    mainScreen_.resetDashboardMonth();
    refreshDashboard();
    resourceLevels_.clear();
    const auto levels = application_.levels();
    if (levels.ok() && levels.value) resourceLevels_ = *levels.value;
    backendAlphabet_.clear();
    backendTable_ = {};
    backendActivity_ = {};
    backendMessage_.clear();
    backendMessageError_ = false;
    passwordUpgradePending_ = loginStatus.save == SaveState::PENDING;
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

void AppController::refreshBackendSection() {
    backendAlphabet_.clear();
    backendTable_ = {};
    backendMessage_.clear();
    backendMessageError_ = false;
    if (!activeAccount_) return;
    if (selectedSection_ == Section::Alphabet) {
        auto result = application_.alphabet();
        if (result.value) backendAlphabet_ = std::move(*result.value);
        else reportBackend(result.status, "");
    } else if (activeAccount_->role == UserRole::ADMIN) {
        refreshAdminBackendSection(selectedSection_);
    } else {
        refreshLearnerBackendSection(selectedSection_);
    }
    backendScreen_.revalidate(backendTable_, backendAlphabet_);
}

void AppController::reportBackend(const Status& status, const char* success) {
    backendMessageError_ = !status.ok() || status.save == SaveState::PENDING;
    if (!status.ok()) backendMessage_ = status.message;
    else if (status.save == SaveState::PENDING) backendMessage_ = "Save pending: " + status.message;
    else backendMessage_ = success;
}

void AppController::handleBackendEvent(const BackendScreenEvent& event) {
    using Event = BackendScreenEvent::Type;
    if (event.type == Event::NONE) return;
    if (event.type == Event::REFRESH) { refreshBackendSection(); return; }
    backendMessage_.clear();
    backendMessageError_ = false;
    if (event.type == Event::CANCEL_FORM || event.type == Event::EDITED) return;
    if (selectedSection_ == Section::Favorites || selectedSection_ == Section::History ||
        selectedSection_ == Section::Review || selectedSection_ == Section::Quiz) {
        handleBackendLearningEvent(event);
    } else if (activeAccount_ && activeAccount_->role == UserRole::ADMIN) {
        handleAdminEvent(event);
    }
}

void AppController::requestClose() {
    loginInProgress_ = false;
    if (changePasswordDialog_.editing()) changePasswordDialog_.close();

    const Status closeStatus = application_.close();
    if (closeStatus.ok() && !application_.loaded()) {
        loginScreen_.clearPassword();
        backendScreen_.closeForm();
        resourcesScreen_.closeForm();
        backendLoaded_ = false;
        activeAccount_.reset();
        exitDialog_ = false;
        running_ = false;
        return;
    }

    exitDialog_ = true;
    startupScreen_.openExitDialog();
    exitMessage_ = closeStatus.message;
}

void AppController::retrySaveAndClose() {
    if (!application_.loaded()) {
        requestClose();
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
}

#pragma once

#include "pbl2/Application.h"
#include "../screens/LoginScreen.h"
#include "../screens/MainScreen.h"
#include "../screens/StartupScreen.h"
#include "../screens/ResourcesScreen.h"
#include "../screens/BackendScreen.h"
#include "../screens/ChangePasswordDialog.h"

#include <optional>
#include <string>

class AppController {
  public:
    ~AppController();
    int run(const std::string& dataFolder);

  private:
    friend struct RaylibStateTest;
    enum class ViewState { STARTUP_ERROR, LOGIN, AUTHENTICATED };

    Application application_;
    StartupScreen startupScreen_;
    LoginScreen loginScreen_;
    MainScreen mainScreen_;
    ResourcesScreen resourcesScreen_;
    BackendScreen backendScreen_;
    ChangePasswordDialog changePasswordDialog_;
    std::vector<AlphabetEntry> backendAlphabet_;
    BackendTableData backendTable_;
    BackendActivityState backendActivity_;
    DashboardData dashboardData_;
    std::optional<bool> selectedResourceFavorited_;
    std::vector<Level> resourceLevels_;
    std::vector<SearchRow> resourceRows_;
    SearchOptions resourceSearch_;
    std::optional<ResourceDetail> selectedResource_;
    std::optional<AccountSummary> activeAccount_;
    ViewState viewState_ = ViewState::STARTUP_ERROR;
    bool windowReady_ = false;
    bool fontReady_ = false;
    bool backendLoaded_ = false;
    bool startupSucceeded_ = false;
    bool running_ = true;
    bool exitDialog_ = false;
    bool loginInProgress_ = false;
    bool passwordUpgradePending_ = false;
    Section selectedSection_ = Section::Dashboard;
    std::string startupMessage_;
    std::string exitMessage_;
    std::string loginMessage_;
    std::string resourcesMessage_;
    std::string backendMessage_;
    bool resourcesMessageError_ = false;
    bool backendMessageError_ = false;
    int windowWidth_ = 1280;
    int windowHeight_ = 800;
    int windowX_ = 0;
    int windowY_ = 0;

    bool initializeWindow();
    void initializeApplication(const std::string& dataFolder);
    void handleStartup();
    void finishInitialization(const Status& status);
    void processFrame();
    void drawFrame();
    void drawScene();
    void shutdownWindow();
    void handleMainScreenEvent(MainScreenEvent event);
    void updateContent();
    bool contentTab();
    bool contentModal() const;
    void navigate(Section section, bool refresh = true);
    void handleLoginEvent(LoginEvent event);
    void performPendingLogin();
    void requestClose();
    void retrySaveAndClose();
    void discardAndClose();
    void cancelClose();
    void handleResourcesEvent(ResourcesEvent event);
    void refreshResources();
    void showResourceResults(Result<std::vector<SearchRow>> result, bool preserveSelection = false);
    void refreshDashboard();
    void refreshSelectedFavoriteState();
    void refreshBackendSection();
    void refreshAdminBackendSection(Section section);
    void refreshLearnerBackendSection(Section section);
    bool handleBackendLearningEvent(const BackendScreenEvent& event);
    void handleBackendEvent(const BackendScreenEvent& event);
    void handleAdminEvent(const BackendScreenEvent& event);
    void reportBackend(const Status& status, const char* success);
};

#pragma once

#include "Application.h"
#include "../screens/LoginScreen.h"
#include "../screens/MainScreen.h"
#include "../screens/StartupScreen.h"
#include "../screens/ResourcesScreen.h"
#include "../screens/BackendScreen.h"

#include <optional>
#include <string>

class AppController {
  public:
    int run();

  private:
    enum class ViewState { STARTUP_ERROR, LOGIN, AUTHENTICATED };
    struct PendingLogin {
        std::string username;
        std::string password;
    };

    Application application_;
    StartupScreen startupScreen_;
    LoginScreen loginScreen_;
    MainScreen mainScreen_;
    ResourcesScreen resourcesScreen_;
    BackendScreen backendScreen_;
    std::vector<std::string> backendRows_;
    BackendTableData backendTable_;
    BackendActivityState backendActivity_;
    DashboardData dashboardData_;
    std::optional<bool> selectedResourceFavorited_;
    std::size_t backendSelected_ = 0;
    std::vector<Level> resourceLevels_;
    std::vector<SearchRow> resourceRows_;
    std::optional<ResourceDetail> selectedResource_;
    std::optional<AccountSummary> activeAccount_;
    std::optional<PendingLogin> pendingLogin_;
    ViewState viewState_ = ViewState::STARTUP_ERROR;
    bool windowReady_ = false;
    bool fontReady_ = false;
    bool backendLoaded_ = false;
    bool startupSucceeded_ = false;
    bool running_ = true;
    bool exitDialog_ = false;
    bool nativeClosePending_ = false;
    bool loginInProgress_ = false;
    std::size_t selectedSection_ = 0;
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
    void initializeApplication();
    void processFrame();
    void drawFrame();
    void shutdownWindow();
    void handleMainScreenEvent(MainScreenEvent event);
    void handleLoginEvent(LoginEvent event);
    void performPendingLogin();
    void requestClose(bool fromNativeWindow);
    void retrySaveAndClose();
    void discardAndClose();
    void cancelClose();
    void handleResourcesEvent(ResourcesEvent event);
    void refreshResources(const SearchOptions& options = {});
    void refreshDashboard();
    void refreshSelectedFavoriteState();
    void refreshBackendSection();
    void refreshAdminBackendSection(std::size_t section);
    void refreshLearnerBackendSection(std::size_t section);
    void preloadBackendSectionText();
    bool handleBackendLearningEvent(const BackendScreenEvent& event);
    void handleBackendEvent(BackendScreenEvent event);
};

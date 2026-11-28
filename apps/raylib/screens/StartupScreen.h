#pragma once

#include "FolderField.h"

#include <string>

enum class StartupAction {
    NONE,
    REQUEST_CLOSE,
    OPEN_FOLDER,
    BEGIN_CREATE,
    CREATE_DATABASE,
    RETRY_INITIALIZE,
    RETRY_SAVE,
    DISCARD_CLOSE,
    CANCEL_CLOSE
};

class StartupScreen {
  public:
    StartupAction poll(bool exitDialog, const std::string& exitMessage = {});
    void openExitDialog() { exitFocus_.id = 2; exitMessageOffset_ = 0; }
    void setFolder(const std::string& folder, bool empty);
    std::string folder() const { return folder_.path(); }
    const std::string& admin() const { return admin_; }
    const PasswordInput& password() const { return password_; }
    void clearPassword() { password_.clear(); passwordCursor_.reset(); }
    void waitForSave();
    void draw(const std::string& startupMessage) const;
    void drawExitDialog(const std::string& exitMessage) const;

  private:
    enum Control { Folder, Browse, Open, Create, Close, Admin, Password, Cancel };
    UiInput::Focus focus_{Folder}, exitFocus_{2};
    float exitMessageOffset_ = 0.0f;
    FolderField folder_;
    bool empty_ = false;
    bool creating_ = false;
    bool pending_ = false;
    std::string admin_;
    PasswordInput password_;
    UiInput::Cursor adminCursor_, passwordCursor_;
    Rectangle closeButton() const;
    Rectangle exitChoiceButton(int index) const;
};

#pragma once

#include "UiInput.h"
#include "UiText.h"
#include "pbl2/Application.h"

#include <array>

class ChangePasswordDialog {
  public:
    enum class Action { NONE, SUBMIT, RETRY_SAVE, REQUEST_CLOSE };

    void open();
    void close() noexcept;
    bool isOpen() const { return state_ != State::CLOSED; }
    bool editing() const { return state_ == State::EDITING; }
    const std::string& currentPassword() const { return passwords_[Current].value(); }
    const std::string& newPassword() const { return passwords_[New].value(); }
    Action update();
    void draw() const;
    void showResult(const Status& status);

  private:
    friend struct RaylibStateTest;
    enum class State { CLOSED, EDITING, SAVE_PENDING, SAVED };
    enum Control { Current, New, Confirm, Visibility, Cancel, Submit };
    State state_ = State::CLOSED;
    std::array<PasswordInput, 3> passwords_;
    std::array<UiInput::Cursor, 3> cursors_;
    mutable std::array<UiText::PrivateFont, 3> revealedFonts_;
    UiInput::Focus focus_{Current};
    bool visible_ = false;
    std::string message_;

    void clearPasswords() noexcept;
};

#pragma once

#include "raylib.h"
#include "../UiInput.h"
#include "../UiText.h"

#include <string>

struct LoginEvent {
    enum class Type { NONE, EDITED, EMPTY_FIELDS, PASSWORD_TOO_LONG, SUBMIT, REQUEST_CLOSE } type = Type::NONE;
};

class LoginScreen {
  public:
    LoginEvent update(bool submitting);
    void draw(bool submitting, const std::string& message) const;
    void drawAuthenticated(const std::string& username, const std::string& role) const;
    void clearPassword() noexcept;
    const std::string& username() const noexcept { return username_; }
    const std::string& password() const noexcept { return password_.value(); }
    void reset() noexcept;

  private:
    std::string username_;
    PasswordInput password_;
    enum Control { Username, Password, Visibility, Submit, Close };
    UiInput::Focus focus_{Username};
    UiInput::Cursor usernameCursor_, passwordCursor_;
    mutable UiText::PrivateFont revealedFont_;
    bool passwordVisible_ = false;

    static Rectangle usernameField();
    static Rectangle passwordField();
    static Rectangle visibilityButton();
    static Rectangle submitButton();
    static Rectangle closeButton();
    static void drawButton(Rectangle bounds, const char* label, bool primary, bool enabled = true);
    static void drawTextWrapped(const std::string& message,
                                int x,
                                int y,
                                int maxWidth,
                                int fontSize,
                                Color color);
};

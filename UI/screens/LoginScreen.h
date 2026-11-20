#pragma once

#include "raylib.h"

#include <string>

struct LoginEvent {
    enum class Type { NONE, EMPTY_FIELDS, SUBMIT, REQUEST_CLOSE } type = Type::NONE;
    std::string username;
    std::string password;
};

class LoginScreen {
  public:
    LoginEvent update(bool submitting);
    void draw(bool submitting, const std::string& message) const;
    void drawAuthenticated(const std::string& username, const std::string& role) const;
    void clearPassword() noexcept;
    void reset() noexcept;

  private:
    std::string username_;
    std::string password_;
    bool usernameFocused_ = true;
    bool passwordFocused_ = false;
    bool passwordVisible_ = false;

    static Rectangle usernameField();
    static Rectangle passwordField();
    static Rectangle visibilityButton();
    static Rectangle submitButton();
    static Rectangle closeButton();
    static bool clicked(Rectangle bounds);
    static bool validUsernameCharacter(int codepoint);
    static std::string encodeCodepoint(int codepoint);
    static void removeLastCodepoint(std::string& text);
    static std::size_t characterCount(const std::string& text);
    static void drawButton(Rectangle bounds, const char* label, bool primary, bool enabled = true);
    static void drawTextWrapped(const std::string& message,
                                int x,
                                int y,
                                int maxWidth,
                                int fontSize,
                                Color color);
};

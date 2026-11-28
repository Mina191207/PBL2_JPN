#include "LoginScreen.h"
#include "../UITheme.h"
#include "../UiLayout.h"
#include "../Utf8.h"
#include <algorithm>
#include <sstream>

namespace {
constexpr Color kBackground = UITheme::background;
constexpr Color kCard = UITheme::card;
constexpr Color kPrimary = UITheme::primary;
constexpr Color kPrimaryHover = UITheme::primaryHover;
constexpr Color kBorder = UITheme::border;
constexpr Color kText = UITheme::text;
constexpr Color kMuted = UITheme::muted;
constexpr Color kError = UITheme::error;

}

Rectangle LoginScreen::usernameField() {
    const float x = (GetScreenWidth() - 620.0f) / 2.0f + 48.0f;
    const float y = (GetScreenHeight() - 560.0f) / 2.0f + 200.0f;
    return {x, y, 524.0f, 58.0f};
}

Rectangle LoginScreen::passwordField() {
    const Rectangle user = usernameField();
    return {user.x, user.y + 116.0f, 430.0f, 58.0f};
}

Rectangle LoginScreen::visibilityButton() {
    const Rectangle password = passwordField();
    return {password.x + password.width + 12.0f, password.y, 82.0f, password.height};
}

Rectangle LoginScreen::submitButton() {
    const Rectangle user = usernameField();
    return {user.x, user.y + 226.0f, user.width, 58.0f};
}

Rectangle LoginScreen::closeButton() {
    return {static_cast<float>(GetScreenWidth() - 176), 28.0f, 144.0f, 46.0f};
}

bool LoginScreen::clicked(Rectangle bounds) {
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           CheckCollisionPointRec(GetMousePosition(), bounds);
}

bool LoginScreen::validUsernameCharacter(int codepoint) {
    return (codepoint >= 'A' && codepoint <= 'Z') ||
           (codepoint >= 'a' && codepoint <= 'z') ||
           (codepoint >= '0' && codepoint <= '9') ||
           codepoint == '_' || codepoint == '.' || codepoint == '-';
}

void LoginScreen::removeLastCodepoint(std::string& text) {
    Utf8::eraseLast(text);
}

LoginEvent LoginScreen::update(bool submitting) {
    if (submitting) return {};
    if (clicked(closeButton()) || IsKeyPressed(KEY_ESCAPE)) return {LoginEvent::Type::REQUEST_CLOSE};

    if (IsKeyPressed(KEY_TAB)) {
        usernameFocused_ = !usernameFocused_;
        passwordFocused_ = !passwordFocused_;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, usernameField())) {
            usernameFocused_ = true;
            passwordFocused_ = false;
        } else if (CheckCollisionPointRec(mouse, passwordField())) {
            usernameFocused_ = false;
            passwordFocused_ = true;
        } else if (CheckCollisionPointRec(mouse, visibilityButton())) {
            passwordVisible_ = !passwordVisible_;
        }
    }

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (usernameFocused_) removeLastCodepoint(username_);
        if (passwordFocused_) password_.backspace();
    }

    int codepoint = GetCharPressed();
    while (codepoint > 0) {
        if (usernameFocused_ && validUsernameCharacter(codepoint) && username_.size() < 32) {
            username_.push_back(static_cast<char>(codepoint));
        } else if (passwordFocused_) password_.append(codepoint);
        codepoint = GetCharPressed();
    }

    const bool submitRequested = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
                                 clicked(submitButton());
    if (!submitRequested) return {};
    if (password_.tooLong()) {
        clearPassword();
        return {LoginEvent::Type::PASSWORD_TOO_LONG};
    }
    if (username_.empty() || password().empty()) {
        clearPassword();
        return {LoginEvent::Type::EMPTY_FIELDS};
    }
    passwordVisible_ = false;
    return {LoginEvent::Type::SUBMIT};
}

void LoginScreen::drawButton(Rectangle bounds, const char* label, bool primary, bool enabled) {
    UITheme::drawButton(bounds, label, primary, enabled);
}

void LoginScreen::drawTextWrapped(const std::string& message,
                                  int x,
                                  int y,
                                  int maxWidth,
                                  int fontSize,
                                  Color color) {
    UITheme::drawWrapped(message, x, y, maxWidth, fontSize, color);
}

void LoginScreen::draw(bool submitting, const std::string& message) const {
    ClearBackground(kBackground);
    DrawCircle(GetScreenWidth() - 80, GetScreenHeight() - 75, 142.0f, Color{255, 229, 237, 130});
    DrawCircle(88, 110, 76.0f, Color{228, 217, 250, 125});

    const Rectangle card = UiLayout::centeredDialog(620.0f, 560.0f, 12.0f);
    DrawRectangleRounded(card, 0.08f, 16, kCard);
    DrawRectangleRoundedLinesEx(card, 0.08f, 16, 2.0f, kBorder);
    UITheme::drawText("PBL2", static_cast<int>(card.x + 48), static_cast<int>(card.y + 42), 20, kPrimaryHover);
    UITheme::drawText("Welcome back", static_cast<int>(card.x + 48), static_cast<int>(card.y + 76), 34, kText);
    UITheme::drawText("Sign in to your Japanese learning library", static_cast<int>(card.x + 50), static_cast<int>(card.y + 124), 17, kMuted);

    const Rectangle user = usernameField();
    UITheme::drawText("Username", static_cast<int>(user.x), static_cast<int>(user.y - 28), 16, kText);
    DrawRectangleRounded(user, 0.18f, 8, {255, 252, 253, 255});
    DrawRectangleRoundedLinesEx(user, 0.18f, 8, usernameFocused_ ? 2.0f : 1.0f,
                                usernameFocused_ ? kPrimary : kBorder);
    UITheme::drawText(username_.empty() ? "Enter username" : username_.c_str(),
             static_cast<int>(user.x + 16), static_cast<int>(user.y + 19), 18,
             username_.empty() ? kMuted : kText);

    const Rectangle password = passwordField();
    UITheme::drawText("Password", static_cast<int>(password.x), static_cast<int>(password.y - 28), 16, kText);
    DrawRectangleRounded(password, 0.18f, 8, {255, 252, 253, 255});
    DrawRectangleRoundedLinesEx(password, 0.18f, 8, passwordFocused_ ? 2.0f : 1.0f,
                                passwordFocused_ ? kPrimary : kBorder);
    const int passwordX = static_cast<int>(password.x + 16);
    const int passwordY = static_cast<int>(password.y + 19);
    BeginScissorMode(passwordX, static_cast<int>(password.y), static_cast<int>(password.width - 32),
                     static_cast<int>(password.height));
    if (passwordVisible_ && !this->password().empty()) {
        // Use Raylib directly so revealed characters stay out of the shared font atlas queue.
        DrawText(this->password().c_str(), passwordX, passwordY, 18, kText);
    } else {
        const auto masked = password_.masked();
        UITheme::drawText(masked.empty() ? "Enter password" : masked.c_str(),
                         passwordX, passwordY, 18, masked.empty() ? kMuted : kText);
    }
    EndScissorMode();
    drawButton(visibilityButton(), passwordVisible_ ? "Hide" : "Show", false);

    if (!message.empty()) {
        drawTextWrapped(message,
                        static_cast<int>(user.x),
                        static_cast<int>(user.y + 190),
                        static_cast<int>(user.width),
                        16,
                        kError);
    }
    drawButton(submitButton(), submitting ? "Signing in..." : "Sign in", true, !submitting);
    drawButton(closeButton(), "Close", false);
    UITheme::drawText("Press ESC to close", 32, GetScreenHeight() - 42, 16, kMuted);
}

void LoginScreen::drawAuthenticated(const std::string& username, const std::string& role) const {
    ClearBackground(kBackground);
    const Rectangle card = {
        (GetScreenWidth() - 720.0f) / 2.0f,
        (GetScreenHeight() - 400.0f) / 2.0f,
        720.0f,
        400.0f
    };
    DrawRectangleRounded(card, 0.08f, 16, kCard);
    DrawRectangleRoundedLinesEx(card, 0.08f, 16, 2.0f, kBorder);
    UITheme::drawText("PBL2", static_cast<int>(card.x + 48), static_cast<int>(card.y + 46), 20, kPrimaryHover);
    UITheme::drawText("Sign in successful", static_cast<int>(card.x + 48), static_cast<int>(card.y + 92), 34, kText);
    UITheme::drawText(TextFormat("Account: %s", username.c_str()),
             static_cast<int>(card.x + 50), static_cast<int>(card.y + 174), 21, kText);
    UITheme::drawText(TextFormat("Role: %s", role.c_str()),
             static_cast<int>(card.x + 50), static_cast<int>(card.y + 218), 21, kText);
    UITheme::drawText("Role-based navigation will be added in the next step.",
             static_cast<int>(card.x + 50), static_cast<int>(card.y + 286), 17, kMuted);
    drawButton(closeButton(), "Close", false);
}

void LoginScreen::clearPassword() noexcept {
    password_.clear();
    passwordVisible_ = false;
}

void LoginScreen::reset() noexcept {
    clearPassword();
    std::fill(username_.begin(), username_.end(), '\0');
    username_.clear();
    usernameFocused_ = true;
    passwordFocused_ = false;
    passwordVisible_ = false;
}

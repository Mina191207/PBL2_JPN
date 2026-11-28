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

LoginEvent LoginScreen::update(bool submitting) {
    using Event = LoginEvent::Type;
    if (submitting) return {};
    if (UiInput::tab()) {
        focus_.move({Username, Password, Visibility, Submit, Close});
        return {};
    }
    if (IsKeyPressed(KEY_ESCAPE) || focus_.activate(Close, closeButton())) return {Event::REQUEST_CLOSE};
    if (UiInput::click(usernameField())) focus_.id = Username;
    if (UiInput::click(passwordField())) focus_.id = Password;
    if (focus_.activate(Visibility, visibilityButton())) {
        passwordVisible_ = !passwordVisible_;
        if (!passwordVisible_) revealedFont_.clear();
        return {};
    }

    const bool inField = focus_.on(Username) || focus_.on(Password);
    UiInput::Edit edit;
    if (focus_.on(Username)) edit = UiInput::text(username_, usernameCursor_, 32, UiInput::username);
    if (focus_.on(Password)) edit = UiInput::password(password_, passwordCursor_);
    if (!focus_.activate(Submit, submitButton()) && !(inField && UiInput::enter()))
        return {edit.changed ? Event::EDITED : Event::NONE};
    if (password_.tooLong()) {
        clearPassword();
        focus_.id = Password;
        return {Event::PASSWORD_TOO_LONG};
    }
    if (username_.empty() || password().empty()) {
        focus_.id = username_.empty() ? Username : Password;
        return {Event::EMPTY_FIELDS};
    }
    passwordVisible_ = false;
    revealedFont_.clear();
    return {Event::SUBMIT};
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
    UITheme::drawField(user, "Username", username_, focus_.on(Username),
                       usernameCursor_.at(username_), "Enter username");
    const auto caret = passwordCursor_.at(password());
    if (passwordVisible_) {
        UITheme::drawField(passwordField(), "Password", password(), focus_.on(Password),
                           caret, "Enter password", revealedFont_.prepare(password()));
    } else {
        const auto masked = password_.masked();
        const auto maskedCaret = Utf8::countValid(std::string_view(password()).substr(0, caret));
        UITheme::drawField(passwordField(), "Password", masked, focus_.on(Password),
                           maskedCaret, "Enter password");
    }
    UITheme::drawButton(visibilityButton(), passwordVisible_ ? "Hide" : "Show", false, true, false, focus_.on(Visibility));

    if (!message.empty()) {
        drawTextWrapped(message,
                        static_cast<int>(user.x),
                        static_cast<int>(user.y + 190),
                        static_cast<int>(user.width),
                        16,
                        kError);
    }
    UITheme::drawButton(submitButton(), submitting ? "Signing in..." : "Sign in", true, !submitting, false, focus_.on(Submit));
    UITheme::drawText("Need an account or forgot your password?", static_cast<int>(user.x),
                      static_cast<int>(user.y + 299), 17, kText);
    UITheme::drawText("Contact your Admin for an account or a password reset.", static_cast<int>(user.x),
                      static_cast<int>(user.y + 323), 16, kMuted);
    UITheme::drawButton(closeButton(), "Close", false, true, false, focus_.on(Close));
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
    passwordCursor_.reset();
    revealedFont_.clear();
    passwordVisible_ = false;
}

void LoginScreen::reset() noexcept {
    clearPassword();
    std::fill(username_.begin(), username_.end(), '\0');
    username_.clear();
    usernameCursor_.reset();
    focus_.id = Username;
}

#include "StartupScreen.h"
#include "../UITheme.h"
#include "../UiLayout.h"
#include <sstream>

namespace {
constexpr Color kBackground = UITheme::background;
constexpr Color kCard = UITheme::card;
constexpr Color kPrimaryHover = UITheme::primaryHover;
constexpr Color kBorder = UITheme::border;
constexpr Color kText = UITheme::text;
constexpr Color kMuted = UITheme::muted;
constexpr Color kSuccess = {82, 145, 112, 255};
constexpr Color kError = UITheme::error;
constexpr Color kOverlay = {46, 31, 38, 150};

bool pointInside(Rectangle bounds, Vector2 point) {
    return CheckCollisionPointRec(point, bounds);
}
}

Rectangle StartupScreen::closeButton() const {
    return {static_cast<float>(GetScreenWidth() - 176), 28.0f, 144.0f, 46.0f};
}

Rectangle StartupScreen::exitChoiceButton(int index) const {
    constexpr float width = 194.0f;
    constexpr float gap = 14.0f;
    const float totalWidth = width * 3.0f + gap * 2.0f;
    const float startX = (static_cast<float>(GetScreenWidth()) - totalWidth) / 2.0f;
    const float y = static_cast<float>(GetScreenHeight()) / 2.0f + 86.0f;
    return {startX + index * (width + gap), y, width, 54.0f};
}

bool StartupScreen::clicked(Rectangle bounds) {
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           pointInside(bounds, GetMousePosition());
}

StartupAction StartupScreen::poll(bool exitDialog) const {
    if (IsKeyPressed(KEY_ESCAPE)) {
        return exitDialog ? StartupAction::CANCEL_CLOSE : StartupAction::REQUEST_CLOSE;
    }

    if (!exitDialog) {
        return clicked(closeButton()) ? StartupAction::REQUEST_CLOSE : StartupAction::NONE;
    }

    if (clicked(exitChoiceButton(0))) return StartupAction::RETRY_SAVE;
    if (clicked(exitChoiceButton(1))) return StartupAction::DISCARD_CLOSE;
    if (clicked(exitChoiceButton(2))) return StartupAction::CANCEL_CLOSE;
    return StartupAction::NONE;
}

void StartupScreen::drawButton(Rectangle bounds, const char* label, bool primary) {
    UITheme::drawButton(bounds, label, primary);
}

void StartupScreen::drawWrapped(const std::string& message,
                                int x,
                                int y,
                                int maxWidth,
                                int fontSize,
                                Color color) {
    UITheme::drawWrapped(message, x, y, maxWidth, fontSize, color);
}

void StartupScreen::draw(bool backendReady,
                         const std::string& startupMessage,
                         bool exitDialog,
                         const std::string& exitMessage) const {
    ClearBackground(kBackground);

    DrawCircle(GetScreenWidth() - 90, GetScreenHeight() - 80, 150.0f, Color{255, 229, 237, 130});
    DrawCircle(90, 110, 82.0f, Color{228, 217, 250, 125});

    const Rectangle card = UiLayout::centeredDialog(820.0f, 440.0f, 20.0f);
    DrawRectangleRounded(card, 0.08f, 16, kCard);
    DrawRectangleRoundedLinesEx(card, 0.08f, 16, 2.0f, kBorder);

    UITheme::drawText("PBL2", static_cast<int>(card.x + 58), static_cast<int>(card.y + 48), 22, kPrimaryHover);
    UITheme::drawText("Japanese Learning", static_cast<int>(card.x + 58), static_cast<int>(card.y + 92), 38, kText);
    UITheme::drawText("Application startup", static_cast<int>(card.x + 60), static_cast<int>(card.y + 156), 20, kMuted);

    if (backendReady) {
        UITheme::drawText("Backend data loaded", static_cast<int>(card.x + 60), static_cast<int>(card.y + 222), 24, kSuccess);
        UITheme::drawText("The application shell is ready.", static_cast<int>(card.x + 60), static_cast<int>(card.y + 266), 19, kText);
        UITheme::drawText("Login and role-based navigation are ready.",
                 static_cast<int>(card.x + 60), static_cast<int>(card.y + 302), 17, kMuted);
    } else {
        UITheme::drawText("Could not load backend data", static_cast<int>(card.x + 60), static_cast<int>(card.y + 222), 24, kError);
        drawWrapped(startupMessage.empty() ? "The database folder could not be loaded."
                                           : startupMessage,
                    static_cast<int>(card.x + 60),
                    static_cast<int>(card.y + 266),
                    690,
                    17,
                    kText);
        UITheme::drawText("Check the database folder, then close this window safely.",
                 static_cast<int>(card.x + 60), static_cast<int>(card.y + 354), 16, kMuted);
    }

    drawButton(closeButton(), "Close", false);
    UITheme::drawText("Press ESC to close", 32, GetScreenHeight() - 42, 16, kMuted);
    if (exitDialog) drawExitDialog(exitMessage);
}

void StartupScreen::drawExitDialog(const std::string& exitMessage) const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), kOverlay);
    const Rectangle dialog = UiLayout::centeredDialog(720.0f, 300.0f, 20.0f);
    DrawRectangleRounded(dialog, 0.08f, 16, kCard);
    DrawRectangleRoundedLinesEx(dialog, 0.08f, 16, 2.0f, kBorder);
    UITheme::drawText("Close application?", static_cast<int>(dialog.x + 34), static_cast<int>(dialog.y + 34), 27, kText);
    UITheme::drawText("Choose how to handle pending backend data.",
             static_cast<int>(dialog.x + 34), static_cast<int>(dialog.y + 82), 18, kMuted);
    if (!exitMessage.empty()) {
        drawWrapped(exitMessage,
                    static_cast<int>(dialog.x + 34),
                    static_cast<int>(dialog.y + 116),
                    650,
                    16,
                    kError);
    }

    drawButton(exitChoiceButton(0), "Try save", true);
    drawButton(exitChoiceButton(1), "Discard and exit", false);
    drawButton(exitChoiceButton(2), "Cancel", false);
}

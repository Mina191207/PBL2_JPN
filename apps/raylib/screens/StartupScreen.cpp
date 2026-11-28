#include "StartupScreen.h"
#include "../UITheme.h"
#include "../UiLayout.h"
#include "pbl2/InputLimits.h"

namespace {
constexpr Color kBackground = UITheme::background;
constexpr Color kCard = UITheme::card;
constexpr Color kBorder = UITheme::border;
constexpr Color kText = UITheme::text;
constexpr Color kMuted = UITheme::muted;
constexpr Color kError = UITheme::error;
constexpr Color kOverlay = {46, 31, 38, 150};

struct StartupLayout {
    Rectangle card = UiLayout::centeredDialog(820, 436, 20);
    Rectangle folder{card.x + 30, card.y + 108, card.width - 60, 42};
    Rectangle admin{folder.x, card.y + 185, folder.width, 42};
    Rectangle password{folder.x, card.y + 258, folder.width, 42};
    Rectangle open{folder.x, card.y + card.height - 62, 144, 40};
    Rectangle create{open.x + 156, open.y, 174, open.height};
};

Rectangle exitBanner(Rectangle dialog, float buttonY, const std::string& message) {
    const float y = dialog.y + 116.0f;
    const float width = dialog.width - 68.0f;
    return {dialog.x + 34.0f, y, width, std::min(UITheme::bannerHeight(width, message), buttonY - y - 10.0f)};
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

StartupAction StartupScreen::poll(bool exitDialog, const std::string& exitMessage) {
    if (exitDialog) {
        const auto banner = exitBanner(UiLayout::centeredDialog(720, 300, 20), exitChoiceButton(0).y, exitMessage);
        const float height = UITheme::bannerHeight(banner.width, exitMessage);
        exitMessageOffset_ = height > banner.height ? UiInput::scroll(banner, height, exitMessageOffset_, true) : 0.0f;
        if (IsKeyPressed(KEY_ESCAPE)) return StartupAction::CANCEL_CLOSE;
        if (UiInput::tab()) { exitFocus_.move({0, 1, 2}); return StartupAction::NONE; }
        if (exitFocus_.activate(0, exitChoiceButton(0))) return StartupAction::RETRY_SAVE;
        if (exitFocus_.activate(1, exitChoiceButton(1))) return StartupAction::DISCARD_CLOSE;
        if (exitFocus_.activate(2, exitChoiceButton(2))) return StartupAction::CANCEL_CLOSE;
        return StartupAction::NONE;
    }

    const StartupLayout ui;
    if (UiInput::tab()) {
        if (pending_) focus_.move({Open, Close});
        else if (creating_) focus_.move({Admin, Password, Create, Cancel, Close});
        else {
            std::vector<int> order{Folder};
            if (FolderField::nativeBrowse) order.push_back(Browse);
            order.push_back(Open);
            if (empty_) order.push_back(Create);
            order.push_back(Close);
            focus_.move(order);
        }
        return StartupAction::NONE;
    }
    if (focus_.activate(Close, closeButton())) return StartupAction::REQUEST_CLOSE;
    if (creating_ && (IsKeyPressed(KEY_ESCAPE) || focus_.activate(Cancel, ui.create))) {
        creating_ = false;
        clearPassword();
        admin_.clear();
        adminCursor_.reset();
        focus_.id = Create;
        return StartupAction::OPEN_FOLDER;
    }
    if (IsKeyPressed(KEY_ESCAPE)) return StartupAction::REQUEST_CLOSE;
    if (pending_) {
        if (focus_.activate(Open, ui.open)) return StartupAction::RETRY_INITIALIZE;
    } else if (creating_) {
        if (UiInput::click(ui.admin)) focus_.id = Admin;
        if (UiInput::click(ui.password)) focus_.id = Password;
        if (focus_.on(Admin)) UiInput::text(admin_, adminCursor_, 32, UiInput::username);
        if (focus_.on(Password)) UiInput::password(password_, passwordCursor_);
        if (focus_.activate(Create, ui.open) ||
            (UiInput::enter() && (focus_.on(Admin) || focus_.on(Password))))
            return StartupAction::CREATE_DATABASE;
    } else {
        if (folder_.update(ui.folder, focus_, Folder, Browse)) empty_ = false;
        if (focus_.activate(Open, ui.open) || (focus_.on(Folder) && UiInput::enter())) return StartupAction::OPEN_FOLDER;
        if (focus_.activate(Create, ui.create, empty_)) {
            creating_ = true;
            focus_.id = Admin;
            return StartupAction::BEGIN_CREATE;
        }
    }
    return StartupAction::NONE;
}

void StartupScreen::setFolder(const std::string& folder, bool empty) {
    folder_.set(folder);
    empty_ = empty;
    creating_ = pending_ = false;
    admin_.clear();
    adminCursor_.reset();
    clearPassword();
    focus_.id = Folder;
}

void StartupScreen::waitForSave() {
    creating_ = false;
    pending_ = true;
    focus_.id = Open;
}

void StartupScreen::draw(const std::string& startupMessage) const {
    ClearBackground(kBackground);

    DrawCircle(GetScreenWidth() - 90, GetScreenHeight() - 80, 150.0f, Color{255, 229, 237, 130});
    DrawCircle(90, 110, 82.0f, Color{228, 217, 250, 125});

    const StartupLayout ui;
    const Rectangle card = ui.card;
    DrawRectangleRounded(card, 0.08f, 16, kCard);
    DrawRectangleRoundedLinesEx(card, 0.08f, 16, 2.0f, kBorder);

    const int x = static_cast<int>(ui.folder.x);
    UITheme::drawText(creating_ ? "Set up the first Admin" : "Open a database", x,
                      static_cast<int>(card.y + 32), 28, kText);
    folder_.draw(ui.folder, "Data folder", focus_, Folder, Browse, !creating_ && !pending_);
    if (creating_) {
        UITheme::drawField(ui.admin, "Admin username", admin_, focus_.on(Admin), adminCursor_.at(admin_));
        UITheme::drawField(ui.password, "Admin password", password_.masked(), focus_.on(Password),
                           Utf8::countValid(std::string_view(password_.value()).substr(0, passwordCursor_.at(password_.value()))));
        UITheme::drawButton(ui.open, "Create", true, true, false, focus_.on(Create));
        UITheme::drawButton(ui.create, "Cancel", false, true, false, focus_.on(Cancel));
    } else {
        UITheme::drawButton(ui.open, pending_ ? "Retry save" : "Open folder", true, true, false, focus_.on(Open));
        if (empty_ && !pending_)
            UITheme::drawButton(ui.create, "Create database", false, true, false, focus_.on(Create));
    }
    UITheme::drawWrapped(startupMessage, x, static_cast<int>(card.y + (creating_ ? 314.0f : 176.0f)),
                         static_cast<int>(ui.folder.width), 16, kError, creating_ ? 2 : 8);
    UITheme::drawButton(closeButton(), "Close", false, true, false, focus_.on(Close));
}

void StartupScreen::drawExitDialog(const std::string& exitMessage) const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), kOverlay);
    const Rectangle dialog = UiLayout::centeredDialog(720.0f, 300.0f, 20.0f);
    DrawRectangleRounded(dialog, 0.08f, 16, kCard);
    DrawRectangleRoundedLinesEx(dialog, 0.08f, 16, 2.0f, kBorder);
    UITheme::drawText("Close application?", static_cast<int>(dialog.x + 34), static_cast<int>(dialog.y + 34), 27, kText);
    UITheme::drawText("Choose how to handle pending backend data.",
             static_cast<int>(dialog.x + 34), static_cast<int>(dialog.y + 82), 18, kMuted);
    UITheme::drawStatusBanner(exitBanner(dialog, exitChoiceButton(0).y, exitMessage), exitMessage, true, exitMessageOffset_);

    UITheme::drawButton(exitChoiceButton(0), "Try save", true, true, false, exitFocus_.on(0));
    UITheme::drawButton(exitChoiceButton(1), "Discard and exit", false, true, false, exitFocus_.on(1));
    UITheme::drawButton(exitChoiceButton(2), "Cancel", false, true, false, exitFocus_.on(2));
}

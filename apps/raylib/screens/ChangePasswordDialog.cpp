#include "ChangePasswordDialog.h"
#include "UITheme.h"
#include "UiLayout.h"

namespace {
Rectangle panel() { return UiLayout::centeredDialog(560, 435); }
Rectangle field(int index) {
    const auto box = panel();
    return {box.x + 24, box.y + 94 + index * 70.0f, box.width - 48, 42};
}
Rectangle button(float x, float width) {
    const auto box = panel();
    return {box.x + x, box.y + box.height - 58, width, 38};
}
}

void ChangePasswordDialog::clearPasswords() noexcept {
    for (auto& password : passwords_) password.clear();
    for (auto& cursor : cursors_) cursor.reset();
    for (auto& font : revealedFonts_) font.clear();
    visible_ = false;
}

void ChangePasswordDialog::close() noexcept {
    clearPasswords();
    message_.clear();
    state_ = State::CLOSED;
}

void ChangePasswordDialog::open() {
    close();
    focus_.id = Current;
    state_ = State::EDITING;
}

ChangePasswordDialog::Action ChangePasswordDialog::update() {
    const bool escape = IsKeyPressed(KEY_ESCAPE);
    if (state_ == State::SAVE_PENDING) {
        if (UiInput::tab()) focus_.move({Submit, Cancel});
        else if (escape || focus_.activate(Cancel, button(274, 116))) return Action::REQUEST_CLOSE;
        else if (focus_.activate(Submit, button(402, 134))) return Action::RETRY_SAVE;
        return Action::NONE;
    }
    if (state_ == State::SAVED) {
        if (escape || focus_.activate(Submit, button(402, 134))) close();
        return Action::NONE;
    }
    if (UiInput::tab()) {
        focus_.move({Current, New, Confirm, Visibility, Cancel, Submit});
        return Action::NONE;
    }
    if (escape || focus_.activate(Cancel, button(274, 116))) {
        close();
        return Action::NONE;
    }
    if (focus_.activate(Visibility, button(24, 172))) {
        visible_ = !visible_;
        if (!visible_)
            for (auto& font : revealedFonts_) font.clear();
        return Action::NONE;
    }
    for (int index = Current; index <= Confirm; ++index) {
        if (UiInput::click(field(index))) focus_.id = index;
        if (focus_.on(index) && UiInput::password(passwords_[index], cursors_[index]).changed) message_.clear();
    }
    if (!focus_.activate(Submit, button(402, 134)) && !(focus_.id <= Confirm && UiInput::enter()))
        return Action::NONE;
    for (int index = Current; index <= Confirm; ++index) {
        if (passwords_[index].tooLong()) {
            message_ = "Password exceeds " + std::to_string(maxPasswordBytes) + " bytes.";
            focus_.id = index;
            return Action::NONE;
        }
        if (passwords_[index].value().empty()) {
            message_ = "Fill in all three password fields.";
            focus_.id = index;
            return Action::NONE;
        }
    }
    if (newPassword() != passwords_[Confirm].value()) {
        message_ = "New password and confirmation do not match.";
        focus_.id = Confirm;
        return Action::NONE;
    }
    return Action::SUBMIT;
}

void ChangePasswordDialog::showResult(const Status& status) {
    if (status.save == SaveState::PENDING) {
        clearPasswords();
        if (state_ != State::SAVE_PENDING) focus_.id = Submit;
        state_ = State::SAVE_PENDING;
        message_ = "Your password change is waiting to be saved. Retry Save to write it to disk.\n" + status.message;
    } else if (!status.ok()) {
        message_ = status.message;
    } else {
        clearPasswords();
        focus_.id = Submit;
        state_ = State::SAVED;
        message_ = "Password changed and saved.";
    }
}

void ChangePasswordDialog::draw() const {
    const auto box = panel();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{46, 31, 38, 150});
    DrawRectangleRounded(box, 0.04f, 10, RAYWHITE);
    DrawRectangleRoundedLinesEx(box, 0.04f, 10, 1.5f, UITheme::border);
    UITheme::drawText("Change password", static_cast<int>(box.x + 24), static_cast<int>(box.y + 22), 22, UITheme::text);
    if (editing()) {
        constexpr const char* labels[] = {"Current password", "New password", "Confirm new password"};
        for (int index = Current; index <= Confirm; ++index) {
            const auto& password = passwords_[index];
            const auto& value = password.value();
            const auto caret = cursors_[index].at(value);
            if (visible_) {
                UITheme::drawField(field(index), labels[index], value, focus_.on(index), caret, "",
                                   revealedFonts_[index].prepare(value));
            } else {
                UITheme::drawField(field(index), labels[index], password.masked(), focus_.on(index),
                                   Utf8::countValid(std::string_view(value).substr(0, caret)));
            }
        }
        UITheme::drawButton(button(24, 172), visible_ ? "Hide passwords" : "Show passwords",
                            false, true, false, focus_.on(Visibility));
    }
    if (!message_.empty()) {
        const auto y = box.y + (editing() ? 296 : 78);
        UITheme::drawWrapped(message_, static_cast<int>(box.x + 24), static_cast<int>(y),
                             static_cast<int>(box.width - 48), 16,
                             state_ == State::SAVED ? UITheme::text : UITheme::error, editing() ? 3 : 10);
    }
    if (state_ != State::SAVED)
        UITheme::drawButton(button(274, 116), editing() ? "Cancel" : "Close app", false, true, false, focus_.on(Cancel));
    UITheme::drawButton(button(402, 134), editing() ? "Change" : state_ == State::SAVE_PENDING ? "Retry Save" : "Done",
                        true, true, false, focus_.on(Submit));
}

#include "BackendScreen.h"
#include "UITheme.h"
#include "UiLayout.h"
#include "Utf8.h"
#include "pbl2/InputLimits.h"

namespace {
using Field = FieldId;
Rectangle dialog() { return UiLayout::centeredDialog(760, 520, 20); }
Rectangle fieldBounds(std::size_t index, std::size_t count) {
    const auto panel = dialog();
    const bool twoColumns = count > 1;
    const float width = (panel.width - (twoColumns ? 64.0f : 48.0f)) / (twoColumns ? 2.0f : 1.0f);
    return {panel.x + 24 + (twoColumns ? static_cast<float>(index % 2) * (width + 16) : 0),
            panel.y + 84 + static_cast<float>(twoColumns ? index / 2 : index) * 58, width, 38};
}
Rectangle cancelButton() { const auto p = dialog(); return {p.x+p.width-248,p.y+p.height-52,104,36}; }
Rectangle submitButton() { const auto p = dialog(); return {p.x+p.width-128,p.y+p.height-52,104,36}; }

bool choice(Field id) {
    return id == Field::Role || id == Field::Script || id == Field::Correct || id == Field::Status;
}
void cycle(FormField& field) {
    auto& value = field.value;
    switch (field.id) {
    case Field::Role: value = value == "ADMIN" ? "LEARNER" : "ADMIN"; break;
    case Field::Script: value = value == "KATAKANA" ? "HIRAGANA" : "KATAKANA"; break;
    case Field::Status: value = value == "INACTIVE" ? "ACTIVE" : "INACTIVE"; break;
    case Field::Correct: value = value == "A" ? "B" : value == "B" ? "C" : value == "C" ? "D" : "A"; break;
    default: break;
    }
}
}

FormFields adminFormFields(Section section) {
    switch (section) {
    case Section::Levels: return {{Field::Category, "Category", "JLPT"}, {Field::Name, "Name"},
                                  {Field::Order, "Order"}, {Field::Description, "Description"}};
    case Section::Accounts: return {{Field::Username, "Username"}, {Field::Password, "Password"},
                                    {Field::Role, "Role", "LEARNER"}};
    case Section::Alphabet: return {{Field::Script, "Script", "HIRAGANA"}, {Field::Glyph, "Glyph"},
                                    {Field::Romaji, "Romaji"}, {Field::Group, "Group"},
                                    {Field::Order, "Display order"}, {Field::Note, "Note"}};
    case Section::Questions: return {{Field::Level, "Level ID"}, {Field::Resource, "Resource ID (optional)"},
        {Field::Prompt, "Prompt"}, {Field::OptionA, "Option A"}, {Field::OptionB, "Option B"},
        {Field::OptionC, "Option C"}, {Field::OptionD, "Option D"}, {Field::Correct, "Correct option", "A"},
        {Field::Explanation, "Explanation"}, {Field::Status, "Status", "ACTIVE"}};
    default: return {};
    }
}

void BackendScreen::closeForm() noexcept {
    clearPassword();
    formFields_.clear();
    editId_ = {};
    focusedField_ = 0;
    formOpen_ = editing_ = resetPasswordForm_ = false;
}

void BackendScreen::openCreateForm(Section section) {
    closeForm();
    formFields_ = adminFormFields(section);
    formOpen_ = !formFields_.empty();
}

void BackendScreen::openEditForm(FormFields fields, BackendId id) {
    closeForm();
    formFields_ = std::move(fields);
    editId_ = std::move(id);
    formOpen_ = editing_ = true;
}

void BackendScreen::openResetPasswordForm(const UserId& id) {
    closeForm();
    formFields_ = {{Field::Password, "New password"}};
    editId_ = id;
    formOpen_ = resetPasswordForm_ = true;
}

BackendScreenEvent BackendScreen::updateForm() {
    using Event = BackendScreenEvent::Type;
    if (IsKeyPressed(KEY_ESCAPE) || click(cancelButton())) {
        closeForm();
        return {Event::CANCEL_FORM};
    }
    const auto count = formFields_.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (!click(fieldBounds(i, count))) continue;
        focusedField_ = i;
        if (choice(formFields_[i].id)) cycle(formFields_[i]);
    }
    if (IsKeyPressed(KEY_TAB)) {
        const bool backwards = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        focusedField_ = (focusedField_ + (backwards ? count - 1 : 1)) % count;
    }
    auto& field = formFields_[focusedField_];
    const bool secret = field.id == Field::Password;
    const bool choose = choice(field.id);
    if (!choose && (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE))) {
        if (secret) password_.backspace();
        else Utf8::eraseLast(field.value);
    }
    for (int cp = GetCharPressed(); cp > 0; cp = GetCharPressed()) {
        if (secret) password_.append(cp);
        else if (!choose && cp >= 32 && cp != 127)
            Utf8::appendWithinLimit(field.value, Utf8::encode(cp), maxTxtFieldBytes);
    }
    const bool enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    if (choose && enter) cycle(field);
    if (click(submitButton()) || (!choose && enter)) {
        if (password_.tooLong()) {
            clearPassword();
            return {Event::PASSWORD_TOO_LONG};
        }
        return {resetPasswordForm_ ? Event::RESET_PASSWORD : Event::SAVE_FORM, editId_};
    }
    return {};
}

void BackendScreen::drawForm(Section section, const std::string& error) const {
    const auto panel = dialog();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{46,31,38,150});
    DrawRectangleRounded(panel, 0.04f, 10, RAYWHITE);
    DrawRectangleRoundedLinesEx(panel, 0.04f, 10, 1.5f, UITheme::border);
    const std::string title = resetPasswordForm_ ? "Reset password" :
        std::string(editing_ ? "Edit " : "Create ") + sectionTitle(UserRole::ADMIN, section);
    UITheme::drawText(title.c_str(), static_cast<int>(panel.x+24), static_cast<int>(panel.y+22), 22, UITheme::text);
    for (std::size_t i = 0; i < formFields_.size(); ++i) {
        const auto& field = formFields_[i];
        const auto bounds = fieldBounds(i, formFields_.size());
        if (choice(field.id)) {
            const std::string label = std::string(field.label) + " · " + field.value;
            button(bounds, label.c_str(), focusedField_ == i);
        } else {
            UITheme::drawField(bounds, field.label,
                field.id == Field::Password ? password_.masked() : field.value, focusedField_ == i);
        }
    }
    if (!error.empty())
        UITheme::drawStatusBanner({panel.x+24, panel.y+panel.height-96, panel.width-48, 36}, error, true);
    button(cancelButton(), "Cancel");
    button(submitButton(), resetPasswordForm_ ? "Reset" : editing_ ? "Save" : "Create", true);
}

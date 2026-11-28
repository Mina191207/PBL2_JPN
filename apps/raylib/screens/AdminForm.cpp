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
    modalFocus_.id = 0;
    formCursor_.reset();
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
    const auto count = static_cast<int>(formFields_.size());
    const int cancel = count;
    const int submit = count + 1;
    if (UiInput::tab()) {
        std::vector<int> order;
        for (int id = 0; id <= submit; ++id) order.push_back(id);
        modalFocus_.move(order);
        formCursor_.reset();
        return {};
    }
    if (IsKeyPressed(KEY_ESCAPE) || modalFocus_.activate(cancel, cancelButton())) {
        closeForm();
        return {Event::CANCEL_FORM};
    }
    if (UiInput::click(submitButton())) modalFocus_.id = submit;
    UiInput::Edit edit;
    bool submitRequested = modalFocus_.on(submit) && (UiInput::click(submitButton()) || UiInput::confirm());
    for (int i = 0; i < count; ++i) {
        auto& field = formFields_[i];
        if (choice(field.id)) {
            if (modalFocus_.activate(i, fieldBounds(i, count))) {
                cycle(field);
                formCursor_.reset();
                return {Event::EDITED};
            }
            continue;
        }
        if (UiInput::click(fieldBounds(i, count)) && !modalFocus_.on(i)) {
            modalFocus_.id = i;
            formCursor_.reset();
        }
        if (!modalFocus_.on(i)) continue;
        if (field.id == Field::Password) edit = UiInput::password(password_, formCursor_);
        else if (field.id == Field::Username) edit = UiInput::text(field.value, formCursor_, 32, UiInput::username);
        else edit = UiInput::text(field.value, formCursor_, maxTxtFieldBytes);
        submitRequested = UiInput::enter();
    }
    if (!submitRequested) return {edit.changed ? Event::EDITED : Event::NONE};
    if (password_.tooLong()) {
        clearPassword();
        formCursor_.reset();
        return {Event::PASSWORD_TOO_LONG};
    }
    return {resetPasswordForm_ ? Event::RESET_PASSWORD : Event::SAVE_FORM, editId_};
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
            UITheme::drawButton(bounds, label.c_str(), false, true, false, modalFocus_.on(static_cast<int>(i)));
        } else {
            const bool focused = modalFocus_.on(static_cast<int>(i));
            if (field.id == Field::Password) {
                const auto mask = password_.masked();
                const auto caret = Utf8::countValid(std::string_view(password()).substr(0, formCursor_.at(password())));
                UITheme::drawField(bounds, field.label, mask, focused, focused ? caret : mask.size());
            } else {
                UITheme::drawField(bounds, field.label, field.value, focused,
                                   focused ? formCursor_.at(field.value) : field.value.size());
            }
        }
    }
    if (!error.empty()) {
        const float width = panel.width - 48.0f;
        const float height = UITheme::bannerHeight(width, error);
        UITheme::drawStatusBanner({panel.x + 24, submitButton().y - height - 8, width, height}, error, true);
    }
    const auto cancel = static_cast<int>(formFields_.size());
    UITheme::drawButton(cancelButton(), "Cancel", false, true, false, modalFocus_.on(cancel));
    UITheme::drawButton(submitButton(), resetPasswordForm_ ? "Reset" : editing_ ? "Save" : "Create",
                        true, true, false, modalFocus_.on(cancel + 1));
}

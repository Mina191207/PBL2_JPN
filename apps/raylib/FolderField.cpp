#include "FolderField.h"
#include <utility>

#ifdef _WIN32
std::optional<std::string> pickNativeFolder(void* owner, const std::string& current);
#endif

namespace {
Rectangle browseBounds(Rectangle field) {
    return {field.x + field.width - 104, field.y, 104, field.height};
}

Rectangle pathBounds(Rectangle field) {
    if (FolderField::nativeBrowse) field.width -= 116;
    return field;
}

std::optional<std::string> browse(const std::string& current) {
#ifdef _WIN32
    return pickNativeFolder(GetWindowHandle(), current);
#else
    (void)current;
    return {};
#endif
}
}

void FolderField::set(std::string value) {
    value_ = std::move(value);
    cursor_.reset();
    replaceOnEdit_ = false;
}

std::string FolderField::path() const {
#ifdef _WIN32
    if (value_.size() >= 2 && value_.front() == '"' && value_.back() == '"')
        return value_.substr(1, value_.size() - 2);
#endif
    return value_;
}

bool FolderField::update(Rectangle bounds, UiInput::Focus& focus, int pathId, int browseId) {
    if (UiInput::click(pathBounds(bounds))) focus.id = pathId;
    if (focus.activate(browseId, browseBounds(bounds), nativeBrowse)) {
        if (auto selected = browse(path())) { set(std::move(*selected)); return true; }
        return false;
    }
    if (!focus.on(pathId)) return false;
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_A)) {
        // Treat the next edit as a replacement, including a pasted Explorer path.
        cursor_.offset = 0;
        replaceOnEdit_ = true;
        return false;
    }
    if (replaceOnEdit_) {
        std::string replacement;
        UiInput::Cursor editCursor;
        const auto edit = UiInput::text(replacement, editCursor, 4096, UiInput::printable);
        if (edit.changed || UiInput::key(KEY_BACKSPACE) || UiInput::key(KEY_DELETE)) {
            set(std::move(replacement));
            return true;
        }
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_HOME)) replaceOnEdit_ = false;
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_END)) { cursor_.reset(); replaceOnEdit_ = false; }
        return false;
    }
    return UiInput::text(value_, cursor_, 4096, UiInput::printable).changed;
}

void FolderField::draw(Rectangle bounds, const char* label, const UiInput::Focus& focus,
                       int pathId, int browseId, bool enabled) const {
    const bool pathFocused = enabled && focus.on(pathId);
    UITheme::drawField(pathBounds(bounds), label, value_, pathFocused, cursor_.at(value_));
    if (nativeBrowse)
        UITheme::drawButton(browseBounds(bounds), "Browse...", false, enabled, false, enabled && focus.on(browseId));
}

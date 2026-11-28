#pragma once

#include "PasswordInput.h"
#include "raylib.h"
#include <algorithm>
#include <iterator>
#include <optional>
#include <string_view>
#include <vector>

namespace UiInput {
inline bool click(Rectangle bounds) {
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), bounds);
}
inline bool shift() { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); }
inline bool tab() { return IsKeyPressed(KEY_TAB); }
inline bool enter() { return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER); }
inline bool confirm() { return enter() || IsKeyPressed(KEY_SPACE); }
inline bool key(int code) { return IsKeyPressed(code) || IsKeyPressedRepeat(code); }

struct Focus {
    static constexpr int none = -1;
    int id = none;

    bool on(int control) const { return id == control; }
    bool move(const std::vector<int>& order, bool wrap = true) {
        if (order.empty()) { id = none; return false; }
        const auto current = std::find(order.begin(), order.end(), id);
        const bool back = shift();
        if (current == order.end()) id = back ? order.back() : order.front();
        else if (back && current != order.begin()) id = *std::prev(current);
        else if (!back && std::next(current) != order.end()) id = *std::next(current);
        else id = wrap ? (back ? order.back() : order.front()) : none;
        return id != none;
    }
    bool activate(int control, Rectangle bounds, bool enabled = true) {
        if (!enabled) return false;
        if (click(bounds)) { id = control; return true; }
        return on(control) && confirm();
    }
};

struct Cursor {
    std::size_t offset = std::string_view::npos;
    std::size_t at(std::string_view value) const { return Utf8::boundary(value, offset); }
    void reset() { offset = std::string_view::npos; }
};

inline bool rowKey() { return confirm() || key(KEY_UP) || key(KEY_DOWN) || key(KEY_HOME) || key(KEY_END); }

inline std::optional<std::size_t> row(std::size_t count, std::optional<std::size_t> selected) {
    if (!count) return {};
    if (key(KEY_DOWN)) return selected ? std::min(*selected + 1, count - 1) : 0;
    if (key(KEY_UP)) return selected ? (*selected == 0 ? 0 : *selected - 1) : count - 1;
    if (key(KEY_HOME)) return 0;
    if (key(KEY_END)) return count - 1;
    if (confirm()) return selected.value_or(0);
    return {};
}

inline void revealRow(std::size_t row, std::size_t visible, std::size_t& offset) {
    if (row < offset) offset = row;
    else if (visible > 0 && row >= offset + visible) offset = row + 1 - visible;
}

inline float scroll(Rectangle viewport, float height, float offset, bool focused) {
    const float maximum = std::max(0.0f, height - viewport.height);
    if (CheckCollisionPointRec(GetMousePosition(), viewport)) offset -= GetMouseWheelMove() * 48.0f;
    if (focused) {
        const float page = std::max(24.0f, viewport.height - 24.0f);
        if (key(KEY_UP)) offset -= 24.0f;
        if (key(KEY_DOWN)) offset += 24.0f;
        if (key(KEY_PAGE_UP)) offset -= page;
        if (key(KEY_PAGE_DOWN)) offset += page;
        if (key(KEY_HOME)) offset = 0;
        if (key(KEY_END)) offset = maximum;
    }
    return std::clamp(offset, 0.0f, maximum);
}

struct Edit { bool changed = false; bool overLimit = false; };
inline bool printable(int cp) { return cp >= 32 && cp != 127; }
inline bool txt(int cp) { return printable(cp) && cp != '|'; }
inline bool username(int cp) {
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
           (cp >= '0' && cp <= '9') || cp == '_' || cp == '.' || cp == '-';
}

using PrepareClipboard = std::string_view (*)(std::string_view, bool, bool);

template<class Value, class Insert, class Erase, class Accept>
Edit edit(Cursor& cursor, std::size_t limit, Value value, Insert insert, Erase erase, Accept accept,
          PrepareClipboard prepareClipboard = nullptr) {
    Edit result;
    auto at = cursor.at(value());
    if (IsKeyPressed(KEY_HOME)) at = 0;
    if (IsKeyPressed(KEY_END)) at = value().size();
    if (key(KEY_LEFT)) at = Utf8::previous(value(), at);
    if (key(KEY_RIGHT) && at < value().size()) Utf8::next(value(), at);
    if (key(KEY_BACKSPACE) && at > 0) {
        const auto begin = Utf8::previous(value(), at);
        erase(begin, at - begin);
        at = begin;
        result.changed = true;
    } else if (key(KEY_DELETE) && at < value().size()) {
        auto end = at;
        Utf8::next(value(), end);
        erase(at, end - at);
        result.changed = true;
    }
    const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const auto append = [&](std::string_view source) {
        std::size_t bytes = 0;
        for (std::size_t i = 0; i < source.size();) {
            const auto unit = Utf8::next(source, i);
            if (unit.valid && accept(unit.codepoint)) bytes += unit.end - unit.begin;
            if (value().size() + bytes > limit) { result.overLimit = true; return; }
        }
        if (!bytes) return;
        if (bytes == source.size()) { insert(at, source); at += bytes; }
        else for (std::size_t i = 0; i < source.size();) {
            const auto unit = Utf8::next(source, i);
            if (!unit.valid || !accept(unit.codepoint)) continue;
            const auto count = unit.end - unit.begin;
            insert(at, source.substr(unit.begin, count));
            at += count;
        }
        result.changed = true;
    };
    if (control && IsKeyPressed(KEY_V)) {
        if (const char* clipboard = GetClipboardText())
            append(prepareClipboard ? prepareClipboard(clipboard, at == 0, at == value().size())
                                    : std::string_view(clipboard));
    }
    for (int cp = GetCharPressed(); cp > 0; cp = GetCharPressed()) {
        if (control || !accept(cp)) continue;
        auto encoded = Utf8::encode(cp);
        append(encoded);
        for (std::size_t i = 0; i < encoded.size(); ++i) static_cast<volatile char*>(encoded.data())[i] = 0;
    }
    cursor.offset = at;
    return result;
}

inline Edit text(std::string& value, Cursor& cursor, std::size_t limit, bool (*accept)(int) = txt,
                 PrepareClipboard prepareClipboard = nullptr) {
    return edit(cursor, limit, [&] { return std::string_view(value); },
                [&](std::size_t at, std::string_view bytes) { value.insert(at, bytes); },
                [&](std::size_t at, std::size_t count) { value.erase(at, count); }, accept, prepareClipboard);
}

inline Edit password(PasswordInput& value, Cursor& cursor) {
    auto result = edit(cursor, maxPasswordBytes, [&] { return std::string_view(value.value()); },
                       [&](std::size_t at, std::string_view bytes) { value.insert(at, bytes); },
                       [&](std::size_t at, std::size_t count) { value.erase(at, count); },
                       [](int cp) { return cp == '\t' || txt(cp); });
    if (result.overLimit) value.markTooLong();
    return result;
}
}

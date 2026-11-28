#include "UITheme.h"

#include "UiText.h"
#include "Utf8.h"

#include <algorithm>

namespace UITheme {
namespace {
constexpr int buttonSize = 18;
constexpr float buttonPadding = 8.0f;
constexpr int bannerSize = 16;
constexpr float bannerInsetX = 12.0f;
constexpr float bannerInsetY = 9.0f;
}
void drawText(const char* value, int x, int y, int size, Color color) {
    if (value) UiText::draw(value, x, y, size, color);
}

int measureText(const char* value, int size) {
    return UiText::measure(value, size);
}

void drawFocus(Rectangle bounds) {
    DrawRectangleRoundedLinesEx(bounds, 0.12f, 6, 2.0f, error);
}

float buttonWidth(const char* label) {
    return measureText(label, buttonSize) + buttonPadding * 2.0f;
}

void drawButton(Rectangle bounds, const char* label, bool isPrimary, bool enabled, bool leftAligned, bool focused) {
    const bool isHover = enabled && CheckCollisionPointRec(GetMousePosition(), bounds);
    const Color fill = !enabled ? Color{238, 225, 230, 255} :
                       isPrimary ? (isHover ? primaryHover : primary) : (isHover ? hover : card);
    DrawRectangleRounded(bounds, 0.22f, 8, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.22f, 8, 1.0f, border);
    const float available = bounds.width - buttonPadding * 2.0f;
    const auto labelText = wrap(label, available, buttonSize);
    const Color ink = isPrimary ? strongText : (enabled ? text : muted);
    if (available > 0 && bounds.height >= buttonSize) {
        if (labelText.height() - labelText.lineHeight() + buttonSize <= bounds.height - 6.0f) {
            float y = bounds.y + (bounds.height - labelText.height() + labelText.lineHeight() - buttonSize) * 0.5f;
            for (const auto& line : labelText.lines) {
                const float x = leftAligned ? bounds.x + buttonPadding
                    : bounds.x + (bounds.width - measureText(line.c_str(), buttonSize)) * 0.5f;
                drawText(line.c_str(), static_cast<int>(x), static_cast<int>(y), buttonSize, ink);
                y += labelText.lineHeight();
            }
        } else {
            const auto shortened = ellipsize(label, available, buttonSize);
            const float x = leftAligned ? bounds.x + buttonPadding
                : bounds.x + (bounds.width - measureText(shortened.c_str(), buttonSize)) * 0.5f;
            drawText(shortened.c_str(), static_cast<int>(x),
                     static_cast<int>(bounds.y + (bounds.height - buttonSize) * 0.5f), buttonSize, ink);
        }
    }
    if (enabled && focused) drawFocus(bounds);
}

FieldView fieldView(std::string_view value, std::size_t cursor, float width, int size, const Font* privateFont) {
    const auto caret = Utf8::boundary(value, cursor);
    FieldView view{caret, caret, 0.0f};
    float right = 0.0f;
    // Leave context on both sides when editing in the middle of a long field.
    while (view.end < value.size() && right < width * 0.25f) {
        auto pos = view.end;
        const auto advance = UiText::glyphAdvance(Utf8::next(value, pos).codepoint, size, privateFont);
        if (right + advance > width * 0.25f) break;
        view.end = pos;
        right += advance;
    }
    while (view.begin > 0) {
        const auto prev = Utf8::previous(value, view.begin);
        auto pos = prev;
        const auto advance = UiText::glyphAdvance(Utf8::next(value, pos).codepoint, size, privateFont);
        if (view.caretX + advance > width - right) break;
        view.begin = prev;
        view.caretX += advance;
    }
    float used = view.caretX + right;
    while (view.end < value.size() && used < width) {
        auto pos = view.end;
        used += UiText::glyphAdvance(Utf8::next(value, pos).codepoint, size, privateFont);
        // Keep the glyph whole in the string. The viewport clips its pixels at the edge.
        view.end = pos;
    }
    return view;
}

void drawField(Rectangle bounds, const std::string& label, std::string_view value, bool focused,
               std::size_t cursor, const char* placeholder, const Font* privateFont) {
    constexpr int valueSize = 18;
    drawText(label.c_str(), static_cast<int>(bounds.x), static_cast<int>(bounds.y - 22), 15, muted);
    DrawRectangleRounded(bounds, 0.12f, 6, Color{255, 252, 253, 255});
    DrawRectangleRoundedLinesEx(bounds, 0.12f, 6, 1.0f, border);
    if (focused) drawFocus(bounds);
    const float available = std::max(0.0f, bounds.width - 20.0f);
    const auto view = fieldView(value, cursor, std::max(0.0f, available - 2.0f), valueSize, privateFont);
    const int x = static_cast<int>(bounds.x + 10);
    const int y = static_cast<int>(bounds.y + (bounds.height - valueSize) * 0.5f);
    BeginScissorMode(x, static_cast<int>(bounds.y + 4), static_cast<int>(available), static_cast<int>(bounds.height - 8));
    UiText::draw(value.empty() ? std::string_view(placeholder) : value.substr(view.begin, view.end - view.begin),
                 x, y, valueSize, value.empty() ? muted : text, privateFont);
    if (focused) DrawRectangle(x + static_cast<int>(view.caretX), y, 1, valueSize, text);
    EndScissorMode();
}

int WrappedText::lineHeight() const { return fontSize + std::max(4, fontSize / 3); }
int WrappedText::height() const { return static_cast<int>(lines.size()) * lineHeight(); }

void WrappedText::draw(int x, int y, Color color, int maxLines) const {
    for (std::size_t i = 0; i < lines.size() && (maxLines <= 0 || i < static_cast<std::size_t>(maxLines)); ++i)
        drawText(lines[i].c_str(), x, y + static_cast<int>(i) * lineHeight(), fontSize, color);
}

WrappedText wrap(std::string_view value, float width, int fontSize) {
    WrappedText result{fontSize, {}};
    if (width <= 0 || value.empty()) return result;
    UiText::prepare(value);
    const auto units = Utf8::decode(value);
    const auto space = [](int cp) { return cp == ' ' || cp == '\t' || cp == '\r'; };
    for (std::size_t start = 0; start < units.size();) {
        while (start < units.size() && space(units[start].codepoint)) ++start;
        if (start == units.size()) break;
        std::size_t end = start;
        std::size_t next = start;
        float used = 0;
        for (; end < units.size(); ++end) {
            const int cp = units[end].codepoint;
            if (cp == '\n') break;
            if (space(cp)) next = end + 1;
            used += UiText::glyphAdvance(cp, fontSize);
            if (used > width && end > start) break;
        }
        std::size_t lineEnd = end;
        if (end == units.size()) next = end;
        else if (units[end].codepoint == '\n') next = end + 1;
        else if (next > start) lineEnd = next - 1;
        else next = end;
        while (lineEnd > start && space(units[lineEnd - 1].codepoint)) --lineEnd;
        const auto beginByte = units[start].begin;
        const auto endByte = lineEnd > start ? units[lineEnd - 1].end : beginByte;
        result.lines.emplace_back(value.substr(beginByte, endByte - beginByte));
        start = next;
    }
    return result;
}

std::string ellipsize(std::string_view value, float width, int fontSize) {
    if (value.empty() || width <= 0) return {};
    UiText::prepare(value);
    constexpr const char* ellipsis = "\xE2\x80\xA6";
    const float ellipsisWidth = measureText(ellipsis, fontSize);
    float used = 0;
    std::size_t end = 0;
    for (std::size_t next = 0; next < value.size();) {
        const int cp = Utf8::next(value, next).codepoint;
        if (cp == '\n') break;
        used += UiText::glyphAdvance(cp, fontSize);
        if (used > width) break;
        if (used + ellipsisWidth <= width) end = next;
        if (next == value.size()) return std::string(value);
    }
    return ellipsisWidth <= width ? std::string(value.substr(0, end)) + ellipsis : std::string{};
}

void drawWrapped(const std::string& value, int x, int y, int width, int fontSize,
                 Color color, int maxLines) {
    wrap(value, width, fontSize).draw(x, y, color, maxLines);
}

float bannerHeight(float width, const std::string& message) {
    return message.empty() ? 0.0f : wrap(message, width - bannerInsetX * 2, bannerSize).height() + bannerInsetY * 2;
}

void drawStatusBanner(Rectangle bounds, const std::string& message, bool isError, float offset) {
    if (message.empty() || bounds.height < bannerInsetY * 2) return;
    const Color fill = isError ? Color{255, 237, 241, 255} : Color{237, 248, 240, 255};
    const Color outline = isError ? error : Color{92, 145, 107, 255};
    const Color foreground = isError ? error : Color{49, 106, 65, 255};
    DrawRectangleRounded(bounds, 0.18f, 6, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.18f, 6, 1.0f, outline);
    const auto lines = wrap(message, bounds.width - bannerInsetX * 2, bannerSize);
    const Rectangle viewport{bounds.x + 4, bounds.y + bannerInsetY, bounds.width - 8, bounds.height - bannerInsetY * 2};
    offset = std::clamp(offset, 0.0f, std::max(0.0f, lines.height() - viewport.height));
    beginClip(viewport);
    lines.draw(static_cast<int>(bounds.x + bannerInsetX), static_cast<int>(viewport.y - offset), foreground);
    EndScissorMode();
    drawScrollbar(viewport, lines.height(), offset);
}

void beginClip(Rectangle viewport) {
    BeginScissorMode(static_cast<int>(viewport.x), static_cast<int>(viewport.y),
                     static_cast<int>(viewport.width), static_cast<int>(viewport.height));
}

void drawScrollbar(Rectangle viewport, float contentHeight, float offset) {
    if (contentHeight <= viewport.height) return;
    const Rectangle track{viewport.x + viewport.width - 4.0f, viewport.y, 3.0f, viewport.height};
    const float thumb = std::min(track.height, std::max(20.0f, track.height * viewport.height / contentHeight));
    DrawRectangleRounded(track, 0.5f, 3, Color{239,224,230,255});
    DrawRectangleRounded({track.x, track.y + (track.height - thumb) * offset / (contentHeight - viewport.height),
                          track.width, thumb}, 0.5f, 3, border);
}
}

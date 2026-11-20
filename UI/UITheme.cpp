#include "UITheme.h"

#include "UiText.h"
#include "Utf8.h"

#include <algorithm>
#include <sstream>

namespace UITheme {
void drawText(const char* value, int x, int y, int size, Color color) {
    UiText::draw(value, x, y, size, color);
}

int measureText(const char* value, int size) {
    return UiText::measure(value, size);
}

void drawButton(Rectangle bounds, const char* label, bool isPrimary, bool enabled, bool leftAligned) {
    const bool isHover = enabled && CheckCollisionPointRec(GetMousePosition(), bounds);
    const Color fill = !enabled ? Color{238, 225, 230, 255} :
                       isPrimary ? (isHover ? primaryHover : primary) : (isHover ? hover : card);
    DrawRectangleRounded(bounds, 0.22f, 8, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.22f, 8, 1.0f, border);
    constexpr int labelSize = 18;
    const int textWidth = measureText(label, labelSize);
    const int textX = leftAligned ? static_cast<int>(bounds.x + 12) :
                      static_cast<int>(bounds.x + std::max(8.0f, (bounds.width - textWidth) * 0.5f));
    const int textY = static_cast<int>(bounds.y + (bounds.height - labelSize) * 0.5f);
    drawText(label, textX, textY, labelSize, isPrimary ? strongText : (enabled ? text : muted));
}

void drawField(Rectangle bounds, const std::string& label, const std::string& value,
               bool focused, bool password) {
    constexpr int valueSize = 18;
    drawText(label.c_str(), static_cast<int>(bounds.x), static_cast<int>(bounds.y - 22), 15, muted);
    DrawRectangleRounded(bounds, 0.12f, 6, Color{255, 252, 253, 255});
    DrawRectangleRoundedLinesEx(bounds, 0.12f, 6, focused ? 2.0f : 1.0f, focused ? primary : border);
    std::string visible = value;
    if (password) visible.assign(Utf8::count(value), '*');
    const float available = std::max(0.0f, bounds.width - 20.0f);
    auto units = Utf8::decode(visible);
    std::size_t first = 0;
    while (first < units.size()) {
        const std::string candidate = visible.substr(units[first].begin);
        if (measureText(candidate.c_str(), valueSize) <= available) break;
        ++first;
    }
    if (first < units.size()) visible.erase(0, units[first].begin);
    BeginScissorMode(static_cast<int>(bounds.x + 8), static_cast<int>(bounds.y + 4),
                     static_cast<int>(available), static_cast<int>(bounds.height - 8));
    drawText(visible.c_str(), static_cast<int>(bounds.x + 10), static_cast<int>(bounds.y + (bounds.height - valueSize) * 0.5f), valueSize, text);
    EndScissorMode();
}

void drawWrapped(const std::string& value, int x, int y, int width, int fontSize,
                 Color color, int maxLines) {
    if (width <= 0 || value.empty()) return;
    const int lineHeight = fontSize + std::max(4, fontSize / 3);
    int lines = 0;
    std::string line;
    std::istringstream words(value);
    std::string token;
    auto emit = [&]() {
        if (line.empty() || (maxLines > 0 && lines >= maxLines)) return;
        drawText(line.c_str(), x, y + lines * lineHeight, fontSize, color);
        ++lines;
        line.clear();
    };
    while (words >> token) {
        const std::string candidate = line.empty() ? token : line + " " + token;
        if (measureText(candidate.c_str(), fontSize) <= width) { line = candidate; continue; }
        if (!line.empty()) emit();
        if (measureText(token.c_str(), fontSize) <= width) { line = token; continue; }
        const auto units = Utf8::decode(token);
        for (const auto& unit : units) {
            const std::string character = token.substr(unit.begin, unit.end - unit.begin);
            if (!line.empty() && measureText((line + character).c_str(), fontSize) > width) emit();
            if (maxLines > 0 && lines >= maxLines) return;
            line += character;
        }
    }
    emit();
}

void drawStatusBanner(Rectangle bounds, const std::string& message, bool isError) {
    if (message.empty() || bounds.width <= 0.0f || bounds.height <= 0.0f) return;
    const Color fill = isError ? Color{255, 237, 241, 255} : Color{237, 248, 240, 255};
    const Color outline = isError ? error : Color{92, 145, 107, 255};
    const Color foreground = isError ? error : Color{49, 106, 65, 255};
    DrawRectangleRounded(bounds, 0.18f, 6, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.18f, 6, 1.0f, outline);
    drawWrapped(message, static_cast<int>(bounds.x + 12.0f), static_cast<int>(bounds.y + 9.0f),
                static_cast<int>(bounds.width - 24.0f), 16, foreground, 2);
}
}

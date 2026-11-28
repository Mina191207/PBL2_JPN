#pragma once

#include "raylib.h"

#include <string>
#include <string_view>
#include <vector>

namespace UITheme {
inline constexpr Color background = {255, 248, 250, 255};
inline constexpr Color sidebar = {255, 241, 246, 255};
inline constexpr Color card = {255, 255, 255, 255};
inline constexpr Color primary = {245, 139, 170, 255};
inline constexpr Color primaryHover = {231, 109, 152, 255};
inline constexpr Color border = {183, 122, 142, 255};
inline constexpr Color text = {64, 52, 58, 255};
inline constexpr Color strongText = {50, 38, 44, 255};
inline constexpr Color muted = {74, 74, 74, 255};
inline constexpr Color error = {155, 59, 86, 255};
inline constexpr Color hover = {248, 216, 226, 255};

void drawText(const char* value, int x, int y, int size, Color color);
int measureText(const char* value, int size);
void drawButton(Rectangle bounds, const char* label, bool primary = false,
                bool enabled = true, bool leftAligned = false, bool focused = false);
void drawFocus(Rectangle bounds);
struct FieldView { std::size_t begin, end; float caretX; };
FieldView fieldView(std::string_view value, std::size_t cursor, float width, int size,
                    const Font* privateFont = nullptr);
void drawField(Rectangle bounds, const std::string& label, std::string_view value, bool focused = false,
               std::size_t cursor = std::string_view::npos, const char* placeholder = "",
               const Font* privateFont = nullptr);
void drawWrapped(const std::string& value, int x, int y, int width, int fontSize,
                 Color color, int maxLines = 0);
struct WrappedText {
    int fontSize;
    std::vector<std::string> lines;
    int lineHeight() const;
    int height() const;
    void draw(int x, int y, Color color, int maxLines = 0) const;
};
WrappedText wrap(std::string_view value, float width, int fontSize);
std::string ellipsize(std::string_view value, float width, int fontSize);
float buttonWidth(const char* label);
float bannerHeight(float width, const std::string& message);
void drawStatusBanner(Rectangle bounds, const std::string& message, bool isError, float offset = 0.0f);
void beginClip(Rectangle viewport);
void drawScrollbar(Rectangle viewport, float contentHeight, float offset);
}

#pragma once

#include "raylib.h"

#include <string>

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
                bool enabled = true, bool leftAligned = false);
void drawField(Rectangle bounds, const std::string& label, const std::string& value,
               bool focused = false, bool password = false);
void drawWrapped(const std::string& value, int x, int y, int width, int fontSize,
                 Color color, int maxLines = 0);
void drawStatusBanner(Rectangle bounds, const std::string& message, bool isError);
}

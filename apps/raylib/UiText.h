#pragma once

#include "raylib.h"

#include <string>
#include <string_view>
#include <vector>

namespace UiText {
bool initialize();
void shutdown();
void prepare(std::string_view text);
void draw(std::string_view text, int x, int y, int fontSize, Color tint, const Font* privateFont = nullptr);
float glyphAdvance(int codepoint, int fontSize, const Font* privateFont = nullptr);
int measure(const char* text, int fontSize);
bool hasGlyph(int codepoint);
const std::string& error();

class PrivateFont {
    Font font_{};
    std::vector<int> required_;

  public:
    PrivateFont() = default;
    PrivateFont(const PrivateFont&) = delete;
    PrivateFont& operator=(const PrivateFont&) = delete;
    ~PrivateFont() { clear(); }
    void clear() noexcept;
    const Font* prepare(std::string_view text);
};
}

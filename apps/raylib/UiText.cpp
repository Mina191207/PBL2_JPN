#include "UiText.h"
#include "Utf8.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
constexpr int atlasSize = 48;
constexpr std::size_t pageSize = 512;
constexpr float spacing = 1.0f;
constexpr int traceLevel = LOG_WARNING;

struct Glyph { const Font* font; int index; int codepoint; };
struct Location { std::size_t page; int index; };
std::vector<Font> pages;
std::unordered_map<int, Location> locations;
std::unordered_set<int> unavailable;
unsigned char* fontData = nullptr;
int fontBytes = 0;
std::string errorMessage;

Font load(std::vector<int>& codes) {
    if (!fontData || codes.empty()) return {};
    Font font{};
    font.baseSize = atlasSize;
    font.glyphCount = static_cast<int>(codes.size());
    font.glyphPadding = 1;
    font.glyphs = LoadFontData(fontData, fontBytes, atlasSize, codes.data(), font.glyphCount, FONT_DEFAULT);
    if (!font.glyphs) return {};
    // Size each cell from the rasterized glyphs. Raylib 5.5's default atlas can
    // place tall glyphs past the end of a small page.
    int cellWidth = 2, cellHeight = 2;
    for (int i = 0; i < font.glyphCount; ++i) {
        cellWidth = std::max(cellWidth, font.glyphs[i].image.width + 2);
        cellHeight = std::max(cellHeight, font.glyphs[i].image.height + 2);
    }
    const int columns = static_cast<int>(std::ceil(std::sqrt(font.glyphCount)));
    const int width = columns * cellWidth;
    const int height = ((font.glyphCount + columns - 1) / columns) * cellHeight;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 2);
    font.recs = static_cast<Rectangle*>(MemAlloc(static_cast<unsigned>(font.glyphCount * sizeof(Rectangle))));
    if (!font.recs) { UnloadFontData(font.glyphs, font.glyphCount); return {}; }
    for (int i = 0; i < font.glyphCount; ++i) {
        const auto& bitmap = font.glyphs[i].image;
        const int x = (i % columns) * cellWidth + 1;
        const int y = (i / columns) * cellHeight + 1;
        font.recs[i] = {static_cast<float>(x), static_cast<float>(y),
                        static_cast<float>(bitmap.width), static_cast<float>(bitmap.height)};
        const auto* source = static_cast<const unsigned char*>(bitmap.data);
        for (int row = 0; row < bitmap.height; ++row) {
            for (int col = 0; col < bitmap.width; ++col) {
                const auto pixel = 2 * (static_cast<std::size_t>(y + row) * width + x + col);
                pixels[pixel] = 255;
                pixels[pixel + 1] = source[row * bitmap.width + col];
            }
        }
    }
    font.texture = LoadTextureFromImage({pixels.data(), width, height, 1, PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA});
    if (font.texture.id != 0) return font;
    UnloadFont(font);
    return {};
}

int exactIndex(const Font& font, int cp) {
    if (!font.glyphs || !font.recs) return -1;
    const int index = GetGlyphIndex(font, cp);
    return index >= 0 && index < font.glyphCount && font.glyphs[index].value == cp &&
           (font.recs[index].width > 0.0f || cp == ' ') ? index : -1;
}

void addPage(std::vector<int> codes) {
    Font font = load(codes);
    if (font.texture.id != 0) {
        const auto index = pages.size();
        pages.push_back(font);
        for (int i = 0; i < font.glyphCount; ++i) {
            const int cp = font.glyphs[i].value;
            if (font.recs[i].width > 0.0f || cp == ' ') locations.emplace(cp, Location{index, i});
        }
    }
    for (int cp : codes)
        if (!locations.count(cp)) unavailable.insert(cp);
}

Glyph resolve(int cp, const Font* privateFont, const Font& fallback) {
    if (cp == '\t') cp = ' ';
    const auto found = locations.find(cp);
    if (found != locations.end()) return {&pages[found->second.page], found->second.index, cp};
    if (privateFont) {
        if (const int index = exactIndex(*privateFont, cp); index >= 0) return {privateFont, index, cp};
    }
    const auto replacement = locations.find('?');
    if (replacement != locations.end())
        return {&pages[replacement->second.page], replacement->second.index, '?'};
    const int index = exactIndex(fallback, cp);
    if (index >= 0) return {&fallback, index, cp};
    return {&fallback, exactIndex(fallback, '?'), '?'};
}

float advance(const Glyph& glyph, int size) {
    if (glyph.index < 0 || glyph.font->baseSize <= 0) return static_cast<float>(size) * 0.5f;
    const auto& info = glyph.font->glyphs[glyph.index];
    const float width = info.advanceX == 0 ? glyph.font->recs[glyph.index].width : static_cast<float>(info.advanceX);
    return std::max(1.0f, width * size / glyph.font->baseSize + spacing);
}
}

namespace UiText {
bool initialize() {
    if (!pages.empty()) return true;
    SetTraceLogLevel(traceLevel);
    errorMessage.clear();
    const auto path = std::filesystem::path(GetApplicationDirectory()) / "assets" / "fonts" / "NotoSansJP-Medium.ttf";
    fontData = LoadFileData(path.string().c_str(), &fontBytes);
    if (!fontData) {
        errorMessage = "Bundled Noto Sans JP font not found beside the application.";
        return false;
    }
    std::vector<int> codes;
    for (int cp = 32; cp <= 126; ++cp) codes.push_back(cp);
    codes.insert(codes.end(), {0x1EBF, 0x3042, 0x30A2, 0x65E5});
    addPage(std::move(codes));
    for (int cp : {static_cast<int>('A'), 0x1EBF, 0x3042, 0x30A2, 0x65E5}) {
        if (hasGlyph(cp)) continue;
        errorMessage = "Bundled font is missing a required Latin, Vietnamese, kana, or Kanji glyph.";
        shutdown();
        return false;
    }
    return true;
}

void shutdown() {
    for (const auto& font : pages) UnloadFont(font);
    pages.clear();
    locations.clear();
    unavailable.clear();
    if (fontData) UnloadFileData(fontData);
    fontData = nullptr;
    fontBytes = 0;
}

bool hasGlyph(int codepoint) { return locations.count(codepoint) != 0; }

void prepare(std::string_view text) {
    if (!fontData) return;
    std::vector<int> missing;
    std::unordered_set<int> seen;
    for (std::size_t pos = 0; pos < text.size();) {
        const int cp = Utf8::next(text, pos).codepoint;
        if (cp < 32 || hasGlyph(cp) || unavailable.count(cp) || !seen.insert(cp).second) continue;
        missing.push_back(cp);
        if (missing.size() == pageSize) { addPage(std::move(missing)); missing.clear(); }
    }
    if (!missing.empty()) addPage(std::move(missing));
}

void draw(std::string_view text, int x, int y, int fontSize, Color tint, const Font* privateFont) {
    if (!privateFont) prepare(text);
    const auto fallback = GetFontDefault();
    float pen = static_cast<float>(x);
    for (std::size_t pos = 0; pos < text.size();) {
        const int cp = Utf8::next(text, pos).codepoint;
        if (cp == '\n') { pen = static_cast<float>(x); y += fontSize + 8; continue; }
        const auto glyph = resolve(cp, privateFont, fallback);
        if (glyph.index >= 0)
            DrawTextCodepoint(*glyph.font, glyph.codepoint, {pen, static_cast<float>(y)}, static_cast<float>(fontSize), tint);
        pen += advance(glyph, fontSize);
    }
}

float glyphAdvance(int cp, int size, const Font* privateFont) {
    if (!privateFont && !hasGlyph(cp) && !unavailable.count(cp) && cp >= 32) prepare(Utf8::encode(cp));
    const auto fallback = GetFontDefault();
    return advance(resolve(cp, privateFont, fallback), size);
}

int measure(const char* value, int fontSize) {
    if (!value || !*value) return 0;
    std::string_view text(value);
    prepare(text);
    const auto fallback = GetFontDefault();
    float width = 0, longest = 0;
    for (std::size_t pos = 0; pos < text.size();) {
        const int cp = Utf8::next(text, pos).codepoint;
        if (cp == '\n') { longest = std::max(width, longest); width = 0; }
        else width += advance(resolve(cp, nullptr, fallback), fontSize);
    }
    return static_cast<int>(std::ceil(std::max(longest, width)));
}

const Font* PrivateFont::prepare(std::string_view text) {
    std::vector<int> missing;
    for (std::size_t pos = 0; pos < text.size();) {
        const int cp = Utf8::next(text, pos).codepoint;
        if (cp < 32 || hasGlyph(cp)) continue;
        missing.push_back(cp);
    }
    std::sort(missing.begin(), missing.end());
    missing.erase(std::unique(missing.begin(), missing.end()), missing.end());
    if (missing != required_) {
        clear();
        // Raylib can include a requested codepoint in atlas diagnostics.
        SetTraceLogLevel(LOG_NONE);
        font_ = load(missing);
        SetTraceLogLevel(traceLevel);
        required_ = std::move(missing);
    }
    return &font_;
}

void PrivateFont::clear() noexcept {
    if (font_.texture.id != 0) UnloadFont(font_);
    font_ = {};
    required_.clear();
}

const std::string& error() { return errorMessage; }
}

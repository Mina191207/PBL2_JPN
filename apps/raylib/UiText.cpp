#include "UiText.h"
#include "Utf8.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
constexpr int kAtlasSize = 48;
constexpr std::size_t kCodepointsPerPage = 512;
constexpr double kIdleBeforeAtlasUpdate = 0.18;
constexpr float kSpacing = 1.0f;

std::vector<Font> pages;
std::unordered_map<int, std::size_t> glyphPages;
std::unordered_set<int> pendingSet;
std::vector<int> pending;
bool ready = false;
double lastPendingTime = 0.0;
std::string fontPath;
std::string errorMessage;

void addPage(const std::vector<int>& requested) {
    if (requested.empty() || fontPath.empty()) return;
    std::vector<int> codes;
    codes.reserve(requested.size());
    for (const int cp : requested)
        if (glyphPages.find(cp) == glyphPages.end() &&
            std::find(codes.begin(), codes.end(), cp) == codes.end()) codes.push_back(cp);
    if (codes.empty()) return;

    Font font = LoadFontEx(fontPath.c_str(), kAtlasSize, codes.data(), static_cast<int>(codes.size()));
    if (font.texture.id == 0 || !font.glyphs || !font.recs) {
        errorMessage = "Noto Sans JP could not build a font atlas.";
        if (font.texture.id != 0) UnloadFont(font);
        return;
    }
    const std::size_t pageIndex = pages.size();
    for (int i = 0; i < font.glyphCount; ++i) {
        const int cp = font.glyphs[i].value;
        if (std::find(codes.begin(), codes.end(), cp) != codes.end())
            glyphPages.emplace(cp, pageIndex);
    }
    pages.push_back(font);
}

void loadCodepoints(const std::vector<int>& codepoints) {
    for (std::size_t start = 0; start < codepoints.size(); start += kCodepointsPerPage) {
        const std::size_t finish = std::min(codepoints.size(), start + kCodepointsPerPage);
        addPage(std::vector<int>(codepoints.begin() + static_cast<std::ptrdiff_t>(start),
                                 codepoints.begin() + static_cast<std::ptrdiff_t>(finish)));
    }
}

int fallbackPage() {
    const auto it = glyphPages.find('?');
    return it == glyphPages.end() ? 0 : static_cast<int>(it->second);
}

void queueMissing(int cp) {
    if (glyphPages.find(cp) != glyphPages.end() || !pendingSet.insert(cp).second) return;
    pending.push_back(cp);
    lastPendingTime = GetTime();
}

std::string resolveFontPath() {
    const std::filesystem::path besideExe = std::filesystem::path(GetApplicationDirectory()) /
                                              "assets" / "fonts" / "NotoSansJP-Medium.ttf";
    if (FileExists(besideExe.string().c_str())) return besideExe.string();
    return {};
}

bool containsGlyph(const Font& font, int cp) {
    if (!font.glyphs || !font.recs) return false;
    for (int i = 0; i < font.glyphCount; ++i)
        if (font.glyphs[i].value == cp && (font.recs[i].width > 0.0f || cp == ' ')) return true;
    return false;
}
}

namespace UiText {
bool initialize() {
    if (ready) return true;
    errorMessage.clear();
    fontPath = resolveFontPath();
    if (fontPath.empty()) {
        errorMessage = "Bundled Noto Sans JP font not found beside the application.";
        return false;
    }
    std::vector<int> basic;
    for (int cp = 32; cp <= 126; ++cp) basic.push_back(cp);
    basic.insert(basic.end(), {0x00E9, 0x1EBF, 0x3042, 0x30A2, 0x65E5});
    loadCodepoints(basic);
    if (pages.empty() || pages.front().texture.id == 0) {
        if (errorMessage.empty()) errorMessage = "Could not load the bundled Noto Sans JP font.";
        return false;
    }
    const Font& font = pages.front();
    for (const int cp : {static_cast<int>('A'), 0x1EBF, 0x3042, 0x30A2, 0x65E5}) {
        if (!containsGlyph(font, cp)) {
            errorMessage = "Bundled font is missing a required Latin, Vietnamese, kana, or Kanji glyph.";
            shutdown();
            return false;
        }
    }
    ready = true;
    return true;
}

void shutdown() {
    for (auto& font : pages) if (font.texture.id != 0) UnloadFont(font);
    pages.clear(); glyphPages.clear(); pending.clear(); pendingSet.clear(); ready = false;
}

void beginFrame() {}

void endFrame() {
    if (!pending.empty() && GetTime() - lastPendingTime >= kIdleBeforeAtlasUpdate) {
        const auto codes = std::move(pending);
        pending.clear(); pendingSet.clear();
        loadCodepoints(codes);
    }
}

void draw(const char* input, int x, int y, int fontSize, Color tint) {
    if (!input || *input == '\0') return;
    if (pages.empty()) { DrawText(input, x, y, fontSize, tint); return; }
    const std::string_view text(input);
    const auto cps = Utf8::decode(text);
    float penX = static_cast<float>(x);
    float penY = static_cast<float>(y);
    std::size_t runStart = 0;
    int runPage = -1;
    auto flush = [&](std::size_t end) {
        if (runPage < 0 || end <= runStart) return;
        const std::string run(text.substr(runStart, end - runStart));
        const Font& font = pages[static_cast<std::size_t>(runPage)];
        DrawTextEx(font, run.c_str(), {penX, penY}, static_cast<float>(fontSize), kSpacing, tint);
        penX += MeasureTextEx(font, run.c_str(), static_cast<float>(fontSize), kSpacing).x;
    };
    for (const auto& cp : cps) {
        auto it = glyphPages.find(cp.codepoint);
        int page = it == glyphPages.end() ? fallbackPage() : static_cast<int>(it->second);
        if (it == glyphPages.end()) queueMissing(cp.codepoint);
        if (cp.codepoint == '\n') {
            flush(cp.begin);
            runStart = cp.end; runPage = -1; penX = static_cast<float>(x);
            penY += static_cast<float>(fontSize + 8);
            continue;
        }
        if (runPage != page) { flush(cp.begin); runStart = cp.begin; runPage = page; }
    }
    flush(text.size());
}

int measure(const char* input, int fontSize) {
    if (!input || *input == '\0') return 0;
    if (pages.empty()) return MeasureText(input, fontSize);
    const std::string_view text(input);
    const auto cps = Utf8::decode(text);
    float width = 0.0f;
    float maxWidth = 0.0f;
    std::size_t start = 0;
    int current = -1;
    auto flush = [&](std::size_t end) {
        if (current < 0 || end <= start) return;
        const std::string run(text.substr(start, end - start));
        width += MeasureTextEx(pages[static_cast<std::size_t>(current)], run.c_str(), static_cast<float>(fontSize), kSpacing).x;
    };
    for (const auto& cp : cps) {
        const auto it = glyphPages.find(cp.codepoint);
        const int page = it == glyphPages.end() ? fallbackPage() : static_cast<int>(it->second);
        if (it == glyphPages.end()) queueMissing(cp.codepoint);
        if (cp.codepoint == '\n') { flush(cp.begin); maxWidth = std::max(maxWidth, width); width = 0.0f; start = cp.end; current = -1; continue; }
        if (page != current) { flush(cp.begin); start = cp.begin; current = page; }
    }
    flush(text.size());
    return static_cast<int>(std::ceil(std::max(maxWidth, width)));
}

const std::string& error() { return errorMessage; }
}

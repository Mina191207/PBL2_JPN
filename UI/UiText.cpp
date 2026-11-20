#include "UiText.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr int kAtlasSize = 48;
constexpr std::size_t kCodepointsPerPage = 512;
constexpr double kIdleBeforeAtlasUpdate = 0.18;
constexpr float kSpacing = 1.0f;

struct FontPage {
    Font font{};
    std::vector<int> codepoints;
};
struct DecodedCodepoint {
    int value;
    std::size_t begin;
    std::size_t end;
};

std::vector<FontPage> pages;
std::unordered_map<int, std::size_t> glyphPages;
std::unordered_set<int> pendingSet;
std::vector<int> pending;
bool ready = false;
double lastPendingTime = 0.0;
std::string fontPath;
std::string errorMessage;

std::vector<DecodedCodepoint> decode(std::string_view text) {
    std::vector<DecodedCodepoint> result;
    for (std::size_t i = 0; i < text.size();) {
        const std::size_t begin = i;
        const unsigned char first = static_cast<unsigned char>(text[i++]);
        int value = 0xFFFD;
        std::size_t extra = 0;
        if (first < 0x80) value = first;
        else if ((first & 0xE0) == 0xC0) { value = first & 0x1F; extra = 1; }
        else if ((first & 0xF0) == 0xE0) { value = first & 0x0F; extra = 2; }
        else if ((first & 0xF8) == 0xF0) { value = first & 0x07; extra = 3; }
        bool valid = extra == 0 || begin + extra < text.size();
        if (valid) {
            for (std::size_t j = 0; j < extra; ++j) {
                const unsigned char next = static_cast<unsigned char>(text[i + j]);
                if ((next & 0xC0) != 0x80) { valid = false; break; }
                value = (value << 6) | (next & 0x3F);
            }
        }
        if (valid && extra > 0) {
            i += extra;
            const bool overlong = (extra == 1 && value < 0x80) ||
                                  (extra == 2 && value < 0x800) ||
                                  (extra == 3 && value < 0x10000);
            if (overlong || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
                value = 0xFFFD;
                valid = false;
            }
        }
        if (!valid) value = 0xFFFD;
        result.push_back({value, begin, i});
    }
    return result;
}

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
    FontPage page{font, codes};
    for (int i = 0; i < font.glyphCount; ++i) {
        const int cp = font.glyphs[i].value;
        if (std::find(codes.begin(), codes.end(), cp) != codes.end())
            glyphPages.emplace(cp, pageIndex);
    }
    pages.push_back(std::move(page));
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

std::vector<int> uniqueCodepoints(const std::vector<std::string>& strings) {
    std::unordered_set<int> unique;
    for (const auto& text : strings)
        for (const auto& cp : decode(text))
            if (cp.value >= 32 && cp.value != 127) unique.insert(cp.value);
    return {unique.begin(), unique.end()};
}

std::string resolveFontPath() {
    const std::filesystem::path besideExe = std::filesystem::path(GetApplicationDirectory()) /
                                              "assets" / "fonts" / "NotoSansJP-Medium.ttf";
    if (FileExists(besideExe.string().c_str())) return besideExe.string();
    const std::filesystem::path working = std::filesystem::current_path() / "assets" /
                                          "fonts" / "NotoSansJP-Medium.ttf";
    if (FileExists(working.string().c_str())) return working.string();
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
    if (pages.empty() || pages.front().font.texture.id == 0) {
        if (errorMessage.empty()) errorMessage = "Could not load the bundled Noto Sans JP font.";
        return false;
    }
    const Font& font = pages.front().font;
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
    for (auto& page : pages) if (page.font.texture.id != 0) UnloadFont(page.font);
    pages.clear(); glyphPages.clear(); pending.clear(); pendingSet.clear(); ready = false;
}

void suspend() {
    for (auto& page : pages) if (page.font.texture.id != 0) UnloadFont(page.font);
    for (auto& page : pages) page.font = {};
    glyphPages.clear(); ready = false;
}

bool resume() {
    if (ready) return true;
    std::vector<std::vector<int>> saved;
    saved.reserve(pages.size());
    for (const auto& page : pages) saved.push_back(page.codepoints);
    pages.clear(); glyphPages.clear();
    fontPath = resolveFontPath();
    if (fontPath.empty()) { errorMessage = "Bundled Noto Sans JP font not found."; return false; }
    for (const auto& codes : saved) addPage(codes);
    ready = !pages.empty() && pages.front().font.texture.id != 0;
    return ready;
}

void beginFrame() {}

void endFrame() {
    if (!pending.empty() && GetTime() - lastPendingTime >= kIdleBeforeAtlasUpdate) {
        const auto codes = std::move(pending);
        pending.clear(); pendingSet.clear();
        loadCodepoints(codes);
    }
}

void preload(const std::vector<std::string>& strings) {
    if (!ready) return;
    loadCodepoints(uniqueCodepoints(strings));
}

void draw(const char* input, int x, int y, int fontSize, Color tint) {
    if (!input || *input == '\0') return;
    if (pages.empty()) { DrawText(input, x, y, fontSize, tint); return; }
    const std::string_view text(input);
    const auto cps = decode(text);
    float penX = static_cast<float>(x);
    float penY = static_cast<float>(y);
    std::size_t runStart = 0;
    int runPage = -1;
    auto flush = [&](std::size_t end) {
        if (runPage < 0 || end <= runStart) return;
        const std::string run(text.substr(runStart, end - runStart));
        const Font& font = pages[static_cast<std::size_t>(runPage)].font;
        DrawTextEx(font, run.c_str(), {penX, penY}, static_cast<float>(fontSize), kSpacing, tint);
        penX += MeasureTextEx(font, run.c_str(), static_cast<float>(fontSize), kSpacing).x;
    };
    for (const auto& cp : cps) {
        auto it = glyphPages.find(cp.value);
        int page = it == glyphPages.end() ? fallbackPage() : static_cast<int>(it->second);
        if (it == glyphPages.end()) queueMissing(cp.value);
        if (cp.value == '\n') {
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
    const auto cps = decode(text);
    float width = 0.0f;
    float maxWidth = 0.0f;
    std::size_t start = 0;
    int current = -1;
    auto flush = [&](std::size_t end) {
        if (current < 0 || end <= start) return;
        const std::string run(text.substr(start, end - start));
        width += MeasureTextEx(pages[static_cast<std::size_t>(current)].font, run.c_str(), static_cast<float>(fontSize), kSpacing).x;
    };
    for (const auto& cp : cps) {
        const auto it = glyphPages.find(cp.value);
        const int page = it == glyphPages.end() ? fallbackPage() : static_cast<int>(it->second);
        if (it == glyphPages.end()) queueMissing(cp.value);
        if (cp.value == '\n') { flush(cp.begin); maxWidth = std::max(maxWidth, width); width = 0.0f; start = cp.end; current = -1; continue; }
        if (page != current) { flush(cp.begin); start = cp.begin; current = page; }
    }
    flush(text.size());
    return static_cast<int>(std::ceil(std::max(maxWidth, width)));
}

const std::string& error() { return errorMessage; }
}

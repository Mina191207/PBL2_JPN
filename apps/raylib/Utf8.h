#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Utf8 {
struct Unit { int codepoint; std::size_t begin; std::size_t end; };

inline std::vector<Unit> decode(std::string_view text) {
    std::vector<Unit> units;
    for (std::size_t i = 0; i < text.size();) {
        const std::size_t begin = i;
        const unsigned char lead = static_cast<unsigned char>(text[i++]);
        int cp = 0xFFFD;
        std::size_t tail = 0;
        if (lead < 0x80) cp = lead;
        else if ((lead & 0xE0) == 0xC0) { cp = lead & 0x1F; tail = 1; }
        else if ((lead & 0xF0) == 0xE0) { cp = lead & 0x0F; tail = 2; }
        else if ((lead & 0xF8) == 0xF0) { cp = lead & 0x07; tail = 3; }
        bool valid = tail == 0 || begin + tail < text.size();
        if (valid) for (std::size_t n = 0; n < tail; ++n) {
            const unsigned char next = static_cast<unsigned char>(text[i + n]);
            if ((next & 0xC0) != 0x80) { valid = false; break; }
            cp = (cp << 6) | (next & 0x3F);
        }
        if (valid && tail > 0) {
            i += tail;
            if ((tail == 1 && cp < 0x80) || (tail == 2 && cp < 0x800) ||
                (tail == 3 && cp < 0x10000) || cp > 0x10FFFF ||
                (cp >= 0xD800 && cp <= 0xDFFF)) { cp = 0xFFFD; valid = false; }
        }
        if (!valid) cp = 0xFFFD;
        units.push_back({cp, begin, i});
    }
    return units;
}

inline std::string encode(int cp) {
    if (cp < 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return {};
    std::string result;
    if (cp < 0x80) result.push_back(static_cast<char>(cp));
    else if (cp < 0x800) {
        result.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        result.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        result.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        result.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return result;
}

inline std::size_t lastOffset(std::string_view value) noexcept {
    if (value.empty()) return 0;
    std::size_t start = value.size() - 1;
    while (start > 0 && (static_cast<unsigned char>(value[start]) & 0xC0) == 0x80) --start;
    return start;
}

inline void eraseLast(std::string& value) {
    value.erase(lastOffset(value));
}

inline bool appendWithinLimit(std::string& value, std::string_view input, std::size_t limit) {
    if (input.size() > limit || value.size() > limit - input.size()) return false;
    value.append(input);
    return true;
}

// Input created by encode() is valid. Counting its leaders avoids decoding password bytes.
inline std::size_t countValid(std::string_view value) noexcept {
    std::size_t count = 0;
    for (unsigned char byte : value)
        if ((byte & 0xc0) != 0x80) ++count;
    return count;
}
}

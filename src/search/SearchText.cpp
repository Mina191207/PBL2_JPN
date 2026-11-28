#include "search/SearchText.h"
#include "pbl2/Search.h"
#include <algorithm>
#include <iterator>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <unicode/locid.h>
#include <unicode/normalizer2.h>
#endif

std::string_view trimSearchQuery(std::string_view query, bool trimStart, bool trimEnd) {
    constexpr std::string_view spaces[] = {" ", "\t", "\r", "\n", "\f", "\v", u8"\u0085", u8"\u00a0",
        u8"\u1680", u8"\u2000", u8"\u2001", u8"\u2002", u8"\u2003", u8"\u2004", u8"\u2005",
        u8"\u2006", u8"\u2007", u8"\u2008", u8"\u2009", u8"\u200a", u8"\u2028", u8"\u2029",
        u8"\u202f", u8"\u205f", u8"\u3000"};
    while (trimStart && !query.empty()) {
        const auto space = std::find_if(std::begin(spaces), std::end(spaces), [&](auto candidate) {
            return query.substr(0, candidate.size()) == candidate;
        });
        if (space == std::end(spaces)) break;
        query.remove_prefix(space->size());
    }
    while (trimEnd && !query.empty()) {
        const auto space = std::find_if(std::begin(spaces), std::end(spaces), [&](auto candidate) {
            return query.size() >= candidate.size() && query.substr(query.size() - candidate.size()) == candidate;
        });
        if (space == std::end(spaces)) break;
        query.remove_suffix(space->size());
    }
    return query;
}

std::string normalizeSearchText(std::string_view text) {
    if (std::all_of(text.begin(), text.end(), [](unsigned char c) { return c < 0x80; })) {
        std::string result(text);
        for (char& c : result)
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        return result;
    }
#ifdef _WIN32
    const int bytes = static_cast<int>(text.size());
    const int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), bytes, nullptr, 0);
    std::wstring wide(chars, L'\0');
    if (!chars || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), bytes, wide.data(), chars))
        throw std::runtime_error("Cannot decode search text");

    const int lowerSize = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE,
                                        wide.data(), chars, nullptr, 0, nullptr, nullptr, 0);
    std::wstring lower(lowerSize, L'\0');
    if (!lowerSize || !LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(), chars,
                                    lower.data(), lowerSize, nullptr, nullptr, 0))
        throw std::runtime_error("Cannot lowercase search text");

    std::wstring normalized(lower.size(), L'\0');
    int count = NormalizeString(NormalizationC, lower.data(), lowerSize, normalized.data(), lowerSize);
    while (count < 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
           static_cast<std::size_t>(-count) > normalized.size()) {
        normalized.resize(-count);
        count = NormalizeString(NormalizationC, lower.data(), lowerSize, normalized.data(),
                                static_cast<int>(normalized.size()));
    }
    if (count <= 0) throw std::runtime_error("Cannot normalize search text");

    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, normalized.data(), count,
                                         nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    if (!size || !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, normalized.data(), count,
                                     result.data(), size, nullptr, nullptr))
        throw std::runtime_error("Cannot encode search text");
    return result;
#else
    UErrorCode error = U_ZERO_ERROR;
    const auto* nfc = icu::Normalizer2::getNFCInstance(error);
    if (U_FAILURE(error)) throw std::runtime_error("Cannot initialize Unicode normalization");
    auto lower = icu::UnicodeString::fromUTF8(icu::StringPiece(text.data(), static_cast<int32_t>(text.size())));
    lower.toLower(icu::Locale::getRoot());
    auto normalized = nfc->normalize(lower, error);
    if (U_FAILURE(error)) throw std::runtime_error("Cannot normalize search text");
    std::string result;
    return normalized.toUTF8String(result);
#endif
}

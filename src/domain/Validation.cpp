#include "domain/Validation.h"
#include <stdexcept>

bool isBlank(const std::string& text) {
    return trimBasic(text).empty();
}

std::string trimBasic(const std::string& text) {
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string::npos)
        return {};
    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

bool validUtf8(const std::string& text) {
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char first = static_cast<unsigned char>(text[i]);
        if (first < 0x80) {
            ++i;
            continue;
        }
        const int extra = first >= 0xC2 && first <= 0xDF ? 1 :
                          first >= 0xE0 && first <= 0xEF ? 2 :
                          first >= 0xF0 && first <= 0xF4 ? 3 : -1;
        if (extra < 0 || i + static_cast<std::size_t>(extra) >= text.size())
            return false;
        unsigned code = first & (extra == 1 ? 0x1F : extra == 2 ? 0x0F : 0x07);
        for (int j = 1; j <= extra; ++j) {
            const auto c = static_cast<unsigned char>(text[i + static_cast<std::size_t>(j)]);
            if ((c & 0xC0) != 0x80)
                return false;
            code = (code << 6) | (c & 0x3F);
        }
        if ((extra == 1 && code < 0x80) || (extra == 2 && code < 0x800) ||
            (extra == 3 && code < 0x10000) || (code >= 0xD800 && code <= 0xDFFF) ||
            code > 0x10FFFF || code == 0xFEFF)
            return false;
        i += static_cast<std::size_t>(extra + 1);
    }
    return true;
}

void validateTxtField(const std::string& text, const char* field, bool required) {
    if ((required && isBlank(text)) || text.find_first_of("|\r\n") != std::string::npos ||
        text.find('\0') != std::string::npos || text.size() > 8192 || !validUtf8(text)) {
        throw std::invalid_argument(std::string("Invalid field: ") + field);
    }
}

void validateTxtRecord(std::initializer_list<std::string> fields) {
    std::size_t size = fields.size() ? fields.size() - 1 : 0;
    for (const auto& field : fields) {
        validateTxtField(field, "record", false);
        size += field.size();
    }
    if (size > 16384)
        throw std::invalid_argument("record exceeds 16384 bytes");
}

void validatePositive(int value, const char* field) {
    if (value <= 0) {
        throw std::invalid_argument(std::string("Invalid field: ") + field);
    }
}

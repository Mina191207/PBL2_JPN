#include "domain/Validation.h"
#include <cctype>
#include <stdexcept>

bool isBlank(const std::string& text) {
    for (unsigned char c : text) {
        if (!std::isspace(c))
            return false;
    }
    return true;
}

void validateTxtField(const std::string& text, const char* field, bool required) {
    if ((required && isBlank(text)) || text.find_first_of("|\r\n") != std::string::npos ||
        text.find('\0') != std::string::npos) {
        throw std::invalid_argument(std::string("Invalid field: ") + field);
    }
}

void validatePositive(int value, const char* field) {
    if (value <= 0) {
        throw std::invalid_argument(std::string("Invalid field: ") + field);
    }
}

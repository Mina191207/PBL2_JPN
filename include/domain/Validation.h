#pragma once

#include <string>
#include <stdexcept>
#include <cctype>

inline bool isBlank(const std::string& text){
    for( unsigned c : text){
        if(!std::isspace(c)) return false;
    }
    return true;
};

inline void validateTxtField(const std::string& text, const char* field, bool required = true){
    if((required && isBlank(text)) || text.find_first_of("|\r\n")!= std::string::npos || text.find('\0') != std::string::npos){
            throw std::invalid_argument(std::string("Invalid field: ") + field);
    }
}
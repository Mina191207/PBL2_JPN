#pragma once
#include <string>

bool isBlank(const std::string& text);
void validateTxtField(const std::string& text, const char* field, bool required = true);
void validatePositive(int value, const char* field);

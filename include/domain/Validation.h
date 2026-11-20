#pragma once
#include <string>
#include <initializer_list>

bool isBlank(const std::string& text);
std::string trimBasic(const std::string& text);
bool validUtf8(const std::string& text);
void validateTxtField(const std::string& text, const char* field, bool required = true);
void validateTxtRecord(std::initializer_list<std::string> fields);
void validatePositive(int value, const char* field);

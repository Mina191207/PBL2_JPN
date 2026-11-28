#pragma once

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <string>
#include <vector>

enum class FieldId {
    Category, Name, Order, Description, Username, Password, Role,
    Script, Glyph, Romaji, Group, Note, Level, Resource, Prompt,
    OptionA, OptionB, OptionC, OptionD, Correct, Explanation, Status,
    Word, Reading, Meaning, PartOfSpeech, Example, Pattern, Structure, Usage,
    Character, Strokes, Onyomi, Kunyomi
};

struct FormField {
    FieldId id;
    const char* label;
    std::string value{};
};
using FormFields = std::vector<FormField>;

template<class Fields> decltype(auto) fieldValue(Fields& fields, FieldId id) {
    const auto found = std::find_if(fields.begin(), fields.end(), [id](const FormField& f) { return f.id == id; });
    if (found == fields.end()) throw std::logic_error("missing form field");
    return (found->value);
}

inline int wholeNumber(const std::string& value, const char* label) {
    int parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
        throw std::invalid_argument(std::string(label) + " must be a whole number.");
    return parsed;
}

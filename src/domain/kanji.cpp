#include "domain/kanji.h"
#include "domain/ReviewCard.h"
#include "domain/Validation.h"
#include "algorithms/KmpMatcher.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
struct Utf8Codepoint { std::uint32_t value; std::size_t offset; std::size_t length; };

std::vector<Utf8Codepoint> decodeUtf8(const std::string& text) {
    std::vector<Utf8Codepoint> result;
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        const std::size_t length = lead < 0x80 ? 1 : (lead & 0xE0) == 0xC0 ? 2 :
                                   (lead & 0xF0) == 0xE0 ? 3 : 4;
        if (i + length > text.size()) return {};
        std::uint32_t value = lead & (length == 1 ? 0x7F : length == 2 ? 0x1F : length == 3 ? 0x0F : 0x07);
        for (std::size_t j = 1; j < length; ++j) {
            const auto continuation = static_cast<unsigned char>(text[i + j]);
            if ((continuation & 0xC0) != 0x80) return {};
            value = (value << 6) | (continuation & 0x3F);
        }
        result.push_back({value, i, length});
        i += length;
    }
    return result;
}

bool isCjkIdeograph(std::uint32_t value) {
    return (value >= 0x3400 && value <= 0x4DBF) ||
           (value >= 0x4E00 && value <= 0x9FFF) ||
           (value >= 0xF900 && value <= 0xFAFF) ||
           (value >= 0x20000 && value <= 0x3134F);
}
}

Kanji::Kanji(const ResourceId& id,
             const LevelId& level,
             std::string character,
             std::string meaning,
             int strokeCount,
             std::string onyomi,
             std::string kunyomi,
             std::string examples)
    : Resource(id, level, meaning),
      character_(std::move(character)),
      onyomi_(std::move(onyomi)),
      kunyomi_(std::move(kunyomi)),
      examples_(std::move(examples)),
      strokeCount_(strokeCount) {
    validateTxtField(character_, "character");
    validatePositive(strokeCount_, "stroke_count");
    if (strokeCount_ > 255 || (onyomi_.empty() && kunyomi_.empty())) {
        throw std::invalid_argument("invalid kanji detail");
    }
    validateTxtField(onyomi_, "onyomi", false);
    validateTxtField(kunyomi_, "kunyomi", false);
    validateTxtField(examples_, "examples", false);
    validateTxtRecord({getId().getId(), character_, getMeaning(), std::to_string(strokeCount_), onyomi_, kunyomi_, examples_});
}
const std::string& Kanji::getCharacter() const noexcept {
    return character_;
}
int Kanji::getStrokeCount() const noexcept {
    return strokeCount_;
}
const std::string& Kanji::getOnyomi() const noexcept {
    return onyomi_;
}
const std::string& Kanji::getKunyomi() const noexcept {
    return kunyomi_;
}
const std::string& Kanji::getExamples() const noexcept {
    return examples_;
}
ResourceType Kanji::getType() const noexcept {
    return ResourceType::KANJI;
}
std::string Kanji::getTerm() const {
    return character_;
}
int Kanji::matchScore(const KmpMatcher& matcher) const {
    const std::string& query = matcher.pattern();
    if (query.empty())
        return 0;
    if (character_ == query)
        return 4;
    if (matcher.findFirst(character_) == 0)
        return 3;

    const auto codepoints = decodeUtf8(query);
    std::size_t ideographCount = 0;
    bool containsKanji = false;
    for (const auto& codepoint : codepoints) {
        if (!isCjkIdeograph(codepoint.value)) continue;
        ++ideographCount;
        if (character_ == query.substr(codepoint.offset, codepoint.length)) containsKanji = true;
    }
    if (ideographCount > 1 && containsKanji)
        return 3;

    if (matcher.findFirst(character_) != std::string::npos ||
        matcher.findFirst(onyomi_) != std::string::npos ||
        matcher.findFirst(kunyomi_) != std::string::npos)
        return 2;
    return matcher.findFirst(getMeaning()) == std::string::npos ? 0 : 1;
}
ReviewCard Kanji::makeReviewCard() const {
    std::string back = getMeaning();
    if (!onyomi_.empty())
        back += "\nOnyomi: " + onyomi_;
    if (!kunyomi_.empty())
        back += "\nKunyomi: " + kunyomi_;
    if (!examples_.empty())
        back += "\nExamples: " + examples_;
    return {getId(), getLevelId(), getType(), character_, std::move(back)};
}

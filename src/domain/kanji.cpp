#include "domain/kanji.h"
#include "domain/ReviewCard.h"
#include "domain/Validation.h"
#include "algorithms/KmpMatcher.h"
#include <stdexcept>
#include <utility>

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
    if (matcher.pattern().empty())
        return 0;
    if (character_ == matcher.pattern())
        return 4;
    if (matcher.findFirst(character_) == 0)
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

#include <stdexcept>
#include "domain/grammar.h"
#include "domain/ReviewCard.h"
#include "domain/Validation.h"
#include "algorithms/KmpMatcher.h"
#include <utility>

Grammar::Grammar(const ResourceId& id,
                 const LevelId& level,
                 std::string pattern,
                 std::string meaning,
                 std::string structure,
                 std::string usage,
                 std::string example,
                 std::string note)
    : Resource(id, level, meaning),
      pattern_(std::move(pattern)),
      structure_(std::move(structure)),
      usage_(std::move(usage)),
      example_(std::move(example)),
      note_(std::move(note)) {
    validateTxtField(pattern_, "pattern");
    validateTxtField(structure_, "structure", false);
    validateTxtField(usage_, "usage", false);
    validateTxtField(example_, "example", false);
    validateTxtField(note_, "note", false);
    validateTxtRecord({getId().getId(), pattern_, getMeaning(), structure_, usage_, example_, note_});
}
const std::string& Grammar::getPattern() const noexcept {
    return pattern_;
}
const std::string& Grammar::getStructure() const noexcept {
    return structure_;
}
const std::string& Grammar::getUsage() const noexcept {
    return usage_;
}
const std::string& Grammar::getExample() const noexcept {
    return example_;
}
const std::string& Grammar::getNote() const noexcept {
    return note_;
}
ResourceType Grammar::getType() const noexcept {
    return ResourceType::GRAMMAR;
}
std::string Grammar::getTerm() const {
    return pattern_;
}
int Grammar::matchScore(const KmpMatcher& matcher) const {
    if (matcher.pattern().empty())
        return 0;
    if (pattern_ == matcher.pattern())
        return 4;
    if (matcher.findFirst(pattern_) == 0)
        return 3;
    if (matcher.findFirst(pattern_) != std::string::npos ||
        matcher.findFirst(structure_) != std::string::npos ||
        matcher.findFirst(usage_) != std::string::npos)
        return 2;
    return matcher.findFirst(getMeaning()) == std::string::npos ? 0 : 1;
}
ReviewCard Grammar::makeReviewCard() const {
    std::string back = getMeaning() + "\nStructure: " + structure_ + "\nUsage: " + usage_;
    if (!example_.empty())
        back += "\nExample: " + example_;
    return {getId(), getLevelId(), getType(), pattern_, std::move(back)};
}

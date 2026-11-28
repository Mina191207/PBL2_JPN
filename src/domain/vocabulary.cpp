#include <stdexcept>
#include <utility>
#include "pbl2/domain/vocabulary.h"
#include "pbl2/domain/ReviewCard.h"
#include "domain/Validation.h"

const char* partOfSpeechToken(PartOfSpeech value) {
    switch (value) {
    case PartOfSpeech::NOUN: return "NOUN";
    case PartOfSpeech::VERB: return "VERB";
    case PartOfSpeech::IADJECTIVE: return "I_ADJ";
    case PartOfSpeech::NAADJECTIVE: return "NA_ADJ";
    case PartOfSpeech::ADVERB: return "ADV";
    case PartOfSpeech::OTHER: return "OTHER";
    }
    throw std::invalid_argument("invalid part of speech");
}

Vocabulary::Vocabulary(const ResourceId& id,
                       const LevelId& levelId,
                       std::string word,
                       std::string reading,
                       std::string meaning,
                       PartOfSpeech partOfSpeech,
                       std::string example)
    : Resource(id, levelId, meaning),
      word_(std::move(word)),
      reading_(std::move(reading)),
      partOfSpeech_(partOfSpeech),
      example_(std::move(example)) {
    validateTxtField(word_, "word");
    validateTxtField(reading_, "reading", false);
    validateTxtField(example_, "example", false);
    validateTxtRecord({getId().getId(), word_, reading_, getMeaning(), partOfSpeechToken(partOfSpeech_), example_});
    buildSearchKeys(word_, reading_);
}
const std::string& Vocabulary::getWord() const noexcept {
    return word_;
}

const std::string& Vocabulary::getReading() const noexcept {
    return reading_;
}

PartOfSpeech Vocabulary::getPartOfSpeech() const noexcept {
    return partOfSpeech_;
}

const std::string& Vocabulary::getExample() const noexcept {
    return example_;
}
ResourceType Vocabulary::getType() const noexcept {
    return ResourceType::VOCABULARY;
}
std::string Vocabulary::getTerm() const {
    return word_;
}
ReviewCard Vocabulary::makeReviewCard() const {
    std::string back = getMeaning();
    if (!reading_.empty()) {
        back += "\nReading: " + reading_;
    }
    if (!example_.empty()) {
        back += "\nExample: " + example_;
    }
    return {getId(), getLevelId(), getType(), word_, std::move(back)};
}

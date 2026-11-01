#include "../../include/domain/vocabulary.h"
#include "../../include/domain/ReviewCard.h"
#include "../../include/domain/Validation.h"
#include "../../include/algorithms/KmpMatcher.h"

Vocabulary::Vocabulary(const ResourceId& id, const LevelId& levelId, std::string word, std::string reading, std::string meaning, PartOfSpeech partOfSpeech, std::string example)
    : Resource(id, levelId, meaning), word_(std::move(word)), reading_(std::move(reading)), partOfSpeech_(partOfSpeech), example_(std::move(example)) {
       validateTxtField(word_, "word");
       validateTxtField(reading_, "reading", false);
       validateTxtField(example_, "example", false);
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
ResourceType Vocabulary::getType() const noexcept{
    return ResourceType::VOCABULARY;
}
std::string Vocabulary::getTerm() const{
    return word_;
}
int Vocabulary::matchScore(const KmpMatcher& matcher) const{
    const std::string& query = matcher.pattern();
    if(query.empty()) return 0;
    if(word_==query) return 4;
    if(matcher.findFirst(word_) == 0) return 3;
    if(matcher.findFirst(word_) != std::string::npos || matcher.findFirst(reading_) != std::string::npos) return 2;
    if(matcher.findFirst(getMeaning()) != std::string::npos) return 1;
    return 0;
}
ReviewCard Vocabulary::makeReviewCard() const{
    std::string back = getMeaning();
    if(!reading_.empty()){
        back += "\nReading: " + reading_;
    }
    if(!example_.empty()){
        back += "\nExample: " + example_;
    }
    return{getId(), getLevelId(), getType(), word_, std::move(back)};
}
#include "../../include/domain/vocabulary.h"
namespace {
bool isBlank(const std::string& s){
    for (unsigned char c : s){ // dùng unsigned để tránh lỗi khi so sánh với std::isspace
        if(!std::isspace(c)){
            return false;
        }
    }
    return true;
}
}
vocabulary::vocabulary(
    const ResourceId& resourceId, 
    std::string word, 
    std::string reading, 
    std::string meaning, 
    PartOfSpeech type,
    std::string example
)
: id(resourceId),
  word(std::move(word)),
  reading(std::move(reading)),
  meaning(std::move(meaning)),
  partofspeech(type),
  example(std::move(example))
{
    if(isBlank(this->word)) {
        throw std::invalid_argument("Word cannot be blank");
    }
    if(isBlank(this->meaning)) {
        throw std::invalid_argument("Meaning cannot be blank");
    }
}
const ResourceId& vocabulary::getId() const noexcept{
    return this->id;
}
std::string vocabulary::getWord() const{
    return this->word;
}
std::string vocabulary::getReading() const{
    return this->reading;
}
std::string vocabulary::getMeaning() const{
    return this->meaning;
}
PartOfSpeech vocabulary::getPartOfSpeech() const noexcept {
    return this->partofspeech;
}
std::string vocabulary::getExample() const{
    return this->example;
}
#include "vocabulary.h"
vocabulary::vocabulary(string id, string word, string reading, string meaning, string type, string example){
    this->id = id;
    this->word = word;
    this->reading = reading;
    this->meaning = meaning;
    this->type = type;
    this->example = example;
}
string vocabulary::getId(){
    return this->id;
}
string vocabulary::getWord(){
    return this->word;
}
string vocabulary::getMeaning(){
    return this->meaning;
}
string vocabulary::getType(){
    return this->type;
}
string vocabulary::getExample(){
    return this->example;
}
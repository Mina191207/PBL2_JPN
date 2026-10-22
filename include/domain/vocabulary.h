#pragma once // include guard: giúp tránh include file trùng lặp.
#include "PartOfSpeech.h"
#include <string>
#include <stdexcept>
#include <cctype>
class vocabulary{
    std::string id;
    std::string word;
    std::string reading;
    std::string meaning;
    PartOfSpeech partofspeech;
    std::string example;
    public:
        vocabulary(
            std::string, 
            std::string, 
            std::string, 
            std::string, 
            PartOfSpeech, 
            std::string
        );
        std::string getId() const;
        std::string getWord() const;
        std::string getReading() const;
        std::string getMeaning() const;
        PartOfSpeech getPartOfSpeech() const noexcept;
        std::string getExample() const;
};
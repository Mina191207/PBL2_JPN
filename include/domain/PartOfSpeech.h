#pragma once

enum class PartOfSpeech {
    NOUN,
    VERB,
    IADJECTIVE,
    NAADJECTIVE,
    ADVERB,
    OTHER,
};

const char* partOfSpeechToken(PartOfSpeech value);

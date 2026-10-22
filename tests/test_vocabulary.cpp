#include "domain/vocabulary.h"
#include <cstdlib>
#include <iostream>

int main() {
    vocabulary vocab(
        "R001",
        "食べる",
        "たべる",
        "ăn",
        PartOfSpeech::VERB,
        "毎日朝ご飯を食べます。"
    );

    if (vocab.getId() != "R001" ||
        vocab.getWord() != "食べる" ||
        vocab.getMeaning() != "ăn") {

        std::cerr << "Vocabulary test failed\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

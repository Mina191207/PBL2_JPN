#include <iostream>
#include <string>

#include "../include/storage/FileStorage.h"
int main() {
       LinkedList<vocabulary> words;
    std::string error;

    if (!storage::loadVocabularyFile(
        "database/vocabulary.txt",
        words,
        error
    )) {
        std::cerr << error << '\n';
        return 1;
    }

    std::cout << "So ban ghi: " << words.size() << '\n';

    for (int i = 0; i < words.size(); ++i) {
        const vocabulary& item = words.at(i);

        std::cout
            << item.getId().getId() << " | "
            << item.getWord() << " | "
            << item.getMeaning() << " | "
            << item.getExample() << '\n';
    }

    return 0;

};
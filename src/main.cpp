#include <iostream>
#include "../include/domain/vocabulary.h"
#include "../include/domain/level.h"
int main() {
    Level level(
        LevelId("L005"),
        "JLPT",
        "JLPT N5",
        5,
        "Beginner"
    );

    Vocabulary vocab(
        ResourceId("R001"),
        level.getId(),
        "学校",
        "がっこう",
        "trường học",
        PartOfSpeech::NOUN,
        "学校へ行きます。"
    );

    const Resource& resource = vocab;

    std::cout << "ID: "
              << resource.getId().getId() << '\n';

    std::cout << "Level: "
              << resource.getLevelId().getId() << '\n';

    std::cout << "Term: "
              << resource.getTerm() << '\n';

    std::cout << "Meaning: "
              << resource.getMeaning() << '\n';

    return 0;
};

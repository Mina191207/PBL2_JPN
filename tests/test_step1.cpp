#include "../include/domain/EntityId.h"
#include "../include/domain/resource.h"
#include "../include/domain/vocabulary.h"
#include "../include/domain/level.h"
#include "../include/domain/ReviewCard.h"
#include "../include/algorithms/KmpMatcher.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <cstdint>
#include <limits>

int main() {

    // TEST 1: EntityId
    assert(ResourceId::isValid("R001"));
    assert(ResourceId::isValid("R999"));
    assert(ResourceId::isValid("R1000"));
    assert(ResourceId::isValid("R100000"));
    assert(ResourceId::isValid("R1000000"));

    assert(!ResourceId::isValid("R000"));
    assert(!ResourceId::isValid("R01"));
    assert(!ResourceId::isValid("R0001"));
    assert(!ResourceId::isValid("RABC"));
    assert(!ResourceId::isValid("U001"));

    assert(ResourceId("R001").number() == 1);
    assert(ResourceId("R100000").number() == 100000);

    assert(ResourceId("R001") == ResourceId("R001"));
    assert(ResourceId("R001") != ResourceId("R002"));

    // TEST 2: Gioi han uint64_t
    assert(ResourceId::isValid(
        "R18446744073709551615"
    ));

    assert(!ResourceId::isValid(
        "R18446744073709551616"
    ));

    assert(
        ResourceId("R18446744073709551615").number() ==
        std::numeric_limits<std::uint64_t>::max()
    );

    // TEST 3: Constructor ID sai
    bool rejected = false;

    try {
        ResourceId invalid("R0001");
    }
    catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);

    // TEST 4: Level
    Level level(
        LevelId("L005"),
        "jlpt",
        "JLPT N5",
        5,
        "Beginner"
    );

    assert(level.getId().getId() == "L005");
    assert(level.getCategory() == "JLPT");
    assert(level.getName() == "JLPT N5");
    assert(level.getOrder() == 5);

    // TEST 5: Level order sai
    rejected = false;

    try {
        Level invalid(
            LevelId("L006"),
            "JLPT",
            "Invalid",
            0,
            ""
        );
    }
    catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);

    // TEST 6: Vocabulary
    Vocabulary vocab(
        ResourceId("R001"),
        LevelId("L005"),
        "学校",
        "がっこう",
        "trường học",
        PartOfSpeech::NOUN,
        "学校へ行きます。"
    );

    assert(vocab.getId().getId() == "R001");
    assert(vocab.getLevelId().getId() == "L005");
    assert(vocab.getWord() == "学校");
    assert(vocab.getMeaning() == "trường học");
    assert(vocab.getPartOfSpeech() == PartOfSpeech::NOUN);

    // TEST 7: Vocabulary khong hop le
    rejected = false;

    try {
        Vocabulary invalid(
            ResourceId("R002"),
            LevelId("L005"),
            "",
            "",
            "nghĩa",
            PartOfSpeech::NOUN,
            ""
        );
    }
    catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);

    // TEST 8: Da hinh
    const Resource& resource = vocab;

    assert(resource.getType() == ResourceType::VOCABULARY);
    assert(resource.getTerm() == "学校");
    assert(resource.getMeaning() == "trường học");
    assert(resource.getLevelId().getId() == "L005");

    // TEST 9: KMP va matchScore
    assert(resource.matchScore(KmpMatcher("学校")) == 4);
    assert(resource.matchScore(KmpMatcher("学")) == 3);
    assert(resource.matchScore(KmpMatcher("っこう")) == 2);
    assert(resource.matchScore(KmpMatcher("trường")) == 1);
    assert(resource.matchScore(KmpMatcher("xyz")) == 0);
    assert(resource.matchScore(KmpMatcher("")) == 0);

    // TEST 10: ReviewCard
    ReviewCard card = resource.makeReviewCard();

    assert(card.resourceId == ResourceId("R001"));
    assert(card.levelId == LevelId("L005"));
    assert(card.type == ResourceType::VOCABULARY);
    assert(card.front == "学校");

    assert(card.back.find("trường học") != std::string::npos);
    assert(card.back.find("がっこう") != std::string::npos);

    // TEST 11: Copy Vocabulary
    Vocabulary copied = vocab;

    assert(copied.getId() == vocab.getId());
    assert(copied.getWord() == vocab.getWord());

    std::cout << "ALL STEP 1 TESTS PASSED\n";

    return 0;
}
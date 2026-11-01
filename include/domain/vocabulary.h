#pragma once
#include "PartOfSpeech.h"
#include "EntityId.h"
#include "resource.h"
#include <string>
#include <stdexcept>
#include <cctype>
class Vocabulary final : public Resource {
    std::string word_;
    std::string reading_;
    PartOfSpeech partOfSpeech_;
    std::string example_;

  public:
    Vocabulary(const ResourceId& id,
               const LevelId& levelId,
               std::string word,
               std::string reading,
               std::string meaning,
               PartOfSpeech partOfSpeech,
               std::string example);
    const std::string& getWord() const noexcept;
    const std::string& getReading() const noexcept;
    PartOfSpeech getPartOfSpeech() const noexcept;
    const std::string& getExample() const noexcept;
    ResourceType getType() const noexcept override;
    std::string getTerm() const override;
    int matchScore(const KmpMatcher& matcher) const override;

    ReviewCard makeReviewCard() const override;
};

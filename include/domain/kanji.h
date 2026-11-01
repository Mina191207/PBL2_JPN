#pragma once
#include "resource.h"
#include <string>

class Kanji final : public Resource {
    std::string character_, onyomi_, kunyomi_, examples_;
    int strokeCount_;

  public:
    Kanji(const ResourceId& id,
          const LevelId& level,
          std::string character,
          std::string meaning,
          int strokeCount,
          std::string onyomi,
          std::string kunyomi,
          std::string examples);
    const std::string& getCharacter() const noexcept;
    int getStrokeCount() const noexcept;
    const std::string& getOnyomi() const noexcept;
    const std::string& getKunyomi() const noexcept;
    const std::string& getExamples() const noexcept;
    ResourceType getType() const noexcept override;
    std::string getTerm() const override;
    int matchScore(const KmpMatcher& matcher) const override;
    ReviewCard makeReviewCard() const override;
};

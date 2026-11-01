#pragma once
#include "resource.h"
#include <string>

class Grammar final : public Resource {
    std::string pattern_, structure_, usage_, example_, note_;

  public:
    Grammar(const ResourceId& id,
            const LevelId& level,
            std::string pattern,
            std::string meaning,
            std::string structure,
            std::string usage,
            std::string example,
            std::string note);
    const std::string& getPattern() const noexcept;
    const std::string& getStructure() const noexcept;
    const std::string& getUsage() const noexcept;
    const std::string& getExample() const noexcept;
    const std::string& getNote() const noexcept;
    ResourceType getType() const noexcept override;
    std::string getTerm() const override;
    int matchScore(const KmpMatcher& matcher) const override;
    ReviewCard makeReviewCard() const override;
};

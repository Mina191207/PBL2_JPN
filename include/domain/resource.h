#pragma once

#include "EntityId.h"
#include "ResourceType.h"
#include <string>

class KmpMatcher;
struct ReviewCard;

class Resource {
  private:
    ResourceId id_;
    LevelId levelId_;
    std::string meaning_;

  protected:
    Resource(const ResourceId& id, const LevelId& levelId, const std::string& meaning);
    Resource(const Resource&) = default;
    Resource& operator=(const Resource&) = default;

  public:
    virtual ~Resource() = default;
    const ResourceId& getId() const noexcept;
    const LevelId& getLevelId() const noexcept;
    const std::string& getMeaning() const noexcept;
    virtual ResourceType getType() const noexcept = 0;
    virtual std::string getTerm() const = 0;
    virtual int matchScore(const KmpMatcher& matcher) const = 0;
    virtual ReviewCard makeReviewCard() const = 0;
};

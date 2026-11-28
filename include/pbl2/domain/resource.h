#pragma once

#include "EntityId.h"
#include "ResourceType.h"
#include "ReviewCard.h"
#include <string>
#include <string_view>

class Resource {
  private:
    ResourceId id_;
    LevelId levelId_;
    std::string meaning_;
    struct SearchKeys {
        std::string term, meaning, first, second;
    } searchKeys_;
    friend class ResourceManager;

  protected:
    Resource(const ResourceId& id, const LevelId& levelId, const std::string& meaning);
    Resource(const Resource&) = default;
    Resource& operator=(const Resource&) = default;
    void buildSearchKeys(std::string_view term, std::string_view first, std::string_view second = {});

  public:
    virtual ~Resource() = default;
    const ResourceId& getId() const noexcept;
    const LevelId& getLevelId() const noexcept;
    const std::string& getMeaning() const noexcept;
    virtual ResourceType getType() const noexcept = 0;
    virtual std::string getTerm() const = 0;
    virtual ReviewCard makeReviewCard() const = 0;
};

#pragma once
#include "EntityId.h"

class Favorite {
    FavoriteId id_;
    UserId userId_;
    ResourceId resourceId_;

  public:
    Favorite(FavoriteId id, UserId userId, ResourceId resourceId);
    const FavoriteId& getId() const noexcept;
    const UserId& getUserId() const noexcept;
    const ResourceId& getResourceId() const noexcept;
};

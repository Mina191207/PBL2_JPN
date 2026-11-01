#include "domain/favorite.h"
#include <utility>

Favorite::Favorite(FavoriteId id, UserId userId, ResourceId resourceId)
    : id_(std::move(id)),
      userId_(std::move(userId)),
      resourceId_(std::move(resourceId)) {
}

const FavoriteId& Favorite::getId() const noexcept {
    return id_;
}
const UserId& Favorite::getUserId() const noexcept {
    return userId_;
}
const ResourceId& Favorite::getResourceId() const noexcept {
    return resourceId_;
}

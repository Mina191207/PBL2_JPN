#pragma once

#include "EntityId.h"
#include "ResourceType.h"
#include <string>

struct ReviewCard{
    ResourceId resourceId;
    LevelId levelId;
    ResourceType type;

    std::string front;
    std::string back;
};
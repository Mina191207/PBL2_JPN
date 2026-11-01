#pragma once
#include "EntityId.h"
#include <string>

class Level{
    private: 
        LevelId id_;
        std::string category_;
        std::string name_;
        int order_;
        std::string description_;
    public:
        Level(const LevelId& id, std::string category, std::string name, int order, std::string description);
        const LevelId& getId() const noexcept;
        const std::string& getCategory() const noexcept;
        const std::string& getName() const noexcept;
        int getOrder() const noexcept;
        const std::string& getDescription() const noexcept;
};
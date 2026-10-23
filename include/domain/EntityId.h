#pragma once
#include <utility>
#include <string>
#include <stdexcept>
template<char prefix>
class EntityId{
    std::string value_;
    static bool isValid(const std::string&);
    public:
    explicit EntityId(std::string);
    const std::string& getId() const noexcept;
    bool operator==(const EntityId&) const noexcept;
};
using ResourceId = EntityId<'R'>;
using UserId = EntityId<'U'>;
using LevelId = EntityId<'L'>;
using FavoriteId = EntityId<'F'>;
using HistoryId = EntityId<'H'>;
template<char prefix>
bool EntityId<prefix>::isValid(const std::string& id) {
        if (id.size() < 4 || id[0] != prefix)
            return false;
        for (std::size_t i = 1; i < id.size(); ++i) {
            if (id[i] < '0' || id[i] > '9')
                return false;
        }
        if (id.size() == 4)
            return id[1] != '0' || id[2] != '0' || id[3] != '0' ;
        return id[1] != '0';
}

template<char prefix>
EntityId<prefix>::EntityId(std::string value) : value_(std::move(value)){
    if(!isValid(value)) throw std::invalid_argument("Invalid entity ID format");
}
template<char prefix>
const std::string& EntityId<prefix>::getId() const noexcept{
    return this->value_;
}
template<char prefix>
bool EntityId<prefix>::operator==(const EntityId& other) const noexcept{
    return this->value_ == other.value_;
}

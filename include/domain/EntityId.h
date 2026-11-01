#pragma once
#include <cstddef>
#include <utility>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <limits>
template<char prefix>
class EntityId{
    std::string value_;
    public:
    static bool isValid(const std::string&);
    explicit EntityId(std::string);
    const std::string& getId() const noexcept;
    bool operator==(const EntityId&) const noexcept;
    std::uint64_t number() const noexcept;
    bool operator!=(const EntityId&) const noexcept;
};

template<char prefix>
bool EntityId<prefix>::isValid(const std::string& id) {
        if (id.size() < 4 || id[0] != prefix)
            return false;
        if(id.size() > 4 && id[1]=='0') return false;
        std::uint64_t value = 0;
        for (std::size_t i = 1; i < id.size(); ++i) {
            if (id[i] < '0' || id[i] > '9')
                return false;
            unsigned digit = static_cast<unsigned>(id[i] - '0');

            if(value > (std::numeric_limits<uint64_t>::max()-digit)/10) return false;
            value = value * 10 + digit;
        }
        return value != 0;
}

template<char prefix>
EntityId<prefix>::EntityId(std::string value) : value_(std::move(value)){
    if(!isValid(this->value_)) throw std::invalid_argument("Invalid entity ID format");
}
template<char prefix>
const std::string& EntityId<prefix>::getId() const noexcept{
    return this->value_;
}
template<char prefix>
bool EntityId<prefix>::operator==(const EntityId& other) const noexcept{
    return this->value_ == other.value_;
}
template<char prefix>
std::uint64_t EntityId<prefix>::number() const noexcept{
    std::uint64_t result = 0;
    for(std::size_t i = 0; i< value_.size(); i++) result = result*10 + static_cast<unsigned>(value_[i] - '0');
    return result;
}
template<char prefix>
bool EntityId<prefix>::operator!=(const EntityId<prefix>& other) const noexcept{
    return !(*this == other);
}

using ResourceId = EntityId<'R'>;
using UserId = EntityId<'U'>;
using LevelId = EntityId<'L'>;
using FavoriteId = EntityId<'F'>;
using HistoryId = EntityId<'H'>;
using AlphabetId = EntityId<'A'>;
using QuestionId = EntityId<'Q'>;
using AttemptId = EntityId<'T'>;
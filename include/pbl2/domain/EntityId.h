#pragma once
#include <cstdint>
#include <string>

template <char prefix> class EntityId {
    std::string value_;

  public:
    static constexpr char prefixValue = prefix;
    static bool isValid(const std::string& value);
    explicit EntityId(std::string value);
    const std::string& getId() const noexcept;
    std::uint64_t number() const noexcept;
    bool operator==(const EntityId& other) const noexcept;
    bool operator!=(const EntityId& other) const noexcept;
};

using ResourceId = EntityId<'R'>;
using UserId = EntityId<'U'>;
using LevelId = EntityId<'L'>;
using FavoriteId = EntityId<'F'>;
using HistoryId = EntityId<'H'>;
using AlphabetId = EntityId<'A'>;
using QuestionId = EntityId<'Q'>;
using AttemptId = EntityId<'T'>;

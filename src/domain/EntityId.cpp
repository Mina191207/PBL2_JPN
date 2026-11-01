#include "domain/EntityId.h"
#include <limits>
#include <stdexcept>
#include <utility>

template <char prefix> bool EntityId<prefix>::isValid(const std::string& value) {
    if (value.size() < 4 || value[0] != prefix) {
        return false;
    }
    if (value.size() > 4 && value[1] == '0') {
        return false;
    }
    std::uint64_t number = 0;
    for (std::size_t i = 1; i < value.size(); ++i) {
        if (value[i] < '0' || value[i] > '9') {
            return false;
        }
        const unsigned digit = static_cast<unsigned>(value[i] - '0');
        if (number > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
            return false;
        }
        number = number * 10 + digit;
    }
    return number != 0;
}

template <char prefix>
EntityId<prefix>::EntityId(std::string value)
    : value_(std::move(value)) {
    if (!isValid(value_)) {
        throw std::invalid_argument("invalid entity id");
    }
}

template <char prefix> const std::string& EntityId<prefix>::getId() const noexcept {
    return value_;
}

template <char prefix> std::uint64_t EntityId<prefix>::number() const noexcept {
    std::uint64_t result = 0;
    for (std::size_t i = 1; i < value_.size(); ++i) {
        result = result * 10 + static_cast<unsigned>(value_[i] - '0');
    }
    return result;
}

template <char prefix> bool EntityId<prefix>::operator==(const EntityId& other) const noexcept {
    return value_ == other.value_;
}

template <char prefix> bool EntityId<prefix>::operator!=(const EntityId& other) const noexcept {
    return !(*this == other);
}

template class EntityId<'R'>;
template class EntityId<'U'>;
template class EntityId<'L'>;
template class EntityId<'F'>;
template class EntityId<'H'>;
template class EntityId<'A'>;
template class EntityId<'Q'>;
template class EntityId<'T'>;

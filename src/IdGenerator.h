#include <cstdint>
#include <limits>
#include <stdexcept>

template <class Id, class Container> Id nextId(const Container& values) {
    std::uint64_t maxValue = 0;
    for (const auto& value : values) {
        if (value.getId().number() > maxValue) {
            maxValue = value.getId().number();
        }
    }
    if (maxValue == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("id space exhausted");
    }
    std::string digits = std::to_string(maxValue + 1);
    if (digits.size() < 3) {
        digits.insert(0, 3 - digits.size(), '0');
    }
    return Id(std::string(1, Id::prefixValue) + digits);
}

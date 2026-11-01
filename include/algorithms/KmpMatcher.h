#pragma once
#include <cstddef>
#include <string>
#include <vector>

class KmpMatcher {
    std::string pattern_;
    std::vector<std::size_t> lps_;

  public:
    explicit KmpMatcher(std::string pattern);
    const std::string& pattern() const noexcept;
    std::size_t findFirst(const std::string& value) const noexcept;
};

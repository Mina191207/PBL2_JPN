#include "algorithms/KmpMatcher.h"
#include <utility>

KmpMatcher::KmpMatcher(std::string pattern)
    : pattern_(std::move(pattern)),
      lps_(pattern_.size()) {
    for (std::size_t i = 1, length = 0; i < pattern_.size();) {
        if (pattern_[i] == pattern_[length]) {
            lps_[i++] = ++length;
        } else if (length) {
            length = lps_[length - 1];
        } else {
            lps_[i++] = 0;
        }
    }
}

const std::string& KmpMatcher::pattern() const noexcept {
    return pattern_;
}

std::size_t KmpMatcher::findFirst(std::string_view value) const noexcept {
    if (pattern_.empty()) return 0;
    if (value.size() < pattern_.size()) return std::string_view::npos;
    for (std::size_t i = 0, j = 0; i < value.size();) {
        if (value[i] == pattern_[j]) {
            ++i;
            if (++j == pattern_.size()) {
                return i - j;
            }
        } else if (j) {
            j = lps_[j - 1];
        } else {
            ++i;
        }
    }
    return std::string_view::npos;
}

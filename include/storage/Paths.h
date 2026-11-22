#pragma once
#include <filesystem>
#include <string>

// Public path strings are UTF-8. Keep conversion explicit at filesystem boundaries.
std::filesystem::path pathFromUtf8(const std::string& path);
std::string pathToUtf8(const std::filesystem::path& path);

// True when either resolved directory is the same as, an ancestor of, or a
// descendant of the other. Throws filesystem_error if the relationship cannot
// be verified safely.
bool pathsOverlap(const std::string& left, const std::string& right);

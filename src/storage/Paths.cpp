#include "storage/Paths.h"
#include <filesystem>

namespace {
std::filesystem::path resolvedPath(const std::string& input) {
    namespace fs = std::filesystem;
    auto existing = fs::absolute(input);
    fs::path suffix;
    // Resolve real directories first, then append the not-yet-created suffix.
    // This also avoids MSVC weakly_canonical failing on a missing final directory.
    while (!fs::exists(existing)) {
        const auto parent = existing.parent_path();
        if (parent == existing)
            throw fs::filesystem_error("cannot resolve directory", existing,
                                       std::make_error_code(std::errc::no_such_file_or_directory));
        suffix = existing.filename() / suffix;
        existing = parent;
    }
    return (fs::canonical(existing) / suffix).lexically_normal();
}
}

bool sameDirectory(const std::string& left, const std::string& right) {
    std::error_code ec;
    if (std::filesystem::equivalent(left, right, ec)) return true;
    return resolvedPath(left) == resolvedPath(right);
}

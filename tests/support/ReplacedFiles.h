#pragma once

#include "TestSupport.h"
#include <set>

// A hard link keeps the old file identity alive across an atomic replacement.
struct ReplacedFiles {
    TempDirectory links;
    std::filesystem::path folder;

    explicit ReplacedFiles(const std::filesystem::path& path) : folder(path) {
        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.path().extension() == ".txt" && entry.is_regular_file())
                std::filesystem::create_hard_link(entry.path(), links.path / entry.path().filename());
        }
    }

    std::set<std::string> names() const {
        std::set<std::string> replaced;
        for (const auto& entry : std::filesystem::directory_iterator(links.path)) {
            const auto name = entry.path().filename();
            if (!std::filesystem::equivalent(entry.path(), folder / name)) replaced.insert(name.string());
        }
        return replaced;
    }
};

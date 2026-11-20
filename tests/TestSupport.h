#pragma once
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>

// Unlike assert, CHECK also executes the expression when NDEBUG is defined.
#define CHECK(...) do { if (!(__VA_ARGS__)) { \
    throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " + #__VA_ARGS__); \
} } while (false)

template<class F> void throws(F action) {
    bool caught = false;
    try { action(); } catch (const std::exception&) { caught = true; }
    CHECK(caught);
}
struct TempDirectory {
    std::filesystem::path path;
    TempDirectory() {
        for (int tries = 0; tries < 10; ++tries) {
            path = std::filesystem::temp_directory_path() / ("pbl2_test_" + std::to_string(std::random_device{}()));
            if (std::filesystem::create_directory(path)) return;
        }
        throw std::runtime_error("cannot create test directory");
    }
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    ~TempDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};
inline void writeFile(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << bytes;
    stream.close();
    CHECK(stream);
}

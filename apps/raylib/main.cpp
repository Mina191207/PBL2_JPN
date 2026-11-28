#include "app/AppController.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
std::filesystem::path defaultDataFolder() {
    const auto executableDir = std::filesystem::u8path(GetApplicationDirectory());
    std::ifstream config(executableDir / "pbl2-data-dir.txt");
    std::string folder;
    std::getline(config, folder);
    if (folder.compare(0, 3, "\xEF\xBB\xBF") == 0) folder.erase(0, 3);
    if (!folder.empty() && folder.back() == '\r') folder.pop_back();
    return executableDir / (folder.empty() ? std::filesystem::path("database") : std::filesystem::u8path(folder));
}

int run(int argc, const std::filesystem::path& option, const std::filesystem::path& data) {
    if (argc == 2 && option == "--help") {
        std::cout << "Usage: pbl2_app [--data-dir PATH]\n";
        return 0;
    }
    if (argc != 1 && (argc != 3 || option != "--data-dir")) {
        std::cerr << "Usage: pbl2_app [--data-dir PATH]\n";
        return 2;
    }
    try {
        const auto folder = argc == 1 ? defaultDataFolder() : data;
        std::error_code error;
        const auto absolute = folder.empty() ? folder : std::filesystem::absolute(folder, error);
        AppController app;
        return app.run((error ? folder : absolute).u8string());
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    return run(argc, argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path{},
                     argc > 2 ? std::filesystem::path(argv[2]) : std::filesystem::path{});
}

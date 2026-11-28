#include "app/AppController.h"
#include <filesystem>
#include <iostream>

namespace {
int run(int argc, const std::filesystem::path& option, const std::filesystem::path& data) {
    if (argc == 2 && option == "--help") {
        std::cout << "Usage: pbl2_app [--data-dir PATH]\n";
        return 0;
    }
    if (argc != 1 && (argc != 3 || option != "--data-dir" || data.empty())) {
        std::cerr << "Usage: pbl2_app [--data-dir PATH]\n";
        return 2;
    }
    try {
        const auto folder = std::filesystem::absolute(argc == 1 ? std::filesystem::path("database") : data);
        AppController app;
        return app.run(folder.u8string());
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

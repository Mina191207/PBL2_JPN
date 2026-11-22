#include "storage/Paths.h"
#include <filesystem>
#include <limits>
#include <string>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstdlib>
#endif

namespace {
namespace fs = std::filesystem;

[[noreturn]] void throwPathError(const char* operation, const fs::path& path,
                                 const std::error_code& error) {
    throw fs::filesystem_error(operation, path, error);
}

#if defined(_WIN32)
std::error_code lastWindowsError() {
    return {static_cast<int>(GetLastError()), std::system_category()};
}

fs::path absolutePath(const fs::path& input) {
    DWORD capacity = GetFullPathNameW(input.c_str(), 0, nullptr, nullptr);
    if (capacity == 0) throwPathError("cannot make export path absolute", input,
                                      lastWindowsError());
    std::vector<wchar_t> buffer(static_cast<std::size_t>(capacity) + 1);
    DWORD length = GetFullPathNameW(input.c_str(), static_cast<DWORD>(buffer.size()),
                                    buffer.data(), nullptr);
    if (length == 0 || length >= buffer.size())
        throwPathError("cannot normalize export path", input, lastWindowsError());
    return fs::path(std::wstring(buffer.data(), length));
}

fs::path resolveExistingDirectory(const fs::path& input) {
    HANDLE handle = CreateFileW(input.c_str(), FILE_READ_ATTRIBUTES,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throwPathError("cannot open directory for path validation", input,
                       lastWindowsError());

    const DWORD flags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
    DWORD capacity = MAX_PATH;
    std::vector<wchar_t> buffer;
    DWORD length = 0;
    for (;;) {
        buffer.resize(static_cast<std::size_t>(capacity) + 1);
        length = GetFinalPathNameByHandleW(handle, buffer.data(),
                                           static_cast<DWORD>(buffer.size()), flags);
        if (length == 0) {
            const auto error = lastWindowsError();
            CloseHandle(handle);
            throwPathError("cannot resolve directory alias", input, error);
        }
        if (length < buffer.size()) break;
        capacity = length;
    }
    CloseHandle(handle);
    return fs::path(std::wstring(buffer.data(), length));
}
#else
fs::path absolutePath(const fs::path& input) {
    std::error_code error;
    auto result = fs::absolute(input, error);
    if (error) throwPathError("cannot make export path absolute", input, error);
    return result;
}

fs::path resolveExistingDirectory(const fs::path& input) {
    std::error_code error;
    auto result = fs::canonical(input, error);
    if (error) throwPathError("cannot resolve existing directory", input, error);
    return result;
}
#endif

bool isMissingPathError(const std::error_code& error) {
    return error == std::errc::no_such_file_or_directory ||
           error == std::errc::not_a_directory;
}

fs::path resolveDirectoryPath(const fs::path& input) {
    if (input.empty())
        throw fs::filesystem_error("empty directory path", input,
                                   std::make_error_code(std::errc::invalid_argument));

    const auto absolute = absolutePath(input);
    auto resolved = resolveExistingDirectory(absolute.root_path());

    for (const auto& component : absolute.relative_path()) {
        if (component == fs::path(".")) continue;
        if (component == fs::path("..")) {
            const auto parent = resolved.parent_path();
            if (!parent.empty()) resolved = parent;
            continue;
        }

        const auto candidate = resolved / component;
        std::error_code linkError;
        const auto linkStatus = fs::symlink_status(candidate, linkError);
        const bool linkExists = !linkError && fs::exists(linkStatus);
        if (linkExists) {
            std::error_code targetError;
            const auto targetStatus = fs::status(candidate, targetError);
            if (targetError)
                throwPathError("cannot resolve directory link", candidate, targetError);
            if (!fs::is_directory(targetStatus))
                throw fs::filesystem_error("path component is not a directory", candidate,
                                           std::make_error_code(std::errc::not_a_directory));
            resolved = resolveExistingDirectory(candidate);
            continue;
        }

        if (linkError && !isMissingPathError(linkError))
            throwPathError("cannot inspect export path", candidate, linkError);

#if defined(_WIN32)
        // A dangling junction or symbolic link must not be treated as an ordinary
        // missing suffix: its eventual target cannot be established safely.
        const DWORD attributes = GetFileAttributesW(candidate.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            throw fs::filesystem_error("cannot resolve dangling directory link", candidate,
                                       std::make_error_code(std::errc::no_such_file_or_directory));
#else
        // symlink_status sees a dangling POSIX link even when status() cannot follow it.
        if (!linkError && fs::is_symlink(linkStatus))
            throw fs::filesystem_error("cannot resolve dangling directory link", candidate,
                                       std::make_error_code(std::errc::no_such_file_or_directory));
#endif

        resolved = candidate;
    }

    return resolved;
}

bool samePathComponent(const fs::path& left, const fs::path& right) {
#if defined(_WIN32)
    const auto& a = left.native();
    const auto& b = right.native();
    return a.size() <= static_cast<std::size_t>((std::numeric_limits<int>::max)()) &&
           b.size() <= static_cast<std::size_t>((std::numeric_limits<int>::max)()) &&
           CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(),
                                static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
#else
    return left == right;
#endif
}

bool isComponentPrefix(const fs::path& prefix, const fs::path& candidate) {
    auto prefixPart = prefix.begin();
    auto candidatePart = candidate.begin();
    for (; prefixPart != prefix.end(); ++prefixPart, ++candidatePart) {
        if (candidatePart == candidate.end() ||
            !samePathComponent(*prefixPart, *candidatePart))
            return false;
    }
    return true;
}
}

fs::path pathFromUtf8(const std::string& path) {
    if (path.find('\0') != std::string::npos)
        throw fs::filesystem_error("filesystem path contains a null byte", fs::path{},
                                   std::make_error_code(std::errc::invalid_argument));
#if defined(_WIN32)
    if (path.empty()) return {};
    if (path.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        throw fs::filesystem_error("UTF-8 filesystem path is too long", fs::path{},
                                   std::make_error_code(std::errc::filename_too_long));
    const int inputLength = static_cast<int>(path.size());
    const int outputLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                                  path.data(), inputLength, nullptr, 0);
    if (outputLength == 0)
        throwPathError("cannot decode UTF-8 filesystem path", fs::path{}, lastWindowsError());
    std::wstring wide(static_cast<std::size_t>(outputLength), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), inputLength,
                            wide.data(), outputLength) == 0)
        throwPathError("cannot decode UTF-8 filesystem path", fs::path{}, lastWindowsError());
    return fs::path(std::move(wide));
#else
    return fs::path(path);
#endif
}

std::string pathToUtf8(const fs::path& path) {
#if defined(_WIN32)
    const auto& wide = path.native();
    if (wide.find(L'\0') != std::wstring::npos)
        throw fs::filesystem_error("filesystem path contains a null character", path,
                                   std::make_error_code(std::errc::invalid_argument));
    if (wide.empty()) return {};
    if (wide.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
        throw fs::filesystem_error("filesystem path is too long for UTF-8", path,
                                   std::make_error_code(std::errc::filename_too_long));
    const int inputLength = static_cast<int>(wide.size());
    const int outputLength = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                                  wide.data(), inputLength, nullptr, 0,
                                                  nullptr, nullptr);
    if (outputLength == 0)
        throwPathError("cannot encode filesystem path as UTF-8", path, lastWindowsError());
    std::string utf8(static_cast<std::size_t>(outputLength), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), inputLength,
                            utf8.data(), outputLength, nullptr, nullptr) == 0)
        throwPathError("cannot encode filesystem path as UTF-8", path, lastWindowsError());
    return utf8;
#else
    return path.native();
#endif
}

bool pathsOverlap(const std::string& left, const std::string& right) {
    const auto resolvedLeft = resolveDirectoryPath(pathFromUtf8(left));
    const auto resolvedRight = resolveDirectoryPath(pathFromUtf8(right));
    return isComponentPrefix(resolvedLeft, resolvedRight) ||
           isComponentPrefix(resolvedRight, resolvedLeft);
}

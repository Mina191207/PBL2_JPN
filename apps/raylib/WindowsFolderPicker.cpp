#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

std::optional<std::string> pickNativeFolder(void* owner, const std::string& current) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) return {};
    struct Apartment { ~Apartment() { CoUninitialize(); } } apartment;
    const auto release = [](IUnknown* item) { item->Release(); };
    IFileOpenDialog* raw = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&raw)))) return {};
    const std::unique_ptr<IFileOpenDialog, decltype(release)> dialog(raw, release);
    if (FAILED(dialog->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR))) return {};
    if (!current.empty()) {
        std::error_code error;
        const auto start = std::filesystem::absolute(std::filesystem::u8path(current), error);
        IShellItem* item = nullptr;
        if (!error && SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&item)))) {
            const std::unique_ptr<IShellItem, decltype(release)> folder(item, release);
            dialog->SetFolder(folder.get());
        }
    }
    if (FAILED(dialog->Show(static_cast<HWND>(owner)))) return {};
    IShellItem* selected = nullptr;
    if (FAILED(dialog->GetResult(&selected))) return {};
    const std::unique_ptr<IShellItem, decltype(release)> folder(selected, release);
    PWSTR name = nullptr;
    if (FAILED(folder->GetDisplayName(SIGDN_FILESYSPATH, &name))) return {};
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> path(name, CoTaskMemFree);
    return std::filesystem::path(path.get()).u8string();
}

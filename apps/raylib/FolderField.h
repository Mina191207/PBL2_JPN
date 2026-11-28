#pragma once

#include "UiInput.h"
#include "UITheme.h"
#include <utility>

class FolderField {
  public:
    explicit FolderField(std::string value = {}) : value_(std::move(value)) {}
    void set(std::string value);
    std::string path() const;
    bool update(Rectangle bounds, UiInput::Focus& focus, int pathId, int browseId);
    void draw(Rectangle bounds, const char* label, const UiInput::Focus& focus,
              int pathId, int browseId, bool enabled = true) const;
#ifdef _WIN32
    static constexpr bool nativeBrowse = true;
#else
    static constexpr bool nativeBrowse = false;
#endif

  private:
    std::string value_;
    UiInput::Cursor cursor_;
    bool replaceOnEdit_ = false;
};

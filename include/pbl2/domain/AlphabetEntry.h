#pragma once
#include "EntityId.h"
#include <string>

enum class Script {
    HIRAGANA,
    KATAKANA
};

class AlphabetEntry {
    AlphabetId id_;
    Script script_;
    std::string glyph_;
    std::string romaji_;
    std::string groupName_;
    std::string note_;
    int displayOrder_;

  public:
    AlphabetEntry(AlphabetId id,
                  Script script,
                  std::string glyph,
                  std::string romaji,
                  std::string groupName,
                  int displayOrder,
                  std::string note);
    const AlphabetId& getId() const noexcept;
    Script getScript() const noexcept;
    const std::string& getGlyph() const noexcept;
    const std::string& getRomaji() const noexcept;
    const std::string& getGroupName() const noexcept;
    int getDisplayOrder() const noexcept;
    const std::string& getNote() const noexcept;
};

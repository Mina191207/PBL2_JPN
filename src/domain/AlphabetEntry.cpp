#include "domain/AlphabetEntry.h"
#include "domain/Validation.h"
#include <utility>

AlphabetEntry::AlphabetEntry(AlphabetId id,
                             Script script,
                             std::string glyph,
                             std::string romaji,
                             std::string groupName,
                             int displayOrder,
                             std::string note)
    : id_(std::move(id)),
      script_(script),
      glyph_(std::move(glyph)),
      romaji_(std::move(romaji)),
      groupName_(std::move(groupName)),
      note_(std::move(note)),
      displayOrder_(displayOrder) {
    validateTxtField(glyph_, "glyph");
    validateTxtField(romaji_, "romaji");
    validateTxtField(groupName_, "group_name");
    validateTxtField(note_, "note", false);
    validatePositive(displayOrder_, "display_order");
}

const AlphabetId& AlphabetEntry::getId() const noexcept {
    return id_;
}
Script AlphabetEntry::getScript() const noexcept {
    return script_;
}
const std::string& AlphabetEntry::getGlyph() const noexcept {
    return glyph_;
}
const std::string& AlphabetEntry::getRomaji() const noexcept {
    return romaji_;
}
const std::string& AlphabetEntry::getGroupName() const noexcept {
    return groupName_;
}
int AlphabetEntry::getDisplayOrder() const noexcept {
    return displayOrder_;
}
const std::string& AlphabetEntry::getNote() const noexcept {
    return note_;
}

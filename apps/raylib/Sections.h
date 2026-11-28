#pragma once

#include "pbl2/UserRole.h"
#include <algorithm>
#include <initializer_list>

enum class Section {
    Dashboard, Resources, Levels, Accounts, Alphabet, Questions, Packages,
    Favorites, History, Review, Quiz, Statistics, Count
};

struct MenuEntry { Section section; const char* label; };

inline const std::initializer_list<MenuEntry>& menu(UserRole role) {
    static const std::initializer_list<MenuEntry> admin = {
        {Section::Dashboard, "Dashboard"}, {Section::Resources, "Resources"},
        {Section::Levels, "Levels"}, {Section::Accounts, "Accounts"}, {Section::Alphabet, "Alphabet"},
        {Section::Questions, "Question bank"}, {Section::Packages, "Import / export"}
    };
    static const std::initializer_list<MenuEntry> learner = {
        {Section::Dashboard, "Dashboard"}, {Section::Resources, "Search library"},
        {Section::Favorites, "Favorites"}, {Section::History, "Search history"}, {Section::Review, "Review"},
        {Section::Quiz, "Quiz"}, {Section::Alphabet, "Alphabet"}, {Section::Statistics, "Statistics"}
    };
    return role == UserRole::ADMIN ? admin : learner;
}

inline const char* sectionTitle(UserRole role, Section section) {
    const auto& items = menu(role);
    const auto found = std::find_if(items.begin(), items.end(), [section](const MenuEntry& item) {
        return item.section == section;
    });
    return found == items.end() ? "Workspace" : found->label;
}

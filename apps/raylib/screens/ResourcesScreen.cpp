#include "ResourcesScreen.h"
#include "../UITheme.h"
#include "../UiLayout.h"
#include "../Utf8.h"
#include "pbl2/InputLimits.h"
#include <algorithm>
#include <cmath>
#include <type_traits>

struct ResourceUiLayout {
    Rectangle content;
    Rectangle query;
    Rectangle search;
    Rectangle mode;
    Rectangle type;
    Rectangle level;
    Rectangle sort;
    Rectangle list;
    Rectangle detail;
    Rectangle add;
    Rectangle edit;
    Rectangle remove;
    Rectangle favorite;
    Rectangle selectionHint;
    Rectangle rowsClip;
    Rectangle browse, previous, next;
    float rowHeight;
    float typeColumnX;
    float levelColumnX;
    float typeBadgeWidth;
    float levelBadgeWidth;
    std::size_t visibleRows;
};

namespace {
constexpr Color kPrimary = UITheme::primary;
constexpr Color kHover = UITheme::hover;
constexpr Color kBorder = UITheme::border;
constexpr Color kText = UITheme::text;
constexpr Color kMuted = UITheme::muted;

const char* typeName(ResourceType type) {
    switch (type) {
    case ResourceType::VOCABULARY: return "Vocabulary";
    case ResourceType::GRAMMAR: return "Grammar";
    case ResourceType::KANJI: return "Kanji";
    }
    return "Resource";
}



ResourceUiLayout layout(UserRole role, bool hasSelected) {
    ResourceUiLayout result{};
    result.content = UiLayout::content();
    const float pad = std::min(24.0f, std::max(16.0f, result.content.width * 0.025f));
    const float x = result.content.x + pad;
    const float innerW = result.content.width - pad * 2.0f;
    const float y = result.content.y;
    result.query = {x, y + 60.0f, std::max(120.0f, innerW - 104.0f), 38.0f};
    result.search = {result.query.x + result.query.width + 10.0f, y + 60.0f, 94.0f, 38.0f};
    const float filterGap = 8.0f;
    const float filterW = (innerW - filterGap * 3.0f) / 4.0f;
    result.mode = {x, y + 106.0f, filterW, 36.0f};
    result.type = {x + filterW + filterGap, y + 106.0f, filterW, 36.0f};
    result.level = {x + (filterW + filterGap) * 2.0f, y + 106.0f, filterW, 36.0f};
    result.sort = {x + (filterW + filterGap) * 3.0f, y + 106.0f, filterW, 36.0f};
    const float bodyY = y + 164.0f;
    const float footerY = result.content.y + result.content.height - 34.0f;
    const float bodyH = std::max(96.0f, footerY - bodyY - 12.0f);
    const float gap = 14.0f;
    const float listW = std::max(130.0f, (innerW - gap) * 0.52f);
    result.list = {x, bodyY, listW, bodyH};
    result.detail = {x + listW + gap, bodyY, innerW - listW - gap, bodyH};
    result.rowHeight = 45.0f;
    const float rowsTop = result.list.y + 62.0f;
    const float footerTop = result.list.y + result.list.height - 34.0f;
    result.next = {x + listW - 57.0f, footerTop + 4.0f, 48.0f, 26.0f};
    result.previous = {result.next.x - 52.0f, result.next.y, 48.0f, 26.0f};
    result.browse = {x + listW - 116.0f, result.next.y, 108.0f, 26.0f};
    result.rowsClip = {result.list.x + 4.0f, rowsTop, result.list.width - 18.0f,
                       std::max(32.0f, footerTop - rowsTop - 6.0f)};
    result.visibleRows = std::min<std::size_t>(8, std::max<std::size_t>(1, static_cast<std::size_t>(result.rowsClip.height / result.rowHeight)));
    result.typeBadgeWidth = 104.0f;
    result.levelBadgeWidth = 62.0f;
    result.levelColumnX = result.list.x + result.list.width - 24.0f - result.levelBadgeWidth;
    result.typeColumnX = result.levelColumnX - 8.0f - result.typeBadgeWidth;
    const float actionGap = 8.0f;
    const float actionW = (result.detail.width - actionGap) * 0.5f;
    const float detailBottom = result.detail.y + result.detail.height;
    const bool adminHint = role == UserRole::ADMIN && !hasSelected;
    const float actionY = detailBottom - (adminHint ? 132.0f : 78.0f);
    result.add = {result.detail.x, actionY, actionW, 34.0f};
    result.edit = {result.detail.x + actionW + actionGap, actionY, actionW, 34.0f};
    result.remove = {result.detail.x, actionY + 40.0f, actionW, 34.0f};
    result.favorite = role == UserRole::LEARNER
        ? Rectangle{result.detail.x, detailBottom - 42.0f, result.detail.width, 34.0f}
        : hasSelected ? Rectangle{result.detail.x + actionW + actionGap, actionY + 40.0f, actionW, 34.0f}
                      : Rectangle{};
    result.selectionHint = adminHint
        ? Rectangle{result.detail.x, actionY + 82.0f, result.detail.width, 44.0f}
        : role == UserRole::LEARNER && !hasSelected
            ? Rectangle{result.detail.x, detailBottom - 56.0f, result.detail.width, 48.0f}
            : Rectangle{};
    return result;
}

Rectangle formDialog() { return UiLayout::centeredDialog(760.0f, 580.0f, 20.0f); }
Rectangle formTypeButton() { const Rectangle d = formDialog(); return {d.x + 24, d.y + 68, (d.width - 56) * 0.48f, 38}; }
Rectangle formLevelButton() { const Rectangle d = formDialog(); return {d.x + 32 + (d.width - 56) * 0.48f, d.y + 68, (d.width - 56) * 0.52f, 38}; }
Rectangle formField(std::size_t index, std::size_t count) {
    const Rectangle d = formDialog();
    const float gap = 14.0f;
    const float width = (d.width - 48.0f - gap) * 0.5f;
    const int columns = count > 4 ? 2 : 1;
    const int col = columns == 1 ? 0 : static_cast<int>(index % 2);
    const int row = columns == 1 ? static_cast<int>(index) : static_cast<int>(index / 2);
    const bool spansBothColumns = count == 5 && index == 4;
    return {d.x + 24.0f + (spansBothColumns ? 0.0f : col * (width + gap)),
            d.y + 142.0f + row * 76.0f,
            columns == 1 || spansBothColumns ? d.width - 48.0f : width, 40.0f};
}
Rectangle formSaveButton() { const Rectangle d = formDialog(); return {d.x + d.width - 244.0f, d.y + d.height - 54.0f, 104.0f, 38.0f}; }
Rectangle formCancelButton() { const Rectangle d = formDialog(); return {d.x + d.width - 128.0f, d.y + d.height - 54.0f, 104.0f, 38.0f}; }
using Field = FieldId;
FormFields resourceFields(ResourceType type) {
    switch (type) {
    case ResourceType::VOCABULARY: return {{Field::Word, "Word"}, {Field::Reading, "Reading"},
        {Field::Meaning, "Meaning"}, {Field::PartOfSpeech, "Part of speech", "NOUN"}, {Field::Example, "Example"}};
    case ResourceType::GRAMMAR: return {{Field::Pattern, "Pattern"}, {Field::Meaning, "Meaning"},
        {Field::Structure, "Structure"}, {Field::Usage, "Usage"}, {Field::Example, "Example"}, {Field::Note, "Note"}};
    case ResourceType::KANJI: return {{Field::Character, "Character"}, {Field::Meaning, "Meaning"},
        {Field::Strokes, "Stroke count (1-255)"}, {Field::Onyomi, "Onyomi"}, {Field::Kunyomi, "Kunyomi"},
        {Field::Example, "Examples"}};
    }
    return {};
}
void cyclePartOfSpeech(std::string& value) {
    constexpr std::string_view choices[] = {"NOUN", "VERB", "I_ADJ", "NA_ADJ", "ADV", "OTHER"};
    const auto found = std::find(std::begin(choices), std::end(choices), value);
    const auto next = found == std::end(choices) || found+1 == std::end(choices) ? std::begin(choices) : found+1;
    value = *next;
}

std::string detailTitle(const ResourceDetail& detail) {
    return std::visit([](const auto& item) -> std::string {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, Vocabulary>) return item.getWord();
        else if constexpr (std::is_same_v<T, Grammar>) return item.getPattern();
        else return item.getCharacter();
    }, detail);
}

struct DetailField { std::string label; std::string value; bool example = false; };

std::vector<DetailField> detailLines(const ResourceDetail& detail) {
    return std::visit([](const auto& item) -> std::vector<DetailField> {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, Vocabulary>) {
            return {{"Reading", item.getReading()}, {"Meaning", item.getMeaning()},
                    {"Part of speech", partOfSpeechToken(item.getPartOfSpeech())},
                    {"Example", item.getExample(), true}};
        } else if constexpr (std::is_same_v<T, Grammar>) {
            return {{"Meaning", item.getMeaning()}, {"Structure", item.getStructure()},
                    {"Usage", item.getUsage()}, {"Example", item.getExample(), true}, {"Note", item.getNote()}};
        } else {
            return {{"Meaning", item.getMeaning()}, {"Strokes", std::to_string(item.getStrokeCount())},
                    {"Onyomi", item.getOnyomi()}, {"Kunyomi", item.getKunyomi()},
                    {"Examples", item.getExamples(), true}};
        }
    }, detail);
}

std::string levelLabel(const LevelId& id, const std::vector<Level>& levels) {
    const auto found = std::find_if(levels.begin(), levels.end(), [&](const Level& level) { return level.getId() == id; });
    return found == levels.end() ? id.getId() : found->getName();
}

void drawBadge(Rectangle bounds, const std::string& label, Color fill) {
    DrawRectangleRounded(bounds, 0.35f, 8, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.35f, 8, 1.0f, UITheme::border);
    const int textWidth = UITheme::measureText(label.c_str(), 15);
    BeginScissorMode(static_cast<int>(bounds.x + 6.0f), static_cast<int>(bounds.y),
                     std::max(1, static_cast<int>(bounds.width - 12.0f)), static_cast<int>(bounds.height));
    UITheme::drawText(label.c_str(), static_cast<int>(bounds.x + (bounds.width - textWidth) * 0.5f),
                      static_cast<int>(bounds.y + (bounds.height - 15.0f) * 0.5f), 15, UITheme::strongText);
    EndScissorMode();
}

Color typeBadgeColor(ResourceType type) {
    switch (type) {
    case ResourceType::VOCABULARY: return {255, 235, 242, 255};
    case ResourceType::GRAMMAR: return {241, 235, 252, 255};
    case ResourceType::KANJI: return {233, 244, 250, 255};
    }
    return UITheme::sidebar;
}

int estimatedLines(const std::string& value, int fontSize, float width, int maxLines) {
    if (value.empty() || width <= 0.0f) return 1;
    return std::clamp(static_cast<int>(std::ceil(static_cast<float>(UITheme::measureText(value.c_str(), fontSize)) / width)), 1, maxLines);
}

}

Rectangle ResourcesScreen::bounds(float x, float y, float w, float h) { return {x, y, w, h}; }
bool ResourcesScreen::click(Rectangle r) {
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), r);
}
void ResourcesScreen::button(Rectangle r, const char* label, bool primary, bool enabled) {
    UITheme::drawButton(r, label, primary, enabled);
}
void ResourcesScreen::field(Rectangle r, const std::string& label, const std::string& value, bool focused) {
    UITheme::drawField(r, label, value, focused);
}
void ResourcesScreen::appendInput(std::string& value, int limit) {
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (!value.empty()) {
            Utf8::eraseLast(value);
        }
    }
    for (int cp = GetCharPressed(); cp > 0; cp = GetCharPressed()) {
        if (cp >= 32 && cp != 127) Utf8::appendWithinLimit(value, Utf8::encode(cp), static_cast<std::size_t>(limit));
    }
}

ResourcesEvent ResourcesScreen::update(const std::vector<SearchRow>& rows,
                                       const std::vector<Level>& levels,
                                       const AccountSummary& account,
                                       const std::optional<bool>& selectedFavorited) {
    const ResourceUiLayout ui = layout(account.role, hasSelected_);
    if (rows.empty()) rowOffset_ = 0;
    else {
        const std::size_t maxOffset = rows.size() > ui.visibleRows ? rows.size() - ui.visibleRows : 0;
        rowOffset_ = std::min(rowOffset_, maxOffset);
    }
    if (confirmDelete_) return updateDeleteConfirmation();
    if (formOpen_) return updateResourceForm(levels);
    if (const auto event = updateSearchControls(levels, rows.empty(), ui)) return *event;
    if (const auto event = updateResourceSelection(rows, ui)) return *event;
    return updateResourceActions(account, levels, selectedFavorited, ui);
}

ResourcesEvent ResourcesScreen::updateDeleteConfirmation() {
    const Rectangle d = UiLayout::centeredDialog(460.0f, 220.0f, 20.0f);
    const Rectangle yes{d.x + d.width - 236, d.y + d.height - 58, 104, 38};
    const Rectangle no{d.x + d.width - 120, d.y + d.height - 58, 96, 38};
    if (IsKeyPressed(KEY_ESCAPE) || click(no)) { confirmDelete_ = false; return {}; }
    if (click(yes)) { confirmDelete_ = false; return {ResourcesEvent::Type::REMOVE, {}, selectedId_}; }
    return {};
}

ResourcesEvent ResourcesScreen::updateResourceForm(const std::vector<Level>& levels) {
    using Event = ResourcesEvent::Type;
    if (IsKeyPressed(KEY_ESCAPE) || click(formCancelButton())) {
        closeForm();
        return {Event::CANCEL_FORM};
    }
    if (!editing_ && click(formTypeButton())) {
        form_.type = static_cast<ResourceType>((static_cast<int>(form_.type)+1)%3);
        form_.fields = resourceFields(form_.type);
        focusedField_ = 0;
        return {};
    }
    if (click(formLevelButton()) && !levels.empty()) {
        formLevelIndex_ = (formLevelIndex_+1)%levels.size();
        form_.levelId = levels[formLevelIndex_].getId();
        return {};
    }
    const auto count = form_.fields.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (!click(formField(i, count))) continue;
        focusedField_ = i;
        if (form_.fields[i].id == Field::PartOfSpeech) cyclePartOfSpeech(form_.fields[i].value);
    }
    if (IsKeyPressed(KEY_TAB)) {
        const bool backwards = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        focusedField_ = (focusedField_ + (backwards ? count-1 : 1))%count;
    }
    auto& draft = form_.fields[focusedField_];
    const bool choose = draft.id == Field::PartOfSpeech;
    const bool enter = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    if (choose && enter) cyclePartOfSpeech(draft.value);
    if (!choose) appendInput(draft.value, maxTxtFieldBytes);
    if (!levels.empty() && (click(formSaveButton()) || (!choose && enter)))
        return {editing_ ? Event::UPDATE : Event::CREATE, {}, selectedId_, form_};
    return {};
}

std::optional<ResourcesEvent> ResourcesScreen::updateSearchControls(const std::vector<Level>& levels, bool empty,
                                                                    const ResourceUiLayout& ui) {
    using Event = ResourcesEvent::Type;
    if (empty && click(ui.browse)) {
        setSearch({});
        return ResourcesEvent{Event::PREVIEW_SEARCH, search_};
    }
    if (IsKeyPressed(KEY_ESCAPE) && queryFocused_) {
        search_.query.clear();
        return ResourcesEvent{Event::PREVIEW_SEARCH, search_};
    }
    if (click(ui.query)) queryFocused_ = true;
    else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), ui.query)) queryFocused_ = false;
    if (queryFocused_) {
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) Utf8::eraseLast(search_.query);
        const bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool withinLimit = true;
        if (control && IsKeyPressed(KEY_V)) {
            if (const char* pasted = GetClipboardText())
                withinLimit = Utf8::appendWithinLimit(search_.query, pasted, maxTxtFieldBytes);
        }
        for (int cp = GetCharPressed(); cp > 0; cp = GetCharPressed()) {
            if (!control && cp >= 32 && cp != 127 &&
                !Utf8::appendWithinLimit(search_.query, Utf8::encode(cp), maxTxtFieldBytes)) withinLimit = false;
        }
        if (!withinLimit) return ResourcesEvent{Event::INPUT_LIMIT};
    }
    if (click(ui.search) || (queryFocused_ && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))))
        return ResourcesEvent{Event::SUBMIT_SEARCH, search_};
    if (click(ui.mode)) {
        search_.exactId = !search_.exactId;
        return ResourcesEvent{Event::PREVIEW_SEARCH, search_};
    }
    if (click(ui.type)) {
        if (!search_.type) search_.type = ResourceType::VOCABULARY;
        else if (*search_.type == ResourceType::KANJI) search_.type.reset();
        else search_.type = static_cast<ResourceType>(static_cast<int>(*search_.type) + 1);
        return ResourcesEvent{Event::PREVIEW_SEARCH, search_};
    }
    if (click(ui.level)) {
        const auto current = std::find_if(levels.begin(), levels.end(), [&](const Level& level) {
            return search_.levelId && level.getId() == *search_.levelId;
        });
        const auto next = current == levels.end() ? levels.begin() : current + 1;
        search_.levelId = next == levels.end() ? std::nullopt : std::optional<LevelId>(next->getId());
        return ResourcesEvent{Event::PREVIEW_SEARCH, search_};
    }
    if (click(ui.sort)) {
        search_.sort = static_cast<SearchSort>((static_cast<int>(search_.sort) + 1) % 6);
        return ResourcesEvent{Event::PREVIEW_SEARCH, search_};
    }

    return std::nullopt;
}

std::optional<ResourcesEvent> ResourcesScreen::updateResourceSelection(const std::vector<SearchRow>& rows,
                                                                        const ResourceUiLayout& ui) {
    if (rows.size() > ui.visibleRows) {
        const auto maxOffset = rows.size() - ui.visibleRows;
        if (click(ui.previous) || (!queryFocused_ && IsKeyPressed(KEY_PAGE_UP)))
            rowOffset_ = rowOffset_ > ui.visibleRows ? rowOffset_ - ui.visibleRows : 0;
        if (click(ui.next) || (!queryFocused_ && IsKeyPressed(KEY_PAGE_DOWN)))
            rowOffset_ = std::min(maxOffset, rowOffset_ + ui.visibleRows);
    }
    if(CheckCollisionPointRec(GetMousePosition(),ui.list)){
        const float wheel=GetMouseWheelMove();
        if(wheel>0&&rowOffset_>0)--rowOffset_;
        if(wheel<0&&rowOffset_+ui.visibleRows<rows.size())++rowOffset_;
    }
    for (std::size_t i = 0; i < ui.visibleRows && rowOffset_+i < rows.size(); ++i) {
        const Rectangle row = bounds(ui.list.x + 8.0f, ui.rowsClip.y + static_cast<float>(i) * ui.rowHeight,
                                      ui.rowsClip.width - 8.0f, ui.rowHeight - 3.0f);
        if (click(row)) { selectedId_ = rows[rowOffset_+i].id; hasSelected_ = true; return ResourcesEvent{ResourcesEvent::Type::SELECT, {}, rows[rowOffset_+i].id}; }
    }

    return std::nullopt;
}

ResourcesEvent ResourcesScreen::updateResourceActions(const AccountSummary& account, const std::vector<Level>& levels,
                                                       const std::optional<bool>& selectedFavorited,
                                                       const ResourceUiLayout& ui) {
    if (account.role == UserRole::ADMIN) {
        if (click(ui.add)) {
            closeForm();
            formOpen_ = true;
            form_.type = search_.type.value_or(ResourceType::VOCABULARY);
            form_.fields = resourceFields(form_.type);
            formLevelIndex_ = 0;
            if (!levels.empty()) form_.levelId = levels.front().getId();
            return {};
        }
        if (click(ui.edit) && hasSelected_) {
            return {ResourcesEvent::Type::OPEN_EDIT, {}, selectedId_};
        }
        if (click(ui.remove) && hasSelected_) {
            confirmDelete_ = true;
            return {};
        }
    }
    if (click(ui.favorite) && hasSelected_ && selectedFavorited.has_value())
        return {ResourcesEvent::Type::FAVORITE, {}, selectedId_};
    return {};
}

void ResourcesScreen::drawSearchSection(const ResourceUiLayout& ui, const std::vector<Level>& levels) const {
    const float pad = std::min(24.0f, std::max(16.0f, ui.content.width * 0.025f));
    const int x = static_cast<int>(ui.content.x + pad);
    UITheme::drawText("Find learning resources", x, static_cast<int>(ui.content.y + 10), 22, kText);
    const auto& query = search_.query;
    std::size_t start = query.size() > 240 ? query.size() - 240 : 0;
    while (start < query.size() && (static_cast<unsigned char>(query[start]) & 0xc0) == 0x80) ++start;
    field(ui.query, search_.exactId ? "Resource ID (e.g. R001)" : "Search term", query.substr(start), queryFocused_);
    button(ui.search, "Search", true);
    button(ui.mode, search_.exactId ? "ID" : "Nội dung");
    const char* typeLabel = search_.type ? typeName(*search_.type) : "All types";
    const std::string selectedLevelLabel = search_.levelId ? levelLabel(*search_.levelId, levels) : "All levels";
    button(ui.type, typeLabel);
    button(ui.level, selectedLevelLabel.c_str());
    static const char* sorts[] = {"Relevant", "ID", "Level +", "Level -", "Term A-Z", "Term Z-A"};
    button(ui.sort, sorts[static_cast<int>(search_.sort)]);
}

void ResourcesScreen::drawPanelHeaders(const ResourceUiLayout& ui) const {
    DrawRectangleRounded(ui.list, 0.04f, 8, Color{255, 252, 253, 255});
    DrawRectangleRoundedLinesEx(ui.list, 0.04f, 8, 1.0f, kBorder);
    DrawRectangleRounded(ui.detail, 0.04f, 8, Color{255, 252, 253, 255});
    DrawRectangleRoundedLinesEx(ui.detail, 0.04f, 8, 1.0f, kBorder);
    UITheme::drawText("Results", static_cast<int>(ui.list.x + 14), static_cast<int>(ui.list.y + 10), 22, kText);
    UITheme::drawText("Item", static_cast<int>(ui.list.x + 14), static_cast<int>(ui.list.y + 36), 16, kMuted);
    UITheme::drawText("Type", static_cast<int>(ui.typeColumnX), static_cast<int>(ui.list.y + 36), 16, kMuted);
    UITheme::drawText("Level", static_cast<int>(ui.levelColumnX), static_cast<int>(ui.list.y + 36), 16, kMuted);
}

void ResourcesScreen::drawResourceList(const ResourceUiLayout& ui, const std::vector<SearchRow>& rows,
                                       const std::vector<Level>& levels, bool searchError) const {
    for (std::size_t i = rowOffset_; i < rows.size() && i < rowOffset_ + ui.visibleRows; ++i) {
        const Rectangle r = bounds(ui.list.x + 8.0f, ui.rowsClip.y + static_cast<float>(i-rowOffset_)*ui.rowHeight,
                                   ui.list.width - 24.0f, ui.rowHeight - 3.0f);
        const bool selected = hasSelected_ && rows[i].id == selectedId_;
        if (selected || CheckCollisionPointRec(GetMousePosition(), r)) DrawRectangleRounded(r, 0.12f, 5, kHover);
        const int itemRight = static_cast<int>(ui.typeColumnX - 10.0f);
        BeginScissorMode(static_cast<int>(r.x + 6.0f), static_cast<int>(r.y),
                         std::max(1, itemRight - static_cast<int>(r.x + 6.0f)), static_cast<int>(r.height));
        UITheme::drawText(rows[i].term.c_str(), static_cast<int>(r.x + 6), static_cast<int>(r.y + 2), 18, kText);
        const auto& row = rows[i];
        const std::string subtitle = row.reading.empty() ? row.meaning : row.reading + " · " + row.meaning;
        UITheme::drawText(subtitle.c_str(), static_cast<int>(r.x + 6), static_cast<int>(r.y + 23), 14, kMuted);
        EndScissorMode();
        const Rectangle typeBadge{ui.typeColumnX, r.y + (r.height - 26.0f) * 0.5f, ui.typeBadgeWidth, 26.0f};
        const Rectangle levelBadge{ui.levelColumnX, r.y + (r.height - 26.0f) * 0.5f, ui.levelBadgeWidth, 26.0f};
        drawBadge(typeBadge, typeName(rows[i].type), typeBadgeColor(rows[i].type));
        drawBadge(levelBadge, levelLabel(rows[i].levelId, levels), Color{244, 239, 243, 255});
    }
    if (rows.size() > ui.visibleRows && ui.rowsClip.height > 0.0f) {
        const Rectangle track{ui.list.x + ui.list.width - 8.0f, ui.rowsClip.y, 3.0f, ui.rowsClip.height};
        DrawRectangleRounded(track, 0.5f, 4, Color{239, 224, 230, 255});
        const float ratio = static_cast<float>(ui.visibleRows) / static_cast<float>(rows.size());
        const float thumbHeight = std::max(20.0f, track.height * ratio);
        const float maxOffset = static_cast<float>(rows.size() - ui.visibleRows);
        const float thumbY = track.y + (track.height - thumbHeight) *
            (maxOffset > 0.0f ? static_cast<float>(rowOffset_) / maxOffset : 0.0f);
        DrawRectangleRounded({track.x, thumbY, track.width, thumbHeight}, 0.5f, 4, kBorder);
    }
    if (rows.empty() && !searchError) UITheme::drawWrapped("No matching resources. Try another search or filter.",
        static_cast<int>(ui.list.x + 14), static_cast<int>(ui.list.y + 76), static_cast<int>(ui.list.width - 28), 14, kMuted, 3);
}

void ResourcesScreen::drawResourceDetails(const ResourceUiLayout& ui,
                                          const std::optional<ResourceDetail>& detail,
                                          const AccountSummary& account, const std::string& message) const {
    UITheme::drawText("Resource details", static_cast<int>(ui.detail.x + 14), static_cast<int>(ui.detail.y + 10), 22, kText);
    float detailY = ui.detail.y + 46.0f;
    if (detail) {
        const ResourceId id = std::visit([](const auto& item){ return item.getId(); }, *detail);
        const ResourceType type = std::visit([](const auto& item){ return item.getType(); }, *detail);
        UITheme::drawText(TextFormat("%s | %s", id.getId().c_str(), typeName(type)),
                          static_cast<int>(ui.detail.x + 14), static_cast<int>(detailY), 15, kMuted);
        detailY += 24.0f;
        const std::string primary = detailTitle(*detail);
        const float titleWidth = std::max(1.0f, ui.detail.width - 28.0f);
        const int primarySize = static_cast<int>(std::clamp(ui.detail.width * 0.08f, 38.0f, 44.0f));
        const int titleLines = estimatedLines(primary, primarySize, titleWidth, 2);
        UITheme::drawWrapped(primary, static_cast<int>(ui.detail.x + 14),
                             static_cast<int>(detailY), static_cast<int>(titleWidth), primarySize, UITheme::strongText, 2);
        detailY += static_cast<float>(titleLines * (primarySize + std::max(4, primarySize / 3)) + 8);
        const float detailBottom = account.role == UserRole::LEARNER ? ui.favorite.y - 8.0f : ui.add.y - 8.0f;
        const float detailsContentBottom = message.empty() ? detailBottom : detailBottom - 48.0f;
        const Rectangle detailsClip{ui.detail.x + 8.0f, detailY, ui.detail.width - 16.0f,
                                    std::max(0.0f, detailsContentBottom - detailY)};
        BeginScissorMode(static_cast<int>(detailsClip.x), static_cast<int>(detailsClip.y),
                         static_cast<int>(detailsClip.width), static_cast<int>(detailsClip.height));
        for (const DetailField& field : detailLines(*detail)) {
            if (field.value.empty() || detailY >= detailBottom) continue;
            if (field.example) {
                const float valueWidth = std::max(1.0f, ui.detail.width - 44.0f);
                const int lines = estimatedLines(field.value, 19, valueWidth, 4);
                const float panelHeight = 12.0f + 20.0f + 4.0f + lines * 25.0f + 10.0f;
                const Rectangle examplePanel{ui.detail.x + 10.0f, detailY, ui.detail.width - 20.0f, panelHeight};
                DrawRectangleRounded(examplePanel, 0.08f, 8, Color{255, 239, 245, 255});
                DrawRectangleRoundedLinesEx(examplePanel, 0.08f, 8, 1.0f, Color{226, 190, 204, 255});
                UITheme::drawText(field.label.c_str(), static_cast<int>(examplePanel.x + 12.0f),
                                  static_cast<int>(examplePanel.y + 8.0f), 18, kMuted);
                UITheme::drawWrapped(field.value, static_cast<int>(examplePanel.x + 12.0f),
                                     static_cast<int>(examplePanel.y + 32.0f),
                                     static_cast<int>(examplePanel.width - 24.0f), 19, kText, 4);
                detailY += panelHeight + 14.0f;
            } else {
                const float valueWidth = std::max(1.0f, ui.detail.width - 28.0f);
                const int lines = estimatedLines(field.value, 19, valueWidth, 2);
                UITheme::drawText(field.label.c_str(), static_cast<int>(ui.detail.x + 14.0f),
                                  static_cast<int>(detailY), 18, kMuted);
                UITheme::drawWrapped(field.value, static_cast<int>(ui.detail.x + 14.0f),
                                     static_cast<int>(detailY + 23.0f), static_cast<int>(valueWidth), 19, kText, 2);
                detailY += 23.0f + std::max(25.0f, lines * 25.0f) + 14.0f;
            }
        }
        EndScissorMode();
    } else UITheme::drawWrapped("Select a result to see its full details.", static_cast<int>(ui.detail.x + 14),
                                static_cast<int>(detailY), static_cast<int>(ui.detail.width - 28), 16, kMuted, 3);
}

void ResourcesScreen::drawResourceActions(const ResourceUiLayout& ui, const AccountSummary& account,
                                          const std::optional<bool>& selectedFavorited,
                                          const std::string& message, bool messageError) const {
    if (account.role == UserRole::ADMIN) {
        button(ui.add, "Add", true);
        button(ui.edit, "Edit");
        button(ui.remove, "Delete");
    }
    const bool favoriteKnown = selectedFavorited.has_value() && hasSelected_;
    if (!hasSelected_) {
        UITheme::drawWrapped("Select a resource to view its details and manage favorites.",
                             static_cast<int>(ui.selectionHint.x + 8.0f),
                             static_cast<int>(ui.selectionHint.y + 4.0f),
                             static_cast<int>(ui.selectionHint.width - 16.0f), 15, kMuted, 2);
    } else {
        const char* favoriteLabel = !favoriteKnown ? "Favorite status unavailable" :
                                    *selectedFavorited ? "In Favorites" : "Add to Favorites";
        button(ui.favorite, favoriteLabel, favoriteKnown && *selectedFavorited, favoriteKnown);
    }
    if (!message.empty()) {
        const float detailBottom = account.role == UserRole::LEARNER ? ui.favorite.y - 8.0f : ui.add.y - 8.0f;
        UITheme::drawStatusBanner({ui.detail.x + 10.0f, detailBottom - 42.0f,
                                   ui.detail.width - 20.0f, 36.0f}, message, messageError);
    }
}

void ResourcesScreen::drawResultRange(const ResourceUiLayout& ui, const std::vector<SearchRow>& rows, bool searchError) const {
    const std::size_t last = std::min(rowOffset_ + ui.visibleRows, rows.size());
    const std::string range = rows.empty() ? (searchError ? "Check search" : "0 results") :
        std::to_string(rowOffset_ + 1) + "-" + std::to_string(last) + " / " + std::to_string(rows.size());
    UITheme::drawText(range.c_str(), static_cast<int>(ui.list.x + 14), static_cast<int>(ui.list.y + ui.list.height - 24), 15, kMuted);
    if (rows.empty()) button(ui.browse, "Browse all");
    else if (rows.size() > ui.visibleRows) {
        button(ui.previous, "Prev", false, rowOffset_ > 0);
        button(ui.next, "Next", false, last < rows.size());
    }
}

void ResourcesScreen::drawResourceForm(const std::vector<Level>& levels, const std::string& error) const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{46,31,38,150});
    const auto dialog = formDialog();
    DrawRectangleRounded(dialog, 0.04f, 10, RAYWHITE);
    DrawRectangleRoundedLinesEx(dialog, 0.04f, 10, 1.5f, kBorder);
    UITheme::drawText(editing_ ? "Edit resource" : "Add resource", static_cast<int>(dialog.x+24),
                      static_cast<int>(dialog.y+22), 22, kText);
    button(formTypeButton(), typeName(form_.type), false, !editing_);
    const std::string level = formLevelIndex_ < levels.size() ? levels[formLevelIndex_].getName() : "No levels available";
    button(formLevelButton(), level.c_str(), false, !levels.empty());
    for (std::size_t i = 0; i < form_.fields.size(); ++i) {
        const auto& draft = form_.fields[i];
        const auto bounds = formField(i, form_.fields.size());
        if (draft.id == Field::PartOfSpeech) {
            const std::string label = std::string(draft.label) + " ? " + draft.value;
            button(bounds, label.c_str(), focusedField_ == i);
        } else {
            field(bounds, draft.label, draft.value, focusedField_ == i);
        }
    }
    if (!error.empty())
        UITheme::drawStatusBanner({dialog.x+24,dialog.y+dialog.height-99,dialog.width-48,36}, error, true);
    button(formSaveButton(), editing_ ? "Save" : "Create", true, !levels.empty());
    button(formCancelButton(), "Cancel");
}

void ResourcesScreen::drawDeleteConfirmation() const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{46,31,38,150});
    const Rectangle d = UiLayout::centeredDialog(460.0f, 220.0f, 20.0f);
    DrawRectangleRounded(d, 0.05f, 10, RAYWHITE);
    DrawRectangleRoundedLinesEx(d, 0.05f, 10, 1.5f, kBorder);
    UITheme::drawText("Delete this resource?", static_cast<int>(d.x + 24), static_cast<int>(d.y + 24), 20, kText);
    UITheme::drawWrapped("This action cannot be undone.", static_cast<int>(d.x + 24), static_cast<int>(d.y + 66), static_cast<int>(d.width - 48), 15, kMuted, 2);
    button({d.x + d.width - 236, d.y + d.height - 58, 104, 38}, "Delete", true);
    button({d.x + d.width - 120, d.y + d.height - 58, 96, 38}, "Cancel");
}

void ResourcesScreen::draw(const std::vector<SearchRow>& rows, const std::vector<Level>& levels,
                           const std::optional<ResourceDetail>& detail, const AccountSummary& account,
                           const std::string& message, bool messageError,
                           const std::optional<bool>& selectedFavorited) const {
    const ResourceUiLayout ui = layout(account.role, hasSelected_);
    drawSearchSection(ui, levels);
    drawPanelHeaders(ui);
    drawResourceList(ui, rows, levels, messageError);
    drawResourceDetails(ui, detail, account, message);
    drawResourceActions(ui, account, selectedFavorited, message, messageError);
    drawResultRange(ui, rows, messageError);
    if (formOpen_) drawResourceForm(levels, messageError ? message : std::string{});
    if (confirmDelete_) drawDeleteConfirmation();
}

void ResourcesScreen::setEditing(const ResourceDetail& detail, const std::vector<Level>& levels) {
    closeForm();
    formOpen_ = editing_ = hasSelected_ = true;
    std::visit([&](const auto& item) {
        using T = std::decay_t<decltype(item)>;
        selectedId_ = item.getId();
        form_.type = item.getType();
        form_.levelId = item.getLevelId();
        form_.fields = resourceFields(form_.type);
        auto set = [&](Field id, std::string value) { fieldValue(form_.fields, id) = std::move(value); };
        set(Field::Meaning, item.getMeaning());
        if constexpr (std::is_same_v<T, Vocabulary>) {
            set(Field::Word, item.getWord()); set(Field::Reading, item.getReading());
            set(Field::PartOfSpeech, partOfSpeechToken(item.getPartOfSpeech())); set(Field::Example, item.getExample());
        } else if constexpr (std::is_same_v<T, Grammar>) {
            set(Field::Pattern, item.getPattern()); set(Field::Structure, item.getStructure());
            set(Field::Usage, item.getUsage()); set(Field::Example, item.getExample()); set(Field::Note, item.getNote());
        } else {
            set(Field::Character, item.getCharacter()); set(Field::Strokes, std::to_string(item.getStrokeCount()));
            set(Field::Onyomi, item.getOnyomi()); set(Field::Kunyomi, item.getKunyomi()); set(Field::Example, item.getExamples());
        }
    }, detail);
    const auto level = std::find_if(levels.begin(), levels.end(), [&](const Level& item) {
        return item.getId() == form_.levelId;
    });
    formLevelIndex_ = static_cast<std::size_t>(std::distance(levels.begin(), level));
}

void ResourcesScreen::setSearch(const SearchOptions& options, bool resetSelection) {
    search_ = options;
    queryFocused_ = false;
    if (resetSelection) clearSelection();
}
void ResourcesScreen::clearSelection() noexcept {
    rowOffset_ = 0;
    hasSelected_ = false;
}
void ResourcesScreen::closeForm() noexcept {
    form_.fields.clear();
    formOpen_ = editing_ = confirmDelete_ = false;
    focusedField_ = formLevelIndex_ = 0;
}
void ResourcesScreen::reset() noexcept {
    search_ = {};
    clearSelection();
    closeForm();
    queryFocused_ = false;
}

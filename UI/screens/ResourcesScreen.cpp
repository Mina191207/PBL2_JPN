#include "ResourcesScreen.h"
#include "../UITheme.h"
#include "../UiLayout.h"
#include "../Utf8.h"
#include <algorithm>
#include <cmath>
#include <type_traits>

struct ResourceUiLayout {
    Rectangle content;
    Rectangle query;
    Rectangle search;
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
    const float filterW = (innerW - filterGap * 2.0f) / 3.0f;
    result.type = {x, y + 106.0f, filterW, 36.0f};
    result.level = {x + filterW + filterGap, y + 106.0f, filterW, 36.0f};
    result.sort = {x + (filterW + filterGap) * 2.0f, y + 106.0f, filterW, 36.0f};
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
Rectangle partOfSpeechOption(std::size_t index) {
    const Rectangle field = formField(3, 5);
    return {field.x, field.y + field.height + 3.0f + static_cast<float>(index) * 30.0f, field.width, 30.0f};
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

std::string encode(int cp) {
    return Utf8::encode(cp);
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
        if (cp >= 32 && cp != 127 && value.size() < static_cast<std::size_t>(limit)) value += encode(cp);
    }
}
ResourcesEvent ResourcesScreen::searchEvent(const std::vector<Level>& levels) const {
    SearchOptions options;
    options.query = query_;
    if (!allTypes_) options.type = type_;
    if (!levels.empty() && levelIndex_ > 0 && levelIndex_ <= levels.size())
        options.levelId = levels[levelIndex_ - 1].getId();
    options.sort = sort_;
    ResourcesEvent event;
    event.type = ResourcesEvent::Type::SEARCH;
    event.search = std::move(options);
    return event;
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
    if (const auto event = updateSearchControls(levels, ui)) return *event;
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
    if (IsKeyPressed(KEY_ESCAPE)) { formOpen_ = false; return {ResourcesEvent::Type::CANCEL_FORM}; }
    if (!editing_ && click(formTypeButton())) { form_.type = static_cast<ResourceType>((static_cast<int>(form_.type)+1)%3); form_.fields = {}; partOfSpeechOpen_=false; if(form_.type==ResourceType::VOCABULARY)form_.fields[3]="NOUN"; return {}; }
    if (form_.type == ResourceType::VOCABULARY && partOfSpeechOpen_) {
        static const char* values[] = {"NOUN", "VERB", "I_ADJ", "NA_ADJ", "ADV", "OTHER"};
        for (std::size_t i = 0; i < 6; ++i) {
            if (click(partOfSpeechOption(i))) {
                form_.fields[3] = values[i];
                focusedField_ = 3;
                partOfSpeechOpen_ = false;
                return {};
            }
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), formField(3, 5)))
            partOfSpeechOpen_ = false;
    }
    if (form_.type == ResourceType::VOCABULARY && click(formField(3, 5))) {
        focusedField_ = 3;
        partOfSpeechOpen_ = !partOfSpeechOpen_;
        return {};
    }
    if (click(formLevelButton()) && !levels.empty()) {
        formLevelIndex_ = (formLevelIndex_ + 1) % levels.size();
        form_.levelId = levels[formLevelIndex_].getId();
        return {};
    }
    const std::size_t visible = form_.type == ResourceType::VOCABULARY ? 5 : 6;
    for (std::size_t i = 0; i < visible; ++i)
        if (click(formField(i, visible))) focusedField_ = i;
    if (IsKeyPressed(KEY_TAB)) {
        focusedField_ = (focusedField_ + 1) % visible;
    }
    const bool partOfSpeechFocused = form_.type == ResourceType::VOCABULARY && focusedField_ == 3;
    if (partOfSpeechFocused && IsKeyPressed(KEY_ENTER)) partOfSpeechOpen_ = !partOfSpeechOpen_;
    else if (focusedField_ < visible && !partOfSpeechFocused) appendInput(form_.fields[focusedField_], 220);
    if (click(formSaveButton())) {
        if (levels.empty()) return {};
        formOpen_ = false;
        ResourcesEvent event;
        event.type = editing_ ? ResourcesEvent::Type::UPDATE : ResourcesEvent::Type::CREATE;
        event.id = selectedId_;
        event.form = form_;
        if (formLevelIndex_ < levels.size())
            event.form.levelId = levels[formLevelIndex_].getId();
        return event;
    }
    if (click(formCancelButton())) { formOpen_ = false; return {ResourcesEvent::Type::CANCEL_FORM}; }
    return {};
}

std::optional<ResourcesEvent> ResourcesScreen::updateSearchControls(const std::vector<Level>& levels,
                                                                    const ResourceUiLayout& ui) {
    if (IsKeyPressed(KEY_ESCAPE)) query_.clear();
    if (click(ui.query)) queryFocused_ = true;
    else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), ui.query)) queryFocused_ = false;
    if (queryFocused_) appendInput(query_, 128);
    if (click(ui.search) || (queryFocused_ && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))) { hasSelected_=false;rowOffset_=0;return searchEvent(levels); }
    if (click(ui.type)) {
        if (allTypes_) { allTypes_ = false; type_ = ResourceType::VOCABULARY; }
        else if (type_ == ResourceType::VOCABULARY) type_ = ResourceType::GRAMMAR;
        else if (type_ == ResourceType::GRAMMAR) type_ = ResourceType::KANJI;
        else allTypes_ = true;
        return searchEvent(levels);
    }
    if (click(ui.level)) { levelIndex_ = levels.empty() ? 0 : (levelIndex_ + 1) % (levels.size() + 1); return ResourcesEvent{}; }
    if (click(ui.sort)) { sort_ = static_cast<SearchSort>((static_cast<int>(sort_) + 1) % 6); return ResourcesEvent{}; }

    return std::nullopt;
}

std::optional<ResourcesEvent> ResourcesScreen::updateResourceSelection(const std::vector<SearchRow>& rows,
                                                                        const ResourceUiLayout& ui) {
    const Rectangle listArea = ui.list;
    if(CheckCollisionPointRec(GetMousePosition(),listArea)){
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
            formOpen_ = true; editing_ = false; partOfSpeechOpen_ = false; focusedField_ = 0; form_ = {}; form_.type = type_;
            if (form_.type == ResourceType::VOCABULARY) form_.fields[3] = "NOUN";
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
    field(ui.query, "Search term", query_, queryFocused_);
    button(ui.search, "Search", true);
    const char* typeLabel = allTypes_ ? "All types" : typeName(type_);
    std::string selectedLevelLabel = "All levels";
    if (levelIndex_ > 0 && levelIndex_ <= levels.size()) selectedLevelLabel = levels[levelIndex_-1].getName();
    button(ui.type, typeLabel);
    button(ui.level, selectedLevelLabel.c_str());
    static const char* sorts[] = {"Relevant", "ID", "Level +", "Level -", "Term A-Z", "Term Z-A"};
    button(ui.sort, sorts[static_cast<int>(sort_)]);
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
                                       const std::vector<Level>& levels) const {
    BeginScissorMode(static_cast<int>(ui.rowsClip.x), static_cast<int>(ui.rowsClip.y),
                     static_cast<int>(ui.rowsClip.width), static_cast<int>(ui.rowsClip.height));
    for (std::size_t i = rowOffset_; i < rows.size() && i < rowOffset_ + ui.visibleRows; ++i) {
        const Rectangle r = bounds(ui.list.x + 8.0f, ui.rowsClip.y + static_cast<float>(i-rowOffset_)*ui.rowHeight,
                                   ui.list.width - 24.0f, ui.rowHeight - 3.0f);
        const bool selected = rows[i].id.getId() == selectedId_.getId();
        if (selected || CheckCollisionPointRec(GetMousePosition(), r)) DrawRectangleRounded(r, 0.12f, 5, kHover);
        const int labelY = static_cast<int>(r.y + (r.height - 19) * 0.5f);
        const int itemRight = static_cast<int>(ui.typeColumnX - 10.0f);
        BeginScissorMode(static_cast<int>(r.x + 6.0f), static_cast<int>(r.y),
                         std::max(1, itemRight - static_cast<int>(r.x + 6.0f)), static_cast<int>(r.height));
        UITheme::drawText(rows[i].term.c_str(), static_cast<int>(r.x + 6), labelY, 19, kText);
        EndScissorMode();
        const Rectangle typeBadge{ui.typeColumnX, r.y + (r.height - 26.0f) * 0.5f, ui.typeBadgeWidth, 26.0f};
        const Rectangle levelBadge{ui.levelColumnX, r.y + (r.height - 26.0f) * 0.5f, ui.levelBadgeWidth, 26.0f};
        drawBadge(typeBadge, typeName(rows[i].type), typeBadgeColor(rows[i].type));
        drawBadge(levelBadge, levelLabel(rows[i].levelId, levels), Color{244, 239, 243, 255});
    }
    EndScissorMode();
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
    if (rows.empty()) UITheme::drawWrapped("No matching resources. Try another search or filter.",
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

void ResourcesScreen::drawResultRange(const ResourceUiLayout& ui, const std::vector<SearchRow>& rows) const {
    const std::size_t last = std::min(rowOffset_ + ui.visibleRows, rows.size());
    const std::string resultRange=rows.empty()?"0 results":"Showing "+std::to_string(rowOffset_+1)+"-"+std::to_string(last)+" of "+std::to_string(rows.size());
    UITheme::drawText(resultRange.c_str(), static_cast<int>(ui.list.x + 14), static_cast<int>(ui.list.y + ui.list.height - 24), 15, kMuted);
}

void ResourcesScreen::drawResourceForm(const std::vector<Level>& levels) const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{46,31,38,150});
    const Rectangle dialog = formDialog();
    DrawRectangleRounded(dialog, 0.04f, 10, RAYWHITE);
    DrawRectangleRoundedLinesEx(dialog, 0.04f, 10, 1.5f, kBorder);
    UITheme::drawText(editing_ ? "Edit resource" : "Add resource", static_cast<int>(dialog.x + 24), static_cast<int>(dialog.y + 22), 22, kText);
    button(formTypeButton(), typeName(form_.type));
    std::string levelName = formLevelIndex_ < levels.size() ? levels[formLevelIndex_].getName() : "No levels available";
    button(formLevelButton(), levelName.c_str());
    const char* vocabNames[8] = {"Word", "Reading", "Meaning", "Part of speech", "Example", "", "", ""};
    const char* grammarNames[8] = {"Pattern", "Meaning", "Structure", "Usage", "Example", "Note", "", ""};
    const char* kanjiNames[8] = {"Character", "Meaning", "Stroke count (1-255)", "Onyomi", "Kunyomi", "Examples", "", ""};
    const char** names = form_.type == ResourceType::VOCABULARY ? vocabNames : form_.type == ResourceType::GRAMMAR ? grammarNames : kanjiNames;
    const int count = form_.type == ResourceType::VOCABULARY ? 5 : 6;
    for (int i=0; i<count; ++i) {
        std::string value = form_.fields[i];
        const Rectangle bounds = formField(static_cast<std::size_t>(i), static_cast<std::size_t>(count));
        if (form_.type == ResourceType::VOCABULARY && i == 3) {
            const std::string label = "Part of speech: " + value + (partOfSpeechOpen_ ? "  (choose below)" : "  (click to choose)");
            button(bounds, label.c_str());
        } else field(bounds, names[i], value, focusedField_==static_cast<std::size_t>(i));
    }
    if (form_.type == ResourceType::VOCABULARY && partOfSpeechOpen_) {
        static const char* values[] = {"NOUN", "VERB", "I_ADJ", "NA_ADJ", "ADV", "OTHER"};
        for (std::size_t i = 0; i < 6; ++i) button(partOfSpeechOption(i), values[i], form_.fields[3] == values[i]);
    }
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
    drawResourceList(ui, rows, levels);
    drawResourceDetails(ui, detail, account, message);
    drawResourceActions(ui, account, selectedFavorited, message, messageError);
    drawResultRange(ui, rows);
    if (formOpen_) drawResourceForm(levels);
    if (confirmDelete_) drawDeleteConfirmation();
}

void ResourcesScreen::setEditing(const ResourceDetail& detail, const std::vector<Level>& levels) {
    formOpen_ = true; editing_ = true; partOfSpeechOpen_ = false; hasSelected_ = true; focusedField_ = 0;
    std::visit([&](const auto& item) {
        selectedId_ = item.getId(); form_.levelId = item.getLevelId(); form_.type = item.getType();
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T,Vocabulary>) {
            form_.fields[0]=item.getWord(); form_.fields[1]=item.getReading(); form_.fields[2]=item.getMeaning();
            form_.fields[3]=partOfSpeechToken(item.getPartOfSpeech()); form_.fields[4]=item.getExample();
        } else if constexpr (std::is_same_v<T,Grammar>) {
            form_.fields[0]=item.getPattern(); form_.fields[1]=item.getMeaning(); form_.fields[2]=item.getStructure();
            form_.fields[3]=item.getUsage(); form_.fields[4]=item.getExample(); form_.fields[5]=item.getNote();
        } else {
            form_.fields[0]=item.getCharacter(); form_.fields[1]=item.getMeaning(); form_.fields[2]=std::to_string(item.getStrokeCount());
            form_.fields[3]=item.getOnyomi(); form_.fields[4]=item.getKunyomi(); form_.fields[5]=item.getExamples();
        }
    }, detail);
    const auto level = std::find_if(levels.begin(), levels.end(), [&](const Level& item) {
        return item.getId() == form_.levelId;
    });
    formLevelIndex_ = level == levels.end() ? levels.size() : static_cast<std::size_t>(std::distance(levels.begin(), level));
}
void ResourcesScreen::reset() noexcept {
    query_.clear(); allTypes_=true; levelIndex_=0; sort_=SearchSort::RELEVANCE;rowOffset_=0;hasSelected_=false;queryFocused_=false;
    formOpen_=false; editing_=false; partOfSpeechOpen_=false; confirmDelete_=false; selectedId_=ResourceId("R001"); formLevelIndex_=0;
    hasSelected_=false;
}

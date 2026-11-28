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
    Rectangle banner, detailClip;
    Rectangle rowsClip;
    Rectangle browse, previous, next;
    float rowHeight;
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



const char* favoriteLabel(const std::optional<bool>& selected) {
    if (!selected) return "Favorite unavailable";
    return *selected ? "In Favorites" : "Add to Favorites";
}

ResourceUiLayout layout(bool hasSelected, const std::optional<bool>& favorite = {},
                        const std::string& message = {}) {
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
    const float bottom = result.content.y + result.content.height;
    const float bodySpace = bottom - 46.0f - (y + 164.0f);
    result.banner = {x, y + 150.0f, innerW, std::min(UITheme::bannerHeight(innerW, message), bodySpace - 141.0f)};
    const float bodyY = y + 164.0f + result.banner.height;
    const float bodyH = bodySpace - result.banner.height;
    const float gap = 14.0f;
    const float listW = (innerW - gap) * 0.44f;
    result.list = {x, bodyY, listW, bodyH};
    result.detail = {x + listW + gap, bodyY, innerW - listW - gap, bodyH};
    result.detailClip = {result.detail.x + 8.0f, bodyY + 38.0f, result.detail.width - 16.0f, bodyH - 46.0f};
    result.rowHeight = 65.0f;
    const float rowsTop = result.list.y + 38.0f;
    const float footerTop = result.list.y + result.list.height - 34.0f;
    result.next = {x + listW - 47.0f, footerTop + 4.0f, 38.0f, 26.0f};
    result.previous = {result.next.x - 42.0f, result.next.y, 38.0f, 26.0f};
    result.browse = {x + listW - 116.0f, result.next.y, 108.0f, 26.0f};
    result.rowsClip = {result.list.x + 4.0f, rowsTop, result.list.width - 18.0f,
                       std::max(0.0f, footerTop - rowsTop - 4.0f)};
    result.visibleRows = std::min<std::size_t>(8, std::max<std::size_t>(1,
        static_cast<std::size_t>(result.rowsClip.height / result.rowHeight)));
    const float actionY = bottom - 38.0f;
    const float addW = UITheme::buttonWidth("Add") + 16.0f;
    const float editW = UITheme::buttonWidth("Edit") + 16.0f;
    const float removeW = UITheme::buttonWidth("Delete") + 16.0f;
    result.add = {x, actionY, addW, 34.0f};
    result.edit = {x + addW + 8.0f, actionY, editW, 34.0f};
    result.remove = {result.edit.x + editW + 8.0f, actionY, removeW, 34.0f};
    if (hasSelected) {
        const float width = UITheme::buttonWidth(favoriteLabel(favorite)) + 16.0f;
        result.favorite = {x + innerW - width, actionY, width, 34.0f};
    }
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

struct DetailField { std::string label; std::string value;  };

std::vector<DetailField> detailLines(const ResourceDetail& detail) {
    return std::visit([](const auto& item) -> std::vector<DetailField> {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, Vocabulary>) {
            return {{"Reading", item.getReading()}, {"Meaning", item.getMeaning()},
                    {"Part of speech", partOfSpeechToken(item.getPartOfSpeech())},
                    {"Example", item.getExample()}};
        } else if constexpr (std::is_same_v<T, Grammar>) {
            return {{"Meaning", item.getMeaning()}, {"Structure", item.getStructure()},
                    {"Usage", item.getUsage()}, {"Example", item.getExample()}, {"Note", item.getNote()}};
        } else {
            return {{"Meaning", item.getMeaning()}, {"Strokes", std::to_string(item.getStrokeCount())},
                    {"Onyomi", item.getOnyomi()}, {"Kunyomi", item.getKunyomi()},
                    {"Examples", item.getExamples()}};
        }
    }, detail);
}

std::string levelLabel(const LevelId& id, const std::vector<Level>& levels) {
    const auto found = std::find_if(levels.begin(), levels.end(), [&](const Level& level) { return level.getId() == id; });
    return found == levels.end() ? id.getId() : found->getName();
}

struct DetailBlock {
    std::string label;
    UITheme::WrappedText text;
};

std::vector<DetailBlock> detailBlocks(const ResourceDetail& detail, float width) {
    const auto id = std::visit([](const auto& item) { return item.getId(); }, detail);
    const auto type = std::visit([](const auto& item) { return item.getType(); }, detail);
    std::vector<DetailBlock> blocks{
        {{}, UITheme::wrap(id.getId() + " | " + typeName(type), width, 15)},
        {{}, UITheme::wrap(detailTitle(detail), width, 30)}
    };
    for (const auto& field : detailLines(detail))
        if (!field.value.empty()) blocks.push_back({field.label, UITheme::wrap(field.value, width, 18)});
    return blocks;
}

float detailHeight(const std::vector<DetailBlock>& blocks) {
    float height = 0;
    for (const auto& block : blocks) height += block.text.height() + (block.label.empty() ? 6.0f : 28.0f);
    return height;
}

Rectangle resultRow(const ResourceUiLayout& ui, std::size_t index) {
    const float y = ui.rowsClip.y + static_cast<float>(index) * ui.rowHeight;
    return {ui.list.x + 8.0f, y, ui.rowsClip.width - 8.0f,
            std::max(0.0f, std::min(ui.rowHeight - 3.0f, ui.rowsClip.y + ui.rowsClip.height - y))};
}

}

bool ResourcesScreen::click(Rectangle r) {
    return UiInput::click(r);
}
void ResourcesScreen::controlButton(int control, Rectangle r, const char* label, bool primary, bool enabled) const {
    UITheme::drawButton(r, label, primary, enabled, false, !modalOpen() && focus_.on(control));
}

bool ResourcesScreen::tab(UserRole role, std::size_t count, const std::optional<bool>& selectedFavorited,
                          const std::string& message) {
    const auto ui = layout(hasSelected_, selectedFavorited, message);
    rowOffset_ = std::min(rowOffset_, count > ui.visibleRows ? count - ui.visibleRows : 0);
    std::vector<int> order{Query, Search, Mode, Type, LevelFilter, Sort};
    if (count) order.push_back(Results);
    else order.push_back(Browse);
    if (count > ui.visibleRows) {
        if (rowOffset_ > 0) order.push_back(Previous);
        if (rowOffset_ + ui.visibleRows < count) order.push_back(Next);
    }
    if (hasSelected_) order.push_back(Details);
    if (role == UserRole::ADMIN) {
        order.push_back(Add);
        if (hasSelected_) order.insert(order.end(), {Edit, Remove});
    }
    if (hasSelected_ && selectedFavorited.has_value()) order.push_back(Favorite);
    return focus_.move(order, false);
}

ResourcesEvent ResourcesScreen::update(const std::vector<SearchRow>& rows,
                                       const std::vector<Level>& levels,
                                       const AccountSummary& account,
                                       const std::optional<bool>& selectedFavorited,
                                       const std::optional<ResourceDetail>& detail, const std::string& message) {
    if (confirmDelete_) return updateDeleteConfirmation();
    if (formOpen_) return updateResourceForm(levels);
    const ResourceUiLayout ui = layout(hasSelected_, selectedFavorited, message);
    const float bannerHeight = UITheme::bannerHeight(ui.banner.width, message);
    bannerOffset_ = bannerHeight > ui.banner.height
        ? UiInput::scroll(ui.banner, bannerHeight, bannerOffset_, false) : 0.0f;
    if (rows.empty()) rowOffset_ = 0;
    else {
        const std::size_t maxOffset = rows.size() > ui.visibleRows ? rows.size() - ui.visibleRows : 0;
        rowOffset_ = std::min(rowOffset_, maxOffset);
    }
    if (const auto event = updateSearchControls(levels, rows.empty(), ui)) return *event;
    if (const auto event = updateResourceSelection(rows, ui)) return *event;
    if (detail) {
        if (click(ui.detailClip)) focus_.id = Details;
        const auto blocks = detailBlocks(*detail, ui.detailClip.width - 18.0f);
        detailOffset_ = UiInput::scroll(ui.detailClip, detailHeight(blocks), detailOffset_, focus_.on(Details));
    }
    return updateResourceActions(account, levels, selectedFavorited, ui);
}

ResourcesEvent ResourcesScreen::updateDeleteConfirmation() {
    const Rectangle d = UiLayout::centeredDialog(460.0f, 220.0f, 20.0f);
    const Rectangle yes{d.x + d.width - 236, d.y + d.height - 58, 104, 38};
    const Rectangle no{d.x + d.width - 120, d.y + d.height - 58, 96, 38};
    if (UiInput::tab()) { modalFocus_.move({0, 1}); return {}; }
    if (IsKeyPressed(KEY_ESCAPE) || modalFocus_.activate(1, no)) { confirmDelete_ = false; return {}; }
    if (modalFocus_.activate(0, yes)) {
        confirmDelete_ = false;
        focus_.id = Results;
        return {ResourcesEvent::Type::REMOVE, {}, selectedId_};
    }
    return {};
}

ResourcesEvent ResourcesScreen::updateResourceForm(const std::vector<Level>& levels) {
    using Event = ResourcesEvent::Type;
    const auto count = static_cast<int>(form_.fields.size());
    if (UiInput::tab()) {
        std::vector<int> order;
        if (!editing_) order.push_back(FormType);
        if (!levels.empty()) order.push_back(FormLevel);
        for (int id = 0; id < count; ++id) order.push_back(id);
        if (!levels.empty()) order.push_back(FormSave);
        order.push_back(FormCancel);
        modalFocus_.move(order);
        formCursor_.reset();
        return {};
    }
    if (IsKeyPressed(KEY_ESCAPE) || modalFocus_.activate(FormCancel, formCancelButton())) {
        closeForm();
        return {Event::CANCEL_FORM};
    }
    if (modalFocus_.activate(FormType, formTypeButton(), !editing_)) {
        form_.type = static_cast<ResourceType>((static_cast<int>(form_.type)+1)%3);
        form_.fields = resourceFields(form_.type);
        formCursor_.reset();
        return {Event::EDITED};
    }
    if (modalFocus_.activate(FormLevel, formLevelButton(), !levels.empty())) {
        formLevelIndex_ = (formLevelIndex_+1)%levels.size();
        form_.levelId = levels[formLevelIndex_].getId();
        return {Event::EDITED};
    }
    if (modalFocus_.activate(FormSave, formSaveButton(), !levels.empty()))
        return {editing_ ? Event::UPDATE : Event::CREATE, {}, selectedId_, form_};

    for (int i = 0; i < count; ++i) {
        auto& draft = form_.fields[i];
        const auto bounds = formField(i, count);
        if (draft.id == Field::PartOfSpeech) {
            if (modalFocus_.activate(i, bounds)) {
                cyclePartOfSpeech(draft.value);
                return {Event::EDITED};
            }
            continue;
        }
        if (UiInput::click(bounds) && !modalFocus_.on(i)) {
            modalFocus_.id = i;
            formCursor_.reset();
        }
        if (!modalFocus_.on(i)) continue;
        const auto edit = UiInput::text(draft.value, formCursor_, maxTxtFieldBytes);
        if (UiInput::enter() && !levels.empty())
            return {editing_ ? Event::UPDATE : Event::CREATE, {}, selectedId_, form_};
        if (edit.changed) return {Event::EDITED};
    }
    return {};
}

std::optional<ResourcesEvent> ResourcesScreen::updateSearchControls(const std::vector<Level>& levels, bool empty,
                                                                    const ResourceUiLayout& ui) {
    using Event = ResourcesEvent::Type;
    if (empty && focus_.activate(Browse, ui.browse)) {
        setSearch({});
        focus_.id = Query;
        return ResourcesEvent{Event::PREVIEW_SEARCH, search_};
    }
    if (UiInput::click(ui.query)) focus_.id = Query;
    if (focus_.activate(Search, ui.search)) return ResourcesEvent{Event::SUBMIT_SEARCH, search_};
    if (focus_.on(Query)) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            search_.query.clear();
            queryCursor_.reset();
            return ResourcesEvent{Event::PREVIEW_SEARCH, search_};
        }
        // Keep pasted separators and interior line breaks for Application to validate.
        const auto edit = UiInput::text(search_.query, queryCursor_, maxTxtFieldBytes,
            [](int cp) { return UiInput::printable(cp) || cp == '\r' || cp == '\n' || cp == '\t'; }, trimSearchQuery);
        if (edit.overLimit) return ResourcesEvent{Event::INPUT_LIMIT};
        if (UiInput::enter()) return ResourcesEvent{Event::SUBMIT_SEARCH, search_};
        if (edit.changed) return ResourcesEvent{Event::EDITED};
    }
    if (focus_.activate(Mode, ui.mode)) {
        search_.exactId = !search_.exactId;
        return ResourcesEvent{Event::PREVIEW_SEARCH, search_};
    }
    if (focus_.activate(Type, ui.type)) {
        if (!search_.type) search_.type = ResourceType::VOCABULARY;
        else if (*search_.type == ResourceType::KANJI) search_.type.reset();
        else search_.type = static_cast<ResourceType>(static_cast<int>(*search_.type) + 1);
        return ResourcesEvent{Event::PREVIEW_SEARCH, search_};
    }
    if (focus_.activate(LevelFilter, ui.level)) {
        const auto current = std::find_if(levels.begin(), levels.end(), [&](const Level& level) {
            return search_.levelId && level.getId() == *search_.levelId;
        });
        const auto next = current == levels.end() ? levels.begin() : current + 1;
        search_.levelId = next == levels.end() ? std::nullopt : std::optional<LevelId>(next->getId());
        return ResourcesEvent{Event::PREVIEW_SEARCH, search_};
    }
    if (focus_.activate(Sort, ui.sort)) {
        search_.sort = static_cast<SearchSort>((static_cast<int>(search_.sort) + 1) % 6);
        return ResourcesEvent{Event::PREVIEW_SEARCH, search_};
    }

    return std::nullopt;
}

std::optional<ResourcesEvent> ResourcesScreen::updateResourceSelection(const std::vector<SearchRow>& rows,
                                                                        const ResourceUiLayout& ui) {
    if (rows.size() > ui.visibleRows) {
        const auto maxOffset = rows.size() - ui.visibleRows;
        if (focus_.activate(Previous, ui.previous, rowOffset_ > 0) || (focus_.on(Results) && IsKeyPressed(KEY_PAGE_UP)))
            rowOffset_ = rowOffset_ > ui.visibleRows ? rowOffset_ - ui.visibleRows : 0;
        if (focus_.activate(Next, ui.next, rowOffset_ < maxOffset) || (focus_.on(Results) && IsKeyPressed(KEY_PAGE_DOWN)))
            rowOffset_ = std::min(maxOffset, rowOffset_ + ui.visibleRows);
    }
    if(CheckCollisionPointRec(GetMousePosition(),ui.list)){
        const float wheel=GetMouseWheelMove();
        if(wheel>0&&rowOffset_>0)--rowOffset_;
        if(wheel<0&&rowOffset_+ui.visibleRows<rows.size())++rowOffset_;
    }
    for (std::size_t i = 0; i < ui.visibleRows && rowOffset_+i < rows.size(); ++i) {
        const Rectangle row = resultRow(ui, i);
        if (click(row)) {
            focus_.id = Results;
            if (!hasSelected_ || selectedId_ != rows[rowOffset_ + i].id) detailOffset_ = 0;
            selectedId_ = rows[rowOffset_ + i].id;
            hasSelected_ = true;
            return ResourcesEvent{ResourcesEvent::Type::SELECT, {}, selectedId_};
        }
    }

    if (focus_.on(Results) && UiInput::rowKey()) {
        std::optional<std::size_t> current;
        if (hasSelected_) {
            const auto found = std::find_if(rows.begin(), rows.end(), [&](const SearchRow& row) { return row.id == selectedId_; });
            if (found != rows.end()) current = static_cast<std::size_t>(found - rows.begin());
        }
        if (const auto next = UiInput::row(rows.size(), current)) {
            if (!hasSelected_ || selectedId_ != rows[*next].id) detailOffset_ = 0;
            selectedId_ = rows[*next].id;
            hasSelected_ = true;
            UiInput::revealRow(*next, ui.visibleRows, rowOffset_);
            return ResourcesEvent{ResourcesEvent::Type::SELECT, {}, selectedId_};
        }
    }
    return std::nullopt;
}

ResourcesEvent ResourcesScreen::updateResourceActions(const AccountSummary& account, const std::vector<Level>& levels,
                                                       const std::optional<bool>& selectedFavorited,
                                                       const ResourceUiLayout& ui) {
    if (account.role == UserRole::ADMIN) {
        if (focus_.activate(Add, ui.add)) {
            closeForm();
            formOpen_ = true;
            form_.type = search_.type.value_or(ResourceType::VOCABULARY);
            form_.fields = resourceFields(form_.type);
            formLevelIndex_ = 0;
            if (!levels.empty()) form_.levelId = levels.front().getId();
            return {};
        }
        if (focus_.activate(Edit, ui.edit, hasSelected_)) {
            return {ResourcesEvent::Type::OPEN_EDIT, {}, selectedId_};
        }
        if (focus_.activate(Remove, ui.remove, hasSelected_)) {
            confirmDelete_ = true;
            modalFocus_.id = 1;
            return {};
        }
    }
    if (focus_.activate(Favorite, ui.favorite, hasSelected_ && selectedFavorited.has_value()))
        return {ResourcesEvent::Type::FAVORITE, {}, selectedId_};
    return {};
}

void ResourcesScreen::drawSearchSection(const ResourceUiLayout& ui, const std::vector<Level>& levels) const {
    const float pad = std::min(24.0f, std::max(16.0f, ui.content.width * 0.025f));
    const int x = static_cast<int>(ui.content.x + pad);
    UITheme::drawText("Find learning resources", x, static_cast<int>(ui.content.y + 10), 22, kText);
    UITheme::drawField(ui.query, search_.exactId ? "Resource ID (e.g. R001)" : "Search term",
                       search_.query, !modalOpen() && focus_.on(Query), queryCursor_.at(search_.query));
    controlButton(Search, ui.search, "Search", true);
    controlButton(Mode, ui.mode, search_.exactId ? "ID" : "Nội dung");
    const char* typeLabel = search_.type ? typeName(*search_.type) : "All types";
    const std::string selectedLevelLabel = search_.levelId ? levelLabel(*search_.levelId, levels) : "All levels";
    controlButton(Type, ui.type, typeLabel);
    controlButton(LevelFilter, ui.level, selectedLevelLabel.c_str());
    static const char* sorts[] = {"Relevant", "ID", "Level +", "Level -", "Term A-Z", "Term Z-A"};
    controlButton(Sort, ui.sort, sorts[static_cast<int>(search_.sort)]);
}

void ResourcesScreen::drawPanelHeaders(const ResourceUiLayout& ui) const {
    DrawRectangleRounded(ui.list, 0.04f, 8, Color{255, 252, 253, 255});
    DrawRectangleRoundedLinesEx(ui.list, 0.04f, 8, 1.0f, kBorder);
    DrawRectangleRounded(ui.detail, 0.04f, 8, Color{255, 252, 253, 255});
    DrawRectangleRoundedLinesEx(ui.detail, 0.04f, 8, 1.0f, kBorder);
    UITheme::drawText("Results", static_cast<int>(ui.list.x + 14), static_cast<int>(ui.list.y + 10), 22, kText);

}

void ResourcesScreen::drawResourceList(const ResourceUiLayout& ui, const std::vector<SearchRow>& rows,
                                       const std::vector<Level>& levels, bool searchError) const {
    const std::size_t first = std::min(rowOffset_, rows.size() > ui.visibleRows ? rows.size() - ui.visibleRows : 0);
    UITheme::beginClip(ui.rowsClip);
    for (std::size_t i = first; i < rows.size() && i < first + ui.visibleRows; ++i) {
        const Rectangle r = resultRow(ui, i - first);
        const auto& row = rows[i];
        const bool selected = hasSelected_ && row.id == selectedId_;
        if (selected || CheckCollisionPointRec(GetMousePosition(), r)) DrawRectangleRounded(r, 0.12f, 5, kHover);
        const int x = static_cast<int>(r.x + 6);
        const auto drawLine = [&](const std::string& value, int y, int size, Color color) {
            const auto label = UITheme::ellipsize(value, r.width - 12, size);
            UITheme::drawText(label.c_str(), x, static_cast<int>(r.y) + y, size, color);
        };
        drawLine(row.term, 2, 18, kText);
        drawLine(row.reading.empty() ? row.meaning : row.reading + " | " + row.meaning, 25, 15, kText);
        drawLine(std::string(typeName(row.type)) + " | " + levelLabel(row.levelId, levels) + " | " + row.id.getId(),
                 45, 13, kMuted);
    }
    EndScissorMode();
    if (rows.size() > ui.visibleRows && ui.rowsClip.height > 0.0f) {
        const Rectangle track{ui.list.x + ui.list.width - 8.0f, ui.rowsClip.y, 3.0f, ui.rowsClip.height};
        DrawRectangleRounded(track, 0.5f, 4, Color{239, 224, 230, 255});
        const float ratio = static_cast<float>(ui.visibleRows) / static_cast<float>(rows.size());
        const float thumbHeight = std::max(20.0f, track.height * ratio);
        const float maxOffset = static_cast<float>(rows.size() - ui.visibleRows);
        const float thumbY = track.y + (track.height - thumbHeight) *
            (maxOffset > 0.0f ? static_cast<float>(first) / maxOffset : 0.0f);
        DrawRectangleRounded({track.x, thumbY, track.width, thumbHeight}, 0.5f, 4, kBorder);
    }
    if (rows.empty() && !searchError) UITheme::drawWrapped("No matching resources. Try another search or filter.",
        static_cast<int>(ui.list.x + 14), static_cast<int>(ui.list.y + 76), static_cast<int>(ui.list.width - 28), 14, kMuted, 3);
}

void ResourcesScreen::drawResourceDetails(const ResourceUiLayout& ui,
                                          const std::optional<ResourceDetail>& detail) const {
    const Rectangle viewport = ui.detailClip;
    const int x = static_cast<int>(viewport.x + 6.0f);
    UITheme::drawText("Resource details", x, static_cast<int>(ui.detail.y + 10), 22, kText);
    if (!detail) {
        UITheme::drawWrapped("Select a result to see its full details.", x, static_cast<int>(viewport.y),
                             static_cast<int>(viewport.width - 18.0f), 16, kMuted);
        return;
    }
    const auto blocks = detailBlocks(*detail, viewport.width - 18.0f);
    const float height = detailHeight(blocks);
    const float offset = std::min(detailOffset_, std::max(0.0f, height - viewport.height));
    UITheme::beginClip(viewport);
    float y = viewport.y - offset;
    for (const auto& block : blocks) {
        if (!block.label.empty()) {
            UITheme::drawText(block.label.c_str(), x, static_cast<int>(y), 15, kMuted);
            y += 20.0f;
        }
        block.text.draw(x, static_cast<int>(y), kText);
        y += block.text.height() + (block.label.empty() ? 6.0f : 8.0f);
    }
    EndScissorMode();
    UITheme::drawScrollbar(viewport, height, offset);
    if (!modalOpen() && focus_.on(Details)) UITheme::drawFocus(viewport);
}

void ResourcesScreen::drawResourceActions(const ResourceUiLayout& ui, const AccountSummary& account,
                                          const std::optional<bool>& selectedFavorited) const {
    if (account.role == UserRole::ADMIN) {
        controlButton(Add, ui.add, "Add", true);
        controlButton(Edit, ui.edit, "Edit", false, hasSelected_);
        controlButton(Remove, ui.remove, "Delete", false, hasSelected_);
    }
    if (hasSelected_)
        controlButton(Favorite, ui.favorite, favoriteLabel(selectedFavorited),
                      selectedFavorited.value_or(false), selectedFavorited.has_value());
}

void ResourcesScreen::drawResultRange(const ResourceUiLayout& ui, const std::vector<SearchRow>& rows, bool searchError) const {
    const std::size_t first = std::min(rowOffset_, rows.size() > ui.visibleRows ? rows.size() - ui.visibleRows : 0);
    const std::size_t last = std::min(first + ui.visibleRows, rows.size());
    const std::string range = rows.empty() ? (searchError ? "Check search" : "0 results") :
        std::to_string(first + 1) + "-" + std::to_string(last) + "/" + std::to_string(rows.size());
    UITheme::drawText(range.c_str(), static_cast<int>(ui.list.x + 14), static_cast<int>(ui.list.y + ui.list.height - 24), 15, kMuted);
    if (rows.empty()) controlButton(Browse, ui.browse, "Browse all");
    else if (rows.size() > ui.visibleRows) {
        controlButton(Previous, ui.previous, "<", false, first > 0);
        controlButton(Next, ui.next, ">", false, last < rows.size());
    }
}

void ResourcesScreen::drawResourceForm(const std::vector<Level>& levels, const std::string& error) const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{46,31,38,150});
    const auto dialog = formDialog();
    DrawRectangleRounded(dialog, 0.04f, 10, RAYWHITE);
    DrawRectangleRoundedLinesEx(dialog, 0.04f, 10, 1.5f, kBorder);
    UITheme::drawText(editing_ ? "Edit resource" : "Add resource", static_cast<int>(dialog.x+24),
                      static_cast<int>(dialog.y+22), 22, kText);
    UITheme::drawButton(formTypeButton(), typeName(form_.type), false, !editing_, false, modalFocus_.on(FormType));
    const std::string level = formLevelIndex_ < levels.size() ? levels[formLevelIndex_].getName() : "No levels available";
    UITheme::drawButton(formLevelButton(), level.c_str(), false, !levels.empty(), false, modalFocus_.on(FormLevel));
    for (std::size_t i = 0; i < form_.fields.size(); ++i) {
        const auto& draft = form_.fields[i];
        const auto bounds = formField(i, form_.fields.size());
        if (draft.id == Field::PartOfSpeech) {
            const std::string label = std::string(draft.label) + " ? " + draft.value;
            UITheme::drawButton(bounds, label.c_str(), false, true, false, modalFocus_.on(static_cast<int>(i)));
        } else {
            const bool focused = modalFocus_.on(static_cast<int>(i));
            UITheme::drawField(bounds, draft.label, draft.value, focused,
                               focused ? formCursor_.at(draft.value) : draft.value.size());
        }
    }
    if (!error.empty()) {
        const float width = dialog.width - 48.0f;
        const float height = UITheme::bannerHeight(width, error);
        UITheme::drawStatusBanner({dialog.x + 24, formSaveButton().y - height - 8, width, height}, error, true);
    }
    UITheme::drawButton(formSaveButton(), editing_ ? "Save" : "Create", true, !levels.empty(), false, modalFocus_.on(FormSave));
    UITheme::drawButton(formCancelButton(), "Cancel", false, true, false, modalFocus_.on(FormCancel));
}

void ResourcesScreen::drawDeleteConfirmation() const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{46,31,38,150});
    const Rectangle d = UiLayout::centeredDialog(460.0f, 220.0f, 20.0f);
    DrawRectangleRounded(d, 0.05f, 10, RAYWHITE);
    DrawRectangleRoundedLinesEx(d, 0.05f, 10, 1.5f, kBorder);
    UITheme::drawText("Delete this resource?", static_cast<int>(d.x + 24), static_cast<int>(d.y + 24), 20, kText);
    UITheme::drawWrapped("This action cannot be undone.", static_cast<int>(d.x + 24), static_cast<int>(d.y + 66), static_cast<int>(d.width - 48), 15, kMuted, 2);
    UITheme::drawButton({d.x + d.width - 236, d.y + d.height - 58, 104, 38}, "Delete", true, true, false, modalFocus_.on(0));
    UITheme::drawButton({d.x + d.width - 120, d.y + d.height - 58, 96, 38}, "Cancel", false, true, false, modalFocus_.on(1));
}

void ResourcesScreen::draw(const std::vector<SearchRow>& rows, const std::vector<Level>& levels,
                           const std::optional<ResourceDetail>& detail, const AccountSummary& account,
                           const std::string& message, bool messageError,
                           const std::optional<bool>& selectedFavorited) const {
    const ResourceUiLayout ui = layout(hasSelected_, selectedFavorited, message);
    drawSearchSection(ui, levels);
    drawPanelHeaders(ui);
    drawResourceList(ui, rows, levels, messageError);
    if (!modalOpen() && focus_.on(Results)) UITheme::drawFocus(ui.rowsClip);
    drawResourceDetails(ui, detail);
    drawResourceActions(ui, account, selectedFavorited);
    if (!formOpen_) UITheme::drawStatusBanner(ui.banner, message, messageError, bannerOffset_);
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
    if (search_.query != options.query) queryCursor_.reset();
    search_ = options;
    if (resetSelection) clearSelection();
}
void ResourcesScreen::clearSelection() noexcept {
    rowOffset_ = 0;
    detailOffset_ = 0;
    bannerOffset_ = 0;
    hasSelected_ = false;
}
void ResourcesScreen::closeForm() noexcept {
    form_.fields.clear();
    formOpen_ = editing_ = confirmDelete_ = false;
    modalFocus_.id = 0;
    formCursor_.reset();
    formLevelIndex_ = 0;
}
void ResourcesScreen::reset() noexcept {
    search_ = {};
    clearSelection();
    closeForm();
    clearFocus();
    queryCursor_.reset();
}

#include "BackendScreen.h"
#include "../UITheme.h"
#include "../UiLayout.h"
#include "../Utf8.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace {
constexpr Color kBorder = UITheme::border;
constexpr Color kText = UITheme::text;
constexpr Color kMuted = UITheme::muted;
constexpr Color kHover = UITheme::hover;

constexpr float kActionButtonWidth = 170.0f;
struct BackendUiLayout { Rectangle content, list, refresh; float actionX, actionY; std::size_t visibleRows; };
BackendUiLayout layout() {
    BackendUiLayout ui{};
    ui.content = UiLayout::content();
    ui.actionX = ui.content.x + ui.content.width - kActionButtonWidth - 14.0f;
    ui.actionY = ui.content.y + 72.0f;
    ui.refresh = {ui.actionX, ui.content.y + 12.0f, kActionButtonWidth, 36.0f};
    ui.list = {ui.content.x + 12.0f, ui.content.y + 72.0f,
               std::max(150.0f, ui.content.width - 220.0f), ui.content.height - 124.0f};
    ui.visibleRows = std::max<std::size_t>(1, static_cast<std::size_t>(ui.list.height / 34.0f));
    return ui;
}
BackendUiLayout statisticsLayout(BackendUiLayout ui) {
    ui.list = {ui.content.x + 12.0f, ui.content.y + 152.0f, ui.content.width - 24.0f,
               std::max(120.0f, ui.content.height - 204.0f)};
    ui.visibleRows = std::max<std::size_t>(1, static_cast<std::size_t>(ui.list.height / 42.0f));
    return ui;
}
Rectangle accountRoleFilterBounds(const BackendUiLayout& ui) {
    const float x = ui.list.x + 145.0f;
    return {x, ui.content.y + 6.0f, std::min(250.0f, ui.actionX - x - 10.0f), 32.0f};
}
std::vector<std::size_t> accountRows(const BackendTableData& table, std::size_t filter) {
    std::vector<std::size_t> rows;
    for (std::size_t i = 0; i < table.rows.size(); ++i) {
        const auto& row = table.rows[i];
        if (row.role == (filter == 1 ? UserRole::ADMIN : UserRole::LEARNER)) rows.push_back(i);
    }
    return rows;
}
Rectangle action(float x, float y, float w = kActionButtonWidth, float h = 40.0f) { return {x, y, w, h}; }

struct AlphabetGridRow {
    std::string label;
    std::array<std::optional<std::size_t>, 5> entries{};
};
struct AlphabetOverflow { std::string group; std::string romaji; std::size_t source; };

int kanaColumn(const std::string& romaji) {
    if (romaji == "n") return 2;
    if (romaji.empty()) return -1;
    switch (romaji.back()) {
    case 'a': return 0;
    case 'i': return 1;
    case 'u': return 2;
    case 'e': return 3;
    case 'o': return 4;
    default: return -1;
    }
}

std::vector<AlphabetGridRow> alphabetRows(const std::vector<AlphabetEntry>& rows, bool katakana) {
    static const std::array<const char*, 11> groups = {"A", "KA", "SA", "TA", "NA", "HA", "MA", "YA", "RA", "WA", "N"};
    std::vector<AlphabetGridRow> grid;
    grid.reserve(groups.size() + 4);
    for (const char* group : groups) grid.push_back({group, {}});
    std::vector<AlphabetOverflow> overflow;
    const auto script = katakana ? Script::KATAKANA : Script::HIRAGANA;
    for (std::size_t source = 0; source < rows.size(); ++source) {
        const auto& entry = rows[source];
        if (entry.getScript() != script) continue;
        auto group = std::find_if(grid.begin(), grid.begin() + static_cast<std::ptrdiff_t>(groups.size()),
                                  [&](const AlphabetGridRow& candidate) { return candidate.label == entry.getGroupName(); });
        if (group == grid.begin() + static_cast<std::ptrdiff_t>(groups.size())) {
            const auto existing = std::find_if(grid.begin() + static_cast<std::ptrdiff_t>(groups.size()), grid.end(),
                                               [&](const AlphabetGridRow& candidate) { return candidate.label == entry.getGroupName(); });
            if (existing == grid.end()) grid.push_back({entry.getGroupName(), {}});
            group = std::find_if(grid.begin() + static_cast<std::ptrdiff_t>(groups.size()), grid.end(),
                                 [&](const AlphabetGridRow& candidate) { return candidate.label == entry.getGroupName(); });
        }
        const int column = kanaColumn(entry.getRomaji());
        if (column >= 0 && column < 5 && !group->entries[static_cast<std::size_t>(column)]) {
            group->entries[static_cast<std::size_t>(column)] = source;
        } else {
            overflow.push_back({entry.getGroupName(), entry.getRomaji(), source});
        }
    }
    for (const auto& entry : overflow) {
        const int requestedColumn = kanaColumn(entry.romaji);
        AlphabetGridRow extra{entry.group, {}};
        std::size_t column = requestedColumn >= 0 ? static_cast<std::size_t>(requestedColumn) : 0;
        while (column < extra.entries.size() && extra.entries[column]) ++column;
        if (column == extra.entries.size()) column = 0;
        extra.entries[column] = entry.source;
        grid.push_back(std::move(extra));
    }
    return grid;
}

struct AlphabetGridLayout {
    Rectangle hiraganaTab;
    Rectangle katakanaTab;
    Rectangle bounds;
    float groupWidth;
    float gap;
    float cellWidth;
    float cellHeight;
    std::size_t visibleRows;
};

AlphabetGridLayout alphabetLayout(const BackendUiLayout& ui, std::size_t rowCount) {
    const Rectangle list = ui.list;
    AlphabetGridLayout result{};
    result.hiraganaTab = {list.x + 8.0f, list.y + 4.0f, 132.0f, 32.0f};
    result.katakanaTab = {list.x + 148.0f, list.y + 4.0f, 132.0f, 32.0f};
    result.bounds = {list.x + 8.0f, list.y + 68.0f, list.width - 22.0f, list.height - 76.0f};
    result.groupWidth = 34.0f;
    result.gap = 6.0f;
    const float reserved = result.groupWidth + result.gap + result.gap * 4.0f + 10.0f;
    result.cellWidth = std::max(36.0f, (result.bounds.width - reserved) / 5.0f);
    result.cellHeight = std::clamp((result.bounds.height - result.gap * 10.0f) / 11.0f, 60.0f, 64.0f);
    const auto visible = static_cast<std::size_t>(std::max(1.0f,
        std::floor((result.bounds.height + result.gap) / (result.cellHeight + result.gap))));
    result.visibleRows = std::max<std::size_t>(1, std::min(visible, rowCount));
    return result;
}

Rectangle alphabetCell(const AlphabetGridLayout& ui, std::size_t visibleRow, std::size_t column) {
    const float x = ui.bounds.x + ui.groupWidth + ui.gap + static_cast<float>(column) * (ui.cellWidth + ui.gap);
    const float y = ui.bounds.y + static_cast<float>(visibleRow) * (ui.cellHeight + ui.gap);
    return {x, y, ui.cellWidth, ui.cellHeight};
}

bool isTableSection(Section section) {
    return section == Section::Levels || section == Section::Accounts || section == Section::Questions ||
           section == Section::Favorites || section == Section::History || section == Section::Statistics;
}

using Event = BackendScreenEvent::Type;
struct TableAction { Event event; const char* label; float offset; bool needsTarget; bool primary = false; };

const std::initializer_list<TableAction>& tableActions(Section section, UserRole role) {
    static const std::initializer_list<TableAction> levels = {{Event::DELETE_ROW, "Delete level", 0, true},
        {Event::OPEN_CREATE, "Add level", 48, false, true}, {Event::EDIT_ROW, "Edit level", 96, true}};
    static const std::initializer_list<TableAction> accounts = {{Event::TOGGLE_ACCOUNT, "Toggle lock", 0, true},
        {Event::OPEN_CREATE, "Add account", 48, false, true}, {Event::EDIT_ROW, "Reset password", 96, true}};
    static const std::initializer_list<TableAction> alphabet = {{Event::DELETE_ROW, "Delete entry", 0, true},
        {Event::OPEN_CREATE, "Add entry", 48, false, true}, {Event::EDIT_ROW, "Edit entry", 96, true}};
    static const std::initializer_list<TableAction> questions = {{Event::TOGGLE_QUESTION, "Toggle question", 0, true},
        {Event::DELETE_ROW, "Delete question", 48, true}, {Event::OPEN_CREATE, "Add question", 96, false, true},
        {Event::EDIT_ROW, "Edit question", 144, true}};
    static const std::initializer_list<TableAction> favorites = {{Event::REMOVE_FAVORITE, "Remove favorite", 0, true}};
    static const std::initializer_list<TableAction> history = {{Event::REPLAY_HISTORY, "Search again", 0, true, true},
        {Event::CLEAR_HISTORY, "Clear history", 48, false}};
    static const std::initializer_list<TableAction> none{};
    switch (section) {
    case Section::Levels: return levels;
    case Section::Accounts: return accounts;
    case Section::Alphabet: return role == UserRole::ADMIN ? alphabet : none;
    case Section::Questions: return questions;
    case Section::Favorites: return favorites;
    case Section::History: return history;
    default: return none;
    }
}

struct BackendTableLayout {
    Rectangle clip;
    float columnsX;
    float columnsWidth;
    float rowHeight;
    std::size_t visibleRows;
    std::vector<float> weights;
};

BackendTableLayout tableLayout(const BackendUiLayout& ui, Section section, const BackendTableData& table) {
    BackendTableLayout result{};
    result.clip = {ui.list.x + 6.0f, ui.list.y + 48.0f, ui.list.width - 20.0f,
                   std::max(24.0f, ui.list.height - 76.0f)};
    result.columnsX = ui.list.x + 12.0f;
    result.columnsWidth = std::max(1.0f, ui.list.width - 38.0f);
    result.rowHeight = 42.0f;
    result.visibleRows = std::max<std::size_t>(1, static_cast<std::size_t>(result.clip.height / result.rowHeight));
    if (section == Section::Levels) result.weights = {0.12f, 0.18f, 0.18f, 0.10f, 0.42f};
    else if (section == Section::Accounts) result.weights = {0.15f, 0.33f, 0.23f, 0.29f};
    else if (section == Section::Questions) result.weights = {0.14f, 0.50f, 0.18f, 0.18f};
    else if (section == Section::Favorites) result.weights = {0.14f, 0.23f, 0.31f, 0.16f, 0.16f};
    else if (section == Section::History) result.weights = {0.16f, 0.48f, 0.36f};
    else if (section == Section::Statistics) result.weights = {0.20f, 0.28f, 0.17f, 0.35f};
    if (result.weights.size() != table.headers.size()) {
        result.weights.assign(table.headers.size(), table.headers.empty() ? 0.0f : 1.0f / static_cast<float>(table.headers.size()));
    }
    return result;
}

Rectangle tableRowBounds(const BackendUiLayout& ui, std::size_t visibleRow) {
    return {ui.list.x + 6.0f, ui.list.y + 48.0f + static_cast<float>(visibleRow) * 42.0f,
            ui.list.width - 18.0f, 40.0f};
}

float tableColumnX(const BackendTableLayout& ui, std::size_t column) {
    float x = ui.columnsX;
    for (std::size_t i = 0; i < column && i < ui.weights.size(); ++i) x += ui.columnsWidth * ui.weights[i];
    return x;
}

float tableColumnWidth(const BackendTableLayout& ui, std::size_t column) {
    return column < ui.weights.size() ? ui.columnsWidth * ui.weights[column] : 0.0f;
}
Rectangle confirmDialog() { return UiLayout::centeredDialog(460.0f, 220.0f, 20.0f); }
Rectangle confirmYes() { const Rectangle d=confirmDialog(); return {d.x+d.width-236.0f,d.y+d.height-58.0f,104,38}; }
Rectangle confirmNo() { const Rectangle d=confirmDialog(); return {d.x+d.width-120.0f,d.y+d.height-58.0f,96,38}; }

}

bool BackendScreen::click(Rectangle r) {
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), r);
}
void BackendScreen::button(Rectangle r, const char* label, bool primary, bool enabled) {
    UITheme::drawButton(r,label,primary,enabled);
}

void BackendScreen::navigate(Section section) {
    if (section == section_) return;
    closeForm();
    importConfirm_ = deleteConfirm_ = packagePathFocused_ = false;
    positions_[static_cast<std::size_t>(section_)] = {std::move(selected_), scrollOffset_};
    section_ = section;
    const auto& position = positions_[static_cast<std::size_t>(section)];
    selected_ = position.selected;
    scrollOffset_ = position.scroll;
}

void BackendScreen::revalidate(const BackendTableData& table, const std::vector<AlphabetEntry>& alphabet) {
    if (!hasSelection()) return;
    const auto present = section_ == Section::Alphabet
        ? std::any_of(alphabet.begin(), alphabet.end(), [&](const AlphabetEntry& entry) {
              return BackendId(entry.getId()) == selected_;
          })
        : std::any_of(table.rows.begin(), table.rows.end(), [&](const BackendTableRow& row) {
              return row.id == selected_ && (section_ != Section::Accounts || accountRoleFilter_ == 0 ||
                  row.role == (accountRoleFilter_ == 1 ? UserRole::ADMIN : UserRole::LEARNER));
          });
    if (!present) selected_ = {};
}

void BackendScreen::reset() noexcept {
    closeForm();
    positions_ = {};
    section_ = Section::Dashboard;
    selected_ = {};
    scrollOffset_ = alphabetScrollOffset_ = quizQuestionIndex_ = accountRoleFilter_ = 0;
    reviewDetailsScrollOffset_ = 0;
    alphabetKatakana_ = importConfirm_ = deleteConfirm_ = packagePathFocused_ = false;
}

BackendScreenEvent BackendScreen::update(Section section, UserRole role, const std::vector<AlphabetEntry>& alphabet,
                                         const BackendTableData& table, const BackendActivityState& activity) {
    const bool statisticsPage = section == Section::Statistics;
    const BackendUiLayout base = layout();
    const auto ui = statisticsPage ? statisticsLayout(base) : base;
    if (deleteConfirm_ || importConfirm_) return updateConfirmation();
    if (formOpen_) {
        if (role == UserRole::ADMIN) return updateForm();
        closeForm();
    }
    if (section == Section::Packages && role == UserRole::ADMIN) {
        const Rectangle path{ui.content.x+12,ui.content.y+100,ui.list.width,42};
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            packagePathFocused_ = CheckCollisionPointRec(GetMousePosition(), path);
        if (packagePathFocused_) {
            if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) Utf8::eraseLast(packagePath_);
            for (int cp = GetCharPressed(); cp > 0; cp = GetCharPressed())
                if (cp >= 32) Utf8::appendWithinLimit(packagePath_, Utf8::encode(cp), 4096);
        }
        if (click({path.x,path.y+52,kActionButtonWidth,38})) {
            BackendScreenEvent event(Event::EXPORT_PACKAGE);
            event.text = packagePath_;
            return event;
        }
        if (click({path.x+kActionButtonWidth+12,path.y+52,kActionButtonWidth,38})) { importConfirm_ = true; return {}; }
    }
    if (click(ui.refresh)) return {Event::REFRESH};
    if (section == Section::Review) return updateReview(ui.list, ui.actionX, ui.actionY, activity);
    if (section == Section::Quiz) return updateQuiz(ui.list, ui.actionX, ui.actionY, activity);

    if (section == Section::Accounts && click(accountRoleFilterBounds(ui))) {
        accountRoleFilter_ = (accountRoleFilter_+1)%3;
        scrollOffset_ = 0;
        revalidate(table, alphabet);
        return {};
    }
    if (isTableSection(section)) {
        const bool filtered = section == Section::Accounts && accountRoleFilter_ != 0;
        const auto rows = filtered ? accountRows(table, accountRoleFilter_) : std::vector<std::size_t>{};
        const auto count = filtered ? rows.size() : table.rows.size();
        const auto bounds = tableLayout(ui, section, table);
        const auto maxOffset = count > bounds.visibleRows ? count - bounds.visibleRows : 0;
        scrollOffset_ = std::min(scrollOffset_, maxOffset);
        if (CheckCollisionPointRec(GetMousePosition(), ui.list)) {
            const float wheel = GetMouseWheelMove();
            if (wheel > 0 && scrollOffset_ > 0) --scrollOffset_;
            if (wheel < 0 && scrollOffset_ < maxOffset) ++scrollOffset_;
        }
        for (std::size_t i = 0; !statisticsPage && i < bounds.visibleRows && scrollOffset_ + i < count; ++i) {
            if (!click(tableRowBounds(ui, i))) continue;
            auto index = scrollOffset_ + i;
            selected_ = table.rows[filtered ? rows[index] : index].id;
            return {};
        }
    } else if (section == Section::Alphabet) {
        const auto grid = alphabetRows(alphabet, alphabetKatakana_);
        const auto bounds = alphabetLayout(ui, grid.size());
        if (click(bounds.hiraganaTab) || click(bounds.katakanaTab)) {
            const bool next = click(bounds.katakanaTab);
            if (next != alphabetKatakana_) {
                alphabetKatakana_ = next;
                alphabetScrollOffset_ = 0;
                selected_ = {};
            }
            return {};
        }
        const auto maxOffset = grid.size() > bounds.visibleRows ? grid.size() - bounds.visibleRows : 0;
        alphabetScrollOffset_ = std::min(alphabetScrollOffset_, maxOffset);
        if (CheckCollisionPointRec(GetMousePosition(), bounds.bounds)) {
            const float wheel = GetMouseWheelMove();
            if (wheel > 0 && alphabetScrollOffset_ > 0) --alphabetScrollOffset_;
            if (wheel < 0 && alphabetScrollOffset_ < maxOffset) ++alphabetScrollOffset_;
        }
        for (std::size_t i = 0; i < bounds.visibleRows && alphabetScrollOffset_+i < grid.size(); ++i) {
            const auto& row = grid[alphabetScrollOffset_+i];
            for (std::size_t col = 0; col < row.entries.size(); ++col) {
                if (!row.entries[col] || !click(alphabetCell(bounds, i, col))) continue;
                selected_ = alphabet[*row.entries[col]].getId();
                return {};
            }
        }
    }
    return updateActions(section, role, ui.actionX, ui.actionY);
}

BackendScreenEvent BackendScreen::updateConfirmation() {
    if (IsKeyPressed(KEY_ESCAPE) || click(confirmNo())) {
        deleteConfirm_ = importConfirm_ = false;
        return {};
    }
    if (!click(confirmYes())) return {};
    if (importConfirm_) {
        importConfirm_ = false;
        BackendScreenEvent event(Event::IMPORT_PACKAGE);
        event.text = packagePath_;
        return event;
    }
    deleteConfirm_ = false;
    return {Event::DELETE_ROW, selected_};
}

BackendScreenEvent BackendScreen::updateActions(Section section, UserRole role, float x, float y) {
    for (const auto& item : tableActions(section, role)) {
        if ((item.needsTarget && !hasSelection()) || !click(action(x, y + item.offset))) continue;
        if (item.event == Event::OPEN_CREATE) { openCreateForm(section); return {}; }
        if (item.event == Event::DELETE_ROW) { deleteConfirm_ = true; return {}; }
        return {item.event, item.needsTarget ? selected_ : BackendId{}};
    }
    return {};
}

void BackendScreen::drawStatisticsCards(Rectangle content, const BackendActivityState& activity) const {
        BackendUiLayout ui{};
        ui.content = content;
    const char* labels[] = {"Vocabulary", "Grammar", "Kanji"};
    const std::size_t values[] = {activity.resourceStats ? activity.resourceStats->vocabulary : 0,
                                  activity.resourceStats ? activity.resourceStats->grammar : 0,
                                  activity.resourceStats ? activity.resourceStats->kanji : 0};
    const float gap = 12.0f;
    const float cardWidth = (ui.content.width - 24.0f - gap * 2.0f) / 3.0f;
    for (std::size_t i = 0; i < 3; ++i) {
        const Rectangle card{ui.content.x + 12.0f + static_cast<float>(i) * (cardWidth + gap),
                             ui.content.y + 72.0f, cardWidth, 66.0f};
        DrawRectangleRounded(card, 0.10f, 8, Color{255, 246, 249, 255});
        DrawRectangleRoundedLinesEx(card, 0.10f, 8, 1.0f, kBorder);
        UITheme::drawText(labels[i], static_cast<int>(card.x + 16.0f), static_cast<int>(card.y + 9.0f), 15, kMuted);
        if (activity.resourceStats) {
            const std::string value = std::to_string(values[i]);
            UITheme::drawText(value.c_str(), static_cast<int>(card.x + 16.0f), static_cast<int>(card.y + 30.0f), 24, UITheme::strongText);
        } else {
            UITheme::drawText("—", static_cast<int>(card.x + 16.0f), static_cast<int>(card.y + 30.0f), 24, UITheme::strongText);
        }
    }

}

void BackendScreen::drawTablePage(Rectangle content, Rectangle list, Section section,
                                  const BackendTableData& table, const std::vector<std::size_t>* sourceRows,
                                  bool statisticsPage) const {
        BackendUiLayout ui{};
        ui.content = content;
        ui.list = list;
    const std::size_t rowCount = sourceRows ? sourceRows->size() : table.rows.size();
    DrawRectangleRounded(ui.list,0.04f,8,Color{255,252,253,255});
    DrawRectangleRoundedLinesEx(ui.list,0.04f,8,1.0f,kBorder);
    const auto tableUi = tableLayout(ui, section, table);
    const float headerY = ui.list.y + 14.0f;
    for (std::size_t column = 0; column < table.headers.size(); ++column) {
        const float columnX = tableColumnX(tableUi, column);
        const float columnWidth = tableColumnWidth(tableUi, column);
        BeginScissorMode(static_cast<int>(columnX), static_cast<int>(ui.list.y + 4.0f),
                         std::max(1, static_cast<int>(columnWidth - 8.0f)), 32);
        UITheme::drawText(table.headers[column].c_str(), static_cast<int>(columnX + 4.0f),
                          static_cast<int>(headerY), 18, UITheme::strongText);
        EndScissorMode();
    }
    DrawLine(static_cast<int>(ui.list.x + 8.0f), static_cast<int>(ui.list.y + 42.0f),
             static_cast<int>(ui.list.x + ui.list.width - 14.0f), static_cast<int>(ui.list.y + 42.0f), kBorder);
    const std::size_t maxOffset = rowCount > tableUi.visibleRows ? rowCount - tableUi.visibleRows : 0;
    const std::size_t first = std::min(scrollOffset_, maxOffset);
    BeginScissorMode(static_cast<int>(tableUi.clip.x), static_cast<int>(tableUi.clip.y),
                     static_cast<int>(tableUi.clip.width), static_cast<int>(tableUi.clip.height));
    for (std::size_t visible = 0; visible < tableUi.visibleRows && first + visible < rowCount; ++visible) {
        const std::size_t source = first + visible;
        const auto originalSource = sourceRows ? (*sourceRows)[source] : source;
        const auto& rowData = table.rows[originalSource];
        const auto& cells = rowData.cells;
        const Rectangle row = tableRowBounds(ui, visible);
        if ((!statisticsPage && hasSelection() && rowData.id == selected_) || CheckCollisionPointRec(GetMousePosition(), row))
            DrawRectangleRounded(row,0.10f,5,kHover);
        for (std::size_t column = 0; column < table.headers.size() && column < cells.size(); ++column) {
            const float columnX = tableColumnX(tableUi, column);
            const float columnWidth = tableColumnWidth(tableUi, column);
            BeginScissorMode(static_cast<int>(columnX + 4.0f), static_cast<int>(row.y + 2.0f),
                             std::max(1, static_cast<int>(columnWidth - 8.0f)), static_cast<int>(row.height - 4.0f));
            UITheme::drawText(cells[column].c_str(), static_cast<int>(columnX + 4.0f),
                              static_cast<int>(row.y + (row.height - 18.0f) * 0.5f), 18, kText);
            EndScissorMode();
        }
    }
    EndScissorMode();
    if (rowCount > tableUi.visibleRows) {
        const Rectangle track{ui.list.x + ui.list.width - 8.0f, tableUi.clip.y, 3.0f, tableUi.clip.height};
        DrawRectangleRounded(track,0.5f,3,Color{239,224,230,255});
        const float thumbHeight = std::max(22.0f, track.height * static_cast<float>(tableUi.visibleRows) / static_cast<float>(rowCount));
        const float offsetRatio = maxOffset > 0 ? static_cast<float>(first) / static_cast<float>(maxOffset) : 0.0f;
        DrawRectangleRounded({track.x,track.y+(track.height-thumbHeight)*offsetRatio,track.width,thumbHeight},0.5f,3,kBorder);
    }
    if (rowCount == 0)
        UITheme::drawWrapped(statisticsPage ? "No quiz attempts recorded yet." : "No records are available.", static_cast<int>(ui.list.x + 14.0f),
                             static_cast<int>(tableUi.clip.y + 12.0f), static_cast<int>(ui.list.width - 40.0f), 15, kMuted, 2);

}

void BackendScreen::drawAlphabetPage(Rectangle list, const std::vector<AlphabetEntry>& rows,
                                     bool katakana, std::size_t scrollOffset) const {
        BackendUiLayout ui{};
        ui.list = list;
    DrawRectangleRounded(ui.list,0.04f,8,Color{255,252,253,255});
    DrawRectangleRoundedLinesEx(ui.list,0.04f,8,1.0f,kBorder);
    const auto gridRows = alphabetRows(rows, katakana);
    const auto alphabetUi = alphabetLayout(ui, gridRows.size());
    button(alphabetUi.hiraganaTab, "Hiragana", !katakana);
    button(alphabetUi.katakanaTab, "Katakana", katakana);
    const std::size_t maxOffset = gridRows.size() > alphabetUi.visibleRows ? gridRows.size() - alphabetUi.visibleRows : 0;
    const std::size_t first = std::min(scrollOffset, maxOffset);
    static const std::array<const char*, 5> columnLabels = {"a", "i", "u", "e", "o"};
    for (std::size_t column = 0; column < columnLabels.size(); ++column) {
        const Rectangle cell = alphabetCell(alphabetUi, 0, column);
        const int textWidth = UITheme::measureText(columnLabels[column], 17);
        UITheme::drawText(columnLabels[column], static_cast<int>(cell.x + (cell.width - textWidth) * 0.5f),
                          static_cast<int>(alphabetUi.bounds.y - 22.0f), 17, UITheme::strongText);
    }
    BeginScissorMode(static_cast<int>(alphabetUi.bounds.x), static_cast<int>(alphabetUi.bounds.y),
                     static_cast<int>(alphabetUi.bounds.width - 8.0f), static_cast<int>(alphabetUi.bounds.height));
    for (std::size_t visible = 0; visible < alphabetUi.visibleRows && first + visible < gridRows.size(); ++visible) {
        const auto& row = gridRows[first + visible];
        const float rowY = alphabetUi.bounds.y + static_cast<float>(visible) * (alphabetUi.cellHeight + alphabetUi.gap);
        std::string rowLabel = row.label == "A" ? std::string{} : row.label.substr(0, 1);
        if (!rowLabel.empty() && rowLabel[0] >= 'A' && rowLabel[0] <= 'Z') rowLabel[0] = static_cast<char>(rowLabel[0] - 'A' + 'a');
        const int rowLabelWidth = UITheme::measureText(rowLabel.c_str(), 17);
        UITheme::drawText(rowLabel.c_str(), static_cast<int>(alphabetUi.bounds.x + (alphabetUi.groupWidth - rowLabelWidth) * 0.5f),
                          static_cast<int>(rowY + (alphabetUi.cellHeight - 17.0f) * 0.5f), 17, UITheme::strongText);
        for (std::size_t column = 0; column < row.entries.size(); ++column) {
            if (!row.entries[column] || *row.entries[column] >= rows.size()) continue;
            const std::size_t source = *row.entries[column];
            const auto& entry = rows[source];
            const Rectangle cell = alphabetCell(alphabetUi, visible, column);
            const bool active = BackendId(entry.getId()) == selected_;
            const bool hover = CheckCollisionPointRec(GetMousePosition(), cell);
            DrawRectangleRounded(cell,0.14f,6,active ? Color{255,224,234,255} : hover ? Color{255,239,244,255} : RAYWHITE);
            DrawRectangleRoundedLinesEx(cell,0.14f,6,active ? 2.0f : 1.0f,active ? UITheme::primary : kBorder);
            const int glyphSize = static_cast<int>(std::clamp(alphabetUi.cellHeight * 0.54f, 30.0f, 34.0f));
            const int glyphWidth = UITheme::measureText(entry.getGlyph().c_str(), glyphSize);
            UITheme::drawText(entry.getGlyph().c_str(), static_cast<int>(cell.x + (cell.width - glyphWidth) * 0.5f),
                              static_cast<int>(cell.y + 3.0f), glyphSize, active ? UITheme::strongText : kText);
            const int romajiWidth = UITheme::measureText(entry.getRomaji().c_str(), 17);
            UITheme::drawText(entry.getRomaji().c_str(), static_cast<int>(cell.x + (cell.width - romajiWidth) * 0.5f),
                              static_cast<int>(cell.y + cell.height - 20.0f), 17, kMuted);
        }
    }
    EndScissorMode();
    if (gridRows.size() > alphabetUi.visibleRows && alphabetUi.bounds.height > 0.0f) {
        const Rectangle track{alphabetUi.bounds.x + alphabetUi.bounds.width - 5.0f, alphabetUi.bounds.y, 3.0f, alphabetUi.bounds.height};
        DrawRectangleRounded(track,0.5f,3,Color{239,224,230,255});
        const float thumbHeight = std::max(22.0f, track.height * static_cast<float>(alphabetUi.visibleRows) / static_cast<float>(gridRows.size()));
        const float ratio = maxOffset > 0 ? static_cast<float>(first) / static_cast<float>(maxOffset) : 0.0f;
        DrawRectangleRounded({track.x,track.y+(track.height-thumbHeight)*ratio,track.width,thumbHeight},0.5f,3,kBorder);
    }

}

void BackendScreen::drawActions(Rectangle content, Rectangle list, float actionX, float actionY, Section section,
                                UserRole role, const std::string& message, bool messageError,
                                const BackendActivityState& activity) const {
    const float x=actionX,y=actionY;
    drawStudyActions(section, x, y, activity);
    for (const auto& item : tableActions(section, role))
        button(action(x, y + item.offset), item.label, item.primary, !item.needsTarget || hasSelection());
    if (section == Section::Packages) {
        const Rectangle path{content.x+12,content.y+100,list.width,42};
        UITheme::drawField(path,"Package folder",packagePath_,packagePathFocused_);
        button({path.x,path.y+52,kActionButtonWidth,38},"Export package",true);
        button({path.x+kActionButtonWidth+12,path.y+52,kActionButtonWidth,38},"Import package");
        UITheme::drawWrapped("Export creates the package folder. Import requires resource_levels.txt and the related resource files.",static_cast<int>(path.x),static_cast<int>(path.y+106),static_cast<int>(list.width),14,kMuted,3);
    }
    if(!message.empty())UITheme::drawStatusBanner(
        {content.x+12.0f,content.y+content.height-52.0f,list.width,40.0f},message,messageError);
    if (formOpen_) drawForm(section, messageError ? message : std::string{});
    if(deleteConfirm_||importConfirm_){
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Color{46,31,38,150});const Rectangle d=confirmDialog();
        DrawRectangleRounded(d,0.05f,10,RAYWHITE);DrawRectangleRoundedLinesEx(d,0.05f,10,1.5f,kBorder);
        UITheme::drawText(importConfirm_?"Import this package?":"Delete this record?",static_cast<int>(d.x+24),static_cast<int>(d.y+24),20,kText);
        UITheme::drawWrapped(importConfirm_?"The package will be merged into current data.":"This action cannot be undone.",static_cast<int>(d.x+24),static_cast<int>(d.y+66),static_cast<int>(d.width-48),15,kMuted,2);
        button(confirmYes(),importConfirm_?"Import":"Delete",true);button(confirmNo(),"Cancel");
    }
}

void BackendScreen::draw(Section section, UserRole role, const std::vector<AlphabetEntry>& entries,
                         const BackendTableData& table, const std::string& message,
                         bool messageError,
                         const BackendActivityState& activity) const {
    const bool statisticsPage = section == Section::Statistics;
    const BackendUiLayout baseUi=layout();
    const BackendUiLayout ui=statisticsPage ? statisticsLayout(baseUi) : baseUi;
    DrawRectangleRounded(ui.content,0.035f,12,RAYWHITE);
    DrawRectangleRoundedLinesEx(ui.content,0.035f,12,1.0f,kBorder);
    UITheme::drawText(sectionTitle(role, section),static_cast<int>(ui.content.x+12),static_cast<int>(ui.content.y+12),22,kText);
    button(ui.refresh,"Refresh");
    const bool tableView = isTableSection(section);
    const bool accountsTable = section == Section::Accounts;
    const bool filterAccounts = accountsTable && accountRoleFilter_ != 0;
    const auto sourceRows = filterAccounts ? accountRows(table, accountRoleFilter_) : std::vector<std::size_t>{};
    const bool alphabet = section == Section::Alphabet;
    const bool importExport = section == Section::Packages;
    const bool reviewPage = section == Section::Review;
    const bool quizPage = section == Section::Quiz;
    if (statisticsPage) drawStatisticsCards(ui.content, activity);
    const char* helper = statisticsPage ? "Review your resource totals and past quiz attempts." :
                         reviewPage ? "Review your saved favorites one card at a time." :
                         quizPage ? "Choose a JLPT level with active questions to begin." :
                         alphabet ? "Choose Hiragana or Katakana, then select a kana." :
                         importExport ? "Choose a package folder, then export or import." :
                         "Select an entry to act on it. Data is loaded from your account.";
    UITheme::drawText(helper,static_cast<int>(ui.content.x+12),static_cast<int>(ui.content.y+44),15,kMuted);
    if (accountsTable) {
        static const char* filters[] = {"All roles", "Admin", "Learner"};
        const std::string label = std::string("Role: ") + filters[accountRoleFilter_];
        button(accountRoleFilterBounds(ui), label.c_str());
    }
    if (reviewPage) drawReviewPage(ui.list, activity, reviewDetailsScrollOffset_);
    else if (quizPage) drawQuizPage(ui.list, activity, quizQuestionIndex_, scrollOffset_);
    else if (tableView) drawTablePage(ui.content, ui.list, section, table,
                                      filterAccounts ? &sourceRows : nullptr, statisticsPage);
    else if (alphabet) drawAlphabetPage(ui.list, entries, alphabetKatakana_, alphabetScrollOffset_);
    drawActions(ui.content, ui.list, ui.actionX, ui.actionY, section, role, message, messageError, activity);
}

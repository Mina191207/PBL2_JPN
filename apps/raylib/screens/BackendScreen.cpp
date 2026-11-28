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
struct BackendUiLayout {
    Rectangle content, list, refresh, banner, helper;
    float actionX, actionY;
};

const char* sectionHelp(Section section) {
    switch (section) {
    case Section::Statistics: return "Review your resource totals and past quiz attempts.";
    case Section::Review: return "Review your saved favorites one card at a time.";
    case Section::Quiz: return "Choose a JLPT level with active questions to begin.";
    case Section::Alphabet: return "Choose Hiragana or Katakana, then select a kana.";
    case Section::Packages: return "Choose a package folder, then export or import.";
    case Section::Accounts:
    case Section::Questions: return "Select a row, then choose View details to read the full record.";
    default: return "Select an entry to act on it.";
    }
}

BackendUiLayout layout(Section section, const std::string& message) {
    BackendUiLayout ui{};
    ui.content = UiLayout::content();
    const float fullWidth = ui.content.width - 24.0f;
    const float left = ui.content.x + 12.0f;
    const float bottom = ui.content.y + ui.content.height;
    const bool stats = section == Section::Statistics;
    const bool quiz = section == Section::Quiz;
    const float bannerHeight = std::min(UITheme::bannerHeight(fullWidth, message), stats ? 100.0f : 120.0f);
    ui.banner = {left, bottom - bannerHeight - 8.0f, fullWidth, bannerHeight};
    const float usableBottom = bannerHeight > 0 ? ui.banner.y - 8.0f : bottom - 8.0f;
    ui.actionX = ui.content.x + ui.content.width - kActionButtonWidth - 14.0f;
    ui.actionY = ui.content.y + 72.0f;
    ui.refresh = {ui.actionX, ui.content.y + 8.0f, kActionButtonWidth, 32.0f};
    ui.helper = {left, ui.content.y + 44.0f, fullWidth, 20.0f};
    const float top = ui.content.y + (stats ? 152.0f : 72.0f);
    if (quiz) {
        ui.actionX = left;
        ui.actionY = usableBottom - 40.0f;
    }
    const float listBottom = quiz ? ui.actionY - 8.0f : std::min(usableBottom, bottom - 52.0f);
    const float sideSpace = section == Section::Accounts ? 208.0f : 220.0f;
    ui.list = {left, top, stats || quiz ? fullWidth : ui.content.width - sideSpace,
               std::max(60.0f, listBottom - top)};
    return ui;
}

bool supportsRowDetails(Section section) {
    return section == Section::Accounts || section == Section::Questions || section == Section::Statistics;
}

Rectangle rowDetailsButton(Rectangle list) {
    return {list.x + 10.0f, list.y + list.height - 36.0f, UITheme::buttonWidth("View details") + 8.0f, 30.0f};
}

Rectangle rowDetailsDialog() { return UiLayout::centeredDialog(680, 500); }
Rectangle rowDetailsViewport(Rectangle panel) {
    return {panel.x + 20.0f, panel.y + 62.0f, panel.width - 40.0f, panel.height - 126.0f};
}
Rectangle rowDetailsClose(Rectangle panel) {
    return {panel.x + panel.width - 122.0f, panel.y + panel.height - 52.0f, 102.0f, 36.0f};
}

std::vector<UITheme::WrappedText> rowDetails(const BackendTableData& table, const BackendId& selected,
                                             Rectangle viewport) {
    std::vector<UITheme::WrappedText> fields;
    const auto row = std::find_if(table.rows.begin(), table.rows.end(),
        [&](const auto& candidate) { return candidate.id == selected; });
    if (row != table.rows.end()) {
        for (std::size_t i = 0; i < table.headers.size() && i < row->cells.size(); ++i)
            fields.push_back(UITheme::wrap(table.headers[i] + ": " + row->cells[i], viewport.width - 20.0f, 18));
    }
    return fields;
}

float rowDetailsHeight(const std::vector<UITheme::WrappedText>& fields) {
    float height = 0.0f;
    for (const auto& field : fields) height += field.height() + 12.0f;
    return height;
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
        {Event::REQUEST_CLEAR_HISTORY, "Clear history", 48, false}};
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
    std::size_t visibleRows;
    std::vector<float> widths;
};

BackendTableLayout tableLayout(const BackendUiLayout& ui, Section section, const BackendTableData& table) {
    BackendTableLayout result{};
    const float footer = supportsRowDetails(section) ? 44.0f : 24.0f;
    result.clip = {ui.list.x + 6.0f, ui.list.y + 42.0f, ui.list.width - 18.0f,
                   std::max(0.0f, ui.list.height - 42.0f - footer)};
    result.columnsX = ui.list.x + 12.0f;
    const float width = ui.list.width - 28.0f;
    result.visibleRows = std::max<std::size_t>(1, static_cast<std::size_t>(result.clip.height / 42.0f));
    const auto column = [](const char* sample) { return UITheme::measureText(sample, 16) + 14.0f; };
    if (section == Section::Accounts && table.headers.size() == 4) {
        const float id = column("U00000"), role = column("Learner"), status = column("Locked");
        result.widths = {id, width - id - role - status, role, status};
    } else if (section == Section::Questions && table.headers.size() == 4) {
        const float id = column("Q00000"), level = column("JLPT N5"), status = column("Inactive");
        result.widths = {id, width - id - level - status, level, status};
    } else if (section == Section::Statistics && table.headers.size() == 4) {
        const float id = column("Attempt ID"), level = column("JLPT N5"), date = column("28 Nov 2026, 23:59:59 UTC");
        result.widths = {id, level, width - id - level - date, date};
    } else {
        std::vector<float> weights;
        if (section == Section::Levels) weights = {0.12f, 0.18f, 0.18f, 0.10f, 0.42f};
        else if (section == Section::Favorites) weights = {0.14f, 0.23f, 0.31f, 0.16f, 0.16f};
        else if (section == Section::History) weights = {0.16f, 0.48f, 0.36f};
        if (weights.size() != table.headers.size() && !table.headers.empty())
            weights.assign(table.headers.size(), 1.0f / static_cast<float>(table.headers.size()));
        for (float weight : weights) result.widths.push_back(width * weight);
    }
    return result;
}

Rectangle tableRowBounds(const BackendTableLayout& ui, std::size_t visibleRow) {
    const float y = ui.clip.y + static_cast<float>(visibleRow) * 42.0f;
    return {ui.clip.x, y, ui.clip.width, std::max(0.0f, std::min(40.0f, ui.clip.y + ui.clip.height - y))};
}

Rectangle confirmDialog(bool package) {
    return package ? UiLayout::centeredDialog(700, 360, 20) : UiLayout::centeredDialog(460, 220, 20);
}
Rectangle confirmYes(Rectangle d) { return {d.x+d.width-236.0f,d.y+d.height-58.0f,104,38}; }
Rectangle confirmNo(Rectangle d) { return {d.x+d.width-120.0f,d.y+d.height-58.0f,96,38}; }

}

bool BackendScreen::click(Rectangle r) {
    return UiInput::click(r);
}
void BackendScreen::button(Rectangle r, const char* label, bool primary, bool enabled) {
    UITheme::drawButton(r,label,primary,enabled);
}
void BackendScreen::controlButton(int control, Rectangle r, const char* label, bool primary, bool enabled) const {
    UITheme::drawButton(r, label, primary, enabled, false, !modalOpen() && focus_.on(control));
}

bool BackendScreen::tab(Section section, UserRole role, const BackendTableData& table,
                        const std::vector<AlphabetEntry>& alphabet, const BackendActivityState& activity,
                        const std::string& message) {
    std::vector<int> order{Refresh};
    if (section == Section::Accounts) order.push_back(Roles);
    if (section == Section::Alphabet) {
        order.insert(order.end(), {Hiragana, Katakana});
        if (std::any_of(alphabet.begin(), alphabet.end(), [&](const AlphabetEntry& entry) {
            return entry.getScript() == (alphabetKatakana_ ? Script::KATAKANA : Script::HIRAGANA);
        })) order.push_back(Rows);
    }
    if (isTableSection(section) && !table.rows.empty() &&
        (section != Section::Accounts || accountRoleFilter_ == 0 || !accountRows(table, accountRoleFilter_).empty()))
        order.push_back(Rows);
    if (section == Section::Packages) {
        order.push_back(PackagePath);
        if (FolderField::nativeBrowse) order.push_back(PackageBrowse);
        order.insert(order.end(), {Export, Import});
    }
    if (supportsRowDetails(section) && hasSelection()) order.push_back(RowDetails);
    addStudyControls(order, section, activity);
    for (const auto& item : tableActions(section, role))
        if (!item.needsTarget || hasSelection()) order.push_back(actionControl(item.event));
    const bool moved = focus_.move(order, false);
    if (section == Section::Quiz && moved) revealQuizOption(layout(section, message).list, activity);
    return moved;
}

void BackendScreen::navigate(Section section) {
    if (section == section_) return;
    closeForm();
    confirmation_.reset();
    rowDetailsOpen_ = false;
    clearFocus();
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
    reviewDetailsScrollOffset_ = quizOffset_ = quizResultOffset_ = 0;
    alphabetKatakana_ = false;
    confirmation_.reset();
    rowDetailsOpen_ = false;
    clearFocus();
}

BackendScreenEvent BackendScreen::update(Section section, UserRole role, const std::vector<AlphabetEntry>& alphabet,
                                         const BackendTableData& table, const BackendActivityState& activity,
                                         const std::string& message) {
    const auto ui = layout(section, message);
    if (rowDetailsOpen_) {
        const Rectangle panel = rowDetailsDialog();
        const Rectangle viewport = rowDetailsViewport(panel);
        if (IsKeyPressed(KEY_ESCAPE) || modalFocus_.activate(0, rowDetailsClose(panel))) rowDetailsOpen_ = false;
        else rowDetailsOffset_ = UiInput::scroll(viewport, rowDetailsHeight(rowDetails(table, selected_, viewport)),
                                                 rowDetailsOffset_, true);
        return {};
    }
    if (confirmation_) return updateConfirmation();
    if (formOpen_) {
        if (role == UserRole::ADMIN) return updateForm();
        closeForm();
    }
    const float bannerHeight = UITheme::bannerHeight(ui.banner.width, message);
    bannerOffset_ = bannerHeight > ui.banner.height
        ? UiInput::scroll(ui.banner, bannerHeight, bannerOffset_, false) : 0.0f;
    if (section == Section::Packages && role == UserRole::ADMIN) {
        const Rectangle path{ui.content.x+12,ui.content.y+100,ui.content.width-24,42};
        if (focus_.activate(Export, {path.x,path.y+52,kActionButtonWidth,38})) {
            BackendScreenEvent event(Event::EXPORT_PACKAGE);
            event.text = packageFolder_.path();
            return event;
        }
        if (focus_.activate(Import, {path.x+kActionButtonWidth+12,path.y+52,kActionButtonWidth,38})) {
            BackendScreenEvent event(Event::IMPORT_PACKAGE);
            event.text = packageFolder_.path();
            confirm(std::move(event), "Import this package?", "The package will be merged into current data.");
            return {};
        }
        if (packageFolder_.update(path, focus_, PackagePath, PackageBrowse))
            return {Event::EDITED};
    }
    if (focus_.activate(Refresh, ui.refresh)) return {Event::REFRESH};
    if (section == Section::Review) return updateReview(ui.list, ui.actionX, ui.actionY, activity);
    if (section == Section::Quiz) return updateQuiz(ui.list, ui.actionX, ui.actionY, activity);

    if (section == Section::Accounts && focus_.activate(Roles, accountRoleFilterBounds(ui))) {
        accountRoleFilter_ = (accountRoleFilter_+1)%3;
        scrollOffset_ = 0;
        revalidate(table, alphabet);
        return {};
    }
    if (isTableSection(section)) {
        if (supportsRowDetails(section) &&
            (focus_.activate(RowDetails, rowDetailsButton(ui.list), hasSelection()) ||
             (focus_.on(Rows) && hasSelection() && UiInput::confirm()))) {
            rowDetailsOpen_ = true;
            rowDetailsOffset_ = 0.0f;
            modalFocus_.id = 0;
            return {};
        }
        const bool filtered = section == Section::Accounts && accountRoleFilter_ != 0;
        const auto rows = filtered ? accountRows(table, accountRoleFilter_) : std::vector<std::size_t>{};
        const auto count = filtered ? rows.size() : table.rows.size();
        const auto bounds = tableLayout(ui, section, table);
        const auto idAt = [&](std::size_t i) -> const BackendId& { return table.rows[filtered ? rows[i] : i].id; };
        const auto maxOffset = count > bounds.visibleRows ? count - bounds.visibleRows : 0;
        scrollOffset_ = std::min(scrollOffset_, maxOffset);
        if (CheckCollisionPointRec(GetMousePosition(), ui.list)) {
            const float wheel = GetMouseWheelMove();
            if (wheel > 0 && scrollOffset_ > 0) --scrollOffset_;
            if (wheel < 0 && scrollOffset_ < maxOffset) ++scrollOffset_;
        }
        for (std::size_t i = 0; i < bounds.visibleRows && scrollOffset_ + i < count; ++i) {
            if (!click(tableRowBounds(bounds, i))) continue;
            focus_.id = Rows;
            selected_ = idAt(scrollOffset_ + i);
            return {};
        }
        if (focus_.on(Rows) && UiInput::rowKey()) {
            std::optional<std::size_t> current;
            if (hasSelection())
                for (std::size_t i = 0; i < count; ++i) if (idAt(i) == selected_) { current = i; break; }
            if (const auto next = UiInput::row(count, current)) {
                selected_ = idAt(*next);
                UiInput::revealRow(*next, bounds.visibleRows, scrollOffset_);
                return {};
            }
        }
    } else if (section == Section::Alphabet) {
        const auto grid = alphabetRows(alphabet, alphabetKatakana_);
        const auto bounds = alphabetLayout(ui, grid.size());
        if (focus_.activate(Hiragana, bounds.hiraganaTab) || focus_.activate(Katakana, bounds.katakanaTab)) {
            const bool next = focus_.on(Katakana);
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
                focus_.id = Rows;
                selected_ = alphabet[*row.entries[col]].getId();
                return {};
            }
        }
        if (focus_.on(Rows) && UiInput::rowKey()) {
            std::vector<std::pair<std::size_t, AlphabetId>> entries;
            std::optional<std::size_t> current;
            for (std::size_t row = 0; row < grid.size(); ++row) {
                for (const auto& index : grid[row].entries) {
                    if (!index) continue;
                    const auto& id = alphabet[*index].getId();
                    if (selected_ == BackendId(id)) current = entries.size();
                    entries.emplace_back(row, id);
                }
            }
            if (const auto next = UiInput::row(entries.size(), current)) {
                selected_ = entries[*next].second;
                UiInput::revealRow(entries[*next].first, bounds.visibleRows, alphabetScrollOffset_);
            }
            return {};
        }
    }
    return updateActions(section, role, ui.actionX, ui.actionY);
}

void BackendScreen::confirm(BackendScreenEvent event, std::string title, std::string message) {
    confirmation_ = Confirmation{std::move(event), std::move(title), std::move(message)};
    modalFocus_.id = 1;
}

BackendScreenEvent BackendScreen::updateConfirmation() {
    if (UiInput::tab()) { modalFocus_.move({0, 1}); return {}; }
    const auto dialog = confirmDialog(confirmation_->event.type == Event::REPLACE_PACKAGE);
    if (IsKeyPressed(KEY_ESCAPE) || modalFocus_.activate(1, confirmNo(dialog))) {
        confirmation_.reset();
        return {};
    }
    if (!modalFocus_.activate(0, confirmYes(dialog))) return {};
    auto event = std::move(confirmation_->event);
    confirmation_.reset();
    if (event.type == Event::DELETE_ROW) focus_.id = Rows;
    if (event.type == Event::SUBMIT_QUIZ) focus_.id = QuizText;
    return event;
}

BackendScreenEvent BackendScreen::updateActions(Section section, UserRole role, float x, float y) {
    for (const auto& item : tableActions(section, role)) {
        if (!focus_.activate(actionControl(item.event), action(x, y + item.offset), !item.needsTarget || hasSelection())) continue;
        if (item.event == Event::OPEN_CREATE) { openCreateForm(section); return {}; }
        if (item.event == Event::DELETE_ROW) {
            confirm({Event::DELETE_ROW, selected_}, "Delete this record?", "This action cannot be undone.");
            return {};
        }
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
    const float headerY = ui.list.y + 12.0f;
    float columnX = tableUi.columnsX;
    for (std::size_t column = 0; column < table.headers.size(); ++column) {
        const auto label = UITheme::ellipsize(table.headers[column], tableUi.widths[column] - 8.0f, 16);
        UITheme::drawText(label.c_str(), static_cast<int>(columnX), static_cast<int>(headerY), 16, UITheme::strongText);
        columnX += tableUi.widths[column];
    }
    DrawLine(static_cast<int>(ui.list.x + 8.0f), static_cast<int>(ui.list.y + 36.0f),
             static_cast<int>(ui.list.x + ui.list.width - 14.0f), static_cast<int>(ui.list.y + 36.0f), kBorder);
    const std::size_t maxOffset = rowCount > tableUi.visibleRows ? rowCount - tableUi.visibleRows : 0;
    const std::size_t first = std::min(scrollOffset_, maxOffset);
    UITheme::beginClip(tableUi.clip);
    for (std::size_t visible = 0; visible < tableUi.visibleRows && first + visible < rowCount; ++visible) {
        const std::size_t source = first + visible;
        const auto& rowData = table.rows[sourceRows ? (*sourceRows)[source] : source];
        const Rectangle row = tableRowBounds(tableUi, visible);
        if ((hasSelection() && rowData.id == selected_) || CheckCollisionPointRec(GetMousePosition(), row))
            DrawRectangleRounded(row, 0.10f, 5, kHover);
        float x = tableUi.columnsX;
        for (std::size_t column = 0; column < table.headers.size() && column < rowData.cells.size(); ++column) {
            const auto value = UITheme::ellipsize(rowData.cells[column], tableUi.widths[column] - 8.0f, 16);
            UITheme::drawText(value.c_str(), static_cast<int>(x), static_cast<int>(row.y + 10.0f), 16, kText);
            x += tableUi.widths[column];
        }
    }
    EndScissorMode();
    if (supportsRowDetails(section))
        controlButton(RowDetails, rowDetailsButton(ui.list), "View details", false, hasSelection());
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
    controlButton(Hiragana, alphabetUi.hiraganaTab, "Hiragana", !katakana);
    controlButton(Katakana, alphabetUi.katakanaTab, "Katakana", katakana);
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

void BackendScreen::drawActions(Rectangle content, Rectangle helper, float actionX, float actionY, Section section,
                                UserRole role, const std::string& message, bool messageError,
                                const BackendActivityState& activity) const {
    const float x=actionX,y=actionY;
    drawStudyActions(section, x, y, activity);
    for (const auto& item : tableActions(section, role))
        controlButton(actionControl(item.event), action(x, y + item.offset), item.label, item.primary, !item.needsTarget || hasSelection());
    if (section == Section::Packages) {
        const Rectangle path{content.x+12,content.y+100,content.width-24,42};
        packageFolder_.draw(path, "Package folder", focus_, PackagePath, PackageBrowse, !modalOpen());
        controlButton(Export, {path.x,path.y+52,kActionButtonWidth,38},"Export package",true);
        controlButton(Import, {path.x+kActionButtonWidth+12,path.y+52,kActionButtonWidth,38},"Import package");
        UITheme::drawWrapped("Export creates the package folder. Import requires resource_levels.txt and the related resource files.",static_cast<int>(path.x),static_cast<int>(path.y+106),static_cast<int>(helper.width),14,kMuted);
    }
    if (formOpen_) drawForm(section, messageError ? message : std::string{});
    if (confirmation_) {
        const auto& dialog = *confirmation_;
        const auto type = dialog.event.type;
        const bool package = type == Event::REPLACE_PACKAGE;
        const Rectangle d = confirmDialog(package);
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Color{46,31,38,150});
        DrawRectangleRounded(d,0.05f,10,RAYWHITE);DrawRectangleRoundedLinesEx(d,0.05f,10,1.5f,kBorder);
        const int left = static_cast<int>(d.x + 24);
        const int width = static_cast<int>(d.width - 48);
        UITheme::drawText(dialog.title.c_str(), left, static_cast<int>(d.y + 24), 20, kText);
        if (package) {
            UITheme::drawWrapped("Destination: " + dialog.event.text, left, static_cast<int>(d.y + 64),
                                 width, 15, kText, 6);
        }
        UITheme::drawWrapped(dialog.message, left, static_cast<int>(d.y + (package ? 198 : 66)), width,
                             15, kMuted, package ? 4 : 3);
        const char* label = type == Event::IMPORT_PACKAGE ? "Import" :
                            type == Event::DELETE_ROW ? "Delete" : "Confirm";
        UITheme::drawButton(confirmYes(d), label, true, true, false, modalFocus_.on(0));
        UITheme::drawButton(confirmNo(d), "Cancel", false, true, false, modalFocus_.on(1));
    }
}

void BackendScreen::draw(Section section, UserRole role, const std::vector<AlphabetEntry>& entries,
                         const BackendTableData& table, const std::string& message,
                         bool messageError,
                         const BackendActivityState& activity) const {
    const bool statisticsPage = section == Section::Statistics;
    const BackendUiLayout ui = layout(section, message);
    DrawRectangleRounded(ui.content,0.035f,12,RAYWHITE);
    DrawRectangleRoundedLinesEx(ui.content,0.035f,12,1.0f,kBorder);
    UITheme::drawText(sectionTitle(role, section),static_cast<int>(ui.content.x+12),static_cast<int>(ui.content.y+12),22,kText);
    controlButton(Refresh, ui.refresh, "Refresh");
    const bool tableView = isTableSection(section);
    const bool accountsTable = section == Section::Accounts;
    const bool filterAccounts = accountsTable && accountRoleFilter_ != 0;
    const auto sourceRows = filterAccounts ? accountRows(table, accountRoleFilter_) : std::vector<std::size_t>{};
    const bool alphabet = section == Section::Alphabet;
    const bool reviewPage = section == Section::Review;
    const bool quizPage = section == Section::Quiz;
    if (statisticsPage) drawStatisticsCards(ui.content, activity);
    const char* helper = sectionHelp(section);
    if (quizPage && activity.quiz)
        helper = activity.quiz->submitted ? "Scroll to read each question and its explanation."
                                          : "Scroll to read the full question and all four choices.";
    UITheme::drawWrapped(helper, static_cast<int>(ui.helper.x), static_cast<int>(ui.helper.y),
                         static_cast<int>(ui.helper.width), 15, kMuted);
    if (accountsTable) {
        static const char* filters[] = {"All roles", "Admin", "Learner"};
        const std::string label = std::string("Role: ") + filters[accountRoleFilter_];
        controlButton(Roles, accountRoleFilterBounds(ui), label.c_str());
    }
    if (reviewPage) drawReviewPage(ui.list, activity, reviewDetailsScrollOffset_);
    else if (quizPage) drawQuizPage(ui.list, activity);
    else if (tableView) drawTablePage(ui.content, ui.list, section, table,
                                      filterAccounts ? &sourceRows : nullptr, statisticsPage);
    else if (alphabet) drawAlphabetPage(ui.list, entries, alphabetKatakana_, alphabetScrollOffset_);
    if (!modalOpen() && focus_.on(Rows)) UITheme::drawFocus(ui.list);
    if (!formOpen_) UITheme::drawStatusBanner(ui.banner, message, messageError, bannerOffset_);
    drawActions(ui.content, ui.helper, ui.actionX, ui.actionY, section, role, message, messageError, activity);
    if (rowDetailsOpen_) drawRowDetails(table);
}

void BackendScreen::drawRowDetails(const BackendTableData& table) const {
    const Rectangle panel = rowDetailsDialog();
    const Rectangle viewport = rowDetailsViewport(panel);
    const auto fields = rowDetails(table, selected_, viewport);
    const float height = rowDetailsHeight(fields);
    const float offset = std::min(rowDetailsOffset_, std::max(0.0f, height - viewport.height));
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{46,31,38,150});
    DrawRectangleRounded(panel, 0.05f, 10, RAYWHITE);
    DrawRectangleRoundedLinesEx(panel, 0.05f, 10, 1.5f, kBorder);
    UITheme::drawText("Record details", static_cast<int>(viewport.x), static_cast<int>(panel.y + 24), 22, kText);
    UITheme::beginClip(viewport);
    float y = viewport.y - offset;
    for (const auto& field : fields) {
        field.draw(static_cast<int>(viewport.x + 4.0f), static_cast<int>(y), kText);
        y += field.height() + 12.0f;
    }
    EndScissorMode();
    UITheme::drawScrollbar(viewport, height, offset);
    UITheme::drawButton(rowDetailsClose(panel), "Close", true, true, false, modalFocus_.on(0));
}

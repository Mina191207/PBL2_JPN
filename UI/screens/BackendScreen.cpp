#include "BackendScreen.h"
#include "../UITheme.h"
#include "../UiLayout.h"
#include "../Utf8.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <sstream>
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
    return {ui.list.x + 145.0f, ui.content.y + 6.0f, 250.0f, 32.0f};
}
struct AccountTableView {
    BackendTableData table;
    std::vector<std::size_t> sourceRows;
};
AccountTableView accountTableView(const BackendTableData& source, std::size_t filter) {
    AccountTableView view;
    view.table.headers = source.headers;
    for (std::size_t i = 0; i < source.rows.size(); ++i) {
        const auto& row = source.rows[i];
        const bool include = filter == 0 || (row.size() > 2 &&
            ((filter == 1 && row[2] == "Admin") || (filter == 2 && row[2] == "Learner")));
        if (include) { view.table.rows.push_back(row); view.sourceRows.push_back(i); }
    }
    return view;
}
Rectangle action(float x, float y, float w = kActionButtonWidth, float h = 40.0f) { return {x, y, w, h}; }

struct QuizUiLayout {
    Rectangle level;
    Rectangle options[4];
    Rectangle previous;
    Rectangle next;
};

QuizUiLayout quizLayout(const BackendUiLayout& ui) {
    QuizUiLayout result{};
    result.level = {ui.list.x + 12.0f, ui.list.y + 10.0f, 250.0f, 38.0f};
    const float optionGap = 10.0f;
    const float optionWidth = (ui.list.width - 36.0f - optionGap) * 0.5f;
    const float optionY = ui.list.y + 154.0f;
    result.options[0] = {ui.list.x + 12.0f, optionY, optionWidth, 64.0f};
    result.options[1] = {ui.list.x + 12.0f + optionWidth + optionGap, optionY, optionWidth, 64.0f};
    result.options[2] = {ui.list.x + 12.0f, optionY + 74.0f, optionWidth, 64.0f};
    result.options[3] = {ui.list.x + 12.0f + optionWidth + optionGap, optionY + 74.0f, optionWidth, 64.0f};
    result.previous = {ui.list.x + 12.0f, ui.list.y + ui.list.height - 48.0f, 112.0f, 36.0f};
    result.next = {ui.list.x + ui.list.width - 124.0f, ui.list.y + ui.list.height - 48.0f, 112.0f, 36.0f};
    return result;
}

bool isAlphabetSection(std::size_t section, UserRole role) {
    return (role == UserRole::ADMIN && section == 4) || (role == UserRole::LEARNER && section == 6);
}

bool quizHasWrongAnswers(const BackendActivityState& activity) {
    const bool answerDetailsContainWrong = std::any_of(activity.quizAnswers.begin(), activity.quizAnswers.end(),
        [](const QuizAnswerDetail& answer) { return answer.selectedAnswer != answer.correctAnswer; });
    const bool submittedScoreContainsWrong = activity.quizResult &&
        activity.quizResult->getCorrectAnswers() < activity.quizResult->getTotalQuestions();
    return answerDetailsContainWrong || submittedScoreContainsWrong;
}

std::string partOfSpeechLabel(PartOfSpeech value) {
    switch (value) {
    case PartOfSpeech::NOUN: return "Noun";
    case PartOfSpeech::VERB: return "Verb";
    case PartOfSpeech::IADJECTIVE: return "i-adjective";
    case PartOfSpeech::NAADJECTIVE: return "na-adjective";
    case PartOfSpeech::ADVERB: return "Adverb";
    case PartOfSpeech::OTHER: return "Other";
    }
    return {};
}

void addReviewField(std::vector<std::pair<std::string, std::string>>& fields,
                    const std::string& label, const std::string& value) {
    if (!value.empty()) fields.emplace_back(label, value);
}

struct ReviewDisplayDetails {
    std::vector<std::pair<std::string, std::string>> fields;
    std::string example;
    std::string exampleLabel = "Example";
};

ReviewDisplayDetails reviewDisplayDetails(const BackendActivityState& activity, const ReviewView& card) {
    ReviewDisplayDetails details;
    if (activity.reviewResource) {
        std::visit([&](const auto& resource) {
            addReviewField(details.fields, "Meaning", resource.getMeaning());
            using T = std::decay_t<decltype(resource)>;
            if constexpr (std::is_same_v<T, Vocabulary>) {
                addReviewField(details.fields, "Reading", resource.getReading());
                addReviewField(details.fields, "Part of speech", partOfSpeechLabel(resource.getPartOfSpeech()));
                details.example = resource.getExample();
            } else if constexpr (std::is_same_v<T, Grammar>) {
                addReviewField(details.fields, "Structure", resource.getStructure());
                addReviewField(details.fields, "Usage", resource.getUsage());
                details.example = resource.getExample();
            } else {
                addReviewField(details.fields, "Onyomi", resource.getOnyomi());
                addReviewField(details.fields, "Kunyomi", resource.getKunyomi());
                details.exampleLabel = "Examples";
                details.example = resource.getExamples();
            }
        }, *activity.reviewResource);
        return details;
    }

    if (!card.back) return details;
    std::size_t start = 0;
    while (start <= card.back->size()) {
        const std::size_t end = card.back->find('\n', start);
        const std::string line = card.back->substr(start,
            end == std::string::npos ? std::string::npos : end - start);
        const std::size_t separator = line.find(':');
        if (separator == std::string::npos) addReviewField(details.fields, "Meaning", line);
        else {
            const std::string label = line.substr(0, separator);
            const std::string value = line.substr(separator + 1);
            if (label == "Example" || label == "Examples") { details.exampleLabel = label; details.example = value; }
            else addReviewField(details.fields, label, value);
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return details;
}

std::size_t wrappedLineCount(const std::string& value, int width, int fontSize) {
    std::size_t lines = 0;
    std::string line;
    std::istringstream words(value);
    std::string token;
    while (words >> token) {
        const std::string candidate = line.empty() ? token : line + " " + token;
        if (UITheme::measureText(candidate.c_str(), fontSize) <= width) {
            line = candidate;
            continue;
        }
        if (!line.empty()) { ++lines; line.clear(); }
        if (UITheme::measureText(token.c_str(), fontSize) <= width) { line = token; continue; }
        for (const auto& unit : Utf8::decode(token)) {
            const std::string character = token.substr(unit.begin, unit.end - unit.begin);
            if (!line.empty() && UITheme::measureText((line + character).c_str(), fontSize) > width) {
                ++lines;
                line.clear();
            }
            line += character;
        }
    }
    if (!line.empty()) ++lines;
    return std::max<std::size_t>(1, lines);
}

Rectangle reviewDetailsViewport(const BackendUiLayout& ui) {
    const float top = ui.list.y + 154.0f;
    const float bottom = ui.list.y + ui.list.height - 52.0f;
    return {ui.list.x + 18.0f, top, ui.list.width - 36.0f, std::max(18.0f, bottom - top)};
}

std::vector<float> reviewFieldRowHeights(const ReviewDisplayDetails& details, float width) {
    const float tileWidth = (width - 10.0f) * 0.5f;
    const std::size_t rows = (details.fields.size() + 1) / 2;
    std::vector<float> heights(rows, 62.0f);
    for (std::size_t index = 0; index < details.fields.size(); ++index) {
        const auto lines = wrappedLineCount(details.fields[index].second,
            std::max(1, static_cast<int>(tileWidth - 20.0f)), 15);
        heights[index / 2] = std::max(heights[index / 2], 27.0f + static_cast<float>(lines) * 20.0f);
    }
    return heights;
}

float reviewDetailsContentHeight(const ReviewDisplayDetails& details, float width) {
    const auto rowHeights = reviewFieldRowHeights(details, width);
    float height = 0.0f;
    for (const float rowHeight : rowHeights) height += rowHeight;
    if (rowHeights.size() > 1) height += static_cast<float>(rowHeights.size() - 1) * 8.0f;
    if (!details.example.empty()) {
        const auto lines = wrappedLineCount(details.example, std::max(1, static_cast<int>(width - 24.0f)), 16);
        height += (height > 0.0f ? 8.0f : 0.0f) + 36.0f + static_cast<float>(lines) * 21.0f;
    }
    return height;
}

std::array<std::string, 4> alphabetFields(const std::string& row) {
    std::array<std::string, 4> fields{};
    std::size_t start = 0;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const std::size_t end = row.find('|', start);
        if (end == std::string::npos) {
            if (i == fields.size() - 1) fields[i] = row.substr(start);
            break;
        }
        fields[i] = row.substr(start, end - start);
        start = end + 1;
    }
    return fields;
}

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

std::vector<AlphabetGridRow> alphabetRows(const std::vector<std::string>& rows, bool katakana) {
    static const std::array<const char*, 11> groups = {"A", "KA", "SA", "TA", "NA", "HA", "MA", "YA", "RA", "WA", "N"};
    std::vector<AlphabetGridRow> grid;
    grid.reserve(groups.size() + 4);
    for (const char* group : groups) grid.push_back({group, {}});
    std::vector<AlphabetOverflow> overflow;
    const std::string script = katakana ? "KATAKANA" : "HIRAGANA";
    for (std::size_t source = 0; source < rows.size(); ++source) {
        const auto fields = alphabetFields(rows[source]);
        if (fields[0] != script || fields[1].empty()) continue;
        auto group = std::find_if(grid.begin(), grid.begin() + static_cast<std::ptrdiff_t>(groups.size()),
                                  [&](const AlphabetGridRow& candidate) { return candidate.label == fields[3]; });
        if (group == grid.begin() + static_cast<std::ptrdiff_t>(groups.size())) {
            const auto existing = std::find_if(grid.begin() + static_cast<std::ptrdiff_t>(groups.size()), grid.end(),
                                               [&](const AlphabetGridRow& candidate) { return candidate.label == fields[3]; });
            if (existing == grid.end()) grid.push_back({fields[3], {}});
            group = std::find_if(grid.begin() + static_cast<std::ptrdiff_t>(groups.size()), grid.end(),
                                 [&](const AlphabetGridRow& candidate) { return candidate.label == fields[3]; });
        }
        const int column = kanaColumn(fields[2]);
        if (column >= 0 && column < 5 && !group->entries[static_cast<std::size_t>(column)]) {
            group->entries[static_cast<std::size_t>(column)] = source;
        } else {
            overflow.push_back({fields[3], fields[2], source});
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

bool isTableSection(std::size_t section, UserRole role) {
    return (role == UserRole::ADMIN && (section == 2 || section == 3 || section == 5)) ||
           (role == UserRole::LEARNER && (section == 2 || section == 3 || section == 7));
}

struct BackendTableLayout {
    Rectangle clip;
    float columnsX;
    float columnsWidth;
    float rowHeight;
    std::size_t visibleRows;
    std::vector<float> weights;
};

BackendTableLayout tableLayout(const BackendUiLayout& ui, std::size_t section, UserRole role,
                               const BackendTableData& table) {
    BackendTableLayout result{};
    result.clip = {ui.list.x + 6.0f, ui.list.y + 48.0f, ui.list.width - 20.0f,
                   std::max(24.0f, ui.list.height - 76.0f)};
    result.columnsX = ui.list.x + 12.0f;
    result.columnsWidth = std::max(1.0f, ui.list.width - 38.0f);
    result.rowHeight = 42.0f;
    result.visibleRows = std::max<std::size_t>(1, static_cast<std::size_t>(result.clip.height / result.rowHeight));
    if (role == UserRole::ADMIN && section == 2) result.weights = {0.12f, 0.18f, 0.18f, 0.10f, 0.42f};
    else if (role == UserRole::ADMIN && section == 3) result.weights = {0.15f, 0.33f, 0.23f, 0.29f};
    else if (role == UserRole::ADMIN && section == 5) result.weights = {0.14f, 0.50f, 0.18f, 0.18f};
    else if (role == UserRole::LEARNER && section == 2) result.weights = {0.14f, 0.23f, 0.31f, 0.16f, 0.16f};
    else if (role == UserRole::LEARNER && section == 3) result.weights = {0.16f, 0.48f, 0.36f};
    else if (role == UserRole::LEARNER && section == 7) result.weights = {0.20f, 0.28f, 0.17f, 0.35f};
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
Rectangle formDialog() { return UiLayout::centeredDialog(760.0f, 580.0f, 20.0f); }
Rectangle formField(std::size_t i, std::size_t count) {
    const Rectangle d = formDialog();
    const int columns = count > 1 ? 2 : 1;
    const float gap = 16.0f;
    const float w = columns == 1 ? d.width - 48.0f : (d.width - 48.0f - gap) * 0.5f;
    const int col = columns == 1 ? 0 : static_cast<int>(i % 2);
    const int row = columns == 1 ? static_cast<int>(i) : static_cast<int>(i / 2);
    return {d.x + 24.0f + col * (w + gap), d.y + 98.0f + row * 62.0f, w, 38.0f};
}
Rectangle formCancel() { const Rectangle d=formDialog(); return {d.x+d.width-248.0f,d.y+d.height-52.0f,104,36}; }
Rectangle formSubmit() { const Rectangle d=formDialog(); return {d.x+d.width-128.0f,d.y+d.height-52.0f,104,36}; }
Rectangle confirmDialog() { return UiLayout::centeredDialog(460.0f, 220.0f, 20.0f); }
Rectangle confirmYes() { const Rectangle d=confirmDialog(); return {d.x+d.width-236.0f,d.y+d.height-58.0f,104,38}; }
Rectangle confirmNo() { const Rectangle d=confirmDialog(); return {d.x+d.width-120.0f,d.y+d.height-58.0f,96,38}; }
bool isChoiceField(std::size_t section, std::size_t field) {
    return (section == 3 && field == 2) || (section == 4 && field == 0) ||
           (section == 5 && (field == 7 || field == 9));
}
void cycleChoice(std::string& value, std::size_t section, std::size_t field) {
    if (section == 3 && field == 2) value = value == "ADMIN" ? "LEARNER" : "ADMIN";
    else if (section == 4 && field == 0) value = value == "KATAKANA" ? "HIRAGANA" : "KATAKANA";
    else if (section == 5 && field == 7) {
        const std::string options[] = {"A", "B", "C", "D"};
        const auto found = std::find(std::begin(options), std::end(options), value);
        const std::size_t index = found == std::end(options) ? 0 : static_cast<std::size_t>(std::distance(std::begin(options), found));
        value = options[(index + 1) % 4];
    } else if (section == 5 && field == 9) value = value == "INACTIVE" ? "ACTIVE" : "INACTIVE";
}
}

bool BackendScreen::click(Rectangle r) {
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), r);
}
void BackendScreen::button(Rectangle r, const char* label, bool primary, bool enabled) {
    UITheme::drawButton(r,label,primary,enabled);
}

BackendScreenEvent BackendScreen::update(std::size_t section, UserRole role, const std::vector<std::string>& rows,
                                         const BackendTableData& table, const BackendActivityState& activity) {
    const bool statisticsPage = role == UserRole::LEARNER && section == 7;
    const BackendUiLayout baseUi = layout();
    const BackendUiLayout ui = statisticsPage ? statisticsLayout(baseUi) : baseUi;
    const bool tableView = isTableSection(section, role);
    const bool accountsTable = role == UserRole::ADMIN && section == 3;
    const AccountTableView filteredAccounts = accountsTable ? accountTableView(table, accountRoleFilter_) : AccountTableView{table,{}};
    const BackendTableData& visibleTable = accountsTable ? filteredAccounts.table : table;
    const std::size_t rowCount = tableView ? visibleTable.rows.size() : rows.size();
    if (deleteConfirm_ || importConfirm_) return updateConfirmation();
    if (formOpen_) return updateForm(section);
    if (role == UserRole::ADMIN && section == 6) {
        Rectangle path{ui.content.x+12,ui.content.y+100,ui.list.width,42};
        if(click(path)) packagePathFocused_=true;
        else if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) packagePathFocused_=false;
        if(packagePathFocused_){
            if((IsKeyPressed(KEY_BACKSPACE)||IsKeyPressedRepeat(KEY_BACKSPACE))&&!packagePath_.empty())Utf8::eraseLast(packagePath_);
            for(int cp=GetCharPressed();cp>0;cp=GetCharPressed())if(cp>=32&&packagePath_.size()<220)packagePath_+=Utf8::encode(cp);
        }
        if(click({path.x,path.y+52,kActionButtonWidth,38}))return {BackendScreenEvent::Type::EXPORT_PACKAGE,0,'A',packagePath_};
        if(click({path.x+kActionButtonWidth+12,path.y+52,kActionButtonWidth,38})){importConfirm_=true;return {};}
    }
    if(click(ui.refresh))return {BackendScreenEvent::Type::REFRESH};
    if(role==UserRole::LEARNER&&section==4)return updateReview(ui.list,ui.actionX,ui.actionY,activity);
    if(role==UserRole::LEARNER&&section==5)return updateQuiz(ui.list,ui.actionX,ui.actionY,activity);
    if(accountsTable&&click(accountRoleFilterBounds(ui))){accountRoleFilter_=(accountRoleFilter_+1)%3;selected_=0;hasSelected_=false;scrollOffset_=0;return {BackendScreenEvent::Type::CLEAR_SELECTION};}
    if(tableView){
        const auto tableUi=tableLayout(ui,section,role,visibleTable);
        if(CheckCollisionPointRec(GetMousePosition(),ui.list)){
            const float wheel=GetMouseWheelMove();const std::size_t maxOffset=rowCount>tableUi.visibleRows?rowCount-tableUi.visibleRows:0;
            if(wheel>0.0f&&scrollOffset_>0)--scrollOffset_;
            if(wheel<0.0f&&scrollOffset_<maxOffset)++scrollOffset_;
        }
        const std::size_t first=rowCount>tableUi.visibleRows?std::min(scrollOffset_,rowCount-tableUi.visibleRows):0;
        for(std::size_t visible=0;!statisticsPage&&visible<tableUi.visibleRows&&first+visible<rowCount;++visible){
            if(click(tableRowBounds(ui,visible))){selected_=accountsTable?filteredAccounts.sourceRows[first+visible]:first+visible;hasSelected_=true;deleteConfirm_=false;return {BackendScreenEvent::Type::SELECT,selected_};}
        }
        scrollOffset_=first;
    }else if(isAlphabetSection(section,role)){
        const auto gridRows=alphabetRows(rows,alphabetKatakana_);const auto alphabetUi=alphabetLayout(ui,gridRows.size());
        if(click(alphabetUi.hiraganaTab)){alphabetKatakana_=false;alphabetScrollOffset_=0;hasSelected_=false;}
        else if(click(alphabetUi.katakanaTab)){alphabetKatakana_=true;alphabetScrollOffset_=0;hasSelected_=false;}
        else{
            if(CheckCollisionPointRec(GetMousePosition(),alphabetUi.bounds)){
                const float wheel=GetMouseWheelMove();const std::size_t maxOffset=gridRows.size()>alphabetUi.visibleRows?gridRows.size()-alphabetUi.visibleRows:0;
                if(wheel>0.0f&&alphabetScrollOffset_>0)--alphabetScrollOffset_;
                if(wheel<0.0f&&alphabetScrollOffset_<maxOffset)++alphabetScrollOffset_;
            }
            for(std::size_t visible=0;visible<alphabetUi.visibleRows&&alphabetScrollOffset_+visible<gridRows.size();++visible){
                const auto& row=gridRows[alphabetScrollOffset_+visible];
                for(std::size_t column=0;column<row.entries.size();++column)
                    if(row.entries[column]&&*row.entries[column]<rows.size()&&click(alphabetCell(alphabetUi,visible,column))){selected_=*row.entries[column];hasSelected_=true;return {BackendScreenEvent::Type::SELECT,selected_};}
            }
        }
    }else{
        if(CheckCollisionPointRec(GetMousePosition(),ui.list)){const float wheel=GetMouseWheelMove();if(wheel>0&&scrollOffset_>0)--scrollOffset_;if(wheel<0&&scrollOffset_+ui.visibleRows<rowCount)++scrollOffset_;}
        for(std::size_t i=0;i<ui.visibleRows&&scrollOffset_+i<rowCount;++i){Rectangle r{ui.list.x,ui.list.y+static_cast<float>(i)*34,ui.list.width,32};if(click(r)){selected_=scrollOffset_+i;hasSelected_=true;deleteConfirm_=false;return {BackendScreenEvent::Type::SELECT,selected_};}}
    }
    return updateActions(section,role,ui.actionX,ui.actionY);
}

BackendScreenEvent BackendScreen::updateConfirmation(){
    if(IsKeyPressed(KEY_ESCAPE)||click(confirmNo())){deleteConfirm_=false;importConfirm_=false;return {};}
    if(click(confirmYes())){if(importConfirm_){importConfirm_=false;return {BackendScreenEvent::Type::IMPORT_PACKAGE,0,'A',packagePath_};}deleteConfirm_=false;return {BackendScreenEvent::Type::DELETE_ROW,selected_};}
    return {};
}

BackendScreenEvent BackendScreen::updateForm(std::size_t section){
    const std::size_t count=resetPasswordForm_?1:section==2?4:section==3?3:section==4?6:10;
    if(IsKeyPressed(KEY_ESCAPE)){formOpen_=false;return {BackendScreenEvent::Type::CANCEL_FORM};}
    for(std::size_t i=0;i<count;++i)if(click(formField(i,count))){focusedField_=i;if(isChoiceField(section,i))cycleChoice(formFields_[i],section,i);}
    if(IsKeyPressed(KEY_TAB))focusedField_=(focusedField_+1)%count;
    auto& value=formFields_[focusedField_];const bool choiceFocused=isChoiceField(section,focusedField_);
    if(!choiceFocused&&(IsKeyPressed(KEY_BACKSPACE)||IsKeyPressedRepeat(KEY_BACKSPACE))&&!value.empty())Utf8::eraseLast(value);
    for(int cp=GetCharPressed();cp>0;cp=GetCharPressed())if(!choiceFocused&&cp>=32&&value.size()<240)value+=Utf8::encode(cp);
    if(choiceFocused&&IsKeyPressed(KEY_ENTER))cycleChoice(value,section,focusedField_);
    if(click(formSubmit())){formOpen_=false;BackendScreenEvent e;e.type=resetPasswordForm_?BackendScreenEvent::Type::RESET_PASSWORD:editing_?BackendScreenEvent::Type::UPDATE_ROW:BackendScreenEvent::Type::CREATE_ROW;e.index=section;e.text=editId_;e.fields=formFields_;editing_=false;resetPasswordForm_=false;return e;}
    if(click(formCancel())){formOpen_=false;return {BackendScreenEvent::Type::CANCEL_FORM};}
    return {};
}

BackendScreenEvent BackendScreen::updateReview(Rectangle list,float x,float y,const BackendActivityState& activity){
    const auto& state=activity.review;const bool active=state&&!state->finished&&state->current.has_value();const bool revealed=active&&state->current->back.has_value();
    if(active&&revealed){const auto details=reviewDisplayDetails(activity,*state->current);BackendUiLayout ui{};ui.list=list;const Rectangle viewport=reviewDetailsViewport(ui);const float maxScroll=std::max(0.0f,reviewDetailsContentHeight(details,viewport.width)-viewport.height);
        if(CheckCollisionPointRec(GetMousePosition(),viewport))reviewDetailsScrollOffset_=std::clamp(reviewDetailsScrollOffset_-GetMouseWheelMove()*48.0f,0.0f,maxScroll);else reviewDetailsScrollOffset_=std::clamp(reviewDetailsScrollOffset_,0.0f,maxScroll);
    }else reviewDetailsScrollOffset_=0.0f;
    if((!state||state->finished)&&click(action(x,y))){reviewDetailsScrollOffset_=0.0f;return {BackendScreenEvent::Type::START_REVIEW};}
    if(active&&!revealed&&click(action(x,y+48.0f))){reviewDetailsScrollOffset_=0.0f;return {BackendScreenEvent::Type::REVEAL_REVIEW};}
    if(active&&revealed&&click(action(x,y+96.0f,80.0f,40.0f))){reviewDetailsScrollOffset_=0.0f;return {BackendScreenEvent::Type::RATE_KNOWN};}
    if(active&&revealed&&click(action(x+90.0f,y+96.0f,80.0f,40.0f))){reviewDetailsScrollOffset_=0.0f;return {BackendScreenEvent::Type::RATE_UNKNOWN};}
    if(active&&click(action(x,y+144.0f))){reviewDetailsScrollOffset_=0.0f;return {BackendScreenEvent::Type::STOP_REVIEW};}
    return {};
}

BackendScreenEvent BackendScreen::updateQuiz(Rectangle list, float x, float y,
                                             const BackendActivityState& activity) {
    BackendUiLayout ui{};
    ui.list = list;
    const auto quizUi = quizLayout(ui);
    const bool hasSession = activity.quiz.has_value();
    const bool active = hasSession && !activity.quiz->submitted;
    const bool submitted = hasSession && activity.quiz->submitted;

    if (!hasSession && activity.quizLevels.size() > 1 && click(quizUi.level)) {
        const auto current = std::find_if(activity.quizLevels.begin(), activity.quizLevels.end(),
            [&](const Level& level) { return activity.selectedQuizLevel && level.getId() == *activity.selectedQuizLevel; });
        const std::size_t currentIndex = current == activity.quizLevels.end()
            ? 0 : static_cast<std::size_t>(std::distance(activity.quizLevels.begin(), current));
        const Level& next = activity.quizLevels[(currentIndex + 1) % activity.quizLevels.size()];
        return {BackendScreenEvent::Type::SELECT_QUIZ_LEVEL, 0, 'A', next.getId().getId()};
    }
    if (active && !activity.quiz->questions.empty()) {
        quizQuestionIndex_ = std::min(quizQuestionIndex_, activity.quiz->questions.size() - 1);
        for (std::size_t option = 0; option < 4; ++option) {
            if (click(quizUi.options[option]))
                return {BackendScreenEvent::Type::ANSWER, quizQuestionIndex_, static_cast<char>('A' + option)};
        }
        if (quizQuestionIndex_ > 0 && click(quizUi.previous)) --quizQuestionIndex_;
        else if (quizQuestionIndex_ + 1 < activity.quiz->questions.size() && click(quizUi.next)) ++quizQuestionIndex_;
    }
    if (!hasSession && activity.activeQuestionCount > 0 && click(action(x, y))) {
        quizQuestionIndex_ = 0;
        return {BackendScreenEvent::Type::START_QUIZ};
    }
    if (active && click(action(x, y + 48.0f))) return {BackendScreenEvent::Type::SUBMIT_QUIZ};
    const bool hasWrongAnswers = quizHasWrongAnswers(activity);
    if (submitted && hasWrongAnswers && click(action(x, y + 48.0f))) {
        quizQuestionIndex_ = 0;
        return {BackendScreenEvent::Type::RETRY_WRONG};
    }
    if (hasSession && click(action(x, y + (submitted && !hasWrongAnswers ? 120.0f : 96.0f))))
        return {BackendScreenEvent::Type::STOP_QUIZ};
    if (submitted && !activity.quizAnswers.empty() && CheckCollisionPointRec(GetMousePosition(), list)) {
        const float wheel = GetMouseWheelMove();
        const std::size_t visible = std::max<std::size_t>(1,
            static_cast<std::size_t>((list.height - 112.0f) / 128.0f));
        const std::size_t maxOffset = activity.quizAnswers.size() > visible
            ? activity.quizAnswers.size() - visible : 0;
        if (wheel > 0.0f && scrollOffset_ > 0) --scrollOffset_;
        if (wheel < 0.0f && scrollOffset_ < maxOffset) ++scrollOffset_;
        scrollOffset_ = std::min(scrollOffset_, maxOffset);
    }
    return {};
}

BackendScreenEvent BackendScreen::updateActions(std::size_t section, UserRole role, float x, float y) {
    if (role == UserRole::ADMIN && section >= 2 && section <= 4 && click(action(x, y + 48))) {
        formOpen_ = true;
        editing_ = false;
        resetPasswordForm_ = false;
        focusedField_ = 0;
        formFields_ = {};
        if (section == 3) formFields_[2] = "LEARNER";
        if (section == 4) formFields_[0] = "HIRAGANA";
        return {};
    }
    if (role == UserRole::ADMIN && section == 5 && click(action(x, y + 96))) {
        formOpen_ = true;
        editing_ = false;
        resetPasswordForm_ = false;
        focusedField_ = 0;
        formFields_ = {};
        formFields_[7] = "A";
        formFields_[9] = "ACTIVE";
        return {};
    }
    if (role == UserRole::ADMIN && hasSelected_ && (section == 2 || section == 4) && click(action(x, y + 96)))
        return {BackendScreenEvent::Type::EDIT_ROW, selected_};
    if (role == UserRole::ADMIN && section == 5 && hasSelected_ && click(action(x, y + 144)))
        return {BackendScreenEvent::Type::EDIT_ROW, selected_};
    if (role == UserRole::ADMIN && section == 3 && hasSelected_ && click(action(x, y + 96)))
        return {BackendScreenEvent::Type::EDIT_ROW, selected_};
    if (role == UserRole::ADMIN && section == 3 && hasSelected_ && click(action(x, y)))
        return {BackendScreenEvent::Type::TOGGLE_ACCOUNT, selected_};
    if (role == UserRole::ADMIN && section == 2 && hasSelected_ && click(action(x, y))) {
        deleteConfirm_ = true;
        return {};
    }
    if (role == UserRole::ADMIN && section == 4 && hasSelected_ && click(action(x, y))) {
        deleteConfirm_ = true;
        return {};
    }
    if (role == UserRole::ADMIN && section == 5 && hasSelected_ && click(action(x, y)))
        return {BackendScreenEvent::Type::TOGGLE_QUESTION, selected_};
    if (role == UserRole::ADMIN && section == 5 && hasSelected_ && click(action(x, y + 48))) {
        deleteConfirm_ = true;
        return {};
    }
    if (role == UserRole::LEARNER && section == 2 && hasSelected_ && click(action(x, y)))
        return {BackendScreenEvent::Type::REMOVE_FAVORITE, selected_};
    if (role == UserRole::LEARNER && section == 3) {
        if (hasSelected_ && click(action(x, y))) return {BackendScreenEvent::Type::REPLAY_HISTORY, selected_};
        if (click(action(x, y + 48))) return {BackendScreenEvent::Type::CLEAR_HISTORY};
    }
    if (role == UserRole::LEARNER && section == 4) {
        if (click(action(x, y))) return {BackendScreenEvent::Type::START_REVIEW};
        if (click(action(x, y + 48))) return {BackendScreenEvent::Type::REVEAL_REVIEW};
        if (click(action(x, y + 96, 80, 40))) return {BackendScreenEvent::Type::RATE_KNOWN};
        if (click(action(x + 90, y + 96, 80, 40))) return {BackendScreenEvent::Type::RATE_UNKNOWN};
        if (click(action(x, y + 144))) return {BackendScreenEvent::Type::STOP_REVIEW};
    }
    if (role == UserRole::LEARNER && section == 5) {
        if (click(action(x, y))) return {BackendScreenEvent::Type::START_QUIZ};
        const Rectangle answers[] = {action(x, y + 48, 78, 40), action(x + 90, y + 48, 78, 40),
                                     action(x, y + 96, 78, 40), action(x + 90, y + 96, 78, 40)};
        for (int i = 0; i < 4; ++i) {
            if (click(answers[i])) return {BackendScreenEvent::Type::ANSWER, 0, static_cast<char>('A' + i)};
        }
        if (click(action(x, y + 144))) return {BackendScreenEvent::Type::SUBMIT_QUIZ};
        if (click(action(x, y + 192))) return {BackendScreenEvent::Type::RETRY_WRONG};
        if (click(action(x, y + 240))) return {BackendScreenEvent::Type::STOP_QUIZ};
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

void BackendScreen::drawReviewPage(Rectangle list, const BackendActivityState& activity, float scrollOffset) const {
        BackendUiLayout ui{};
        ui.list = list;
    DrawRectangleRounded(ui.list,0.04f,8,Color{255,252,253,255});
    DrawRectangleRoundedLinesEx(ui.list,0.04f,8,1.0f,kBorder);
    if (!activity.review) {
        UITheme::drawWrapped("No review session is active. Start a review to load cards from your favorites.",
            static_cast<int>(ui.list.x + 24.0f), static_cast<int>(ui.list.y + 34.0f),
            static_cast<int>(ui.list.width - 48.0f), 19, kMuted, 4);
    } else if (activity.review->finished) {
        const char* endText = activity.reviewStopped ? "Review stopped" : "Review complete";
        UITheme::drawText(endText, static_cast<int>(ui.list.x + 24.0f), static_cast<int>(ui.list.y + 34.0f), 26, kText);
        UITheme::drawWrapped(TextFormat("Ratings recorded: %zu", activity.review->ratingCount),
            static_cast<int>(ui.list.x + 24.0f), static_cast<int>(ui.list.y + 82.0f),
            static_cast<int>(ui.list.width - 48.0f), 18, kMuted, 3);
    } else if (activity.review->current) {
        const auto& card = *activity.review->current;
        UITheme::drawText(TextFormat("Review step %zu", activity.review->ratingCount + 1),
            static_cast<int>(ui.list.x + 24.0f), static_cast<int>(ui.list.y + 22.0f), 16, kMuted);
        UITheme::drawWrapped(card.front, static_cast<int>(ui.list.x + 24.0f), static_cast<int>(ui.list.y + 64.0f),
            static_cast<int>(ui.list.width - 48.0f), 34, UITheme::strongText, 2);
        UITheme::drawText(TextFormat("%zu remaining", activity.review->remaining),
            static_cast<int>(ui.list.x + 24.0f), static_cast<int>(ui.list.y + ui.list.height - 38.0f), 16, kMuted);
        if (card.back) {
            const auto details = reviewDisplayDetails(activity, card);
            const Rectangle viewport = reviewDetailsViewport(ui);
            const float contentHeight = reviewDetailsContentHeight(details, viewport.width);
            const float maxScroll = std::max(0.0f, contentHeight - viewport.height);
            const float scroll = std::clamp(scrollOffset, 0.0f, maxScroll);
            const float columnGap = 10.0f;
            const float rowGap = 8.0f;
            const float tileWidth = (viewport.width - columnGap) * 0.5f;
            const std::size_t fieldRows = (details.fields.size() + 1) / 2;
            const auto rowHeights = reviewFieldRowHeights(details, viewport.width);
            BeginScissorMode(static_cast<int>(viewport.x), static_cast<int>(viewport.y),
                std::max(1, static_cast<int>(viewport.width - 8.0f)), std::max(1, static_cast<int>(viewport.height)));
            float rowY = viewport.y;
            for (std::size_t index = 0; index < details.fields.size(); ++index) {
                const std::size_t row = index / 2;
                const std::size_t column = index % 2;
                if (column == 0 && row > 0) rowY += rowHeights[row - 1] + rowGap;
                const Rectangle tile{viewport.x + static_cast<float>(column) * (tileWidth + columnGap),
                    rowY - scroll, tileWidth, rowHeights[row]};
                DrawRectangleRounded(tile, 0.08f, 7, RAYWHITE);
                DrawRectangleRoundedLinesEx(tile, 0.08f, 7, 1.0f, kBorder);
                UITheme::drawText(details.fields[index].first.c_str(), static_cast<int>(tile.x + 10.0f),
                    static_cast<int>(tile.y + 6.0f), 13, kMuted);
                UITheme::drawWrapped(details.fields[index].second, static_cast<int>(tile.x + 10.0f),
                    static_cast<int>(tile.y + 25.0f), static_cast<int>(tile.width - 20.0f), 15, kText,
                    static_cast<int>(wrappedLineCount(details.fields[index].second,
                        std::max(1, static_cast<int>(tile.width - 20.0f)), 15)));
            }
            float exampleY = viewport.y;
            for (const float rowHeight : rowHeights) exampleY += rowHeight + rowGap;
            if (fieldRows > 0) exampleY -= rowGap;
            if (!details.example.empty()) {
                const std::size_t lines = wrappedLineCount(details.example,
                    std::max(1, static_cast<int>(viewport.width - 24.0f)), 16);
                const float exampleHeight = 36.0f + static_cast<float>(lines) * 21.0f;
                const Rectangle exampleBounds{viewport.x, exampleY - scroll, viewport.width - 8.0f, exampleHeight};
                DrawRectangleRounded(exampleBounds, 0.08f, 7, Color{255,239,245,255});
                UITheme::drawText(details.exampleLabel.c_str(), static_cast<int>(exampleBounds.x + 12.0f),
                    static_cast<int>(exampleBounds.y + 8.0f), 13, kMuted);
                UITheme::drawWrapped(details.example, static_cast<int>(exampleBounds.x + 12.0f),
                    static_cast<int>(exampleBounds.y + 28.0f), static_cast<int>(exampleBounds.width - 24.0f), 16, kText,
                    static_cast<int>(lines));
            }
            EndScissorMode();
            if (maxScroll > 0.0f) {
                const Rectangle track{viewport.x + viewport.width - 4.0f, viewport.y, 3.0f, viewport.height};
                DrawRectangleRounded(track, 0.5f, 3, Color{239,224,230,255});
                const float thumbHeight = std::max(20.0f, track.height * viewport.height / contentHeight);
                DrawRectangleRounded({track.x, track.y + (track.height - thumbHeight) * scroll / maxScroll,
                    track.width, thumbHeight}, 0.5f, 3, kBorder);
            }
        } else {
            UITheme::drawWrapped("Reveal the answer before marking this card Known or Again.",
                static_cast<int>(ui.list.x + 24.0f), static_cast<int>(ui.list.y + ui.list.height * 0.60f),
                static_cast<int>(ui.list.width - 48.0f), 16, kMuted, 3);
        }
    }

}

void BackendScreen::drawQuizPage(Rectangle list, const BackendActivityState& activity, std::size_t questionIndex,
                                 std::size_t scrollOffset) const {
        BackendUiLayout ui{};
        ui.list = list;
    DrawRectangleRounded(ui.list,0.04f,8,Color{255,252,253,255});
    DrawRectangleRoundedLinesEx(ui.list,0.04f,8,1.0f,kBorder);
    const auto quizUi = quizLayout(ui);
    const bool hasSession = activity.quiz.has_value();
    const bool active = hasSession && !activity.quiz->submitted;
    const bool submitted = hasSession && activity.quiz->submitted;
    std::string selectedLevelName = "Select JLPT level";
    if (activity.selectedQuizLevel) {
        const auto found = std::find_if(activity.quizLevels.begin(), activity.quizLevels.end(), [&](const Level& level) {
            return level.getId() == *activity.selectedQuizLevel;
        });
        selectedLevelName = found == activity.quizLevels.end() ? activity.selectedQuizLevel->getId() : found->getName();
    }
    button(quizUi.level, selectedLevelName.c_str(), false, !hasSession && activity.quizLevels.size() > 1);
    UITheme::drawText(TextFormat("%zu active questions", activity.activeQuestionCount),
        static_cast<int>(quizUi.level.x + quizUi.level.width + 14.0f),
        static_cast<int>(quizUi.level.y + 10.0f), 16, kMuted);
    if (active && !activity.quiz->questions.empty()) {
        const std::size_t index = std::min(questionIndex, activity.quiz->questions.size() - 1);
        const auto& question = activity.quiz->questions[index];
        const std::size_t answered = static_cast<std::size_t>(std::count_if(activity.quiz->questions.begin(), activity.quiz->questions.end(),
            [](const QuizQuestionView& item) { return item.selectedAnswer != '-'; }));
        UITheme::drawText(TextFormat("Question %zu of %zu  |  Answered %zu", index + 1,
            activity.quiz->questions.size(), answered), static_cast<int>(ui.list.x + 16.0f),
            static_cast<int>(ui.list.y + 58.0f), 17, kMuted);
        UITheme::drawWrapped(question.prompt, static_cast<int>(ui.list.x + 16.0f),
            static_cast<int>(ui.list.y + 90.0f), static_cast<int>(ui.list.width - 32.0f), 20, kText, 3);
        for (std::size_t option = 0; option < 4; ++option) {
            const Rectangle bounds = quizUi.options[option];
            const bool chosen = question.selectedAnswer == static_cast<char>('A' + option);
            DrawRectangleRounded(bounds,0.12f,7,chosen?Color{255,224,234,255}:RAYWHITE);
            DrawRectangleRoundedLinesEx(bounds,0.12f,7,chosen?2.0f:1.0f,chosen?UITheme::primary:kBorder);
            BeginScissorMode(static_cast<int>(bounds.x+10.0f),static_cast<int>(bounds.y+4.0f),
                std::max(1,static_cast<int>(bounds.width-20.0f)),static_cast<int>(bounds.height-8.0f));
            UITheme::drawWrapped(std::string(1,static_cast<char>('A'+option))+". "+question.options[option],
                static_cast<int>(bounds.x+12.0f),static_cast<int>(bounds.y+9.0f),
                static_cast<int>(bounds.width-24.0f),17,kText,2);
            EndScissorMode();
        }
        button(quizUi.previous,"Previous",false,index>0);
        button(quizUi.next,"Next",false,index+1<activity.quiz->questions.size());
    } else if (submitted) {
        if (activity.quizResult) {
            UITheme::drawText(TextFormat("Score: %d / %d  (%.0f%%)", activity.quizResult->getCorrectAnswers(),
                activity.quizResult->getTotalQuestions(), activity.quizResult->percentage()),
                static_cast<int>(ui.list.x+16.0f),static_cast<int>(ui.list.y+58.0f),22,UITheme::strongText);
        } else {
            UITheme::drawText("Quiz submitted",static_cast<int>(ui.list.x+16.0f),static_cast<int>(ui.list.y+58.0f),22,UITheme::strongText);
        }
        const float rowHeight = 128.0f;
        const float rowsTop = ui.list.y + 96.0f;
        const std::size_t visible = std::max<std::size_t>(1,static_cast<std::size_t>((ui.list.height-112.0f)/rowHeight));
        const std::size_t maxOffset = activity.quizAnswers.size()>visible?activity.quizAnswers.size()-visible:0;
        const std::size_t first = std::min(scrollOffset,maxOffset);
        BeginScissorMode(static_cast<int>(ui.list.x+6.0f),static_cast<int>(rowsTop),
            static_cast<int>(ui.list.width-18.0f),static_cast<int>(ui.list.height-108.0f));
        for(std::size_t visibleIndex=0;visibleIndex<visible&&first+visibleIndex<activity.quizAnswers.size();++visibleIndex){
            const auto& answer=activity.quizAnswers[first+visibleIndex];
            const float y=rowsTop+static_cast<float>(visibleIndex)*rowHeight;
            DrawRectangleRounded({ui.list.x+10.0f,y+2.0f,ui.list.width-30.0f,rowHeight-8.0f},0.06f,6,
                answer.selectedAnswer==answer.correctAnswer?Color{241,250,244,255}:Color{255,244,246,255});
            UITheme::drawText(TextFormat("Question %zu",first+visibleIndex+1),static_cast<int>(ui.list.x+20.0f),static_cast<int>(y+8.0f),16,kMuted);
            const auto question=std::find_if(activity.quiz->questions.begin(),activity.quiz->questions.end(),[&](const QuizQuestionView& item){return item.id==answer.id;});
            std::string selectedText=answer.selectedAnswer=='-'?"Not answered":std::string(1,answer.selectedAnswer);
            std::string correctText(1,answer.correctAnswer);
            if(question!=activity.quiz->questions.end()){
                const int selectedIndex=answer.selectedAnswer-'A';
                const int correctIndex=answer.correctAnswer-'A';
                if(selectedIndex>=0&&selectedIndex<static_cast<int>(question->options.size()))selectedText+=" - "+question->options[static_cast<std::size_t>(selectedIndex)];
                if(correctIndex>=0&&correctIndex<static_cast<int>(question->options.size()))correctText+=" - "+question->options[static_cast<std::size_t>(correctIndex)];
            }
            UITheme::drawWrapped("Your answer: "+selectedText,static_cast<int>(ui.list.x+20.0f),static_cast<int>(y+29.0f),
                static_cast<int>(ui.list.width-58.0f),14,kText,2);
            UITheme::drawWrapped("Correct answer: "+correctText,static_cast<int>(ui.list.x+20.0f),static_cast<int>(y+58.0f),
                static_cast<int>(ui.list.width-58.0f),14,kText,2);
            UITheme::drawWrapped("Explanation: "+answer.explanation,static_cast<int>(ui.list.x+20.0f),static_cast<int>(y+88.0f),
                static_cast<int>(ui.list.width-58.0f),14,kMuted,2);
        }
        EndScissorMode();
    } else if (activity.quizLevels.empty()) {
        UITheme::drawWrapped("No JLPT levels are available for a quiz.",static_cast<int>(ui.list.x+18.0f),
            static_cast<int>(ui.list.y+88.0f),static_cast<int>(ui.list.width-36.0f),18,kMuted,3);
    } else if (activity.activeQuestionCount == 0) {
        UITheme::drawWrapped("This level has no active questions. Choose another level or ask an administrator to activate questions.",
            static_cast<int>(ui.list.x+18.0f),static_cast<int>(ui.list.y+88.0f),static_cast<int>(ui.list.width-36.0f),18,kMuted,4);
    } else {
        UITheme::drawWrapped("This level is ready. Start the quiz when you are ready.",static_cast<int>(ui.list.x+18.0f),
            static_cast<int>(ui.list.y+88.0f),static_cast<int>(ui.list.width-36.0f),18,kMuted,3);
    }
    if (submitted && activity.quizAnswers.size() > std::max<std::size_t>(1,static_cast<std::size_t>((ui.list.height-112.0f)/128.0f))) {
        const Rectangle track{ui.list.x+ui.list.width-7.0f,ui.list.y+98.0f,3.0f,ui.list.height-112.0f};
        DrawRectangleRounded(track,0.5f,3,Color{239,224,230,255});
        const std::size_t visible=std::max<std::size_t>(1,static_cast<std::size_t>((ui.list.height-112.0f)/128.0f));
        const std::size_t maxOffset=activity.quizAnswers.size()-visible;
        const float thumb=std::max(22.0f,track.height*static_cast<float>(visible)/static_cast<float>(activity.quizAnswers.size()));
        DrawRectangleRounded({track.x,track.y+(track.height-thumb)*(maxOffset?static_cast<float>(std::min(scrollOffset,maxOffset))/maxOffset:0.0f),track.width,thumb},0.5f,3,kBorder);
    }

}

void BackendScreen::drawTablePage(Rectangle content, Rectangle list, std::size_t section, UserRole role,
                                  const BackendTableData& table, const std::vector<std::size_t>& sourceRows,
                                  std::size_t selected, bool statisticsPage) const {
        BackendUiLayout ui{};
        ui.content = content;
        ui.list = list;
        const bool accountsTable = role == UserRole::ADMIN && section == 3;
        const BackendTableData& visibleTable = table;
        AccountTableView filteredAccounts;
        filteredAccounts.sourceRows = sourceRows;
    DrawRectangleRounded(ui.list,0.04f,8,Color{255,252,253,255});
    DrawRectangleRoundedLinesEx(ui.list,0.04f,8,1.0f,kBorder);
    const auto tableUi = tableLayout(ui, section, role, visibleTable);
    const float headerY = ui.list.y + 14.0f;
    for (std::size_t column = 0; column < visibleTable.headers.size(); ++column) {
        const float columnX = tableColumnX(tableUi, column);
        const float columnWidth = tableColumnWidth(tableUi, column);
        BeginScissorMode(static_cast<int>(columnX), static_cast<int>(ui.list.y + 4.0f),
                         std::max(1, static_cast<int>(columnWidth - 8.0f)), 32);
        UITheme::drawText(visibleTable.headers[column].c_str(), static_cast<int>(columnX + 4.0f),
                          static_cast<int>(headerY), 18, UITheme::strongText);
        EndScissorMode();
    }
    DrawLine(static_cast<int>(ui.list.x + 8.0f), static_cast<int>(ui.list.y + 42.0f),
             static_cast<int>(ui.list.x + ui.list.width - 14.0f), static_cast<int>(ui.list.y + 42.0f), kBorder);
    const std::size_t maxOffset = visibleTable.rows.size() > tableUi.visibleRows ? visibleTable.rows.size() - tableUi.visibleRows : 0;
    const std::size_t first = std::min(scrollOffset_, maxOffset);
    BeginScissorMode(static_cast<int>(tableUi.clip.x), static_cast<int>(tableUi.clip.y),
                     static_cast<int>(tableUi.clip.width), static_cast<int>(tableUi.clip.height));
    for (std::size_t visible = 0; visible < tableUi.visibleRows && first + visible < visibleTable.rows.size(); ++visible) {
        const std::size_t source = first + visible;
        const std::size_t originalSource = accountsTable ? filteredAccounts.sourceRows[source] : source;
        const Rectangle row = tableRowBounds(ui, visible);
        if ((!statisticsPage && originalSource == selected) || CheckCollisionPointRec(GetMousePosition(), row))
            DrawRectangleRounded(row,0.10f,5,kHover);
        for (std::size_t column = 0; column < visibleTable.headers.size() && column < visibleTable.rows[source].size(); ++column) {
            const float columnX = tableColumnX(tableUi, column);
            const float columnWidth = tableColumnWidth(tableUi, column);
            BeginScissorMode(static_cast<int>(columnX + 4.0f), static_cast<int>(row.y + 2.0f),
                             std::max(1, static_cast<int>(columnWidth - 8.0f)), static_cast<int>(row.height - 4.0f));
            UITheme::drawText(visibleTable.rows[source][column].c_str(), static_cast<int>(columnX + 4.0f),
                              static_cast<int>(row.y + (row.height - 18.0f) * 0.5f), 18, kText);
            EndScissorMode();
        }
    }
    EndScissorMode();
    if (visibleTable.rows.size() > tableUi.visibleRows) {
        const Rectangle track{ui.list.x + ui.list.width - 8.0f, tableUi.clip.y, 3.0f, tableUi.clip.height};
        DrawRectangleRounded(track,0.5f,3,Color{239,224,230,255});
        const float thumbHeight = std::max(22.0f, track.height * static_cast<float>(tableUi.visibleRows) / static_cast<float>(visibleTable.rows.size()));
        const float offsetRatio = maxOffset > 0 ? static_cast<float>(first) / static_cast<float>(maxOffset) : 0.0f;
        DrawRectangleRounded({track.x,track.y+(track.height-thumbHeight)*offsetRatio,track.width,thumbHeight},0.5f,3,kBorder);
    }
    if (visibleTable.rows.empty())
        UITheme::drawWrapped(statisticsPage ? "No quiz attempts recorded yet." : "No records are available.", static_cast<int>(ui.list.x + 14.0f),
                             static_cast<int>(tableUi.clip.y + 12.0f), static_cast<int>(ui.list.width - 40.0f), 15, kMuted, 2);

}

void BackendScreen::drawAlphabetPage(Rectangle list, const std::vector<std::string>& rows, std::size_t selected,
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
            const auto fields = alphabetFields(rows[source]);
            const Rectangle cell = alphabetCell(alphabetUi, visible, column);
            const bool active = source == selected;
            const bool hover = CheckCollisionPointRec(GetMousePosition(), cell);
            DrawRectangleRounded(cell,0.14f,6,active ? Color{255,224,234,255} : hover ? Color{255,239,244,255} : RAYWHITE);
            DrawRectangleRoundedLinesEx(cell,0.14f,6,active ? 2.0f : 1.0f,active ? UITheme::primary : kBorder);
            const int glyphSize = static_cast<int>(std::clamp(alphabetUi.cellHeight * 0.54f, 30.0f, 34.0f));
            const int glyphWidth = UITheme::measureText(fields[1].c_str(), glyphSize);
            UITheme::drawText(fields[1].c_str(), static_cast<int>(cell.x + (cell.width - glyphWidth) * 0.5f),
                              static_cast<int>(cell.y + 3.0f), glyphSize, active ? UITheme::strongText : kText);
            const int romajiWidth = UITheme::measureText(fields[2].c_str(), 17);
            UITheme::drawText(fields[2].c_str(), static_cast<int>(cell.x + (cell.width - romajiWidth) * 0.5f),
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

void BackendScreen::drawRowsPage(Rectangle list, const std::vector<std::string>& rows, std::size_t selected,
                                 std::size_t scrollOffset, std::size_t visibleRows, bool importExport) const {
        BackendUiLayout ui{};
        ui.list = list;
    BeginScissorMode(static_cast<int>(ui.list.x),static_cast<int>(ui.list.y),static_cast<int>(ui.list.width),static_cast<int>(ui.list.height));
    for(std::size_t i=scrollOffset;i<rows.size()&&i<scrollOffset+visibleRows;++i){
        Rectangle r{ui.list.x,ui.list.y+static_cast<float>(i-scrollOffset)*34,ui.list.width,32};
        if(i==selected||CheckCollisionPointRec(GetMousePosition(),r))DrawRectangleRounded(r,0.12f,5,kHover);
        UITheme::drawText(rows[i].c_str(),static_cast<int>(r.x+8),static_cast<int>(r.y+8),14,kText);
    }
    EndScissorMode();
    if(rows.empty()&&!importExport)UITheme::drawWrapped("No data is available for this view.",static_cast<int>(ui.list.x+8),static_cast<int>(ui.list.y+12),static_cast<int>(ui.list.width-16),15,kMuted,3);

}

void BackendScreen::drawActions(Rectangle content, Rectangle list, float actionX, float actionY, std::size_t section,
                                UserRole role, const std::string& message, bool messageError,
                                const BackendActivityState& activity) const {
    const float x=actionX,y=actionY;
    const bool reviewPage = role == UserRole::LEARNER && section == 4;
    const bool quizPage = role == UserRole::LEARNER && section == 5;
    if (reviewPage) {
        const bool active = activity.review && !activity.review->finished && activity.review->current.has_value();
        const bool revealed = active && activity.review->current->back.has_value();
        button(action(x,y),activity.review&& !activity.review->finished ? "Review active" : "Start review",
               !activity.review || activity.review->finished, !activity.review || activity.review->finished);
        if (active && !revealed) button(action(x,y+48),"Reveal answer",true,true);
        if (active && revealed) {
            button(action(x,y+96,80,40),"Known",true,true);
            button(action(x+90,y+96,80,40),"Again",false,true);
        }
        if (active) button(action(x,y+144),"Stop review",false,true);
    } else if (quizPage) {
        const bool hasSession = activity.quiz.has_value();
        const bool active = hasSession && !activity.quiz->submitted;
        const bool submitted = hasSession && activity.quiz->submitted;
        const bool hasWrongAnswers = quizHasWrongAnswers(activity);
        if (!hasSession) {
            button(action(x,y),activity.activeQuestionCount>0
                ? TextFormat("Start quiz (%zu)",std::min<std::size_t>(10,activity.activeQuestionCount))
                : "No active questions",true,activity.activeQuestionCount>0);
        } else if (active) {
            button(action(x,y),"Quiz in progress",false,false);
            button(action(x,y+48),"Submit quiz",true,true);
            button(action(x,y+96),"Stop quiz",false,true);
        } else if (submitted) {
            button(action(x,y),"Quiz submitted",false,false);
            if (hasWrongAnswers) button(action(x,y+48),"Retry wrong",true,true);
            else UITheme::drawWrapped("Great job! You got all questions correct.", static_cast<int>(x), static_cast<int>(y + 52.0f),
                                      static_cast<int>(kActionButtonWidth), 14, UITheme::strongText, 3);
            button(action(x,y+(hasWrongAnswers?96.0f:120.0f)),"Stop quiz",false,true);
        }
    }
    if(role==UserRole::ADMIN&&section==2)button(action(x,y),"Delete level");
    if(role==UserRole::ADMIN&&section>=2&&section<=4)button(action(x,y+48),"Add new",true);
    if(role==UserRole::ADMIN&&section==5)button(action(x,y+96),"Add question",true);
    if(role==UserRole::ADMIN&&(section==2||section==4))button(action(x,y+96),"Edit selected");
    if(role==UserRole::ADMIN&&section==5)button(action(x,y+144),"Edit question");
    if(role==UserRole::ADMIN&&section==3)button(action(x,y+96),"Reset password");
    if(role==UserRole::ADMIN&&section==3)button(action(x,y),"Toggle lock");
    if(role==UserRole::ADMIN&&section==4)button(action(x,y),"Delete entry");
    if(role==UserRole::ADMIN&&section==5){button(action(x,y),"Toggle question");button(action(x,y+48),"Delete question");}
    if(role==UserRole::ADMIN&&section==6){
        const Rectangle path{content.x+12,content.y+100,list.width,42};
        UITheme::drawField(path,"Package folder",packagePath_,packagePathFocused_);
        button({path.x,path.y+52,kActionButtonWidth,38},"Export package",true);
        button({path.x+kActionButtonWidth+12,path.y+52,kActionButtonWidth,38},"Import package");
        UITheme::drawWrapped("Export creates the package folder. Import requires resource_levels.txt and the related resource files.",static_cast<int>(path.x),static_cast<int>(path.y+106),static_cast<int>(list.width),14,kMuted,3);
    }
    if(role==UserRole::LEARNER&&section==2)button(action(x,y),"Remove favorite");
    if(role==UserRole::LEARNER&&section==3){button(action(x,y),"Search again",true);button(action(x,y+48),"Clear history");}
    if(!message.empty())UITheme::drawStatusBanner(
        {content.x+12.0f,content.y+content.height-52.0f,list.width,40.0f},message,messageError);
    if(formOpen_){
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Color{46,31,38,150});
        const Rectangle modal=formDialog();DrawRectangleRounded(modal,0.04f,10,RAYWHITE);DrawRectangleRoundedLinesEx(modal,0.04f,10,1.5f,kBorder);
        const char* formTitle=resetPasswordForm_?"Reset password":editing_?(section==2?"Edit level":section==4?"Edit alphabet entry":"Edit question"):section==2?"Add level":section==3?"Create account":section==4?"Add alphabet entry":"Add question";
        UITheme::drawText(formTitle,static_cast<int>(modal.x+24),static_cast<int>(modal.y+22),22,kText);
        const char* levelLabels[]={"Category","Name","Order (number)","Description"};
        const char* accountLabels[]={"Username","Password","Role"};
        const char* alphabetLabels[]={"Script","Glyph","Romaji","Group name","Display order (number)","Note"};
        const char* questionLabels[]={"Level ID","Resource ID (optional)","Prompt","Option A","Option B","Option C","Option D","Correct option","Explanation","Status",""};
        const char** labels=section==2?levelLabels:section==3?accountLabels:section==4?alphabetLabels:questionLabels;
        const std::size_t count=resetPasswordForm_?1:section==2?4:section==3?3:section==4?6:10;
        for(std::size_t i=0;i<count;++i){
            const Rectangle r=formField(i,count);
            if(!resetPasswordForm_&&isChoiceField(section,i)){
                const std::string label=std::string(labels[i])+": "+formFields_[i]+"  (click to change)";
                button(r,label.c_str(),false,true);
            }else UITheme::drawField(r,resetPasswordForm_?"New password":labels[i],formFields_[i],focusedField_==i,resetPasswordForm_||(section==3&&i==1));
        }
        UITheme::drawText("Use Tab to move between fields.",static_cast<int>(modal.x+24),static_cast<int>(modal.y+modal.height-48),13,kMuted);
        button(formCancel(),"Cancel");button(formSubmit(),resetPasswordForm_?"Reset":editing_?"Save":"Create",true);
    }
    if(deleteConfirm_||importConfirm_){
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Color{46,31,38,150});const Rectangle d=confirmDialog();
        DrawRectangleRounded(d,0.05f,10,RAYWHITE);DrawRectangleRoundedLinesEx(d,0.05f,10,1.5f,kBorder);
        UITheme::drawText(importConfirm_?"Import this package?":"Delete this record?",static_cast<int>(d.x+24),static_cast<int>(d.y+24),20,kText);
        UITheme::drawWrapped(importConfirm_?"The package will be merged into current data.":"This action cannot be undone.",static_cast<int>(d.x+24),static_cast<int>(d.y+66),static_cast<int>(d.width-48),15,kMuted,2);
        button(confirmYes(),importConfirm_?"Import":"Delete",true);button(confirmNo(),"Cancel");
    }
}

void BackendScreen::draw(std::size_t section, UserRole role, const std::vector<std::string>& rows,
                         const BackendTableData& table, std::size_t selected, const std::string& message,
                         bool messageError,
                         const BackendActivityState& activity) const {
    const bool statisticsPage = role == UserRole::LEARNER && section == 7;
    const BackendUiLayout baseUi=layout();
    const BackendUiLayout ui=statisticsPage ? statisticsLayout(baseUi) : baseUi;
    DrawRectangleRounded(ui.content,0.035f,12,RAYWHITE);
    DrawRectangleRoundedLinesEx(ui.content,0.035f,12,1.0f,kBorder);
    const char* adminTitles[]={"","","Levels","Accounts","Alphabet","Question bank","Import / export"};
    const char* learnerTitles[]={"","","Favorites","Search history","Review","Quiz","Alphabet","Statistics"};
    const char* title=role==UserRole::ADMIN?(section<7?adminTitles[section]:"Backend data"):(section<8?learnerTitles[section]:"Backend data");
    UITheme::drawText(title,static_cast<int>(ui.content.x+12),static_cast<int>(ui.content.y+12),22,kText);
    button(ui.refresh,"Refresh");
    const bool tableView = isTableSection(section, role);
    const bool accountsTable = role == UserRole::ADMIN && section == 3;
    const AccountTableView filteredAccounts = accountsTable ? accountTableView(table, accountRoleFilter_) : AccountTableView{table,{}};
    const BackendTableData& visibleTable = accountsTable ? filteredAccounts.table : table;
    const bool alphabet = isAlphabetSection(section, role);
    const bool importExport = role == UserRole::ADMIN && section == 6;
    const bool reviewPage = role == UserRole::LEARNER && section == 4;
    const bool quizPage = role == UserRole::LEARNER && section == 5;
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
    else if (tableView) drawTablePage(ui.content, ui.list, section, role, visibleTable,
                                      filteredAccounts.sourceRows, selected, statisticsPage);
    else if (alphabet) drawAlphabetPage(ui.list, rows, selected, alphabetKatakana_, alphabetScrollOffset_);
    else drawRowsPage(ui.list, rows, selected, scrollOffset_, ui.visibleRows, importExport);
    drawActions(ui.content, ui.list, ui.actionX, ui.actionY, section, role, message, messageError, activity);
}

void BackendScreen::openEditForm(const std::array<std::string,12>& fields,const std::string& id){
    formFields_=fields;editId_=id;editing_=true;resetPasswordForm_=false;formOpen_=true;focusedField_=0;
}
void BackendScreen::openResetPasswordForm(const std::string& id){
    formFields_={};editId_=id;editing_=false;resetPasswordForm_=true;formOpen_=true;focusedField_=0;
}

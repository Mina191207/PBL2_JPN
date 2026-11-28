#include "TestSupport.h"
#include "UiLayout.h"
#include "screens/BackendScreen.h"
#include "screens/ResourcesScreen.h"
#include "screens/StartupScreen.h"

#include <algorithm>
#include <deque>

namespace {
struct InputFrame {
    Vector2 mouse{};
    bool click = false;
    int key = KEY_NULL;
    bool control = false;
    float wheel = 0.0f;
    std::string clipboard;
    std::deque<int> text;
};

InputFrame input;
}

int TestGetScreenWidth() { return 800; }
int TestGetScreenHeight() { return 560; }
Vector2 TestGetMousePosition() { return input.mouse; }
bool TestIsMouseButtonPressed(int button) { return input.click && button == MOUSE_BUTTON_LEFT; }
bool TestIsKeyPressed(int key) { return input.key == key; }
bool TestIsKeyPressedRepeat(int) { return false; }
bool TestIsKeyDown(int key) { return input.control && key == KEY_LEFT_CONTROL; }
float TestGetMouseWheelMove() { return input.wheel; }
const char* TestGetClipboardText() { return input.clipboard.c_str(); }
int TestGetCharPressed() {
    if (input.text.empty()) return 0;
    const int codepoint = input.text.front();
    input.text.pop_front();
    return codepoint;
}

namespace {
InputFrame click(float x, float y) {
    InputFrame result;
    result.mouse = {x, y};
    result.click = true;
    return result;
}

InputFrame press(int key) {
    InputFrame result;
    result.key = key;
    return result;
}

void accountSelectionAndForm() {
    TempDirectory fixture;
    Application app;
    CHECK(app.initialize(fixture.path.u8string(), "admin", "test123").ok());
    CHECK(app.login("admin", "test123").ok());
    const auto learnerA = app.createAccount("first", "test456", UserRole::LEARNER);
    const auto learnerB = app.createAccount("second", "test456", UserRole::LEARNER);
    CHECK(learnerA.value && learnerB.value);
    const UserId first = *learnerA.value, second = *learnerB.value;
    BackendTableData data;
    const auto originalAccounts = app.accounts();
    CHECK(originalAccounts.value);
    for (const auto& account : *originalAccounts.value)
        data.rows.push_back({account.id, {account.username}, account.role});

    BackendScreen screen;
    screen.navigate(Section::Accounts);
    const auto update = [&](InputFrame frame = {}) {
        input = std::move(frame);
        return screen.update(Section::Accounts, UserRole::ADMIN, {}, data, {});
    };
    using Event = BackendScreenEvent::Type;
    const auto content = UiLayout::content();
    const float actionX = content.x + content.width - 184.0f;
    const float actionY = content.y + 72.0f;
    CHECK(update(click(content.x + 166.0f, content.y + 20.0f)).type == Event::NONE);
    CHECK(update(click(content.x + 166.0f, content.y + 20.0f)).type == Event::NONE);
    CHECK(update(click(content.x + 24.0f, actionY + 55.0f)).type == Event::NONE);
    std::reverse(data.rows.begin(), data.rows.end());
    screen.revalidate(data, {});
    const auto toggle = update(click(actionX + 4.0f, actionY + 8.0f));
    CHECK(toggle.type == Event::TOGGLE_ACCOUNT);
    CHECK(std::get<UserId>(toggle.id) == first);
    CHECK(app.setAccountStatus(std::get<UserId>(toggle.id), UserStatus::LOCKED).ok());
    const auto accounts = app.accounts();
    CHECK(accounts.value);
    for (const auto& account : *accounts.value) {
        if (account.id == first) CHECK(account.status == UserStatus::LOCKED);
        if (account.id == second) CHECK(account.status == UserStatus::ACTIVE);
    }

    screen.openResetPasswordForm(first);
    InputFrame typed;
    typed.text = {0x65e5, 0x672c};
    CHECK(update(typed).type == Event::NONE);
    CHECK(screen.password() == "日本");
    CHECK(update(press(KEY_BACKSPACE)).type == Event::NONE);
    CHECK(screen.password() == "日");
    const auto submitted = update(press(KEY_ENTER));
    CHECK(submitted.type == Event::RESET_PASSWORD && std::get<UserId>(submitted.id) == first);
    screen.clearPassword();
    CHECK(!screen.formFields().empty() && screen.password().empty());
    CHECK(update(press(KEY_ESCAPE)).type == Event::CANCEL_FORM);
    CHECK(screen.formFields().empty());
    screen.openResetPasswordForm(second);
    update(typed);
    screen.navigate(Section::Levels);
    screen.navigate(Section::Accounts);
    CHECK(screen.formFields().empty() && screen.password().empty());
    CHECK(update(click(actionX + 4.0f, actionY + 8.0f)).id == BackendId(first));
    data.rows.clear();
    screen.revalidate(data, {});
    CHECK(update(click(actionX + 4.0f, actionY + 8.0f)).type == Event::NONE);
}

void searchAndScroll() {
    ResourcesScreen screen;
    const AccountSummary account{UserId("U002"), "first", UserRole::LEARNER, UserStatus::ACTIVE};
    const std::vector<Level> levels{Level(LevelId("L001"), "JLPT", "N5", 5, "")};
    std::vector<SearchRow> rows;
    for (int i = 1; i < 10; ++i)
        rows.push_back({ResourceId("R00" + std::to_string(i)), ResourceType::VOCABULARY,
                        "学校", "trường học", LevelId("L001"), 5, 4, "がっこう"});
    const auto update = [&](InputFrame frame = {}) {
        input = std::move(frame);
        return screen.update(rows, levels, account, {});
    };
    using Event = ResourcesEvent::Type;
    const auto content = UiLayout::content();
    const auto y = content.y;
    const auto x = content.x + 16.0f;
    CHECK(update(click(x + 10, y + 70)).type == Event::NONE);
    auto paste = press(KEY_V);
    paste.control = true;
    paste.clipboard = "　学校 ";
    CHECK(update(paste).type == Event::NONE);
    CHECK(update(press(KEY_BACKSPACE)).type == Event::NONE);
    const auto search = update(press(KEY_ENTER));
    CHECK(search.type == Event::SUBMIT_SEARCH && search.search.query == "　学校");
    auto filter = update(click(x + 126, y + 114));
    CHECK(filter.type == Event::PREVIEW_SEARCH && filter.search.type == ResourceType::VOCABULARY);
    filter = update(click(x + 251, y + 114));
    CHECK(filter.type == Event::PREVIEW_SEARCH && filter.search.levelId == LevelId("L001"));
    filter = update(click(x + 377, y + 114));
    CHECK(filter.type == Event::PREVIEW_SEARCH && filter.search.sort == SearchSort::ID);
    CHECK(filter.search.query == search.search.query);
    screen.setSearch(filter.search);

    InputFrame scroll;
    scroll.mouse = {x + 25, y + 233};
    scroll.wheel = -1.0f;
    CHECK(update(scroll).type == Event::NONE);
    const auto selected = update(click(scroll.mouse.x, scroll.mouse.y));
    CHECK(selected.type == Event::SELECT && selected.id == ResourceId("R002"));
    rows.clear();
    const auto browse = update(click(x + 190, content.y + content.height - 65));
    CHECK(browse.type == Event::PREVIEW_SEARCH && browse.search.query.empty() && !browse.search.type);
}

void quizResizeAndCloseKeys() {
    using Event = BackendScreenEvent::Type;
    BackendScreen screen;
    screen.navigate(Section::Quiz);
    BackendActivityState activity;
    const QuizQuestionView question{"学校?", {"a", "b", "c", "d"}};
    activity.quiz = QuizState{{question, question}, {'-', '-'}, false};
    const auto content = UiLayout::content();
    const float listWidth = content.width - 220.0f;
    const float listY = content.y + 72.0f;
    const float nextY = listY + content.height - 124.0f - 48.0f;
    input = click(content.x + listWidth - 98.0f, nextY + 8.0f);
    CHECK(screen.update(Section::Quiz, UserRole::LEARNER, {}, {}, activity).type == Event::NONE);
    input = click(content.x + 30.0f, listY + 160.0f);
    const auto chosen = screen.update(Section::Quiz, UserRole::LEARNER, {}, {}, activity);
    CHECK(chosen.type == Event::ANSWER && chosen.questionIndex == 1 && chosen.answer == 'A');
    const StartupScreen startup;
    input = press(KEY_ESCAPE);
    CHECK(startup.poll(true) == StartupAction::CANCEL_CLOSE);
    input = click(103, 380);
    CHECK(startup.poll(true) == StartupAction::RETRY_SAVE);
    input = click(311, 380);
    CHECK(startup.poll(true) == StartupAction::DISCARD_CLOSE);
}
}

int main() {
    accountSelectionAndForm();
    searchAndScroll();
    quizResizeAndCloseKeys();
}

#include "TestSupport.h"
#include "Fixture.h"
#include "UITheme.h"
#include "UiLayout.h"
#include "app/AppController.h"
#include "screens/BackendScreen.h"
#include "screens/ResourcesScreen.h"
#include "screens/StartupScreen.h"

#include <algorithm>
#include <deque>
#include <iterator>

namespace fs = std::filesystem;

namespace {
struct InputFrame {
    Vector2 mouse{};
    bool click = false;
    int key = KEY_NULL;
    bool control = false;
    bool shift = false;
    float wheel = 0.0f;
    std::string clipboard;
    std::deque<int> text;
};

InputFrame input;
int wheelReads = 0;
int frameWidth = 800, frameHeight = 560;
}

int TestGetScreenWidth() { return frameWidth; }
int TestGetScreenHeight() { return frameHeight; }
Vector2 TestGetMousePosition() { return input.mouse; }
bool TestIsMouseButtonPressed(int button) { return input.click && button == MOUSE_BUTTON_LEFT; }
bool TestIsKeyPressed(int key) { return input.key == key; }
bool TestIsKeyPressedRepeat(int) { return false; }
bool TestIsKeyDown(int key) { return (input.control && key == KEY_LEFT_CONTROL) || (input.shift && key == KEY_LEFT_SHIFT); }
bool TestWindowShouldClose() { return false; }
float TestGetMouseWheelMove() { ++wheelReads; return input.wheel; }
const char* TestGetClipboardText() { return input.clipboard.c_str(); }
int TestGetCharPressed() {
    if (input.text.empty()) return 0;
    const int codepoint = input.text.front();
    input.text.pop_front();
    return codepoint;
}

struct RaylibStateTest {
    AppController controller;

    explicit RaylibStateTest(const fs::path& folder, bool loaded = true) {
        controller.fontReady_ = true;
        controller.initializeApplication(folder.u8string());
        CHECK(controller.backendLoaded_ == loaded);
    }
    void frame(InputFrame event) {
        input = std::move(event);
        controller.processFrame();
        if (controller.loginInProgress_) controller.performPendingLogin();
    }
    Section section() const { return controller.selectedSection_; }
    bool authenticated() const { return controller.activeAccount_.has_value(); }
    const std::string& loginMessage() const { return controller.loginMessage_; }
    const BackendScreen& backend() const { return controller.backendScreen_; }
    const std::string& startupMessage() const { return controller.startupMessage_; }
    const std::string& backendMessage() const { return controller.backendMessage_; }
    const ChangePasswordDialog& passwordDialog() const { return controller.changePasswordDialog_; }
    bool passwordFieldsEmpty() const {
        const auto& dialog = passwordDialog();
        return std::all_of(dialog.passwords_.begin(), dialog.passwords_.end(), [](const PasswordInput& field) {
            return field.value().empty() && !field.tooLong();
        }) && !dialog.visible_;
    }
    bool passwordsVisible() const { return passwordDialog().visible_; }
    const std::string& passwordMessage() const { return passwordDialog().message_; }
    bool exitDialogOpen() const { return controller.exitDialog_; }
    const auto& table() const { return controller.backendTable_; }
    const auto& selection() const { return backend().selected_; }
    std::string prompt() const {
        const auto& dialog = backend().confirmation_;
        return dialog ? dialog->event.text + "\n" + dialog->message : "";
    }
    void navigate(Section target) { controller.navigate(target); }
    Application& app() { return controller.application_; }
    const BackendActivityState& activity() const { return controller.backendActivity_; }
    const std::optional<ResourceDetail>& selectedResource() const { return controller.selectedResource_; }
    const std::vector<SearchRow>& resourceRows() const { return controller.resourceRows_; }
    const std::string& resourcesMessage() const { return controller.resourcesMessage_; }
    void render() { controller.drawScene(); }
};

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

InputFrame paste(std::string value) {
    auto result = press(KEY_V);
    result.control = true;
    result.clipboard = std::move(value);
    return result;
}

InputFrame backTab() {
    auto result = press(KEY_TAB);
    result.shift = true;
    return result;
}

InputFrame selectAll() {
    auto result = press(KEY_A);
    result.control = true;
    return result;
}

void login(RaylibStateTest& ui, const char* name, const char* password = "demo123") {
    ui.frame(paste(name));
    ui.frame(press(KEY_TAB));
    ui.frame(paste(password));
    ui.frame(press(KEY_ENTER));
    CHECK(ui.authenticated());
}

std::string readBytes(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    CHECK(in);
    return {std::istreambuf_iterator<char>(in), {}};
}

void startupRecovery() {
    TempDirectory temp;
    const auto missing = temp.path / "missing";
    const auto corrupt = temp.path / "corrupt";
    const auto empty = temp.path / fs::u8path(u8"Kho trống có dấu");
    const auto busy = temp.path / "busy";
    fs::create_directory(corrupt);
    fs::create_directory(empty);
    writeFile(corrupt / "user.txt", "corrupt-marker\n");
    Application owner;
    CHECK(owner.initialize(busy.u8string(), "admin", "demo123").save == SaveState::SAVED);
    RaylibStateTest ui(missing, false);
    const auto open = [&](const fs::path& folder) {
        ui.frame(click(60, 181));
        ui.frame(selectAll());
        ui.frame(paste(folder.u8string()));
        ui.frame(press(KEY_ENTER));
    };
    CHECK(ui.startupMessage().find(missing.u8string()) != std::string::npos);
    CHECK(ui.startupMessage().find("does not exist") != std::string::npos && !fs::exists(missing));
    open(corrupt);
    CHECK(ui.startupMessage().find("user.txt") != std::string::npos);
    ui.frame(click(228, 453));
    CHECK(readBytes(corrupt / "user.txt") == "corrupt-marker\n" && !ui.app().loaded());
    open(busy);
    CHECK(ui.startupMessage().find("already in use") != std::string::npos);
    ui.frame(selectAll());
    InputFrame typed;
    for (const auto& unit : Utf8::decode(empty.u8string())) typed.text.push_back(unit.codepoint);
    ui.frame(typed);
    ui.frame(press(KEY_ENTER));
    CHECK(ui.startupMessage().find("is empty") != std::string::npos);
    ui.frame(click(228, 453));
    ui.frame(paste("first_admin"));
    ui.frame(press(KEY_TAB));
    ui.frame(paste("fresh123"));
    ui.frame(press(KEY_ENTER));
    CHECK(ui.app().loaded() && !ui.app().dirty());
    login(ui, "first_admin", "fresh123");
    CHECK(ui.app().accounts().value->size() == 1);
    CHECK(ui.app().close().ok());
    RaylibStateTest reopened(empty);
    login(reopened, "first_admin", "fresh123");
    RaylibStateTest retry(busy, false);
    CHECK(owner.close().ok());
    retry.frame(press(KEY_ENTER));
    login(retry, "admin");
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
    CHECK(update(typed).type == Event::EDITED);
    CHECK(screen.password() == "日本");
    CHECK(update(press(KEY_BACKSPACE)).type == Event::EDITED);
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
    CHECK(update(paste("　学校校 ")).type == Event::EDITED);
    CHECK(update(press(KEY_BACKSPACE)).type == Event::EDITED);
    const auto search = update(press(KEY_ENTER));
    CHECK(search.type == Event::SUBMIT_SEARCH && search.search.query == "学校");
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

void searchHistoryReplay() {
    TempDirectory temp;
    auto data = fixture();
    const LevelId n4("L002");
    data.vocabularies.emplace_back(ResourceId("R004"), n4, "学校", "がっこう", "trường học", PartOfSpeech::NOUN, "");
    data.vocabularies.emplace_back(ResourceId("R005"), n4, "学校生活", "", "school life", PartOfSpeech::NOUN, "");
    data.rebuildIndexes();
    saveFixture(temp.path, data);
    const auto historyFile = temp.path / "search_history.txt";
    const auto legacyRows = readBytes(historyFile);
    const auto content = UiLayout::content();
    const float x = content.x + 16.0f, y = content.y;
    const auto query = click(x + 10, y + 70);
    const auto search = click(content.x + content.width - 60, y + 70);
    const auto ids = [](const RaylibStateTest& ui) {
        std::vector<std::string> result;
        for (const auto& row : ui.resourceRows()) result.push_back(row.id.getId());
        return result;
    };
    std::vector<std::string> first;
    {
        RaylibStateTest ui(temp.path);
        login(ui, "learner1");
        ui.frame(click(60, 185));
        CHECK(ui.section() == Section::Resources);
        ui.frame(query);
        ui.frame(paste("学校"));
        ui.frame(search);
        CHECK(ids(ui) == std::vector<std::string>({"R004", "R001", "R003", "R005"}));
        ui.frame(click(x + 126, y + 114));
        ui.frame(click(x + 251, y + 114));
        for (int i = 0; i < 5; ++i) ui.frame(click(x + 377, y + 114));
        ui.frame(search);
        first = ids(ui);
        CHECK(first == std::vector<std::string>({"R005", "R004"}));
        ui.frame(click(670, 48));
        CHECK(!ui.app().loaded());
    }

    RaylibStateTest ui(temp.path);
    login(ui, "learner1");
    ui.frame(click(60, 267));
    CHECK(ui.section() == Section::History);
    ui.frame(click(x + 28, y + 128));
    ui.frame(click(content.x + content.width - 180, y + 80));
    CHECK(ui.section() == Section::Resources && ids(ui) == first);
    const auto replayedFile = readBytes(historyFile);
    CHECK(replayedFile.compare(0, legacyRows.size(), legacyRows) == 0);
    CHECK(replayedFile.find("|CONTENT|VOCABULARY|L002|TERM_DESC\n") != std::string::npos);
    ui.frame(search); // Submit again from the restored controls.
    CHECK(ids(ui) == first);
    const auto history = ui.app().history();
    const auto& restored = history.value->front().getOptions();
    CHECK(restored.query == "学校" && !restored.exactId && restored.type == ResourceType::VOCABULARY &&
          restored.levelId == n4 && restored.sort == SearchSort::TERM_DESC);

    const auto replaceQuery = [&](const char* text) {
        ui.frame(query);
        ui.frame(press(KEY_ESCAPE));
        ui.frame(paste(text));
        ui.frame(search);
    };
    for (const auto* spelling : {"trường học", "TRƯỜNG HỌC", "tru\u031Bo\u031B\u0300ng ho\u0323c", "trường học\r\n"}) {
        replaceQuery(spelling);
        CHECK(ids(ui) == std::vector<std::string>{"R004"});
        CHECK(ui.resourceRows().front().meaning == "trường học");
    }
    const auto count = ui.app().history().value->size();
    for (const auto* invalid : {"trường\r\n học", "trường| học"}) {
        replaceQuery(invalid);
        CHECK(ui.resourceRows().empty() && ui.resourcesMessage() == "Invalid field: query");
    }
    CHECK(ui.app().history().value->size() == count);
    replaceQuery("がっこう");
    CHECK(ids(ui) == std::vector<std::string>{"R004"});
    ui.frame(click(x + 10, y + 114));
    replaceQuery("R004\r\n");
    CHECK(ids(ui) == std::vector<std::string>{"R004"});
}

void quizResizeAndCloseKeys() {
    using Event = BackendScreenEvent::Type;
    BackendScreen screen;
    screen.navigate(Section::Quiz);
    BackendActivityState activity;
    const QuizQuestionView question{"学校?", {"a", "b", "c", "d"}};
    activity.quiz = QuizState{{question, question}, {'-', '-'}, false};
    const auto content = UiLayout::content();
    const float listY = content.y + 72.0f;
    const float nextY = content.y + content.height - 8.0f - 40.0f - 8.0f - 44.0f;
    input = click(content.x + content.width - 70.0f, nextY + 8.0f);
    CHECK(screen.update(Section::Quiz, UserRole::LEARNER, {}, {}, activity).type == Event::NONE);
    input = click(content.x + 30.0f, listY + 92.0f);
    const auto chosen = screen.update(Section::Quiz, UserRole::LEARNER, {}, {}, activity);
    CHECK(chosen.type == Event::ANSWER && chosen.questionIndex == 1 && chosen.answer == 'A');
    StartupScreen startup;
    input = press(KEY_ESCAPE);
    CHECK(startup.poll(true) == StartupAction::CANCEL_CLOSE);
    input = click(103, 380);
    CHECK(startup.poll(true) == StartupAction::RETRY_SAVE);
    input = click(311, 380);
    CHECK(startup.poll(true) == StartupAction::DISCARD_CLOSE);
}

void controllerInput() {
    TempDirectory temp;
    saveFixture(temp.path, fixture());
    RaylibStateTest ui(temp.path);
    ui.frame(press(KEY_ENTER));
    CHECK(ui.loginMessage() == "Enter both username and password.");
    ui.frame(paste("Ađd\nmin"));
    CHECK(ui.loginMessage().empty());
    ui.frame(press(KEY_TAB));
    ui.frame(paste("demo123"));
    ui.frame(press(KEY_ENTER));
    CHECK(ui.authenticated() && ui.section() == Section::Dashboard);
    for (int i = 0; i < 3; ++i) ui.frame(press(KEY_TAB));
    ui.frame(press(KEY_ENTER));
    CHECK(ui.section() == Section::Accounts);

    const auto content = UiLayout::content();
    ui.frame(click(content.x + content.width - 180.0f, content.y + 128.0f));
    CHECK(ui.backend().modalOpen());
    ui.frame(paste("draft_01"));
    ui.frame(click(120, 118));
    CHECK(ui.section() == Section::Accounts && ui.backend().modalOpen());
    CHECK(fieldValue(ui.backend().formFields(), FieldId::Username) == "draft_01");
    InputFrame scroll;
    scroll.mouse = {content.x + 40, content.y + 150};
    scroll.wheel = -1.0f;
    wheelReads = 0;
    ui.frame(scroll);
    CHECK(wheelReads == 0 && ui.backend().modalOpen());
    ui.frame(press(KEY_TAB));
    ui.frame(paste("đ日あ"));
    CHECK(ui.backend().password() == "đ日あ");
    ui.frame(backTab());
    ui.frame(paste("-ok"));
    CHECK(fieldValue(ui.backend().formFields(), FieldId::Username) == "draft_01-ok");

    auto dismiss = click(120, 118);
    dismiss.key = KEY_ESCAPE;
    dismiss.wheel = -1.0f;
    ui.frame(dismiss);
    CHECK(ui.section() == Section::Accounts && !ui.backend().modalOpen());
    ui.frame(press(KEY_ENTER));
    CHECK(ui.backend().modalOpen() && ui.backend().password().empty());
    ui.frame(press(KEY_ESCAPE));

    ui.frame(click(95, 146));
    CHECK(ui.section() == Section::Dashboard);
    ui.frame(backTab());
    ui.frame(backTab());
    ui.frame(press(KEY_SPACE));
    CHECK(!ui.authenticated());
}

void changePasswordFlow() {
    TempDirectory temp;
    saveFixture(temp.path, fixture());
    const auto file = temp.path / "user.txt";
    const auto before = readBytes(file);
    const char* changed = "đ日あ-12";
    RaylibStateTest ui(temp.path);
    login(ui, "learner1");

    // Eight menu items, then Sign out, then Change password.
    for (int i = 0; i < 9; ++i) ui.frame(press(KEY_TAB));
    ui.frame(press(KEY_ENTER));
    CHECK(ui.passwordDialog().editing());
    auto fill = [&](const char* current, const char* replacement, const char* confirmation) {
        ui.frame(paste(current));
        ui.frame(press(KEY_TAB));
        ui.frame(paste(replacement));
        ui.frame(press(KEY_TAB));
        ui.frame(paste(confirmation));
        ui.frame(press(KEY_ENTER));
    };
    auto open = [&] {
        ui.frame(click(148, 520));
        CHECK(ui.passwordDialog().editing() && ui.passwordFieldsEmpty());
    };
    fill("incorrect", changed, changed);
    CHECK(ui.passwordMessage() == "Current password is incorrect.");
    ui.frame(click(80, 186));
    CHECK(ui.section() == Section::Dashboard && ui.passwordDialog().editing());
    ui.frame(press(KEY_ESCAPE));
    CHECK(!ui.passwordDialog().isOpen() && ui.passwordFieldsEmpty() && ui.authenticated());
    open();
    fill("demo123", changed, "different");
    CHECK(ui.passwordMessage().find("do not match") != std::string::npos);
    ui.frame(click(449, 455));
    CHECK(!ui.passwordDialog().isOpen() && ui.passwordFieldsEmpty());
    CHECK(readBytes(file) == before && !ui.app().dirty());

    open();
    fill("demo123", changed, changed);
    CHECK(ui.passwordDialog().isOpen() && ui.passwordFieldsEmpty());
    CHECK(ui.passwordMessage() == "Password changed and saved." && !ui.app().dirty());
    ui.frame(press(KEY_ENTER));
    CHECK(!ui.passwordDialog().isOpen() && ui.authenticated());
    ui.frame(click(49, 520));
    CHECK(!ui.authenticated());
    login(ui, "learner1", changed);

    open();
    const auto blocker = temp.path / ".pbl2-stage";
    const auto saved = readBytes(file);
    writeFile(blocker, "occupied");
    fill(changed, "fixture-retried", "fixture-retried");
    CHECK(ui.passwordFieldsEmpty() && ui.app().dirty() && readBytes(file) == saved);
    CHECK(ui.passwordMessage().find("waiting to be saved") != std::string::npos);
    ui.frame(press(KEY_ENTER));
    CHECK(ui.app().dirty() && readBytes(file) == saved);
    ui.frame(press(KEY_ESCAPE));
    CHECK(ui.exitDialogOpen());
    ui.frame(press(KEY_ESCAPE));
    CHECK(!ui.exitDialogOpen() && ui.passwordDialog().isOpen());
    CHECK(fs::remove(blocker));
    ui.frame(press(KEY_ENTER));
    CHECK(ui.passwordMessage() == "Password changed and saved." && !ui.app().dirty());
    ui.frame(press(KEY_ESCAPE));
    ui.frame(click(49, 520));
    CHECK(!ui.authenticated() && ui.app().close().ok());

    RaylibStateTest reopened(temp.path);
    reopened.frame(paste("learner1"));
    reopened.frame(press(KEY_TAB));
    reopened.frame(paste(changed));
    reopened.frame(press(KEY_ENTER));
    CHECK(!reopened.authenticated() && reopened.loginMessage() == "invalid credentials");
    reopened.frame(paste("fixture-retried"));
    reopened.frame(press(KEY_ENTER));
    CHECK(reopened.authenticated());
    reopened.frame(click(49, 520));
    login(reopened, "learner2");
    CHECK(reopened.app().favorites().value->empty() && reopened.app().history().value->empty());
    reopened.frame(click(49, 520));
    login(reopened, "Admin");
    reopened.frame(click(148, 520));
    CHECK(!reopened.passwordDialog().isOpen() && !reopened.authenticated());
    std::cout << "OK own password validation, cancel, modal isolation, saved, failed retry, reopen and session switch\n";
}

void packageClipboardAndRows() {
    using Event = BackendScreenEvent::Type;
    BackendScreen screen;
    screen.navigate(Section::Packages);
    const auto content = UiLayout::content();
    const auto update = [&](InputFrame frame) {
        input = std::move(frame);
        return screen.update(Section::Packages, UserRole::ADMIN, {}, {}, {});
    };
    update(click(content.x + 20, content.y + 115));
    CHECK(update(paste("/資料\n")).type == Event::EDITED);
    input = press(KEY_TAB);
    CHECK(screen.tab(Section::Packages, UserRole::ADMIN, {}, {}, {}));
    if (FolderField::nativeBrowse) CHECK(screen.tab(Section::Packages, UserRole::ADMIN, {}, {}, {}));
    const auto exported = update(press(KEY_ENTER));
    CHECK(exported.type == Event::EXPORT_PACKAGE && exported.text == "outputs/resource-package/資料");
    input = press(KEY_TAB);
    CHECK(screen.tab(Section::Packages, UserRole::ADMIN, {}, {}, {}));
    update(press(KEY_ENTER));
    CHECK(screen.modalOpen());
    CHECK(update(press(KEY_ENTER)).type == Event::NONE && !screen.modalOpen());
    update(press(KEY_ENTER));
    update(backTab());
    const auto imported = update(press(KEY_ENTER));
    CHECK(imported.type == Event::IMPORT_PACKAGE && imported.text == exported.text);

    BackendTableData rows{{"ID"}, {{UserId("U001"), {"first"}}, {UserId("U002"), {"second"}}}};
    screen.navigate(Section::Accounts);
    for (int i = 0; i < 3; ++i) CHECK(screen.tab(Section::Accounts, UserRole::ADMIN, rows, {}, {}));
    input = press(KEY_DOWN);
    screen.update(Section::Accounts, UserRole::ADMIN, {}, rows, {});
    screen.update(Section::Accounts, UserRole::ADMIN, {}, rows, {});
    CHECK(screen.tab(Section::Accounts, UserRole::ADMIN, rows, {}, {}));
    input = press(KEY_ENTER);
    CHECK(screen.update(Section::Accounts, UserRole::ADMIN, {}, rows, {}).type == Event::NONE);
    CHECK(screen.modalOpen());
    input = press(KEY_ESCAPE);
    screen.update(Section::Accounts, UserRole::ADMIN, {}, rows, {});
    input = press(KEY_TAB);
    CHECK(screen.tab(Section::Accounts, UserRole::ADMIN, rows, {}, {}));
    input = press(KEY_ENTER);
    const auto selected = screen.update(Section::Accounts, UserRole::ADMIN, {}, rows, {});
    CHECK(selected.type == Event::TOGGLE_ACCOUNT && selected.id == BackendId(UserId("U002")));
}

void packageOverwrite() {
    TempDirectory temp;
    const auto store = temp.path / "store";
    const auto package = temp.path / fs::u8path(u8"Gói học liệu");
    const auto imported = temp.path / "imported";
    const auto marker = package / "grammar.txt";
    saveFixture(store, fixture());
    fs::create_directory(package);
    writeFile(marker, "cancel-marker");
    writeFile(package / "keep.txt", "outside package");
    RaylibStateTest ui(store);
    login(ui, "Admin");
    ui.navigate(Section::Packages);
    const auto content = UiLayout::content();
    const auto setPath = [&](const fs::path& path) {
        ui.frame(click(content.x + 20, content.y + 115));
        ui.frame(selectAll());
#ifdef _WIN32
        ui.frame(paste("\"" + path.u8string() + "\""));
#else
        ui.frame(paste(path.u8string()));
#endif
    };
    const auto exportClick = click(content.x + 20, content.y + 162);
    setPath(store / ".." / "store" / ".");
    ui.frame(exportClick);
    CHECK(!ui.backend().modalOpen() && ui.backendMessage().find("overlaps") != std::string::npos);
    setPath(package);
    ui.frame(exportClick);
    CHECK(ui.backend().modalOpen() && ui.prompt().find(package.u8string()) != std::string::npos);
    CHECK(ui.prompt().find("grammar.txt") != std::string::npos && ui.prompt().find("vocabulary.txt") == std::string::npos);
    CHECK(!fs::exists(package / "vocabulary.txt") && !fs::exists(package / StoreLock::filename));
    ui.frame(click(120, 118));
    CHECK(ui.section() == Section::Packages && ui.backend().modalOpen());
    ui.frame(press(KEY_ENTER));
    CHECK(!ui.backend().modalOpen() && readBytes(marker) == "cancel-marker");
    const auto lateFile = package / "kanji.txt";
    ui.frame(exportClick);
    writeFile(lateFile, "created after preview");
    ui.frame(backTab());
    ui.frame(press(KEY_ENTER));
    CHECK(ui.backendMessage().find("destination changed") != std::string::npos);
    CHECK(readBytes(marker) == "cancel-marker" && readBytes(lateFile) == "created after preview");
    ui.frame(exportClick);
    CHECK(ui.prompt().find("kanji.txt") != std::string::npos);
    ui.frame(backTab());
    ui.frame(press(KEY_ENTER));
    CHECK(!ui.backend().modalOpen() && ui.backendMessage() == "Package exported.");
    CHECK(readBytes(marker) != "cancel-marker" && readBytes(package / "keep.txt") == "outside package");
    const std::vector<std::string> expected{
        "grammar.txt", "kanji.txt", "learning_resource.txt", "resource_levels.txt", "vocabulary.txt"};
    CHECK(ui.app().inspectPackageExport(package.u8string()).value->existingFiles == expected);
    for (const auto& entry : fs::directory_iterator(package)) {
        const auto name = entry.path().filename().u8string();
        CHECK(name == StoreLock::filename || name == "keep.txt" ||
              std::find(expected.begin(), expected.end(), name) != expected.end());
    }
    {
        Application owner;
        CHECK(owner.initialize(imported.u8string(), "admin", "demo123").save == SaveState::SAVED);
        CHECK(owner.close().ok());
    }
    RaylibStateTest receiver(imported);
    login(receiver, "admin");
    receiver.navigate(Section::Packages);
    receiver.frame(click(content.x + 20, content.y + 115));
    receiver.frame(selectAll());
    receiver.frame(paste(package.u8string()));
    const auto importClick = click(content.x + 210, content.y + 162);
    receiver.frame(importClick);
    CHECK(receiver.backend().modalOpen());
    receiver.frame(press(KEY_ESCAPE));
    CHECK(receiver.app().statistics().value->vocabulary == 0);
    receiver.frame(importClick);
    receiver.frame(backTab());
    receiver.frame(press(KEY_ENTER));
    CHECK(receiver.backendMessage() == "Package imported.");
    const auto stats = receiver.app().statistics();
    CHECK(stats.value && stats.value->vocabulary == 1 && stats.value->grammar == 1 && stats.value->kanji == 1);
    receiver.frame(importClick);
    receiver.frame(backTab());
    receiver.frame(press(KEY_ENTER));
    CHECK(receiver.backendMessage() != "Package imported." && receiver.app().statistics().value->vocabulary == 1);
    receiver.navigate(Section::Resources);
    receiver.frame(click(content.x + 32, content.y + 75));
    receiver.frame(paste(u8"学校"));
    receiver.frame(press(KEY_ENTER));
    CHECK(receiver.resourceRows().size() == 2);
    std::cout << "OK export overwrite and import cancel, success, duplicate rejection, find imported resources\n";
}

void learningConfirmations() {
    TempDirectory temp;
    auto data = fixture();
    data.histories.pushBack(SearchHistory(HistoryId("H002"), UserId("U002"), "second", "2026-11-12T01:02:00Z"));
    data.histories.pushBack(SearchHistory(HistoryId("H003"), UserId("U003"), "keep", "2026-11-12T01:03:00Z"));
    saveFixture(temp.path, data);
    const auto oldHistory = readBytes(temp.path / "search_history.txt");
    RaylibStateTest ui(temp.path);
    login(ui, "learner1");
    const auto content = UiLayout::content();
    const auto historyClear = click(content.x + content.width - 180, content.y + 128);
    const auto quizActionY = [&] {
        const float banner = UITheme::bannerHeight(content.width - 24, ui.backendMessage());
        return content.y + content.height - 48.0f - (banner > 0 ? banner + 8.0f : 0.0f);
    };
    const auto primary = [&] { ui.frame(click(content.x + 30, quizActionY() + 10)); };
    const auto stop = [&] { ui.frame(click(content.x + 210, quizActionY() + 10)); };
    const auto answer = click(content.x + 30, content.y + 164);
    const auto next = [&] { ui.frame(click(content.x + content.width - 72, quizActionY() - 36)); };
    ui.navigate(Section::History);
    ui.frame(historyClear);
    CHECK(ui.prompt().find("Delete 2 rows") != std::string::npos);
    ui.frame(press(KEY_ESCAPE));
    CHECK(!ui.backend().modalOpen() && readBytes(temp.path / "search_history.txt") == oldHistory);
    ui.frame(historyClear);
    ui.frame(backTab());
    ui.frame(press(KEY_ENTER));
    CHECK(ui.app().history().value->empty());
    const auto newHistory = readBytes(temp.path / "search_history.txt");
    CHECK(newHistory.find("H003|U003|keep|") != std::string::npos && newHistory.find("|U002|") == std::string::npos);

    ui.navigate(Section::Quiz);
    primary();
    primary();
    CHECK(ui.prompt().find("3 questions are unanswered") != std::string::npos);
    ui.frame(press(KEY_ENTER));
    CHECK(!ui.backend().modalOpen() && ui.app().attempts().value->size() == 1);
    primary();
    ui.frame(backTab());
    for (int i = 0; i < 5; ++i) ui.frame(press(KEY_ENTER));
    CHECK(ui.app().attempts().value->size() == 2 && ui.activity().quizResult->getCorrectAnswers() == 0);
    stop();

    primary();
    ui.frame(answer);
    next();
    primary();
    CHECK(ui.prompt().find("2 questions are unanswered") != std::string::npos);
    ui.frame(press(KEY_ESCAPE));
    CHECK(ui.activity().quiz->selectedAnswers == std::vector<char>({'A', '-', '-'}));
    ui.frame(answer);
    CHECK(ui.activity().quiz->selectedAnswers == std::vector<char>({'A', 'A', '-'}));
    primary();
    CHECK(ui.prompt().find("1 question is unanswered") != std::string::npos);
    ui.frame(backTab());
    for (int i = 0; i < 5; ++i) ui.frame(press(KEY_ENTER));
    CHECK(ui.app().attempts().value->size() == 3 && ui.activity().quizResult->getCorrectAnswers() == 2);
    stop();

    primary();
    ui.frame(answer);
    next();
    ui.frame(answer);
    next();
    ui.frame(answer);
    primary();
    CHECK(!ui.backend().modalOpen() && ui.activity().quizResult->getCorrectAnswers() == 3);
    for (int i = 0; i < 5; ++i) ui.frame(press(KEY_ENTER));
    CHECK(ui.app().attempts().value->size() == 4);
    CHECK(ui.app().close().ok());
    Application reopened;
    CHECK(reopened.load(temp.path.u8string()).ok());
    CHECK(reopened.login("learner1", "demo123").ok() && reopened.attempts().value->size() == 4);
    CHECK(reopened.history().value->empty());
    CHECK(reopened.login("learner2", "demo123").ok() && reopened.history().value->size() == 1);
}

void workspaceSmoke() {
    TempDirectory temp;
    auto data = fixture();
    data.alphabet.front() = AlphabetEntry(AlphabetId("A001"), Script::HIRAGANA, u8"あ", "a", "A", 1, "");
    data.alphabet.emplace_back(AlphabetId("A002"), Script::KATAKANA, u8"ア", "a", "A", 1, "");
    data.rebuildIndexes();
    saveFixture(temp.path, data);
    RaylibStateTest ui(temp.path);
    const auto content = UiLayout::content();
    const float right = content.x + content.width;
    const auto action = [&](float offset) { ui.frame(click(right - 100, content.y + 84 + offset)); };
    const auto firstRow = [&] { ui.frame(click(content.x + 36, content.y + 128)); };
    const auto confirm = [&] { ui.frame(backTab()); ui.frame(press(KEY_ENTER)); };
    const auto fill = [&](std::initializer_list<const char*> values) {
        bool first = true;
        for (const auto* value : values) {
            if (!first) ui.frame(press(KEY_TAB));
            if (*value) ui.frame(paste(value));
            first = false;
        }
        ui.frame(press(KEY_ENTER));
    };
    const auto nav = [&](UserRole role, int index) {
        const auto section = menu(role).begin()[index].section;
        ui.frame(click(50, 145 + index * (role == UserRole::ADMIN ? 40 : 38)));
        CHECK(ui.section() == section);
    };

    login(ui, "Admin");
    for (int i = 0; i < 7; ++i) nav(UserRole::ADMIN, i);
    nav(UserRole::ADMIN, 1);
    const float actionY = content.y + content.height - 22;
    const float addX = content.x + 20;
    const float editX = addX + UITheme::buttonWidth("Add") + 24;
    const float deleteX = editX + UITheme::buttonWidth("Edit") + 24;
    const auto resourceField = [&](int index, const char* value) {
        ui.frame(click(index % 2 ? 433 : 60, 177 + (index / 2) * 76));
        ui.frame(paste(value));
    };
    const auto search = [&](const char* query) {
        ui.frame(click(content.x + 32, content.y + 75));
        ui.frame(press(KEY_ESCAPE));
        ui.frame(paste(query));
        ui.frame(press(KEY_ENTER));
        CHECK(ui.resourceRows().size() == 1);
        ui.frame(click(content.x + 32, content.y + 214));
        CHECK(ui.selectedResource());
    };
    const char* terms[] = {u8"試験単語", u8"試験文法", u8"試"};
    for (int type = 0; type < 3; ++type) {
        ui.frame(click(addX, actionY));
        for (int n = 0; n < type; ++n) ui.frame(click(60, 102));
        resourceField(0, terms[type]);
        const int meaning = type == 0 ? 2 : 1;
        resourceField(meaning, "fixture meaning");
        if (type == 0) resourceField(1, u8"しけんたんご");
        if (type == 2) {
            resourceField(2, "13");
            resourceField(3, u8"シ");
        }
        ui.frame(press(KEY_ENTER));
        CHECK(ui.resourcesMessage() == "Resource created.");
        search(terms[type]);
        const auto id = ui.resourceRows().front().id;
        ui.frame(click(editX, actionY));
        resourceField(meaning, " post-edit searchable marker");
        ui.frame(press(KEY_ENTER));
        CHECK(ui.resourcesMessage() == "Resource updated.");
        search("post-edit searchable marker");
        CHECK(ui.resourceRows().front().id == id);
        ui.frame(click(deleteX, actionY));
        ui.frame(press(KEY_ENTER));
        CHECK(ui.app().resource(id).ok());
        ui.frame(click(deleteX, actionY));
        confirm();
        CHECK(!ui.app().resource(id).ok() && ui.resourceRows().empty());
    }
    std::cout << "OK create, edit and find again, cancel deletion and delete all three resource types\n";
    nav(UserRole::ADMIN, 2);
    action(48);
    fill({"", "Smoke level", "7"});
    CHECK(ui.backendMessage() == "Record created." && ui.table().rows.size() == 3);
    firstRow(); ui.frame(press(KEY_END));
    CHECK(ui.selection() == BackendId(LevelId("L003")));
    action(96);
    ui.frame(press(KEY_TAB)); ui.frame(paste(" updated")); ui.frame(press(KEY_ENTER));
    CHECK(ui.table().rows.back().cells.at(2) == "Smoke level updated");
    action(0); confirm();
    CHECK(ui.table().rows.size() == 2);
    action(48);
    fill({"", "Invalid level", "0"});
    CHECK(ui.backend().modalOpen() && ui.table().rows.size() == 2);
    ui.frame(press(KEY_ESCAPE));

    nav(UserRole::ADMIN, 3);
    action(48);
    fill({"smoke_learner", "fixture-account"});
    CHECK(ui.table().rows.size() == 4);
    ui.frame(click(content.x + 168, content.y + 20));
    ui.frame(click(content.x + 168, content.y + 20));
    firstRow(); ui.frame(press(KEY_END));
    CHECK(ui.selection() == BackendId(UserId("U004")));
    action(0);
    CHECK(ui.table().rows.back().cells.back() == "Locked");
    action(96);
    fill({"fixture-admin-reset"});
    CHECK(ui.backendMessage() == "Password reset.");
    action(0);
    CHECK(ui.table().rows.back().cells.back() == "Active");

    nav(UserRole::ADMIN, 4);
    action(48);
    fill({"", u8"い", "i", "A", "2"});
    CHECK(ui.app().alphabet().value->size() == 3);
    ui.frame(click(content.x + 73, content.y + 156));
    ui.frame(press(KEY_END));
    CHECK(ui.selection() == BackendId(AlphabetId("A003")));
    action(96);
    for (int i = 0; i < 5; ++i) ui.frame(press(KEY_TAB));
    ui.frame(paste("fixture note")); ui.frame(press(KEY_ENTER));
    CHECK(ui.backendMessage() == "Record updated.");
    action(0); confirm();
    CHECK(ui.app().alphabet().value->size() == 2);

    nav(UserRole::ADMIN, 5);
    action(96);
    fill({"L001", "", "Smoke prompt", "a", "b", "c", "d"});
    CHECK(ui.table().rows.size() == 5);
    firstRow(); ui.frame(press(KEY_END));
    CHECK(ui.selection() == BackendId(QuestionId("Q005")));
    action(144);
    ui.frame(press(KEY_TAB)); ui.frame(press(KEY_TAB)); ui.frame(paste(" updated")); ui.frame(press(KEY_ENTER));
    CHECK(ui.table().rows.back().cells.at(1) == "Smoke prompt updated");
    action(0);
    CHECK(ui.table().rows.back().cells.back() == "Inactive");
    action(48); confirm();
    CHECK(ui.table().rows.size() == 4);
    ui.frame(click(right - 100, content.y + 20));
    CHECK(ui.table().rows.size() == 4);

    ui.frame(click(49, 520));
    login(ui, "smoke_learner", "fixture-admin-reset");
    ui.frame(click(49, 520));
    login(ui, "learner1");
    for (int i = 0; i < 8; ++i) nav(UserRole::LEARNER, i);
    CHECK(ui.activity().resourceStats && ui.table().rows.size() == 1);
    nav(UserRole::LEARNER, 6);
    ui.frame(click(content.x + 182, content.y + 91));
    ui.frame(click(content.x + 73, content.y + 156));
    CHECK(ui.selection() == BackendId(AlphabetId("A002")));
    ui.frame(click(content.x + 45, content.y + 91));
    ui.frame(click(content.x + 73, content.y + 156));
    CHECK(ui.selection() == BackendId(AlphabetId("A001")));
    nav(UserRole::LEARNER, 4);
    action(0);
    CHECK(ui.activity().review && ui.activity().review->current && !ui.activity().review->current->back);
    action(48);
    CHECK(ui.activity().review->current->back);
    ui.frame(click(right - 60, content.y + 180));
    CHECK(ui.activity().review->ratingCount == 1 && !ui.activity().review->current->back);
    action(48);
    ui.frame(click(right - 150, content.y + 180));
    CHECK(ui.activity().review->finished && ui.activity().review->ratingCount == 2);
    action(0); action(144);
    CHECK(ui.activity().reviewStopped);
    nav(UserRole::LEARNER, 2);
    firstRow(); action(0);
    CHECK(ui.table().rows.empty() && ui.app().favorites().value->empty());
    nav(UserRole::LEARNER, 4);
    action(0);
    CHECK(ui.backendMessage() == "Add resources to Favorites before starting a review.");
    CHECK(ui.app().close().ok());
    std::cout << "OK seven Admin and eight Learner sections, Accounts/Levels/Kana/Questions admin actions, kana tabs, review, favorites, statistics\n";
}

void corpusSmoke() {
    TempDirectory temp;
    auto data = fixture();
    const LevelId level("L001");
    for (std::size_t n = 3; n < 10720; ++n) {
        const auto id = idAfter<ResourceId>(n);
        data.vocabularies.emplace_back(id, level, "corpus-" + id.getId(), "reading", "meaning", PartOfSpeech::NOUN, "");
    }
    for (std::size_t n = data.questions.size(); n < 8776; ++n)
        data.questions.emplace_back(idAfter<QuestionId>(n), level, std::nullopt, "Question " + std::to_string(n),
                                    std::array<std::string, 4>{"one", "two", "three", "four"}, 'A', "Fixture", QuestionStatus::ACTIVE);
    data.rebuildIndexes();
    saveFixture(temp.path, data);
    RaylibStateTest ui(temp.path);
    login(ui, "learner1");
    const auto content = UiLayout::content();
    ui.frame(click(55, 185));
    CHECK(ui.section() == Section::Resources && ui.resourceRows().size() == 10720);
    const auto firstRow = click(content.x + 34, content.y + 216);
    const auto firstId = [&] { return std::visit([](const auto& row) { return row.getId(); }, *ui.selectedResource()); };
    ui.frame(firstRow);
    CHECK(ui.selectedResource());
    const auto first = firstId();
    ui.frame(press(KEY_PAGE_DOWN));
    ui.frame(firstRow);
    CHECK(ui.selectedResource() && firstId() != first);
    ui.frame(press(KEY_PAGE_UP));
    ui.frame(firstRow);
    CHECK(ui.selectedResource() && firstId() == first);

    ui.frame(click(55, 335));
    CHECK(ui.section() == Section::Quiz && ui.activity().activeQuestionCount == 8775);
    const auto chooseLevel = click(content.x + 40, content.y + 95);
    const auto beginQuiz = click(content.x + 30, content.y + content.height - 30);
    ui.frame(chooseLevel);
    CHECK(ui.activity().activeQuestionCount == 0);
    ui.frame(beginQuiz);
    CHECK(!ui.activity().quiz);
    ui.frame(chooseLevel);
    ui.frame(beginQuiz);
    CHECK(ui.activity().quiz && ui.activity().quiz->questions.size() == 10);
    std::cout << "OK synthetic 10720-resource pagination and 8776-question bank, 10 active quiz questions\n";
}

void longField() {
    const std::string full = "日" + std::string(8186, 'a') + "あ";
    std::string value;
    UiInput::Cursor cursor;
    input = paste(full);
    CHECK(UiInput::text(value, cursor, maxTxtFieldBytes).changed);
    CHECK(value == full && cursor.at(value) == 8192);
    const auto wide = UITheme::fieldView(value, cursor.at(value), 369, 18);
    const auto narrow = UITheme::fieldView(value, cursor.at(value), 189, 18);
    CHECK(wide.begin < narrow.begin && wide.end == value.size() && narrow.end == value.size());
    const auto middle = UITheme::fieldView(value, 4000, 369, 18);
    CHECK(middle.begin < 4000 && middle.end > 4005 && middle.caretX < 310.0f);
    input = press(KEY_BACKSPACE);
    CHECK(UiInput::text(value, cursor, maxTxtFieldBytes).changed && value.size() == 8189);
    input = paste("日本");
    CHECK(UiInput::text(value, cursor, maxTxtFieldBytes).overLimit && value.size() == 8189);
    input = press(KEY_HOME);
    UiInput::text(value, cursor, maxTxtFieldBytes);
    input = press(KEY_RIGHT);
    UiInput::text(value, cursor, maxTxtFieldBytes);
    CHECK(cursor.at(value) == 3);
    input = paste("あ");
    CHECK(UiInput::text(value, cursor, maxTxtFieldBytes).changed && cursor.at(value) == 6);
    CHECK(value.size() == 8192 && value.substr(0, 6) == "日あ");
    input = press(KEY_DELETE);
    CHECK(UiInput::text(value, cursor, maxTxtFieldBytes).changed && value.size() == 8191);
    const auto visible = UITheme::fieldView(value, cursor.at(value), 369, 18);
    CHECK(visible.begin == 0 && visible.end < value.size() && visible.caretX > 0.0f);
    CHECK(UITheme::fieldView(value, 1, 369, 18).caretX == 0.0f);
}

struct CapturedFrame {
    Image image{};

    template<class Draw>
    explicit CapturedFrame(Draw draw) {
        const auto target = LoadRenderTexture(frameWidth, frameHeight);
        CHECK(target.id != 0);
        BeginTextureMode(target);
        draw();
        EndTextureMode();
        image = LoadImageFromTexture(target.texture);
        UnloadRenderTexture(target);
        CHECK(image.data);
        ImageFlipVertical(&image);
    }
    ~CapturedFrame() { UnloadImage(image); }
    CapturedFrame(const CapturedFrame&) = delete;

    int changedFrom(const CapturedFrame& previous, int left, int top, int width, int height) const {
        int changed = 0;
        for (int y = top; y < top + height; ++y) {
            for (int x = left; x < left + width; ++x) {
                const auto now = GetImageColor(image, x, y);
                const auto before = GetImageColor(previous.image, x, y);
                if (now.r != before.r || now.g != before.g || now.b != before.b) ++changed;
            }
        }
        return changed;
    }
};

void firstFrameGlyphs() {
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(800, 560, "PBL2 font check");
    CHECK(IsWindowReady() && UiText::initialize());
    {
        LoginScreen login;
        input = press(KEY_TAB);
        login.update(false);
        input = paste("đ日あ");
        login.update(false);
        input = press(KEY_TAB);
        login.update(false);
        const CapturedFrame masked([&] { login.draw(false, {}); });
        input = press(KEY_ENTER);
        login.update(false);
        const CapturedFrame revealed([&] { login.draw(false, {}); });
        CHECK(revealed.changedFrom(masked, 150, 330, 70, 30) > 30);
        CHECK(!UiText::hasGlyph(0x0111));
        UiText::PrivateFont privateFont;
        const Font* font = privateFont.prepare(login.password());
        CHECK(font->glyphCount == 1 && font->glyphs[0].value == 0x0111 && font->recs[0].width > 0.0f);
        CHECK(UiText::hasGlyph(0x65e5) && UiText::hasGlyph(0x3042));

        TempDirectory temp;
        auto data = fixture();
        data.grammars.clear();
        data.kanjis.clear();
        data.vocabularies.front() = Vocabulary(ResourceId("R001"), LevelId("L001"), "嗚呼", "ああ", "Nội dung", PartOfSpeech::OTHER, "Đúng");
        saveFixture(temp.path, data);
        RaylibStateTest ui(temp.path);
        ui.frame(paste("learner1"));
        ui.frame(press(KEY_TAB));
        ui.frame(paste("demo123"));
        ui.frame(press(KEY_ENTER));
        const CapturedFrame dashboard([&] { ui.render(); });
        CHECK(!UiText::hasGlyph(0x55da) && !UiText::hasGlyph(0x547c) && !UiText::hasGlyph(0x1ed9));
        ui.frame(press(KEY_TAB));
        ui.frame(press(KEY_ENTER));
        CHECK(ui.section() == Section::Resources);
        const CapturedFrame search([&] { ui.render(); });
        CHECK(UiText::hasGlyph(0x55da) && UiText::hasGlyph(0x547c) && UiText::hasGlyph(0x1ed9));
        ui.frame(click(309, 337));
        const auto& selection = ui.selectedResource();
        CHECK(selection && std::holds_alternative<Vocabulary>(*selection));
        CHECK(std::get<Vocabulary>(*selection).getWord() == "嗚呼");
        const CapturedFrame detail([&] { ui.render(); });
        CHECK(detail.changedFrom(search, 546, 335, 94, 48) > 70);
    }
    UiText::shutdown();
    CloseWindow();
}

InputFrame wheelAt(float x, float y, float amount) {
    auto frame = click(x, y);
    frame.click = false;
    frame.wheel = amount;
    return frame;
}

void resizeFrame(int width, int height) {
    frameWidth = width;
    frameHeight = height;
    SetWindowSize(width, height);
}

void layoutFrames(const fs::path& evidence) {
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 560, "PBL2 layout check");
    SetWindowMinSize(800, 560);
    CHECK(IsWindowReady() && UiText::initialize());
    if (!evidence.empty()) fs::create_directories(evidence);
    const auto capture = [&](RaylibStateTest& ui, const std::string& name) {
        const CapturedFrame frame([&] { ui.render(); });
        CHECK(frame.image.width == frameWidth && frame.image.height == frameHeight);
        if (!evidence.empty()) {
            const auto path = evidence / (name + "-" + std::to_string(frameWidth) + ".png");
            CHECK(ExportImage(frame.image, path.string().c_str()));
        }
    };
    {
        TempDirectory temp;
        saveFixture(temp.path, fixture());
        RaylibStateTest ui(temp.path);
        capture(ui, "login-admin-guidance");
        login(ui, "learner1");
        const auto content = UiLayout::content();
        const auto initial = CapturedFrame([&] { ui.render(); });
        ui.frame(click(content.x + content.width - 98, content.y + 198));
        const auto previous = CapturedFrame([&] { ui.render(); });
        const int monthLeft = static_cast<int>(content.x + 45);
        const int monthTop = static_cast<int>(content.y + 217);
        CHECK(previous.changedFrom(initial, monthLeft, monthTop, 150, 24) > 30);
        capture(ui, "learner-dashboard-previous-month");
        ui.frame(click(content.x + content.width - 58, content.y + 198));
        const auto restored = CapturedFrame([&] { ui.render(); });
        CHECK(restored.changedFrom(initial, monthLeft, monthTop, 150, 24) == 0);
        capture(ui, "learner-password-entry");
        ui.frame(click(148, 520));
        ui.frame(paste("demo123"));
        ui.frame(press(KEY_TAB));
        ui.frame(paste(u8"đ日あ-12"));
        capture(ui, "learner-password-masked");
        ui.frame(press(KEY_TAB));
        ui.frame(press(KEY_TAB));
        ui.frame(press(KEY_ENTER));
        const CapturedFrame revealed([&] { ui.render(); });
        CHECK(ui.passwordsVisible() && !UiText::hasGlyph(0x0111));
        capture(ui, "learner-password-revealed");
        resizeFrame(1260, 760);
        ui.frame(backTab());
        ui.frame(paste(u8"đ日あ-12"));
        CHECK(ui.passwordDialog().newPassword() == u8"đ日あ-12");
        capture(ui, "learner-password-revealed");
        ui.frame(press(KEY_TAB));
        ui.frame(press(KEY_ENTER));
        CHECK(!ui.passwordsVisible());
        capture(ui, "learner-password-masked");
        resizeFrame(800, 560);
        const auto blocker = temp.path / ".pbl2-stage";
        writeFile(blocker, "occupied");
        ui.frame(click(570, 455));
        CHECK(ui.app().dirty() && ui.passwordFieldsEmpty());
        capture(ui, "learner-password-save-pending");
        CHECK(fs::remove(blocker));
        ui.frame(press(KEY_ENTER));
        CHECK(ui.passwordMessage() == "Password changed and saved.");
        capture(ui, "learner-password-saved");
        ui.frame(press(KEY_ESCAPE));
        CHECK(!ui.passwordDialog().isOpen() && ui.passwordFieldsEmpty());
    }
    const std::string reading = u8"がっこう、だいいちこうしゃ、にかいのきょうしつ、としょしつ、"
                                u8"うんどうじょう、たいいくかん、にほんごのじゅぎょう。読み終わり";
    const std::string meaning = u8"Trường học có nhiều phòng phục vụ việc học và sinh hoạt. Học sinh gặp giáo viên "
                                u8"ở phòng học riêng trước khi đến thư viện, đọc sách và hoàn thành bài tập. Cuối nghĩa: CỔNG XANH.";
    const std::string example = u8"学校へ行くときは、正門から入って、右に曲がって、廊下の奥にある教室で友だちを待ちます。"
                                u8"Sau giờ học, chúng tôi đi qua thư viện và đợi chuyến xe cuối ở cổng sau. Cuối ví dụ: XE SỐ 87.";
    const std::string prompt = "Read the full question before choosing your answer. Four announcements describe the same "
                               "departure and the same arrival time. Each announcement provides a different destination at "
                               "the very end. Pick the one ending in Osaka. PROMPT END.";
    const std::string prefix = "After meeting under the clock at the central station, follow the signs to the east entrance, "
                                "check the printed timetable and buy a ticket for the last train of the evening. The last stop is ";
    const std::array<std::string, 4> options{prefix + "TOKYO.", prefix + "KYOTO.", prefix + "OSAKA.", prefix + "NAGOYA."};
    const std::string explanation = "All choices repeat the same instructions about the clock, the entrance and the ticket. "
                                    "Compare the final city names to find the destination requested in the question. The travel "
                                    "instructions are shared, and the final destination determines the correct choice. "
                                    "The word Osaka appears in choice C. END OF EXPLANATION 927.";
    auto data = fixture();
    data.vocabularies.front() = Vocabulary(ResourceId("R001"), LevelId("L001"), u8"学校", reading, meaning,
                                           PartOfSpeech::NOUN, example);
    data.vocabularies.emplace_back(ResourceId("R004"), LevelId("L001"), u8"学校別館", u8"がっこうべっかん",
                                   "Annex with a different reading.", PartOfSpeech::NOUN, "");
    data.favorites.clear();
    data.levels.front() = Level(LevelId("L001"), "JLPT", "JLPT N5", 5, "");
    data.users.emplace_back(UserId("U12345"), "learner_long_username_final_47", "demo123",
                            UserRole::LEARNER, UserStatus::LOCKED);
    data.questions.clear();
    for (int i = 1; i <= 3; ++i)
        data.questions.emplace_back(QuestionId("Q5432" + std::to_string(i)), LevelId("L001"), std::nullopt,
                                    prompt, options, 'C', explanation, i == 3 ? QuestionStatus::INACTIVE : QuestionStatus::ACTIVE);
    data.attempts.clear();
    data.attempts.emplace_back(AttemptId("T12345"), UserId("U002"), LevelId("L001"), 10, 7, "2026-11-28T23:59:59Z");
    data.rebuildIndexes();
    data.validateRelations(true);

    for (const bool admin : {false, true}) {
        resizeFrame(800, 560);
        TempDirectory temp;
        saveFixture(temp.path, data);
        RaylibStateTest ui(temp.path);
        login(ui, admin ? "admin" : "learner1");
        const std::string role = admin ? "admin" : "learner";
        ui.navigate(Section::Resources);
        auto content = UiLayout::content();
        ui.frame(click(content.x + 32, content.y + 70));
        ui.frame(paste(u8"学校"));
        ui.frame(click(content.x + 145, content.y + 116));
        CHECK(ui.resourceRows().size() == 2 && ui.resourceRows()[0].id == ResourceId("R001"));
        const auto select = [&](int row) {
            const auto area = UiLayout::content();
            ui.frame(click(area.x + 32, area.y + 164 + 38 + 12 + row * 65));
        };
        select(0);
        CHECK(ui.selectedResource() && std::get<Vocabulary>(*ui.selectedResource()).getId() == ResourceId("R001"));
        ui.frame(press(KEY_TAB));
        ui.frame(press(KEY_END));
        capture(ui, role + "-resource-keyboard-end");
        const auto resourceScreens = [&](const std::string& state) {
            const auto area = UiLayout::content();
            const float pad = std::clamp(area.width * 0.025f, 16.0f, 24.0f);
            const float width = area.width - pad * 2;
            const float detailWidth = (width - 14) * 0.56f;
            const float banner = UITheme::bannerHeight(width, ui.resourcesMessage());
            const float y = area.y + 164 + banner + 44;
            const float x = area.x + area.width * 0.78f;
            const float wrapWidth = detailWidth - 34.0f;
            const auto step = [&](float offset) { ui.frame(wheelAt(x, y, -offset / 48.0f)); };
            ui.frame(wheelAt(x, y, 100));
            capture(ui, role + "-resource-" + state + "-top");
            const float title = UITheme::wrap("R001 | Vocabulary", wrapWidth, 15).height() + 6 +
                                UITheme::wrap(u8"学校", wrapWidth, 30).height() + 6;
            step(title);
            capture(ui, role + "-resource-" + state + "-reading");
            step(UITheme::wrap(reading, wrapWidth, 18).height() + 28);
            capture(ui, role + "-resource-" + state + "-meaning-start");
            const float meaningHeight = UITheme::wrap(meaning, wrapWidth, 18).height();
            const float viewportHeight = area.height - 164 - banner - 46 - 46;
            const float meaningTail = std::max(0.0f, meaningHeight + 25 - viewportHeight);
            step(meaningTail);
            capture(ui, role + "-resource-" + state + "-meaning");
            step(meaningHeight + 28 + UITheme::wrap("NOUN", wrapWidth, 18).height() + 28 - meaningTail);
            capture(ui, role + "-resource-" + state + "-example-start");
            ui.frame(wheelAt(x, y, -100));
            capture(ui, role + "-resource-" + state + "-example");
        };
        resourceScreens("plain");
        resizeFrame(1260, 760);
        ui.frame({});
        capture(ui, role + "-resource-resized");
        resourceScreens("plain");
        select(1);
        CHECK(ui.selectedResource() && std::get<Vocabulary>(*ui.selectedResource()).getId() == ResourceId("R004"));
        capture(ui, role + "-resource-next-item");
        content = UiLayout::content();
        ui.frame(wheelAt(content.x + 40, content.y + 220, -1));
        select(0);
        CHECK(ui.selectedResource() && std::get<Vocabulary>(*ui.selectedResource()).getId() == ResourceId("R001"));
        const auto favorite = [&] {
            const auto area = UiLayout::content();
            ui.frame(click(area.x + area.width - 40, area.y + area.height - 21));
        };
        ui.frame(click(content.x + content.width - 10, content.y + content.height - 21));
        CHECK(ui.resourcesMessage().empty());
        favorite();
        CHECK(ui.resourcesMessage() == "Added to favorites.");
        resourceScreens("favorite");
        resizeFrame(800, 560);
        ui.frame({});
        resourceScreens("favorite");
        content = UiLayout::content();
        const float listY = content.y + 164 + UITheme::bannerHeight(content.width - 32, ui.resourcesMessage()) + 50;
        ui.frame(wheelAt(content.x + 48, listY, -1));
        CHECK(ui.selectedResource() && std::get<Vocabulary>(*ui.selectedResource()).getId() == ResourceId("R001"));
        capture(ui, role + "-resource-list-scroll");
        ui.frame(click(content.x + 48, listY));
        CHECK(ui.selectedResource() && std::get<Vocabulary>(*ui.selectedResource()).getId() == ResourceId("R004"));
        ui.frame(wheelAt(content.x + 48, content.y + 220, 1));
        select(0);
        CHECK(ui.selectedResource() && std::get<Vocabulary>(*ui.selectedResource()).getId() == ResourceId("R001"));
        const auto file = temp.path / "favorites.txt";
        const auto backup = temp.path / "favorites.backup";
        fs::rename(file, backup);
        fs::create_directory(file);
        favorite();
        CHECK(ui.app().dirty() && ui.resourcesMessage().find("cannot replace favorites.txt") != std::string::npos);
        resourceScreens("save-error");
        resizeFrame(1260, 760);
        ui.frame({});
        resourceScreens("save-error");
        resizeFrame(800, 560);
        ui.frame(click(670, 48));
        capture(ui, role + "-save-error-close");
        ui.frame(press(KEY_ESCAPE));
        CHECK(ui.authenticated() && ui.app().dirty());
        CHECK(fs::remove(file));
        fs::rename(backup, file);
        CHECK(ui.app().retrySave().save == SaveState::SAVED);

        if (admin) {
            for (const auto size : {800, 1260}) {
                resizeFrame(size, size == 800 ? 560 : 760);
                const auto area = UiLayout::content();
                ui.navigate(Section::Dashboard);
                capture(ui, "admin-dashboard");
                for (const auto section : {Section::Accounts, Section::Questions}) {
                    const bool accounts = section == Section::Accounts;
                    const std::string name = accounts ? "admin-accounts" : "admin-questions";
                    ui.navigate(section);
                    ui.frame(click(area.x + 34, area.y + 124));
                    ui.frame(press(KEY_END));
                    capture(ui, name);
                    ui.frame(press(KEY_TAB));
                    ui.frame(press(KEY_ENTER));
                    CHECK(ui.backend().modalOpen());
                    capture(ui, name + "-details");
                    if (size == 800) {
                        resizeFrame(1260, 760);
                        ui.frame({});
                        capture(ui, name + "-details-expanded");
                        resizeFrame(800, 560);
                    }
                    ui.frame(press(KEY_ESCAPE));
                    CHECK(!ui.backend().modalOpen());
                }
            }
            continue;
        }

        for (const auto size : {800, 1260}) {
            resizeFrame(size, size == 800 ? 560 : 760);
            const auto area = UiLayout::content();
            ui.navigate(Section::Statistics);
            ui.frame(click(area.x + 34, area.y + 202));
            capture(ui, "learner-statistics");
            ui.frame(press(KEY_ENTER));
            CHECK(ui.backend().modalOpen());
            capture(ui, "learner-statistics-details");
            ui.frame(press(KEY_ESCAPE));
        }

        resizeFrame(800, 560);
        ui.navigate(Section::Quiz);
        const auto actionY = [&] {
            const auto area = UiLayout::content();
            const float banner = UITheme::bannerHeight(area.width - 24, ui.backendMessage());
            return area.y + area.height - 48 - (banner > 0 ? banner + 8 : 0);
        };
        const auto quizAction = [&] { ui.frame(click(UiLayout::content().x + 32, actionY() + 16)); };
        quizAction();
        CHECK(ui.activity().quiz && ui.activity().quiz->questions.size() == 2);
        bool resizedBack = false;
        for (const auto size : {800, 1260, 800}) {
            resizeFrame(size, size == 800 ? 560 : 760);
            const auto area = UiLayout::content();
            const float x = area.x + 62;
            const float top = area.y + 120;
            const float viewHeight = actionY() - 8 - (area.y + 72) - 100;
            const float width = area.width - 48;
            ui.frame(wheelAt(x, top + 15, 100));
            if (!resizedBack) capture(ui, "learner-quiz-prompt");
            float next = UITheme::wrap(prompt, width - 12, 20).height() + 10;
            float offset = 0;
            float answerY = 0;
            for (std::size_t i = 0; i < options.size(); ++i) {
                const std::string letter(1, static_cast<char>('A' + i));
                const auto label = UITheme::wrap(letter + ". " + options[i], width - 36, 17);
                const float lastLine = next + 7 + label.height() - label.lineHeight();
                const float target = std::max(0.0f, lastLine + label.lineHeight() - viewHeight + 10);
                ui.frame(wheelAt(x, top + 15, (offset - target) / 48.0f));
                offset = target;
                answerY = top + lastLine - offset + 8;
                if (!resizedBack) capture(ui, "learner-quiz-choice-" + letter);
                if (i == 2) {
                    ui.frame(click(x, answerY));
                    CHECK(ui.activity().quiz->selectedAnswers == std::vector<char>({'C', '-'}));
                }
                next += label.height() + 22;
            }
            const auto before = ui.activity().quiz->selectedAnswers;
            ui.frame(click(area.x + area.width * 0.5f, actionY() - 30));
            CHECK(ui.activity().quiz->selectedAnswers == before);
            ui.frame(click(x, answerY));
            CHECK(ui.activity().quiz->selectedAnswers == std::vector<char>({'D', '-'}));
            if (resizedBack) capture(ui, "learner-quiz-after-resize");
            resizedBack = resizedBack || size == 1260;
        }
        const auto attempts = temp.path / "quiz_attempt.txt";
        const auto priorAttempts = temp.path / "quiz_attempt.backup";
        fs::rename(attempts, priorAttempts);
        fs::create_directory(attempts);
        quizAction();
        CHECK(ui.prompt().find("1 question is unanswered") != std::string::npos);
        ui.frame(backTab());
        ui.frame(press(KEY_ENTER));
        CHECK(ui.activity().quiz->submitted && ui.activity().quizAnswers.size() == 2);
        CHECK(ui.activity().quizAnswers[0].correctAnswer == 'C' && ui.activity().quizAnswers[0].selectedAnswer == 'D');
        CHECK(ui.activity().quizAnswers[1].selectedAnswer == '-');
        CHECK(ui.app().dirty() && ui.backendMessage().find("cannot replace quiz_attempt.txt") != std::string::npos);
        CHECK(UITheme::bannerHeight(UiLayout::content().width - 24, ui.backendMessage()) > 39);
        for (const auto size : {800, 1260}) {
            resizeFrame(size, size == 800 ? 560 : 760);
            const auto area = UiLayout::content();
            const float x = area.x + 62;
            const float y = area.y + 134;
            ui.frame(click(x, y));
            ui.frame(press(KEY_HOME));
            capture(ui, "learner-quiz-result-top");
            const float width = area.width - 88;
            const auto block = [&](const std::string& text, int fontSize) {
                return UITheme::wrap(text, width, fontSize).height() + 8.0f;
            };
            const float yourAnswer = 10 + block("Question 1", 15) + block(prompt, 19);
            const float correctAnswer = yourAnswer + block("Your answer: D. " + options[3], 17);
            const float feedback = correctAnswer + block("Correct answer: C. " + options[2], 17);
            const float blank = feedback + block("Explanation: " + explanation, 17) + 10 +
                                block("Question 2", 15) + block(prompt, 19);
            const auto scrollTo = [&](float target) {
                ui.frame(press(KEY_HOME));
                ui.frame(wheelAt(x, y, -target / 48.0f));
            };
            scrollTo(yourAnswer - 6);
            capture(ui, "learner-quiz-result-selected");
            scrollTo(correctAnswer - 6);
            capture(ui, "learner-quiz-result-correct");
            scrollTo(feedback - 6);
            capture(ui, "learner-quiz-result-explanation");
            scrollTo(blank - 6);
            capture(ui, "learner-quiz-result-blank");
            ui.frame(press(KEY_END));
            capture(ui, "learner-quiz-result-end");
        }
        CHECK(fs::remove(attempts));
        fs::rename(priorAttempts, attempts);
        CHECK(ui.app().retrySave().save == SaveState::SAVED);
        resizeFrame(800, 560);
        ui.frame(press(KEY_TAB));
        ui.frame(press(KEY_ENTER));
        CHECK(ui.activity().quiz && !ui.activity().quiz->submitted);
        capture(ui, "learner-quiz-retry");
        ui.frame(press(KEY_TAB));
        ui.frame(press(KEY_TAB));
        ui.frame(press(KEY_TAB));
        capture(ui, "learner-quiz-keyboard-focus");
        resizeFrame(1260, 760);
        ui.frame({});
        capture(ui, "learner-quiz-keyboard-focus");
        ui.frame(press(KEY_ENTER));
        CHECK(ui.activity().quiz->selectedAnswers == std::vector<char>({'C', '-'}));
    }
    resizeFrame(800, 560);
    UiText::shutdown();
    CloseWindow();
}
}

int main(int argc, char** argv) {
    try {
        if (argc >= 2 && std::string_view(argv[1]) == "--draw") {
            firstFrameGlyphs();
            layoutFrames(argc == 3 ? fs::path(argv[2]) : fs::path{});
        }
        else {
            accountSelectionAndForm();
            searchAndScroll();
            searchHistoryReplay();
            quizResizeAndCloseKeys();
            controllerInput();
            changePasswordFlow();
            packageClipboardAndRows();
            startupRecovery();
            packageOverwrite();
            learningConfirmations();
            workspaceSmoke();
            corpusSmoke();
            longField();
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

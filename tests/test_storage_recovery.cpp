#include "Fixture.h"
#include "pbl2/Application.h"
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <map>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
namespace fs = std::filesystem;

void requireSaved(const Status& status) {
    if (!status.ok() || status.save != SaveState::SAVED) throw std::runtime_error(status.message);
}

void requireBusy(Application& app, const fs::path& path) {
    const auto status = app.load(path.u8string());
    CHECK(status.error == BackendError::IO_ERROR && status.message.find("already in use") != std::string::npos);
    CHECK(!app.loaded() && app.login("learner2", "demo123").error == BackendError::NOT_LOADED);
    SearchOptions query;
    query.query = "denied";
    CHECK(app.submitSearch(query).status.error == BackendError::NOT_LOADED);
}

void saveSearch(Application& app, const char* user, const char* term) {
    CHECK(app.login(user, "demo123").ok());
    SearchOptions query;
    query.query = term;
    requireSaved(app.submitSearch(query).status);
}

void requireSearch(Application& app, const char* user, const char* term, std::size_t count) {
    CHECK(app.login(user, "demo123").ok());
    const auto history = app.history();
    CHECK(history.ok() && history.value->size() == count && history.value->front().getQuery() == term);
}

void exclusiveSessions() {
    TempDirectory temp;
    const auto root = temp.path / "live";
    saveFixture(root, fixture());
    const auto alias = root / ".." / "live" / ".";
    Application first, second;
    CHECK(first.load(root.u8string()).ok());
    requireBusy(second, alias);
    CHECK(first.load(alias.u8string()).ok());
    saveSearch(first, "learner1", "first-session");
    first.logout();
    requireBusy(second, root);
    CHECK(first.close().ok());
    CHECK(second.load(alias.u8string()).ok());
    saveSearch(second, "learner2", "second-session");
    CHECK(second.close().ok());
    CHECK(first.load(root.u8string()).ok());
    requireSearch(first, "learner1", "first-session", 2);
    requireSearch(first, "learner2", "second-session", 1);
    std::cout << "OK same-process ownership, alias, normal close and both histories\n";
}

#ifdef _WIN32
constexpr DWORD crashExit = 75;
constexpr auto wordId = "R004";

std::string bytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    CHECK(in);
    return {std::istreambuf_iterator<char>(in), {}};
}

std::map<fs::path, std::string> snapshot(const fs::path& root) {
    std::map<fs::path, std::string> result;
    for (const auto& entry : fs::directory_iterator(root))
        if (entry.path().extension() == ".txt") result.emplace(entry.path().filename(), bytes(entry.path()));
    return result;
}

class BlockReplace {
    HANDLE handle_;

  public:
    explicit BlockReplace(const fs::path& path)
        : handle_(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) {
        CHECK(handle_ != INVALID_HANDLE_VALUE);
    }
    ~BlockReplace() { CloseHandle(handle_); }
    BlockReplace(const BlockReplace&) = delete;
};

Result<ResourceId> addWord(Application& app) {
    return app.createVocabulary({LevelId("L001"), "recovery-word", "reading", "meaning", PartOfSpeech::NOUN, "example"});
}

void openAdmin(Application& app, const fs::path& root) {
    const auto status = app.load(root.u8string());
    if (!status.ok()) throw std::runtime_error(status.message);
    CHECK(app.login("admin", "demo123").ok());
}

void requirePending(const Result<ResourceId>& added, const Application& app) {
    CHECK(added.ok() && added.value == ResourceId(wordId) && app.dirty());
    CHECK(added.status.save == SaveState::PENDING && added.status.message == "cannot replace vocabulary.txt");
}

void requirePartialWord(const fs::path& root) {
    CHECK(bytes(root / "learning_resource.txt").find("R004|VOCABULARY|L001\n") != std::string::npos);
    CHECK(bytes(root / "vocabulary.txt").find("R004|") == std::string::npos);
    CHECK(fs::is_directory(root / ".pbl2-rollback"));
}

void requireWord(const Application& app) {
    const auto found = app.resource(ResourceId(wordId));
    CHECK(found.ok());
    const auto& word = std::get<Vocabulary>(*found.value);
    CHECK(word.getWord() == "recovery-word" && word.getReading() == "reading" && word.getMeaning() == "meaning");
    CHECK(word.getLevelId() == LevelId("L001") && app.statistics().value->vocabulary == 2);
}

void lockedRetry() {
    TempDirectory temp;
    const auto& root = temp.path;
    saveFixture(root, fixture());
    const auto old = snapshot(root);
    Application app;
    openAdmin(app, root);
    {
        const BlockReplace block(root / "vocabulary.txt");
        requirePending(addWord(app), app);
        requirePartialWord(root);
        const auto retry = app.retrySave();
        CHECK(retry.error == BackendError::IO_ERROR && retry.save == SaveState::PENDING && app.dirty());
        CHECK(app.close().error == BackendError::UNSAVED_CHANGES);
        Application competing;
        requireBusy(competing, root);
    }
    requireSaved(app.retrySave());
    CHECK(!app.dirty() && app.close().ok());
    openAdmin(app, root);
    requireWord(app);
    const auto updated = snapshot(root);
    for (const auto& entry : old) {
        const bool changed = entry.first == "learning_resource.txt" || entry.first == "vocabulary.txt";
        CHECK((updated.at(entry.first) != entry.second) == changed);
    }
    std::cout << "OK Windows vocabulary sharing violation, pending close and full Retry Save\n";
}

struct ProcessFixture {
    TempDirectory temp;
    const fs::path root = temp.path / "live";
    const fs::path package = temp.path / "package";

    ProcessFixture() {
        writeFile(temp.path / "recovery-fixture.marker", "test");
        saveFixture(root, fixture());
    }

    void makePackage() const {
        DataStore data;
        data.levels.emplace_back(LevelId("L010"), "JLPT", "N2", 2, "");
        data.grammars.emplace_back(ResourceId("R010"), LevelId("L010"), "pattern", "meaning", "structure", "", "", "");
        data.vocabularies.emplace_back(ResourceId("R011"), LevelId("L010"), "imported", "", "meaning", PartOfSpeech::NOUN, "");
        data.rebuildIndexes();
        std::string error;
        CHECK(FileStorage{}.savePackage(package.u8string(), data, error));
    }
};

class Child {
    HANDLE process_;

  public:
    Child(const fs::path& executable, const wchar_t* mode, const fs::path& root) {
        std::wstring command = L"\"" + executable.native() + L"\" " + mode + L" \"" + root.native() + L"\"";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION info{};
        CHECK(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0,
                             nullptr, nullptr, &startup, &info));
        CloseHandle(info.hThread);
        process_ = info.hProcess;
    }

    ~Child() {
        if (WaitForSingleObject(process_, 0) == WAIT_TIMEOUT) {
            TerminateProcess(process_, crashExit);
            WaitForSingleObject(process_, 5000);
        }
        CloseHandle(process_);
    }

    Child(const Child&) = delete;

    void awaitExit(DWORD expected) const {
        CHECK(WaitForSingleObject(process_, 15000) == WAIT_OBJECT_0);
        DWORD actual = 0;
        CHECK(GetExitCodeProcess(process_, &actual) && actual == expected);
    }

    void awaitReady(const fs::path& root) const {
        for (int i = 0; i < 1500; ++i) {
            if (fs::exists(root.parent_path() / "ready")) return;
            CHECK(WaitForSingleObject(process_, 10) == WAIT_TIMEOUT);
        }
        throw std::runtime_error("child never became ready");
    }

    void kill() const {
        CHECK(TerminateProcess(process_, crashExit));
        awaitExit(crashExit);
    }
};

StorageTestPoint crashAt;
int stepsLeft;

void dieAt(StorageTestPoint point, int occurrence = 1) {
    crashAt = point;
    stepsLeft = occurrence;
    storageTestHook = [](StorageTestPoint event) {
        if (event == crashAt && --stepsLeft == 0) {
            TerminateProcess(GetCurrentProcess(), crashExit);
            std::abort();
        }
    };
}

void waitToBeKilled(const fs::path& root) {
    writeFile(root.parent_path() / "ready", "ready");
    Sleep(20000);
    throw std::runtime_error("parent did not terminate the test child");
}

void childMode(const std::string& mode, const fs::path& root) {
    CHECK(fs::exists(root.parent_path() / "recovery-fixture.marker"));
    const auto name = root.filename();
    CHECK(name == "live" || name == "LIVE" || name == "fresh");
    Application app;
    if (mode == "contender") { requireBusy(app, root); return; }
    if (mode == "owner") {
        CHECK(app.load(root.u8string()).ok());
        saveSearch(app, "learner1", "before-process-exit");
        waitToBeKilled(root);
    }
    if (mode == "partial-initialize") {
        dieAt(StorageTestPoint::REPLACED);
        app.initialize(root.u8string(), "admin", "demo123");
    } else if (mode == "recover-crash") {
        dieAt(StorageTestPoint::RESTORED);
        app.load(root.u8string());
    } else {
        openAdmin(app, root);
        if (mode == "blocked-save") {
            const BlockReplace block(root / "vocabulary.txt");
            requirePending(addWord(app), app);
            waitToBeKilled(root);
        }
        if (mode == "partial-import") {
            dieAt(StorageTestPoint::REPLACED, 3);
            app.importPackage((root.parent_path() / "package").u8string());
        } else {
            CHECK(mode == "partial-add" || mode == "after-commit");
            dieAt(mode == "partial-add" ? StorageTestPoint::REPLACED : StorageTestPoint::COMMITTED);
            addWord(app);
        }
    }
    throw std::runtime_error("fault boundary was not reached");
}

void crossProcessOwnership(const fs::path& exe) {
    ProcessFixture fixture;
    const Child first(exe, L"owner", fixture.root);
    first.awaitReady(fixture.root);
    const auto alias = fixture.root / ".." / "LIVE";
    const Child second(exe, L"contender", alias);
    second.awaitExit(0);
    first.kill();
    Application reopened;
    CHECK(reopened.load(alias.u8string()).ok());
    requireSearch(reopened, "learner1", "before-process-exit", 2);
    saveSearch(reopened, "learner2", "after-process-exit");
    CHECK(reopened.close().ok());
    std::cout << "OK Windows competing process, case alias, forced owner exit and reopen\n";
}

void verifyWordCrash(const fs::path& exe, const wchar_t* mode, bool committed) {
    ProcessFixture fixture;
    const auto old = snapshot(fixture.root);
    const Child writer(exe, mode, fixture.root);
    if (std::wstring(mode) == L"blocked-save") {
        writer.awaitReady(fixture.root);
        requirePartialWord(fixture.root);
        writer.kill();
    } else {
        writer.awaitExit(crashExit);
        if (committed) {
            CHECK(!fs::exists(fixture.root / ".pbl2-rollback"));
            CHECK(fs::exists(fixture.root / ".pbl2-stage"));
        } else requirePartialWord(fixture.root);
    }

    Application reopened;
    openAdmin(reopened, fixture.root);
    if (committed) requireWord(reopened);
    else {
        CHECK(!reopened.resource(ResourceId(wordId)).ok());
        CHECK(snapshot(fixture.root) == old);
    }
    CHECK(!fs::exists(fixture.root / ".pbl2-rollback") && !fs::exists(fixture.root / ".pbl2-stage"));
}

void interruptedRecovery(const fs::path& exe) {
    ProcessFixture fixture;
    fixture.makePackage();
    const auto old = snapshot(fixture.root);
    Child(exe, L"partial-import", fixture.root).awaitExit(crashExit);
    CHECK(bytes(fixture.root / "learning_resource.txt").find("R011|VOCABULARY|L010\n") != std::string::npos);
    CHECK(bytes(fixture.root / "vocabulary.txt") == old.at("vocabulary.txt"));
    Child(exe, L"recover-crash", fixture.root).awaitExit(crashExit);
    CHECK(bytes(fixture.root / "grammar.txt") == old.at("grammar.txt"));
    CHECK(fs::exists(fixture.root / ".pbl2-rollback" / "learning_resource.txt.old"));

    Application app;
    {
        const BlockReplace block(fixture.root / "learning_resource.txt");
        const auto error = app.load(fixture.root.u8string());
        CHECK(!app.loaded() && error.error == BackendError::IO_ERROR && error.message == "cannot restore learning_resource.txt");
        CHECK(fs::exists(fixture.root / ".pbl2-rollback" / "learning_resource.txt.old"));
    }
    openAdmin(app, fixture.root);
    CHECK(!app.resource(ResourceId("R010")).ok() && !app.resource(ResourceId("R011")).ok());
    CHECK(snapshot(fixture.root) == old);
    requireSaved(app.importPackage(fixture.package.u8string()));
    CHECK(app.close().ok());
    openAdmin(app, fixture.root);
    const auto grammar = app.resource(ResourceId("R010"));
    const auto vocab = app.resource(ResourceId("R011"));
    CHECK(grammar.ok() && vocab.ok());
    CHECK(std::get<Grammar>(*grammar.value).getLevelId() == LevelId("L010"));
    CHECK(std::get<Vocabulary>(*vocab.value).getLevelId() == LevelId("L010"));
    CHECK(std::get<Vocabulary>(*vocab.value).getWord() == "imported");
    std::cout << "OK interrupted import, killed recovery, blocked recovery and retry from original bytes\n";
}

void interruptedInitialize(const fs::path& exe) {
    ProcessFixture fixture;
    const auto fresh = fixture.temp.path / "fresh";
    Child(exe, L"partial-initialize", fresh).awaitExit(crashExit);
    CHECK(fs::exists(fresh / "alphabet.txt"));
    Application app;
    requireSaved(app.initialize(fresh.u8string(), "admin", "demo123"));
    CHECK(app.close().ok());
    openAdmin(app, fresh);
    CHECK(app.statistics().value->vocabulary == 0);
    std::cout << "OK interrupted initialize recovered empty store and initialized again\n";
}

void exportRecovery() {
    ProcessFixture fixture;
    fixture.makePackage();
    writeFile(fixture.package / "grammar.txt", "R010|old-marker|meaning|structure|||\n");
    writeFile(fixture.package / "untouched.txt", "keep me\n");
    const auto old = snapshot(fixture.package);
    Application app;
    openAdmin(app, fixture.root);
    const auto plan = app.inspectPackageExport(fixture.package.u8string());
    CHECK(plan.value && plan.value->existingFiles.size() == 5);
    CHECK(app.exportPackage(plan.value->folder).error == BackendError::REJECTED);
    CHECK(snapshot(fixture.package) == old);
    {
        const BlockReplace block(fixture.package / "vocabulary.txt");
        const auto result = app.exportPackage(plan.value->folder, plan.value->existingFiles);
        CHECK(result.error == BackendError::IO_ERROR && result.message == "cannot replace vocabulary.txt");
        CHECK(!app.dirty());
        CHECK(snapshot(fixture.package) == old && !fs::exists(fixture.package / ".pbl2-rollback"));
    }
    DataStore package;
    std::string error;
    CHECK(FileStorage{}.loadPackage(fixture.package.u8string(), package, error));
    CHECK(package.findResource(ResourceId("R011")) && snapshot(fixture.package) == old);
    CHECK(app.exportPackage(plan.value->folder, plan.value->existingFiles).ok());
    CHECK(FileStorage{}.loadPackage(fixture.package.u8string(), package, error));
    CHECK(!package.findResource(ResourceId("R011")) && package.findResource(ResourceId("R001")));
    CHECK(bytes(fixture.package / "untouched.txt") == "keep me\n");
    std::cout << "OK export rollback and complete package export\n";
}
#endif
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    try {
        if (argc == 3) { childMode(fs::path(argv[1]).string(), fs::path(argv[2])); return 0; }
        CHECK(argc == 1);
        const auto exe = fs::absolute(argv[0]);
        exclusiveSessions();
        crossProcessOwnership(exe);
        lockedRetry();
        verifyWordCrash(exe, L"blocked-save", false);
        verifyWordCrash(exe, L"partial-add", false);
        verifyWordCrash(exe, L"after-commit", true);
        std::cout << "OK forced exit after blocked write, after first replacement and after commit\n";
        interruptedRecovery(exe);
        interruptedInitialize(exe);
        exportRecovery();
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
#else
int main() {
    try { exclusiveSessions(); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
#endif

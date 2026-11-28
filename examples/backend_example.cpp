#include "pbl2/Application.h"
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <utility>

namespace {
namespace fs = std::filesystem;
constexpr const char* adminPassword = "demo123";
constexpr const char* learnerPassword = "demo456";

void accept(Application& app, const Status& status) {
    if (!status.ok()) throw std::runtime_error(status.message);
    if (status.save == SaveState::PENDING) {
        const auto retry = app.retrySave();
        if (!retry.ok()) throw std::runtime_error(retry.message);
    }
}
template<class T> T value(Application& app, Result<T> result) {
    accept(app, result.status);
    if (!result.value) throw std::runtime_error("missing backend result");
    return std::move(*result.value);
}
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void bootstrap(Application& app, const fs::path& folder) {
    accept(app, app.initialize(folder.u8string(), "admin", adminPassword));
    accept(app, app.login("admin", adminPassword));
    value(app, app.createAccount("learner", learnerPassword, UserRole::LEARNER));
    value(app, app.createAccount("partner", learnerPassword, UserRole::LEARNER));
    const auto level = value(app, app.createLevel({"JLPT", "N5", 5, "Ví dụ"}));
    value(app, app.createLevel({"JLPT", "N4", 4, "Mức trống để thử bộ lọc"}));
    const auto word = value(app, app.createVocabulary(
        {level, "学校", "がっこう", "trường học", PartOfSpeech::NOUN, "学校へ行く"}));
    value(app, app.createKanji({level, "学", "học", 8, "ガク", "まなぶ", ""}));
    value(app, app.createGrammar({level, "～たい", "muốn", "Vます + たい", "", "", ""}));
    value(app, app.createAlphabet({Script::HIRAGANA, "あ", "a", "A", 1, ""}));
    for (int i = 1; i <= 10; ++i) {
        value(app, app.createVocabulary({level, "例" + std::to_string(i), "れい",
            "ví dụ " + std::to_string(i), PartOfSpeech::NOUN, ""}));
    }
    value(app, app.createQuestion({level, word, "学校 nghĩa là gì?",
        {"trường học", "nhà", "sách", "tàu"}, 'A', "学校 là trường học.", QuestionStatus::ACTIVE}));
}

void walkthrough(const fs::path& root) {
    Application app(42);
    const auto folder = root / "learning";
    bootstrap(app, folder);
    accept(app, app.exportPackage((root / "package").u8string()));

    app.logout();
    accept(app, app.login("learner", learnerPassword));
    SearchOptions options;
    options.query = "　学校 ";
    const auto rows = value(app, app.submitSearch(options));
    check(!rows.empty() && rows.front().id == ResourceId("R001"), "missing exact vocabulary hit");
    const auto word = rows.front().id;
    const auto level = rows.front().levelId;
    const auto detail = value(app, app.resource(word));
    check(std::get<Vocabulary>(detail).getReading() == "がっこう", "unexpected reading");
    accept(app, app.addFavorite(word));
    const auto history = value(app, app.history());
    check(history.size() == 1 && history.front().getQuery() == "学校", "history did not preserve prepared query");
    const auto replay = value(app, app.searchAgain(history.front().getId()));
    check(!replay.rows.empty() && replay.rows.front().id == word, "unexpected history result");

    accept(app, app.startReview());
    const auto hidden = value(app, app.review());
    check(hidden.current && !hidden.current->back, "review answer exposed before reveal");
    accept(app, app.revealReview());
    const auto revealed = value(app, app.review());
    check(revealed.current && revealed.current->back, "review answer missing after reveal");
    accept(app, app.rateReview(true));
    check(value(app, app.review()).finished, "review should have finished");
    accept(app, app.startQuiz(level, 1));
    check(!app.quizAnswers().ok(), "quiz answers exposed before submit");
    accept(app, app.selectAnswer(0, 'B'));
    check(value(app, app.submitQuiz()).getCorrectAnswers() == 0, "incorrect quiz score");
    check(!app.submitQuiz().ok(), "quiz accepted a duplicate submit");
    accept(app, app.retryWrong());
    accept(app, app.selectAnswer(0, 'A'));
    check(value(app, app.submitQuiz()).getCorrectAnswers() == 1, "incorrect retry score");
    accept(app, app.close());

    accept(app, app.load(folder.u8string()));
    check(!app.currentAccount().ok(), "reopened store should start as guest");
    accept(app, app.login("learner", learnerPassword));
    check(value(app, app.history()).size() == 2 && value(app, app.favorites()).size() == 1 &&
          value(app, app.attempts()).size() == 2 && !app.quiz().ok(), "unexpected learner state after reload");
    accept(app, app.login("partner", learnerPassword));
    check(value(app, app.history()).empty() && value(app, app.favorites()).empty() &&
          value(app, app.attempts()).empty(), "other learner saw personal data");
    check(!app.searchAgain(history.front().getId()).ok(), "other learner replayed personal history");
    accept(app, app.close());

    Application imported;
    accept(imported, imported.initialize((root / "imported").u8string(), "admin", adminPassword));
    accept(imported, imported.login("admin", adminPassword));
    accept(imported, imported.importPackage((root / "package").u8string()));
    check(value(imported, imported.statistics()).vocabulary == 11, "resource package incomplete");
    accept(imported, imported.close());
}

int run(int argc, const fs::path& option, const fs::path& folder) {
    try {
        if (argc == 3 && option == "--fixture" && !folder.empty()) {
            const auto fixture = fs::absolute(folder);
            Application app;
            bootstrap(app, fixture);
            accept(app, app.close());
            std::cout << "Fixture ready: " << fixture.u8string() << '\n';
            return 0;
        }
        if (argc != 1) {
            std::cerr << "Usage: backend_example [--fixture EMPTY_PATH]\n";
            return 2;
        }
        const auto root = fs::temp_directory_path() /
                          ("pbl2_example_" + std::to_string(std::random_device{}()));
        if (!fs::create_directory(root)) throw std::runtime_error("cannot create example directory");
        try {
            walkthrough(root);
        } catch (...) {
            std::cerr << "Fixture retained at " << root.u8string() << '\n';
            throw;
        }
        fs::remove_all(root);
        std::cout << "Fixture workflow passed: search, history, review, quiz, reload, isolation, import.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    return run(argc, argc > 1 ? fs::path(argv[1]) : fs::path{}, argc > 2 ? fs::path(argv[2]) : fs::path{});
}

#include "Application.h"
#include "algorithms/KmpMatcher.h"
#include "algorithms/MergeSort.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <iostream>
#include <cstdlib>

#undef assert
#define assert(condition)                                                                          \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::cerr << "failed at line " << __LINE__ << ": " << #condition << '\n';              \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)

int main() {
    assert(ResourceId("R100000").number() == 100000);
    assert(ResourceId::isValid("R18446744073709551615"));
    assert(!ResourceId::isValid("R18446744073709551616"));
    assert(!ResourceId::isValid("R0001"));
    assert(KmpMatcher("ababac").findFirst("ababababac") == 4);
    assert(KmpMatcher("abc").findFirst("xyz") == std::string::npos);
    std::vector<int> sorted{4, 2, 3, 1};
    mergeSort(sorted, [](int a, int b) {
        return a < b;
    });
    assert((sorted == std::vector<int>{1, 2, 3, 4}));
    std::vector<std::pair<int, int>> stable{{2, 1}, {1, 2}, {2, 3}};
    mergeSort(stable, [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    assert(stable[1].second == 1 && stable[2].second == 3);

    DoublyLinkedList<int> list;
    list.pushBack(1);
    list.pushBack(2);
    list.pushFront(0);
    DoublyLinkedList<int> copy(list);
    assert(copy.front() == 0 && copy.back() == 2);
    list.eraseIf([](int value) {
        return value == 1;
    });
    assert(list.size() == 2 && copy.size() == 3);
    assert(list.popBack() && list.back() == 0);
    DoublyLinkedList<int> assigned;
    assigned = copy;
    copy.clear();
    assert(assigned.size() == 3 && assigned.back() == 2);

    Application app;
    std::string error;
    assert(app.users().create(
        User(UserId("U001"), "admin", "demo123", UserRole::ADMIN, UserStatus::ACTIVE), error));
    assert(app.users().login("ADMIN", "demo123"));
    assert(app.service().dirty());
    assert(app.levels().add(Level(LevelId("L005"), "JLPT", "N5", 5, "beginner"), error));
    assert(app.resources().addVocabulary(Vocabulary(ResourceId("R001"),
                                                    LevelId("L005"),
                                                    "学校",
                                                    "がっこう",
                                                    "school",
                                                    PartOfSpeech::NOUN,
                                                    "学校へ行く"),
                                         error));
    assert(app.resources().addGrammar(Grammar(ResourceId("R002"),
                                              LevelId("L005"),
                                              "～たい",
                                              "want to",
                                              "Vます + たい",
                                              "desire",
                                              "行きたい",
                                              ""),
                                      error));
    assert(app.resources().addKanji(
        Kanji(ResourceId("R003"), LevelId("L005"), "学", "study", 8, "ガク", "まなぶ", "学校"),
        error));
    assert(app.resources().nextResourceId() == ResourceId("R004"));
    SearchOptions search;
    search.query = "school";
    assert(app.service().search(search, error).size() == 1);
    assert(app.histories().listNewest().size() == 1);
    search.query.clear();
    assert(app.service().search(search, error).size() == 3);
    assert(app.histories().listNewest().size() == 1);
    assert(app.favorites().add(ResourceId("R001"), error));
    assert(!app.favorites().add(ResourceId("R001"), error));
    assert(app.service().startReview(error));
    auto* review = app.service().review();
    assert(review && review->current() && !review->rate(true));
    assert(review->reveal() && review->rate(false) && review->remaining() == 1);
    assert(review->reveal() && review->rate(true) && review->finished());
    assert(app.questions().add(Question(QuestionId("Q001"),
                                        LevelId("L005"),
                                        ResourceId("R001"),
                                        "meaning?",
                                        {"school", "house", "book", "train"},
                                        'A',
                                        "school",
                                        QuestionStatus::ACTIVE),
                               error));
    assert(!app.service().removeResource(ResourceId("R001"), error));
    std::mt19937 engine(42);
    assert(app.quizzes().start(LevelId("L005"), 1, engine, error));
    assert(app.quizzes().selectAnswer(0, 'B'));
    std::optional<QuizAttempt> result;
    assert(app.quizzes().submit(result, error) && result->getCorrectAnswers() == 0);
    assert(!app.quizzes().submit(result, error));
    assert(app.quizzes().retryWrong(error));
    assert(app.quizzes().selectAnswer(0, 'A'));
    assert(app.quizzes().submit(result, error) && result->getCorrectAnswers() == 1);
    assert(app.users().create(
        User(UserId("U002"), "learner", "demo456", UserRole::LEARNER, UserStatus::ACTIVE), error));
    assert(app.quizzes().start(LevelId("L005"), 1, engine, error));
    assert(app.users().login("learner", "demo456"));
    assert(app.quizzes().session() == nullptr);
    assert(app.service().review() == nullptr);
    assert(!app.resources().addVocabulary(
        Vocabulary(
            ResourceId("R004"), LevelId("L005"), "本", "ほん", "book", PartOfSpeech::NOUN, ""),
        error));
    assert(app.users().login("admin", "demo123"));
    assert(app.questions().remove(QuestionId("Q001"), error));
    assert(app.service().removeResource(ResourceId("R001"), error));
    assert(app.favorites().list().empty());

    const auto folder = std::filesystem::temp_directory_path() /
                        ("pbl2_backend_test_" + std::to_string(std::random_device{}()));
    assert(std::filesystem::create_directory(folder));
    FileStorage storage;
    DataStore package;
    package.levels.emplace_back(LevelId("L005"), "JLPT", "N5", 5, "beginner");
    package.vocabularies.emplace_back(
        ResourceId("R100000"), LevelId("L005"), "本", "ほん", "book", PartOfSpeech::NOUN, "");
    package.rebuildIndexes();
    const auto packageFolder = folder / "package";
    assert(storage.savePackage(packageFolder.string(), package, error));
    assert(app.service().importPackage(packageFolder.string(), error));
    assert(app.resources().nextResourceId() == ResourceId("R100001"));
    assert(!app.service().importPackage(packageFolder.string(), error));
    assert(app.resources().nextResourceId() == ResourceId("R100001"));
    assert(app.service().dirty());
    assert(app.service().save(folder.string(), error));
    assert(!app.service().dirty());
    DataStore loaded;
    assert(storage.loadAll(folder.string(), loaded, error));
    assert(loaded.findResource(ResourceId("R100000")) && loaded.attempts.size() == 2);
    assert(loaded.findResource(ResourceId("R100000"))->getTerm() == "本");
    assert(loaded.histories.size() == 1);
    {
        std::ifstream input(folder / "grammar.txt", std::ios::binary);
        const std::string original((std::istreambuf_iterator<char>(input)),
                                   std::istreambuf_iterator<char>());
        std::ofstream output(folder / "grammar.txt", std::ios::binary | std::ios::trunc);
        output << "\xEF\xBB\xBF";
        for (char c : original) {
            if (c == '\n') {
                output << '\r';
            }
            output << c;
        }
    }
    assert(storage.loadAll(folder.string(), loaded, error));
    {
        std::ofstream output(folder / "user.txt", std::ios::binary | std::ios::app);
        output << "U003|ADMIN|demo789|LEARNER|ACTIVE\n";
    }
    assert(!storage.loadAll(folder.string(), loaded, error));
    assert(loaded.users.size() == 2);
    assert(storage.saveAll(folder.string(), app.data(), error));
    std::ofstream(folder / "grammar.txt", std::ios::binary | std::ios::trunc) << "broken\n";
    assert(!storage.loadAll(folder.string(), loaded, error));
    assert(loaded.findResource(ResourceId("R002")));
    std::filesystem::remove_all(folder);
}

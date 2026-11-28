#include "pbl2/Application.h"
#include "Fixture.h"
#include "ReplacedFiles.h"
#include "application/managers/ResourceManager.h"
#include "application/managers/LevelManager.h"
#include "Utf8.h"
#include "domain/Validation.h"
#include <algorithm>
#include <array>
#include <limits>

void preparedQueryTests() {
    std::string input(maxTxtFieldBytes - 3, 'a');
    CHECK(!Utf8::appendWithinLimit(input, "学校", maxTxtFieldBytes));
    CHECK(input.size() == maxTxtFieldBytes - 3);
    CHECK(Utf8::appendWithinLimit(input, "学", maxTxtFieldBytes));
    CHECK(input.size() == maxTxtFieldBytes && validUtf8(input));
    Utf8::eraseLast(input);
    CHECK(input == std::string(maxTxtFieldBytes - 3, 'a'));
    CHECK(Utf8::appendWithinLimit(input, "𠮷", maxTxtFieldBytes + 1));
    Utf8::eraseLast(input);
    CHECK(input.size() == maxTxtFieldBytes - 3 && validUtf8(input));

    TempDirectory temp;
    DataStore data;
    const LevelId n5("L001"), n4("L002");
    data.users.emplace_back(UserId("U001"), "fixture-admin", "fixture-only", UserRole::ADMIN, UserStatus::ACTIVE);
    data.levels.emplace_back(n5, "JLPT", "N5", 5, "");
    data.levels.emplace_back(n4, "JLPT", "N4", 4, "");
    data.vocabularies.emplace_back(ResourceId("R001"), n5, "学校", "がっこう", "trường học", PartOfSpeech::NOUN, "");
    data.kanjis.emplace_back(ResourceId("R002"), n5, "学", "study", 8, "ガク", "まなぶ", "");
    data.grammars.emplace_back(ResourceId("R003"), n5, "Pattern", "grammar", "学校 structure", "Use Here", "ignored example", "ignored note");
    data.vocabularies.emplace_back(ResourceId("R004"), n4, "学校生活", "", "school life", PartOfSpeech::NOUN, "");
    data.vocabularies.emplace_back(ResourceId("R005"), n5, "abABabac", "", "A B", PartOfSpeech::NOUN, "");
    data.vocabularies.emplace_back(ResourceId("R006"), n5, "Reference", "", "学校 reference", PartOfSpeech::NOUN, "");
    const std::string decomposed = "ĐU\u031BO\u031B\u0300NG";
    data.vocabularies.emplace_back(ResourceId("R007"), n4, decomposed, "", "nghĩa", PartOfSpeech::NOUN, "");
    data.rebuildIndexes();
    saveFixture(temp.path, data);
    Application app;
    CHECK(app.load(temp.path.string()).ok() && app.login("fixture-admin", "fixture-only").ok());

    using Scores = std::vector<std::pair<std::string, int>>;
    auto scores = [&](const std::string& query, bool id = false,
                      std::optional<ResourceType> type = {}, std::optional<LevelId> level = {}) {
        const auto found = app.search({query, id, type, level});
        CHECK(found.ok());
        Scores result;
        for (const auto& row : *found.value) result.emplace_back(row.id.getId(), row.score);
        return result;
    };
    const Scores school{{"R001", 4}, {"R004", 3}, {"R002", 3}, {"R003", 2}, {"R006", 1}};
    for (const auto* query : {"学校", " 学校 ", "\t学校\t", "\u00A0学校\u00A0", "\u3000学校\u3000", "\r学校\n"})
        CHECK(scores(query) == school);
    CHECK(scores(" R001 \r\n", true) == Scores{{"R001", 4}});
    CHECK(app.search({"R0001", true, {}, {}}).status.error == BackendError::INVALID_INPUT);
    CHECK(app.search({"r001", true, {}, {}}).status.error == BackendError::INVALID_INPUT);
    const Scores browsed{{"R001", 0}, {"R005", 0}, {"R006", 0}};
    CHECK(scores("", true, ResourceType::VOCABULARY, n5) == browsed);
    CHECK(scores(" \t\u00A0\u3000\r\n", true, ResourceType::VOCABULARY, n5) == browsed);
    CHECK(app.history().value->empty());

    CHECK(scores("学") == Scores{{"R002", 4}, {"R004", 3}, {"R001", 3}, {"R003", 2}, {"R006", 1}});
    CHECK(scores("学校学") == Scores{{"R002", 3}});
    CHECK(scores("がっこう") == Scores{{"R001", 2}});
    for (const auto* query : {"trường học", "TRƯỜNG HỌC", "tru\u031Bo\u031B\u0300ng ho\u0323c", "trường học\r\n"})
        CHECK(scores(query) == Scores{{"R001", 1}});
    CHECK(scores("đường") == Scores{{"R007", 4}} && scores("dường").empty());
    const auto original = app.search({"đường", false, {}, {}});
    CHECK(original.value->front().term == decomposed && original.value->front().meaning == "nghĩa");
    CHECK(scores("truong hoc").empty());
    CHECK(scores("pATTERN") == Scores{{"R003", 4}});
    CHECK(scores("use here") == Scores{{"R003", 2}});
    CHECK(scores("abABAC") == Scores{{"R005", 2}});
    CHECK(scores("a b") == Scores{{"R005", 1}} && scores("a  b").empty());
    for (const auto* query : {"abABabac-longer", "not-found", "ignored note", "ignored example"})
        CHECK(scores(query).empty());
    CHECK(scores(std::string(8193, ' '), true, ResourceType::VOCABULARY, n5) == browsed);
    CHECK(scores(std::string(8192, 'x') + "\r\n").empty());
    for (const auto& invalid : {std::string(8193, 'x'), std::string("\xC0\xAF"), std::string(" | "),
                                std::string("学\r校"), std::string("学\n校"), std::string(" \0 ", 3)})
        CHECK(app.search({invalid, false, {}, {}}).status.error == BackendError::INVALID_INPUT);
    CHECK(app.history().value->empty());
    const auto submitted = app.submitSearch({"\u00a0PaTTeRn\t", false, {}, {}});
    CHECK(submitted.ok() && submitted.value->size() == 1 && submitted.value->front().id == ResourceId("R003"));
    CHECK(app.history().value->front().getQuery() == "PaTTeRn");
}

void historyAndTableSaves() {
    TempDirectory temp;
    const auto folder = temp.path.string();
    const LevelId n5("L001"), n4("L002");
    auto data = fixture();
    data.vocabularies.emplace_back(ResourceId("R050"), n5, "R999", "", "fixture", PartOfSpeech::NOUN, "");
    data.histories.pushBack(SearchHistory(HistoryId("H002"), UserId("U002"), "R001", "2026-11-12T01:01:00Z"));
    data.rebuildIndexes();
    saveFixture(temp.path, data);
    Application app;
    CHECK(app.load(folder).ok() && app.login("learner1", "demo123").ok());
    auto replaces = [&](std::set<std::string> names, auto action) {
        const ReplacedFiles before(temp.path);
        action();
        CHECK(before.names() == names);
    };
    const auto initial = app.history().value->size();
    SearchOptions options{" \u3000R001\t", true, ResourceType::GRAMMAR, n5};
    replaces({}, [&] {
        CHECK(app.search(options).value->empty());
        CHECK(app.search({"\u00A0\u3000", true, ResourceType::GRAMMAR, n5}).value->size() == 1);
        CHECK(app.submitSearch({"\u00A0\u3000", false, {}, {}}).status.save == SaveState::UNCHANGED);
        CHECK(!app.submitSearch({"R0001", true, {}, {}}).ok());
        CHECK(app.history().value->size() == initial);
    });
    replaces({"search_history.txt"}, [&] {
        const auto miss = app.submitSearch(options);
        CHECK(miss.ok() && miss.value->empty() && miss.status.save == SaveState::SAVED);
    });
    const auto recorded = app.history().value->front();
    CHECK(recorded.getQuery() == "R001" && app.history().value->size() == initial + 1);
    CHECK(app.login("learner2", "demo123").ok());
    CHECK(app.searchAgain(recorded.getId()).status.error == BackendError::NOT_FOUND);
    CHECK(app.history().value->empty());
    CHECK(app.login("learner1", "demo123").ok());
    replaces({"search_history.txt"}, [&] {
        const auto result = app.searchAgain(recorded.getId());
        CHECK(result.ok() && result.value->options.exactId && result.value->options.query == "R001");
        CHECK(result.value->options.type == ResourceType::GRAMMAR && result.value->options.levelId == n5);
        CHECK(result.value->rows.empty());
    });
    CHECK(app.history().value->size() == initial + 2);
    const auto legacy = app.searchAgain(HistoryId("H002"));
    CHECK(legacy.ok() && legacy.value->options.exactId && !legacy.value->options.type && !legacy.value->options.levelId);
    CHECK(legacy.value->options.sort == SearchSort::RELEVANCE && legacy.value->rows.front().id == ResourceId("R001"));
    CHECK(app.submitSearch({"R001", false, {}, {}}).value->empty());
    const auto contentId = app.searchAgain(app.history().value->front().getId());
    CHECK(contentId.ok() && !contentId.value->options.exactId && contentId.value->rows.empty());
    CHECK(app.submitSearch({" R999 ", false, {}, {}}).ok());
    const auto fallback = app.searchAgain(app.history().value->front().getId());
    CHECK(fallback.ok() && !fallback.value->options.exactId && fallback.value->rows.front().id == ResourceId("R050"));

    const auto blocker = temp.path / ".pbl2-stage";
    writeFile(blocker, "occupied");
    const auto beforePending = app.history().value->size();
    replaces({}, [&] {
        const auto pending = app.submitSearch({"  学校  ", false, {}, {}});
        CHECK(pending.ok() && !pending.value->empty() && pending.status.save == SaveState::PENDING);
        CHECK(!pending.status.message.empty() && app.dirty());
        CHECK(app.search({"学校", false, {}, {}}).status.save == SaveState::UNCHANGED);
        CHECK(app.retrySave().error == BackendError::IO_ERROR);
    });
    CHECK(std::filesystem::remove(blocker));
    replaces({"search_history.txt"}, [&] { CHECK(app.retrySave().save == SaveState::SAVED); });
    CHECK(app.history().value->size() == beforePending + 1 && app.history().value->front().getQuery() == "学校");

    CHECK(app.login("admin", "demo123").ok());
    auto input = VocabularyInput{n5, "Created", "", "m", PartOfSpeech::NOUN, ""};
    std::optional<ResourceId> created;
    replaces({"learning_resource.txt", "vocabulary.txt"}, [&] {
        const auto result = app.createVocabulary(input);
        CHECK(result.ok() && result.status.save == SaveState::SAVED);
        created = result.value;
    });
    input.word = "Updated"; input.meaning = "ĐIỆN";
    replaces({"vocabulary.txt"}, [&] { CHECK(app.updateVocabulary(*created, input).save == SaveState::SAVED); });
    CHECK(app.search({"điện", false, {}, {}}).value->front().id == *created);
    CHECK(app.search({"Created", false, {}, {}}).value->empty());
    input.levelId = n4;
    replaces({"learning_resource.txt", "vocabulary.txt"}, [&] {
        CHECK(app.updateVocabulary(*created, input).save == SaveState::SAVED);
    });
    replaces({"favorites.txt"}, [&] { CHECK(app.addFavorite(*created).save == SaveState::SAVED); });
    replaces({"favorites.txt", "learning_resource.txt", "vocabulary.txt"}, [&] {
        CHECK(app.removeResource(*created).save == SaveState::SAVED && app.favorites().value->empty());
    });
    replaces({"alphabet.txt"}, [&] {
        CHECK(app.createAlphabet({Script::HIRAGANA, "い", "i", "vowel", 2, ""}).status.save == SaveState::SAVED);
    });
    replaces({"question.txt"}, [&] { CHECK(app.setQuestionStatus(QuestionId("Q001"), QuestionStatus::INACTIVE).save == SaveState::SAVED); });
    CHECK(app.startQuiz(n5, 1).ok());
    replaces({"quiz_attempt.txt"}, [&] { CHECK(app.submitQuiz().status.save == SaveState::SAVED); });
    replaces({"resource_levels.txt"}, [&] { CHECK(app.createLevel({"JLPT", "N3", 3, ""}).status.save == SaveState::SAVED); });

    const auto packagePath = temp.path / "package";
    DataStore package;
    package.levels.emplace_back(n4, "JLPT", "N4", 4, "");
    package.grammars.emplace_back(ResourceId("R100"), n4, "New pattern", "m", "", "ĐIỆN", "", "");
    std::string error;
    CHECK(FileStorage{}.savePackage(packagePath.string(), package, error));
    replaces({"grammar.txt", "learning_resource.txt"}, [&] { CHECK(app.importPackage(packagePath.string()).save == SaveState::SAVED); });
    CHECK(app.search({"điện", false, {}, {}}).value->front().id == ResourceId("R100"));

    // A directory in place of the detail file prevents preparation of the whole save.
    const auto detail = temp.path / "vocabulary.txt";
    const auto held = temp.path / "fixture-held";
    const ReplacedFiles beforeFailure(temp.path);
    std::filesystem::rename(detail, held);
    CHECK(std::filesystem::create_directory(detail));
    const auto pending = app.createVocabulary(input);
    CHECK(pending.ok() && pending.status.save == SaveState::PENDING && app.dirty());
    CHECK(std::filesystem::remove(detail));
    std::filesystem::rename(held, detail);
    CHECK(beforeFailure.names().empty());
    replaces({"learning_resource.txt", "vocabulary.txt"}, [&] { CHECK(app.retrySave().save == SaveState::SAVED); });
    CHECK(app.load(folder).ok() && app.login("admin", "demo123").ok());
    CHECK(app.resource(*pending.value).ok() && app.resource(ResourceId("R100")).ok());
    const auto normalizedAfterReload = app.search({"điện", false, {}, {}});
    CHECK(normalizedAfterReload.value->size() == 2 && normalizedAfterReload.value->front().id == ResourceId("R100"));
}

void searchTests() {
    TempDirectory temp;
    auto data = fixture();
    data.vocabularies.clear(); data.grammars.clear(); data.kanjis.clear();
    data.favorites.clear(); data.histories.clear(); data.questions.clear(); data.attempts.clear();
    data.vocabularies.emplace_back(ResourceId("R999"),LevelId("L001"),"cat","cat","cat",PartOfSpeech::NOUN,"ignoredexample");
    data.vocabularies.emplace_back(ResourceId("R1000"),LevelId("L002"),"cat","","m",PartOfSpeech::NOUN,"");
    data.grammars.emplace_back(ResourceId("R100000"),LevelId("L001"),"catapult","m","","","","ignorednote");
    data.kanjis.emplace_back(ResourceId("R1000000"),LevelId("L001"),"字","m",1,"cat","","");
    data.vocabularies.emplace_back(ResourceId("R008"),LevelId("L001"),"word","","cat",PartOfSpeech::NOUN,"");
    data.grammars.emplace_back(ResourceId("R009"),LevelId("L001"),"pattern","m","cat","","","");
    data.rebuildIndexes();
    saveFixture(temp.path,data);
    Application app;
    CHECK(app.load(temp.path.string()).ok() && app.login("admin","demo123").ok());
    SearchOptions options; options.query = "cat";
    auto rows = app.submitSearch(options);
    CHECK(rows.ok() && rows.value->size() == 6);
    CHECK((*rows.value)[0].id == ResourceId("R1000") && (*rows.value)[0].score == 4);
    CHECK((*rows.value)[1].id == ResourceId("R999") && (*rows.value)[1].score == 4);
    CHECK((*rows.value)[2].score == 3 && (*rows.value)[2].id == ResourceId("R100000"));
    CHECK((*rows.value)[3].score == 2 && (*rows.value)[4].score == 2 && (*rows.value)[5].score == 1);
    CHECK(app.history().value->size() == 1);
    options.type = ResourceType::VOCABULARY; options.levelId = LevelId("L001");
    CHECK(app.search(options).value->size() == 2);
    options.sort = SearchSort::ID;
    rows = app.search(options);
    CHECK(rows.value->front().id == ResourceId("R008") && rows.value->back().id == ResourceId("R999"));
    options.type.reset(); options.levelId.reset();
    const std::array<std::pair<SearchSort, std::vector<std::string>>, 6> orders{{
        {SearchSort::RELEVANCE, {"R1000", "R999", "R100000", "R009", "R1000000", "R008"}},
        {SearchSort::ID, {"R008", "R009", "R999", "R1000", "R100000", "R1000000"}},
        {SearchSort::LEVEL_ASC, {"R1000", "R008", "R009", "R999", "R100000", "R1000000"}},
        {SearchSort::LEVEL_DESC, {"R008", "R009", "R999", "R100000", "R1000000", "R1000"}},
        {SearchSort::TERM_ASC, {"R999", "R1000", "R100000", "R009", "R008", "R1000000"}},
        {SearchSort::TERM_DESC, {"R1000000", "R008", "R009", "R100000", "R999", "R1000"}},
    }};
    const auto ids = [](const std::vector<SearchRow>& matches) {
        std::vector<std::string> result;
        for (const auto& row : matches) result.push_back(row.id.getId());
        return result;
    };
    std::vector<HistoryId> entries;
    for (const auto& [sort, expected] : orders) {
        options.sort = sort;
        const auto result = app.submitSearch(options);
        CHECK(result.ok() && ids(*result.value) == expected);
        entries.push_back(app.history().value->front().getId());
    }
    CHECK(app.close().ok());
    CHECK(app.load(temp.path.u8string()).ok() && app.login("admin", "demo123").ok());
    for (std::size_t i = 0; i < orders.size(); ++i) {
        const auto replayed = app.searchAgain(entries[i]);
        CHECK(replayed.ok() && replayed.value->options.sort == orders[i].first);
        CHECK(ids(replayed.value->rows) == orders[i].second);
    }
    options.query = "ignoredexample"; CHECK(app.search(options).value->empty());
    options.query = "ignorednote"; CHECK(app.search(options).value->empty());
    options.query = "R1000000"; options.exactId = true;
    CHECK(app.search(options).value->front().id == ResourceId("R1000000"));
    options.type = ResourceType::GRAMMAR;
    CHECK(app.search(options).value->empty());
    options.type.reset(); options.query = "R0001";
    const auto before = app.history().value->size();
    CHECK(app.search(options).status.error == BackendError::INVALID_INPUT);
    CHECK(app.history().value->size() == before);
    options.query.clear();
    CHECK(app.search(options).value->size() == 6 && app.history().value->size() == before);
    options.query = "cat"; options.exactId = false; options.sort = static_cast<SearchSort>(99);
    CHECK(!app.search(options).ok() && app.history().value->size() == before);

    // Kanji search can match each CJK codepoint in a multi-kanji query.
    data.kanjis.emplace_back(ResourceId("R1000001"),LevelId("L001"),"日","day",4,"ニチ","ひ","");
    data.kanjis.emplace_back(ResourceId("R1000002"),LevelId("L002"),"本","book",5,"ホン","もと","");
    data.rebuildIndexes();
    CHECK(app.close().ok());
    saveFixture(temp.path,data);
    CHECK(app.load(temp.path.string()).ok() && app.login("admin","demo123").ok());
    SearchOptions kanjiOptions; kanjiOptions.type = ResourceType::KANJI;
    kanjiOptions.query = "日";
    rows = app.search(kanjiOptions);
    CHECK(rows.ok() && rows.value->size() == 1 && rows.value->front().term == "日");
    kanjiOptions.query = "本";
    rows = app.search(kanjiOptions);
    CHECK(rows.ok() && rows.value->size() == 1 && rows.value->front().term == "本");
    kanjiOptions.query = "日本";
    rows = app.search(kanjiOptions);
    CHECK(rows.ok() && rows.value->size() == 2);
    CHECK(std::count_if(rows.value->begin(), rows.value->end(), [](const SearchRow& row) { return row.term == "日"; }) == 1);
    CHECK(std::count_if(rows.value->begin(), rows.value->end(), [](const SearchRow& row) { return row.term == "本"; }) == 1);
    kanjiOptions.query = "ホン";
    CHECK(app.search(kanjiOptions).value->front().term == "本");
    kanjiOptions.query = "もと";
    CHECK(app.search(kanjiOptions).value->front().term == "本");
    kanjiOptions.query = "book";
    CHECK(app.search(kanjiOptions).value->front().term == "本");
    kanjiOptions.query = "日本"; kanjiOptions.levelId = LevelId("L001");
    rows = app.search(kanjiOptions);
    CHECK(rows.ok() && rows.value->size() == 1 && rows.value->front().term == "日");
    kanjiOptions.levelId.reset(); kanjiOptions.query = "日日日本";
    rows = app.search(kanjiOptions);
    CHECK(rows.ok() && rows.value->size() == 2);
    kanjiOptions.query = "無関係";
    CHECK(app.search(kanjiOptions).value->empty());
    SearchOptions unchangedSearch; unchangedSearch.query = "cat";
    unchangedSearch.type = ResourceType::VOCABULARY;
    CHECK(app.search(unchangedSearch).value->size() == 3);

    // Exhausted history ID warns without losing search results or adding a duplicate.
    data.histories.pushBack(SearchHistory(HistoryId("H18446744073709551615"),UserId("U001"),"cat","2026-11-12T01:00:00Z"));
    CHECK(app.close().ok());
    saveFixture(temp.path,data);
    CHECK(app.load(temp.path.string()).ok() && app.login("admin","demo123").ok());
    options.sort = SearchSort::RELEVANCE;
    rows = app.submitSearch(options);
    CHECK(rows.ok() && rows.value->size() == 6 && !rows.status.message.empty());
    CHECK(app.history().value->size() == 1);
}

void indexAndImportTests() {
    auto data = fixture();
    UserManager users(data); ResourceManager resources(data,users); LevelManager levels(data,users);
    CHECK(users.login("admin","demo123"));
    std::string error;
    for (int i = 0; i < 100; ++i) {
        const auto id = resources.nextResourceId();
        CHECK(resources.addVocabulary(Vocabulary(id,LevelId("L001"),id.getId(),"","m",PartOfSpeech::NOUN,""),error));
    }
    for (const auto& item : data.vocabularies)
        CHECK(data.findResource(item.getId())->getTerm() == item.getTerm());
    data.vocabularies.erase(data.vocabularies.begin()+1);
    data.rebuildIndexes();
    for (const auto& item : data.vocabularies)
        CHECK(data.findResource(item.getId())->getTerm() == item.getTerm());
    // UserName index is rebuilt after a value replacement (no public rename use case).
    data.users[1] = User(UserId("U002"),"renamed","pass",UserRole::LEARNER,UserStatus::ACTIVE);
    data.rebuildIndexes();
    CHECK(!users.login("learner1","pass") && users.login("RENAMED","pass"));
    CHECK(!levels.remove(LevelId("L001"),error));

    TempDirectory temp;
    const auto live = temp.path / "live";
    const auto dataFixture = fixture();
    saveFixture(live, dataFixture);
    const auto secondStore = temp.path / "second";
    saveFixture(secondStore, dataFixture);
    Application seededA(42), seededB(42);
    CHECK(seededA.load(live.string()).ok() && seededB.load(secondStore.string()).ok());
    CHECK(seededA.login("admin","demo123").ok() && seededB.login("admin","demo123").ok());
    CHECK(seededA.startQuiz(LevelId("L001"),3).ok() && seededB.startQuiz(LevelId("L001"),3).ok());
    const auto first = *seededA.quiz().value;
    const auto second = *seededB.quiz().value;
    for (std::size_t i = 0; i < first.questions.size(); ++i)
        CHECK(first.questions[i].prompt == second.questions[i].prompt);
    CHECK(first.questions[0].prompt != first.questions[1].prompt &&
          first.questions[1].prompt != first.questions[2].prompt &&
          first.questions[0].prompt != first.questions[2].prompt);
    CHECK(seededA.close().ok() && seededB.close().ok());
    Application app;
    CHECK(app.load(live.string()).ok() && app.login("admin","demo123").ok());
    DataStore package;
    package.levels.emplace_back(LevelId("L001"),"JLPT","N5",5,"");
    package.vocabularies.emplace_back(ResourceId("R100000"),LevelId("L001"),"new","","meaning",PartOfSpeech::NOUN,"");
    package.kanjis.emplace_back(ResourceId("R003"),LevelId("L001"),"字","m",6,"on","","");
    package.rebuildIndexes();
    CHECK(FileStorage{}.savePackage((temp.path/"package").string(),package,error));
    CHECK(!app.importPackage((temp.path/"package").string()).ok());
    CHECK(!app.resource(ResourceId("R100000")).ok() && app.statistics().value->vocabulary == 1);
    package.kanjis.clear(); package.rebuildIndexes();
    CHECK(FileStorage{}.savePackage((temp.path/"package").string(),package,error));
    CHECK(app.importPackage((temp.path/"package").string()).ok());
    CHECK(app.resource(ResourceId("R100000")).ok() && app.levels().value->size() == 2);
    CHECK(!app.importPackage((temp.path/"package").string()).ok());
    package.vocabularies.clear();
    package.levels[0] = Level(LevelId("L001"),"JLPT","changed name",5,"");
    package.rebuildIndexes();
    CHECK(FileStorage{}.savePackage((temp.path/"package").string(),package,error));
    CHECK(!app.importPackage((temp.path/"package").string()).ok());
    CHECK(app.levels().value->size() == 2);

    // These are distinct category/name pairs even though ':' occurs in the values.
    CHECK(app.createLevel({"A:B","C",1,""}).ok());
    CHECK(app.createLevel({"A","B:C",1,""}).ok());
    CHECK(app.load(live.string()).ok());
}
int main() {
    try { preparedQueryTests(); historyAndTableSaves(); searchTests(); indexAndImportTests(); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

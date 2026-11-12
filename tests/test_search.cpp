#include "Application.h"
#include "Fixture.h"
#include "managers/ResourceManager.h"
#include "managers/LevelManager.h"
#include <limits>

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
    auto rows = app.search(options);
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
    options.sort = SearchSort::LEVEL_DESC;
    rows = app.search(options); CHECK(rows.value->front().id == ResourceId("R008"));
    CHECK(rows.value->back().id == ResourceId("R1000"));
    options.sort = SearchSort::LEVEL_ASC;
    CHECK(app.search(options).value->front().id == ResourceId("R1000"));
    options.sort = SearchSort::TERM_ASC;
    CHECK(app.search(options).value->front().id == ResourceId("R999")); // equal terms use numeric ID
    options.sort = SearchSort::TERM_DESC;
    CHECK(app.search(options).value->front().id == ResourceId("R1000000"));
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

    // Exhausted history ID warns without losing search results or adding a duplicate.
    data.histories.pushBack(SearchHistory(HistoryId("H18446744073709551615"),UserId("U001"),"cat","2026-11-12T01:00:00Z"));
    saveFixture(temp.path,data);
    CHECK(app.load(temp.path.string()).ok() && app.login("admin","demo123").ok());
    options.sort = SearchSort::RELEVANCE;
    rows = app.search(options);
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
    saveFixture(live,fixture());
    Application seededA(42), seededB(42);
    CHECK(seededA.load(live.string()).ok() && seededB.load(live.string()).ok());
    CHECK(seededA.login("admin","demo123").ok() && seededB.login("admin","demo123").ok());
    CHECK(seededA.startQuiz(LevelId("L001"),3).ok() && seededB.startQuiz(LevelId("L001"),3).ok());
    const auto first = *seededA.quiz().value;
    const auto second = *seededB.quiz().value;
    for (std::size_t i = 0; i < first.questions.size(); ++i)
        CHECK(first.questions[i].id == second.questions[i].id);
    CHECK(first.questions[0].id != first.questions[1].id && first.questions[1].id != first.questions[2].id &&
          first.questions[0].id != first.questions[2].id);
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
    try { searchTests(); indexAndImportTests(); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

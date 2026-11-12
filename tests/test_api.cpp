#include "Application.h"
#include "TestSupport.h"
#include <set>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<Application>);
static_assert(!std::is_move_constructible_v<Application>);

QuestionInput questionInput(const LevelId& level, std::optional<ResourceId> resource = {}) {
    return {level, resource, "meaning?", {"one", "two", "three", "four"}, 'A', "because one", QuestionStatus::ACTIVE};
}
void requireSaved(const Status& status) {
    CHECK(status.ok() && status.save == SaveState::SAVED);
}

void apiFlow() {
    TempDirectory temp;
    const auto folder = temp.path / "data";
    Application app(42);
    CHECK(!app.loaded() && !app.dirty());
    CHECK(app.login("admin", "demo123").error == BackendError::NOT_LOADED);
    requireSaved(app.initialize(folder.string(), "admin", "demo123"));
    CHECK(app.currentAccount().status.error == BackendError::LOGIN_REQUIRED);
    CHECK(app.login("ADMIN", "demo123").ok());
    CHECK(app.initialize(folder.string(), "other", "pass").error == BackendError::INVALID_STATE);
    auto n5 = app.createLevel({"jlpt", "N5", 5, ""});
    auto n4 = app.createLevel({"JLPT", "N4", 4, ""});
    CHECK(n5.ok() && n4.ok() && n5.value->getId() == "L001");
    CHECK(app.levels().value->front().getId() == *n4.value);
    CHECK(!app.createLevel({"JLPT", "N5", 1, ""}).ok());
    CHECK(app.createLevel({"JLPT", "bad", 0, ""}).status.error == BackendError::INVALID_INPUT);
    VocabularyInput vocab{*n5.value, "学校", "がっこう", "trường;học", PartOfSpeech::NOUN, ""};
    auto word = app.createVocabulary(vocab);
    auto grammar = app.createGrammar({*n5.value, "～たい", "muốn", "Vます", "mong muốn", "", ""});
    auto kanji = app.createKanji({*n4.value, "学", "học", 8, "ガク", "まなぶ", "学生"});
    CHECK(word.ok() && grammar.ok() && kanji.ok());
    CHECK(word.value->getId() == "R001" && kanji.value->getId() == "R003");
    CHECK(std::get<Vocabulary>(*app.resource(*word.value).value).getMeaning() == "trường;học");
    CHECK(!app.updateGrammar(*word.value, {*n5.value, "x", "m", "", "", "", ""}).ok());
    CHECK(app.statistics().value->vocabulary == 1 && app.statistics(*n5.value).value->kanji == 0);
    auto one = app.createAccount("learner1", "demo123", UserRole::LEARNER);
    auto two = app.createAccount("learner2", "demo123", UserRole::LEARNER);
    auto admin2 = app.createAccount("admin2", "pass", UserRole::ADMIN);
    CHECK(one.ok() && two.ok() && admin2.ok());
    CHECK(!app.createAccount("LEARNER1", "pass", UserRole::LEARNER).ok());
    CHECK(app.createAccount("1bad", "pass", UserRole::LEARNER).status.error == BackendError::INVALID_INPUT);
    CHECK(app.createAccount("badrole", "pass", static_cast<UserRole>(99)).status.error == BackendError::INVALID_INPUT);
    CHECK(app.setAccountStatus(UserId("U001"), UserStatus::LOCKED).error == BackendError::REJECTED);
    requireSaved(app.setAccountStatus(*admin2.value, UserStatus::LOCKED));
    CHECK(app.setAccountStatus(*two.value, static_cast<UserStatus>(9)).error == BackendError::REJECTED);
    requireSaved(app.resetPassword(*two.value, "newpass"));
    CHECK(app.accounts().value->size() == 4);

    auto a = app.createAlphabet({Script::HIRAGANA, "あ", "a", "vowel", 2, ""});
    auto b = app.createAlphabet({Script::KATAKANA, "ア", "a", "vowel", 1, ""});
    CHECK(a.ok() && b.ok());
    CHECK(!app.createAlphabet({Script::HIRAGANA, "あ", "a", "vowel", 3, ""}).ok());
    CHECK(app.alphabet().value->front().getId() == *b.value);
    CHECK(app.alphabet(Script::HIRAGANA, "vowel").value->size() == 1);
    requireSaved(app.updateAlphabet(*a.value, {Script::HIRAGANA, "い", "i", "vowel", 1, ""}));
    CHECK(!app.updateAlphabet(*b.value, {Script::HIRAGANA, "い", "i", "vowel", 1, ""}).ok());
    requireSaved(app.removeAlphabet(*b.value));

    auto q1 = app.createQuestion(questionInput(*n5.value, *word.value));
    auto q2 = app.createQuestion(questionInput(*n5.value));
    auto inactiveInput = questionInput(*n5.value, *word.value);
    inactiveInput.status = QuestionStatus::INACTIVE;
    auto q3 = app.createQuestion(inactiveInput);
    CHECK(q1.ok() && q2.ok() && q3.ok());
    auto duplicate = questionInput(*n5.value);
    duplicate.options[1] = " one \t";
    CHECK(app.createQuestion(duplicate).status.error == BackendError::INVALID_INPUT);
    auto mismatch = questionInput(*n4.value, *word.value);
    CHECK(!app.createQuestion(mismatch).ok());
    CHECK(app.questions(*n5.value, QuestionStatus::ACTIVE).value->size() == 2);
    CHECK(app.question(*q1.value).value->getCorrectOption() == 'A');
    CHECK(!app.removeResource(*word.value).ok());
    auto moved = vocab; moved.levelId = *n4.value;
    CHECK(!app.updateVocabulary(*word.value, moved).ok());
    CHECK(!app.removeLevel(*n5.value).ok());
    CHECK(!app.updateLevel(*n5.value, {"OTHER", "N5", 5, ""}).ok());

    requireSaved(app.addFavorite(*word.value));
    requireSaved(app.addFavorite(*grammar.value));
    CHECK(!app.addFavorite(*word.value).ok());
    CHECK(!app.addFavorite(ResourceId("R999")).ok());
    CHECK(app.startReview().ok());
    CHECK(app.review().value->remaining == 2 && !app.review().value->current->back);
    CHECK(!app.rateReview(true).ok());
    vocab.meaning = "updated";
    requireSaved(app.updateVocabulary(*word.value, vocab));
    CHECK(app.revealReview().ok());
    CHECK(app.review().value->current->back->find("trường;học") != std::string::npos);
    CHECK(app.rateReview(false).ok() && app.review().value->remaining == 2);
    CHECK(!app.review().value->current->back);
    CHECK(app.revealReview().ok() && app.rateReview(true).ok());
    CHECK(app.revealReview().ok() && app.rateReview(false).ok());
    CHECK(app.review().value->remaining == 1);
    CHECK(app.revealReview().ok() && app.rateReview(true).ok());
    CHECK(app.review().value->finished && app.review().value->ratingCount == 4);
    CHECK(app.startReview().ok()); app.stopReview();
    CHECK(app.review().value->finished && app.review().value->remaining == 0);

    CHECK(!app.startQuiz(*n5.value, 0).ok() && !app.startQuiz(*n5.value, 51).ok());
    CHECK(!app.startQuiz(*n5.value, 3).ok());
    CHECK(app.startQuiz(*n5.value, 2).ok());
    auto quiz = app.quiz();
    CHECK(quiz.value->questions.size() == 2);
    CHECK(quiz.value->questions[0].id != quiz.value->questions[1].id);
    CHECK(!app.quizAnswers().ok());
    CHECK(!app.selectAnswer(2,'A').ok() && !app.selectAnswer(0,'X').ok());
    CHECK(app.selectAnswer(0,'B').ok() && app.selectAnswer(0,'-').ok());
    auto changed = questionInput(*n5.value, *word.value); changed.correctOption = 'D';
    requireSaved(app.updateQuestion(*q1.value, changed));
    auto result = app.submitQuiz();
    CHECK(result.ok() && result.value->getCorrectAnswers() == 0);
    CHECK(app.quizAnswers().value->size() == 2);
    CHECK(!app.selectAnswer(0,'A').ok() && !app.submitQuiz().ok());
    CHECK(app.attempts().value->size() == 1);
    CHECK(app.retryWrong().ok());
    CHECK(!app.quizAnswers().ok());
    CHECK(app.selectAnswer(0,'A').ok() && app.selectAnswer(1,'A').ok());
    auto perfect = app.submitQuiz();
    CHECK(perfect.ok() && perfect.value->percentage() == 100.0);
    CHECK(!app.retryWrong().ok());

    CHECK(app.startReview().ok() && app.startQuiz(*n5.value, 1).ok());
    CHECK(!app.load((temp.path / "missing").string()).ok());
    CHECK(app.review().ok() && app.quiz().ok() && app.currentAccount().ok());
    CHECK(app.login("learner1", "demo123").ok());
    CHECK(!app.review().ok() && !app.quiz().ok());
    // Call administration methods directly as a learner; UI visibility is irrelevant.
    const std::vector<Status> forbidden = {
        app.createLevel({"X","x",1,""}).status, app.updateLevel(*n5.value,{"JLPT","x",1,""}), app.removeLevel(*n5.value),
        app.createVocabulary(vocab).status, app.createGrammar({*n5.value,"p","m","","","",""}).status,
        app.createKanji({*n5.value,"字","m",1,"on","",""}).status,
        app.updateVocabulary(*word.value,vocab), app.updateGrammar(*grammar.value,{*n5.value,"p","m","","","",""}),
        app.updateKanji(*kanji.value,{*n4.value,"字","m",1,"on","",""}), app.removeResource(*word.value),
        app.accounts().status, app.createAccount("newuser","pass",UserRole::ADMIN).status,
        app.setAccountStatus(*two.value,UserStatus::LOCKED), app.resetPassword(*two.value,"pass"),
        app.questions().status, app.question(*q1.value).status, app.createQuestion(questionInput(*n5.value)).status,
        app.updateQuestion(*q1.value,questionInput(*n5.value)), app.removeQuestion(*q1.value),
        app.setQuestionStatus(*q1.value,QuestionStatus::INACTIVE),
        app.createAlphabet({Script::HIRAGANA,"え","e","vowel",1,""}).status,
        app.updateAlphabet(*a.value,{Script::HIRAGANA,"え","e","vowel",1,""}), app.removeAlphabet(*a.value),
        app.importPackage((temp.path / "package").string()), app.exportPackage((temp.path / "package").string())
    };
    for (const auto& status : forbidden) CHECK(status.error == BackendError::FORBIDDEN);
    CHECK(app.favorites().value->empty() && app.history().value->empty() && app.attempts().value->empty());
    CHECK(!app.removeFavorite(*word.value).ok());
    requireSaved(app.addFavorite(*word.value));
    SearchOptions schoolSearch;
    schoolSearch.query = "学校";
    CHECK(app.search(schoolSearch).value->size() == 1);
    auto personalHistory = app.history(); CHECK(personalHistory.value->size() == 1);
    const auto historyId = personalHistory.value->front().getId();
    CHECK(app.searchAgain(historyId).ok() && app.history().value->size() == 2);
    CHECK(app.startReview().ok() && app.startQuiz(*n5.value,1).ok());
    CHECK(app.login("learner2", "newpass").ok());
    CHECK(!app.review().ok() && !app.quiz().ok());
    CHECK(app.favorites().value->empty() && app.history().value->empty());
    CHECK(!app.searchAgain(historyId).ok());
    CHECK(app.login("learner1", "demo123").ok());
    requireSaved(app.clearHistory()); CHECK(app.history().value->empty());
    requireSaved(app.removeFavorite(*word.value));
    CHECK(!app.startReview().ok());
    app.logout(); CHECK(!app.selectAnswer(0,'A').ok());
    CHECK(!app.login("admin", "wrong").ok() && !app.currentAccount().ok());
    CHECK(app.login("admin", "demo123").ok());

    // Attempt creation succeeds in RAM even when the file write cannot start.
    CHECK(app.startQuiz(*n5.value, 1).ok());
    std::filesystem::create_directory(folder / "user.txt.tmp");
    const auto before = app.attempts().value->size();
    auto pending = app.submitQuiz();
    CHECK(pending.ok() && pending.status.save == SaveState::PENDING && app.dirty());
    CHECK(app.attempts().value->size() == before + 1);
    CHECK(!app.submitQuiz().ok());
    CHECK(app.retrySave().error == BackendError::IO_ERROR && app.dirty());
    CHECK(app.close().error == BackendError::UNSAVED_CHANGES);
    CHECK(app.load(folder.string()).error == BackendError::UNSAVED_CHANGES);
    CHECK(!app.load((temp.path / "missing").string(),UnsavedPolicy::DISCARD).ok() && app.dirty());
    auto search = app.search(schoolSearch);
    CHECK(search.ok() && search.value->size() == 1 && search.status.save == SaveState::PENDING);
    CHECK(app.history().value->size() == 1);
    std::filesystem::remove(folder / "user.txt.tmp");
    requireSaved(app.retrySave()); CHECK(!app.dirty());
    CHECK(app.attempts().value->size() == before + 1);
    CHECK(app.load(folder.string()).ok() && !app.currentAccount().ok());
    CHECK(app.login("admin", "demo123").ok());
    CHECK(!app.quizAnswers().ok() && app.attempts().value->size() == before + 1);

    // Explicit discard is the only load path allowed to drop unsaved changes.
    std::filesystem::create_directory(folder / "user.txt.tmp");
    const auto savedHistories = app.history().value->size();
    CHECK(app.search(schoolSearch).status.save == SaveState::PENDING);
    CHECK(app.history().value->size() == savedHistories + 1 && app.dirty());
    CHECK(app.load(folder.string(), UnsavedPolicy::DISCARD).ok());
    CHECK(!app.dirty() && !app.currentAccount().ok());
    std::filesystem::remove(folder / "user.txt.tmp");
    CHECK(app.login("admin", "demo123").ok());
    CHECK(app.history().value->size() == savedHistories);

    CHECK(!app.exportPackage((folder / ".").string()).ok());
    CHECK(!app.exportPackage((folder / ".." / "data").string()).ok());
#ifdef _WIN32
    auto caseAlias = folder.string();
    for (char& c : caseAlias) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    CHECK(!app.exportPackage(caseAlias).ok());
#endif
    const auto exported = app.exportPackage((temp.path / "package").string());
    if (!exported.ok()) throw std::runtime_error("export failed: " + exported.message);
    CHECK(!app.importPackage((temp.path / "package").string()).ok());
    // Deleting Question during a session is allowed, deleting its Level prevents submit.
    auto ephemeral = app.createLevel({"JLPT","temporary",9,""}); CHECK(ephemeral.ok());
    auto eq = app.createQuestion(questionInput(*ephemeral.value)); CHECK(eq.ok());
    CHECK(app.startQuiz(*ephemeral.value,1).ok());
    requireSaved(app.removeQuestion(*eq.value));
    requireSaved(app.updateLevel(*ephemeral.value, {"OTHER","temporary",9,""}));
    CHECK(!app.submitQuiz().ok());
    requireSaved(app.updateLevel(*ephemeral.value, {"JLPT","temporary",9,""}));
    requireSaved(app.removeLevel(*ephemeral.value));
    CHECK(!app.submitQuiz().ok() && app.attempts().value->size() == before + 1);
    // Inactive questions still block deletion.
    requireSaved(app.removeQuestion(*q1.value));
    CHECK(!app.removeResource(*word.value).ok());
    requireSaved(app.removeQuestion(*q3.value));
    requireSaved(app.removeResource(*word.value));
    CHECK(app.favorites().value->size() == 1);
    requireSaved(app.updateGrammar(*grammar.value,{*n5.value,"new pattern","new meaning","s","u","e","n"}));
    requireSaved(app.updateKanji(*kanji.value,{*n4.value,"字","character",6,"ジ","あざ",""}));
    requireSaved(app.removeResource(*grammar.value));
    requireSaved(app.removeResource(*kanji.value));
    CHECK(!app.removeLevel(*n5.value).ok()); // persisted attempts retain the level
    CHECK(app.close().ok() && !app.loaded());
    CHECK(!app.initialize(folder.string(),"admin","pass").ok());
}

void deletedQuestionSnapshot() {
    TempDirectory temp;
    Application app(123);
    CHECK(app.initialize(temp.path.string(),"admin","pass").ok());
    CHECK(app.login("admin","pass").ok());
    auto level = app.createLevel({"JLPT","N5",1,""}); CHECK(level.ok());
    auto q = app.createQuestion(questionInput(*level.value)); CHECK(q.ok());
    CHECK(app.startQuiz(*level.value,1).ok());
    requireSaved(app.removeQuestion(*q.value));
    CHECK(app.selectAnswer(0,'A').ok());
    CHECK(app.submitQuiz().value->getCorrectAnswers() == 1);
    CHECK(!app.removeLevel(*level.value).ok());
    CHECK(app.close().ok());
}
int main() {
    try { apiFlow(); deletedQuestionSnapshot(); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

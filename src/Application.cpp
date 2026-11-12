#include "Application.h"
#include "ApplicationService.h"
#include "storage/Paths.h"
#include "domain/Validation.h"
#include "managers/LevelManager.h"
#include "algorithms/MergeSort.h"
#include <filesystem>
#include <random>
#include <stdexcept>
#include <utility>

namespace {
Status failure(BackendError code, std::string message) {
    return {code, SaveState::UNCHANGED, std::move(message)};
}
AccountSummary summary(const User& u) {
    return {u.getId(), u.getUsername(), u.getRole(), u.getStatus()};
}
Vocabulary make(const ResourceId& id, const VocabularyInput& in) {
    return {id, in.levelId, in.word, in.reading, in.meaning, in.partOfSpeech, in.example};
}
Grammar make(const ResourceId& id, const GrammarInput& in) {
    return {id, in.levelId, in.pattern, in.meaning, in.structure, in.usage, in.example, in.note};
}
Kanji make(const ResourceId& id, const KanjiInput& in) {
    return {id, in.levelId, in.character, in.meaning, in.strokeCount, in.onyomi, in.kunyomi, in.examples};
}
Level make(const LevelId& id, const LevelInput& in) {
    return {id, in.category, in.name, in.order, in.description};
}
AlphabetEntry make(const AlphabetId& id, const AlphabetInput& in) {
    return {id, in.script, in.glyph, in.romaji, in.groupName, in.displayOrder, in.note};
}
Question make(const QuestionId& id, const QuestionInput& in) {
    return {id, in.levelId, in.resourceId, in.prompt, in.options, in.correctOption, in.explanation, in.status};
}
ResourceDetail detail(const DataStore& data, const ResourceId& id) {
    const auto location = data.resourceById.at(id.getId());
    switch (location.type) {
    case ResourceType::VOCABULARY: return data.vocabularies[location.index];
    case ResourceType::GRAMMAR: return data.grammars[location.index];
    case ResourceType::KANJI: return data.kanjis[location.index];
    }
    throw std::logic_error("invalid resource location");
}
}

struct Application::Impl {
    DataStore data;
    UserManager users{data};
    LevelManager levels{data, users};
    ResourceManager resources{data, users};
    FavoriteManager favorites{data, users};
    HistoryManager histories{data, users};
    AlphabetManager alphabet{data, users};
    QuestionManager questions{data, users};
    QuizManager quizzes{data, users};
    FileStorage storage;
    ApplicationService service{data, users, resources, favorites, histories, quizzes, storage};
    std::mt19937 engine;
    std::string folder;
    bool ready = false;

    explicit Impl(std::uint32_t seed) : engine(seed) {}
    Status guard(bool admin = false) const {
        if (!ready) return failure(BackendError::NOT_LOADED, "load or initialize first");
        if (!users.currentUser()) return failure(BackendError::LOGIN_REQUIRED, "login required");
        if (admin && !users.isAdmin()) return failure(BackendError::FORBIDDEN, "admin required");
        return {};
    }
    Status persist() {
        std::string error;
        if (service.save(folder, error)) return {BackendError::NONE, SaveState::SAVED, {}};
        return {BackendError::NONE, SaveState::PENDING, std::move(error)};
    }
    // The lambda applies one command. Save failure must never replay that command.
    template<class F> Status change(bool admin, F apply) {
        auto status = guard(admin);
        if (!status.ok()) return status;
        std::string error;
        try {
            if (!apply(error)) return failure(BackendError::REJECTED, std::move(error));
        } catch (const std::invalid_argument& e) {
            return failure(BackendError::INVALID_INPUT, e.what());
        } catch (const std::overflow_error& e) {
            return failure(BackendError::ID_EXHAUSTED, e.what());
        }
        return persist();
    }
};

Application::Application() : Application(std::random_device{}()) {}
Application::Application(std::uint32_t quizSeed) : impl_(std::make_unique<Impl>(quizSeed)) {}
Application::~Application() = default;
bool Application::loaded() const noexcept { return impl_->ready; }
bool Application::dirty() const noexcept { return impl_->data.dirty; }

Status Application::initialize(const std::string& folder, const std::string& name,
                               const std::string& password) {
    if (loaded()) return failure(BackendError::INVALID_STATE, "backend already loaded");
    try {
        if (folder.empty()) return failure(BackendError::INVALID_INPUT, "empty folder");
        if (std::filesystem::exists(folder) && !std::filesystem::is_empty(folder))
            return failure(BackendError::REJECTED, "initialize requires an empty directory");
        DataStore staged;
        staged.users.emplace_back(UserId("U001"), name, password, UserRole::ADMIN, UserStatus::ACTIVE);
        staged.rebuildIndexes();
        impl_->folder = std::filesystem::absolute(folder).string();
        staged.dirty = true;
        impl_->data.swap(staged);
        impl_->ready = true;
        return impl_->persist();
    } catch (const std::invalid_argument& e) {
        return failure(BackendError::INVALID_INPUT, e.what());
    } catch (const std::filesystem::filesystem_error& e) {
        return failure(BackendError::IO_ERROR, e.what());
    }
}
Status Application::load(const std::string& folder, UnsavedPolicy unsaved) {
    if (dirty() && unsaved != UnsavedPolicy::DISCARD)
        return failure(BackendError::UNSAVED_CHANGES, "save or explicitly discard changes");
    if (folder.empty() || (unsaved != UnsavedPolicy::REJECT && unsaved != UnsavedPolicy::DISCARD))
        return failure(BackendError::INVALID_INPUT, "invalid load arguments");
    try {
        std::string resolved = std::filesystem::absolute(folder).string();
        std::string error;
        if (!impl_->service.load(resolved, error, unsaved == UnsavedPolicy::DISCARD))
            return failure(BackendError::IO_ERROR, std::move(error));
        impl_->folder.swap(resolved);
        impl_->ready = true;
        return {};
    } catch (const std::filesystem::filesystem_error& e) {
        return failure(BackendError::IO_ERROR, e.what());
    }
}
Status Application::retrySave() {
    if (!loaded()) return failure(BackendError::NOT_LOADED, "load first");
    auto result = impl_->persist();
    if (result.save == SaveState::PENDING) result.error = BackendError::IO_ERROR;
    return result;
}
Status Application::close() {
    if (dirty()) return failure(BackendError::UNSAVED_CHANGES, "save or discard before closing");
    discardAndClose();
    return {};
}
void Application::discardAndClose() {
    auto replacement = std::make_unique<Impl>(0);
    replacement->engine = impl_->engine;
    impl_.swap(replacement);
}
Status Application::login(const std::string& name, const std::string& password) {
    if (!loaded()) return failure(BackendError::NOT_LOADED, "load first");
    logout();
    if (!impl_->users.login(name, password))
        return failure(BackendError::REJECTED, "invalid credentials or locked account");
    return {};
}
void Application::logout() noexcept { impl_->service.logout(); }
Result<AccountSummary> Application::currentAccount() const {
    auto status = impl_->guard();
    if (!status.ok()) return {status, {}};
    return {{}, summary(*impl_->users.currentUser())};
}
Result<std::vector<AccountSummary>> Application::accounts() const {
    auto status = impl_->guard(true);
    if (!status.ok()) return {status, {}};
    std::vector<AccountSummary> rows;
    for (const auto& u : impl_->data.users) rows.push_back(summary(u));
    return {{}, std::move(rows)};
}
Result<UserId> Application::createAccount(const std::string& name, const std::string& password,
                                          UserRole role, UserStatus state) {
    std::optional<UserId> id;
    auto status = impl_->change(true, [&](std::string& error) {
        id = nextId<UserId>(impl_->data.users);
        return impl_->users.create(User(*id, name, password, role, state), error);
    });
    return {status, status.ok() ? id : std::nullopt};
}
Status Application::setAccountStatus(const UserId& id, UserStatus state) {
    return impl_->change(true, [&](std::string& e) { return impl_->users.setStatus(id, state, e); });
}
Status Application::resetPassword(const UserId& id, const std::string& password) {
    return impl_->change(true, [&](std::string& e) { return impl_->users.resetPassword(id, password, e); });
}

Result<std::vector<Level>> Application::levels() const {
    auto status = impl_->guard();
    if (!status.ok()) return {status, {}};
    auto rows = impl_->data.levels;
    mergeSort(rows, [](const Level& a, const Level& b) {
        if (a.getOrder() != b.getOrder()) return a.getOrder() < b.getOrder();
        return a.getId().number() < b.getId().number();
    });
    return {{}, std::move(rows)};
}
Result<ResourceDetail> Application::resource(const ResourceId& id) const {
    auto status = impl_->guard();
    if (!status.ok()) return {status, {}};
    if (!impl_->data.findResource(id)) return {failure(BackendError::NOT_FOUND, "resource missing"), {}};
    return {{}, detail(impl_->data, id)};
}
Result<std::vector<SearchRow>> Application::search(const SearchOptions& options) {
    auto status = impl_->guard();
    if (!status.ok()) return {status, {}};
    if ((options.type && (*options.type < ResourceType::VOCABULARY || *options.type > ResourceType::KANJI)) ||
        options.sort < SearchSort::RELEVANCE || options.sort > SearchSort::TERM_DESC ||
        (options.levelId && !impl_->data.findLevel(*options.levelId)) ||
        (options.exactId && !options.query.empty() && !ResourceId::isValid(options.query)))
        return {failure(BackendError::INVALID_INPUT, "invalid search options"), {}};
    try { validateTxtField(options.query, "query", false); }
    catch (const std::invalid_argument& e) { return {failure(BackendError::INVALID_INPUT, e.what()), {}}; }
    std::string warning;
    auto rows = impl_->service.search(options, warning);
    if (!isBlank(options.query) && warning.empty()) status = impl_->persist();
    // History failure does not invalidate search results.
    if (!warning.empty()) status.message = std::move(warning);
    return {status, std::move(rows)};
}
Result<ResourceStats> Application::statistics(std::optional<LevelId> level) const {
    auto status = impl_->guard();
    if (!status.ok()) return {status, {}};
    if (level && !impl_->data.findLevel(*level)) return {failure(BackendError::NOT_FOUND, "level missing"), {}};
    return {{}, impl_->resources.statistics(level)};
}
Result<std::vector<ResourceDetail>> Application::favorites() const {
    auto status = impl_->guard();
    if (!status.ok()) return {status, {}};
    std::vector<ResourceDetail> rows;
    for (const auto& f : impl_->favorites.list()) rows.push_back(detail(impl_->data, f.getResourceId()));
    return {{}, std::move(rows)};
}
Result<std::vector<SearchHistory>> Application::history() const {
    auto status = impl_->guard();
    if (!status.ok()) return {status, {}};
    return {{}, impl_->histories.listNewest()};
}
Result<std::vector<SearchRow>> Application::searchAgain(const HistoryId& id) {
    auto status = impl_->guard();
    if (!status.ok()) return {status, {}};
    for (const auto& h : impl_->histories.listNewest())
        if (h.getId() == id) {
            SearchOptions options;
            options.query = h.getQuery();
            return search(options);
        }
    return {failure(BackendError::NOT_FOUND, "history missing for current account"), {}};
}
Status Application::clearHistory() {
    return impl_->change(false, [&](std::string& error) { return impl_->histories.clear(error); });
}
Status Application::startReview() {
    auto status = impl_->guard();
    if (!status.ok()) return status;
    std::string error;
    if (!impl_->service.startReview(error)) return failure(BackendError::REJECTED, std::move(error));
    return {};
}
Result<ReviewState> Application::review() const {
    auto status = impl_->guard();
    if (!status.ok()) return {status, {}};
    const auto* session = impl_->service.review();
    if (!session) return {failure(BackendError::INVALID_STATE, "no review session"), {}};
    ReviewState view{{}, session->remaining(), session->initialCount(), session->ratingCount(), session->finished()};
    if (const auto* card = session->current()) {
        view.current = ReviewView{card->resourceId, card->type, card->front, {}};
        if (session->revealed()) view.current->back = card->back;
    }
    return {{}, std::move(view)};
}
Status Application::revealReview() {
    auto status = impl_->guard();
    if (!status.ok()) return status;
    auto* session = impl_->service.review();
    if (!session || !session->reveal()) return failure(BackendError::INVALID_STATE, "no current card");
    return {};
}
Status Application::rateReview(bool remembered) {
    auto status = impl_->guard();
    if (!status.ok()) return status;
    auto* session = impl_->service.review();
    if (!session || !session->rate(remembered))
        return failure(BackendError::INVALID_STATE, "reveal current card before rating");
    return {};
}
void Application::stopReview() noexcept {
    if (auto* session = impl_->service.review()) session->stop();
}
Result<std::vector<AlphabetEntry>> Application::alphabet(std::optional<Script> script, const std::string& group) const {
    auto status = impl_->guard();
    if (!status.ok()) return {status, {}};
    if (script && *script != Script::HIRAGANA && *script != Script::KATAKANA)
        return {failure(BackendError::INVALID_INPUT, "invalid script"), {}};
    return {{}, impl_->alphabet.list(script, group)};
}
Result<std::vector<Question>> Application::questions(std::optional<LevelId> level,
                                                     std::optional<QuestionStatus> state) const {
    auto status = impl_->guard(true);
    if (!status.ok()) return {status, {}};
    if (state && *state != QuestionStatus::ACTIVE && *state != QuestionStatus::INACTIVE)
        return {failure(BackendError::INVALID_INPUT, "invalid question status"), {}};
    std::vector<Question> rows;
    for (const auto& q : impl_->data.questions)
        if ((!level || q.getLevelId() == *level) && (!state || q.getStatus() == *state)) rows.push_back(q);
    return {{}, std::move(rows)};
}
Result<Question> Application::question(const QuestionId& id) const {
    auto status = impl_->guard(true);
    if (!status.ok()) return {status, {}};
    const auto it = impl_->data.questionById.find(id.getId());
    if (it == impl_->data.questionById.end()) return {failure(BackendError::NOT_FOUND, "question missing"), {}};
    return {{}, impl_->data.questions[it->second]};
}
Status Application::setQuestionStatus(const QuestionId& id, QuestionStatus state) {
    return impl_->change(true, [&](std::string& error) { return impl_->questions.setStatus(id, state, error); });
}
Status Application::startQuiz(const LevelId& level, std::size_t count) {
    auto status = impl_->guard();
    if (!status.ok()) return status;
    std::string error;
    if (!impl_->quizzes.start(level, count, impl_->engine, error)) return failure(BackendError::REJECTED, std::move(error));
    return {};
}
Result<QuizState> Application::quiz() const {
    auto status = impl_->guard();
    if (!status.ok()) return {status, {}};
    const auto* session = impl_->quizzes.session();
    if (!session) return {failure(BackendError::INVALID_STATE, "no quiz session"), {}};
    QuizState view{{}, session->submitted()};
    for (std::size_t i = 0; i < session->questions().size(); ++i) {
        const auto& q = session->questions()[i];
        view.questions.push_back({q.getId(), q.getPrompt(), q.getOptions(), session->answers()[i]});
    }
    return {{}, std::move(view)};
}
Status Application::selectAnswer(std::size_t index, char answer) {
    auto status = impl_->guard();
    if (!status.ok()) return status;
    if (!impl_->quizzes.selectAnswer(index, answer))
        return failure(BackendError::INVALID_STATE, "no active quiz, invalid index or answer");
    return {};
}
Result<QuizAttempt> Application::submitQuiz() {
    std::optional<QuizAttempt> result;
    auto status = impl_->change(false, [&](std::string& error) { return impl_->quizzes.submit(result, error); });
    return {status, status.ok() ? result : std::nullopt};
}
Result<std::vector<QuizAnswerDetail>> Application::quizAnswers() const {
    auto status = impl_->guard();
    if (!status.ok()) return {status, {}};
    const auto* session = impl_->quizzes.session();
    if (!session || !session->submitted()) return {failure(BackendError::INVALID_STATE, "submit quiz first"), {}};
    std::vector<QuizAnswerDetail> rows;
    for (std::size_t i = 0; i < session->questions().size(); ++i) {
        const auto& q = session->questions()[i];
        rows.push_back({q.getId(), session->answers()[i], q.getCorrectOption(), q.getExplanation()});
    }
    return {{}, std::move(rows)};
}
Status Application::retryWrong() {
    auto status = impl_->guard();
    if (!status.ok()) return status;
    std::string error;
    if (!impl_->quizzes.retryWrong(error)) return failure(BackendError::INVALID_STATE, std::move(error));
    return {};
}
void Application::stopQuiz() noexcept { impl_->quizzes.stop(); }
Result<std::vector<QuizAttempt>> Application::attempts() const {
    auto status = impl_->guard();
    if (!status.ok()) return {status, {}};
    return {{}, impl_->quizzes.history()};
}
Status Application::importPackage(const std::string& folder) {
    return impl_->change(true, [&](std::string& error) { return impl_->service.importPackage(folder, error); });
}
Status Application::exportPackage(const std::string& folder) const {
    auto status = impl_->guard(true);
    if (!status.ok()) return status;
    try {
        if (folder.empty()) return failure(BackendError::INVALID_INPUT, "empty export folder");
        if (sameDirectory(folder, impl_->folder)) return failure(BackendError::REJECTED, "export folder is active data folder");
        std::string error;
        if (!impl_->service.exportPackage(folder, error)) return failure(BackendError::IO_ERROR, std::move(error));
        return {};
    } catch (const std::filesystem::filesystem_error& e) {
        return failure(BackendError::IO_ERROR, e.what());
    }
}

Result<ResourceId> Application::createVocabulary(const VocabularyInput& input) {
    std::optional<ResourceId> id;
    auto status = impl_->change(true, [&](std::string& error) {
        id = impl_->resources.nextResourceId();
        return impl_->resources.addVocabulary(make(*id, input), error);
    });
    return {status, status.ok() ? id : std::nullopt};
}
Status Application::updateVocabulary(const ResourceId& id, const VocabularyInput& input) {
    return impl_->change(true, [&](std::string& error) {
        return impl_->resources.updateVocabulary(make(id, input), error);
    });
}

Result<ResourceId> Application::createGrammar(const GrammarInput& input) {
    std::optional<ResourceId> id;
    auto status = impl_->change(true, [&](std::string& error) {
        id = impl_->resources.nextResourceId();
        return impl_->resources.addGrammar(make(*id, input), error);
    });
    return {status, status.ok() ? id : std::nullopt};
}
Status Application::updateGrammar(const ResourceId& id, const GrammarInput& input) {
    return impl_->change(true, [&](std::string& error) {
        return impl_->resources.updateGrammar(make(id, input), error);
    });
}

Result<ResourceId> Application::createKanji(const KanjiInput& input) {
    std::optional<ResourceId> id;
    auto status = impl_->change(true, [&](std::string& error) {
        id = impl_->resources.nextResourceId();
        return impl_->resources.addKanji(make(*id, input), error);
    });
    return {status, status.ok() ? id : std::nullopt};
}
Status Application::updateKanji(const ResourceId& id, const KanjiInput& input) {
    return impl_->change(true, [&](std::string& error) {
        return impl_->resources.updateKanji(make(id, input), error);
    });
}

Result<LevelId> Application::createLevel(const LevelInput& input) {
    std::optional<LevelId> id;
    auto status = impl_->change(true, [&](std::string& error) {
        id = nextId<LevelId>(impl_->data.levels);
        return impl_->levels.add(make(*id, input), error);
    });
    return {status, status.ok() ? id : std::nullopt};
}
Status Application::updateLevel(const LevelId& id, const LevelInput& input) {
    return impl_->change(true, [&](std::string& error) {
        return impl_->levels.update(make(id, input), error);
    });
}

Result<AlphabetId> Application::createAlphabet(const AlphabetInput& input) {
    std::optional<AlphabetId> id;
    auto status = impl_->change(true, [&](std::string& error) {
        id = nextId<AlphabetId>(impl_->data.alphabet);
        return impl_->alphabet.add(make(*id, input), error);
    });
    return {status, status.ok() ? id : std::nullopt};
}
Status Application::updateAlphabet(const AlphabetId& id, const AlphabetInput& input) {
    return impl_->change(true, [&](std::string& error) {
        return impl_->alphabet.update(make(id, input), error);
    });
}

Result<QuestionId> Application::createQuestion(const QuestionInput& input) {
    std::optional<QuestionId> id;
    auto status = impl_->change(true, [&](std::string& error) {
        id = nextId<QuestionId>(impl_->data.questions);
        return impl_->questions.add(make(*id, input), error);
    });
    return {status, status.ok() ? id : std::nullopt};
}
Status Application::updateQuestion(const QuestionId& id, const QuestionInput& input) {
    return impl_->change(true, [&](std::string& error) {
        return impl_->questions.update(make(id, input), error);
    });
}

Status Application::removeLevel(const LevelId& id) {
    return impl_->change(true, [&](std::string& error) {
        return impl_->levels.remove(id, error);
    });
}

Status Application::removeResource(const ResourceId& id) {
    return impl_->change(true, [&](std::string& error) {
        return impl_->service.removeResource(id, error);
    });
}

Status Application::removeAlphabet(const AlphabetId& id) {
    return impl_->change(true, [&](std::string& error) {
        return impl_->alphabet.remove(id, error);
    });
}

Status Application::removeQuestion(const QuestionId& id) {
    return impl_->change(true, [&](std::string& error) {
        return impl_->questions.remove(id, error);
    });
}

Status Application::addFavorite(const ResourceId& id) {
    return impl_->change(false, [&](std::string& error) {
        return impl_->favorites.add(id, error);
    });
}

Status Application::removeFavorite(const ResourceId& id) {
    return impl_->change(false, [&](std::string& error) {
        return impl_->favorites.remove(id, error);
    });
}

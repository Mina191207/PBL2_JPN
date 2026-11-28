#pragma once
#include "pbl2/Search.h"
#include "pbl2/domain/AlphabetEntry.h"
#include "pbl2/domain/Question.h"
#include "pbl2/domain/QuizAttempt.h"
#include "pbl2/domain/grammar.h"
#include "pbl2/domain/kanji.h"
#include "pbl2/domain/level.h"
#include "pbl2/domain/search_his.h"
#include "pbl2/UserRole.h"
#include "pbl2/domain/vocabulary.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

enum class BackendError {
    NONE, NOT_LOADED, LOGIN_REQUIRED, FORBIDDEN, INVALID_INPUT, NOT_FOUND,
    REJECTED, INVALID_STATE, ID_EXHAUSTED, IO_ERROR, UNSAVED_CHANGES, EMPTY_DATA_DIRECTORY
};
enum class SaveState { UNCHANGED, SAVED, PENDING };
enum class UnsavedPolicy { REJECT, DISCARD };
struct Status {
    BackendError error = BackendError::NONE;
    SaveState save = SaveState::UNCHANGED;
    std::string message;
    bool ok() const noexcept { return error == BackendError::NONE; }
};
struct PackageExportPlan {
    std::string folder;
    std::vector<std::string> existingFiles;
};
template<class T> struct Result {
    Status status;
    std::optional<T> value;
    bool ok() const noexcept { return status.ok(); }
};

// Requests omit identities. Updates receive the existing ID separately.
struct VocabularyInput {
    LevelId levelId;
    std::string word, reading, meaning;
    PartOfSpeech partOfSpeech;
    std::string example;
};
struct GrammarInput {
    LevelId levelId;
    std::string pattern, meaning, structure, usage, example, note;
};
struct KanjiInput {
    LevelId levelId;
    std::string character, meaning;
    int strokeCount;
    std::string onyomi, kunyomi, examples;
};
struct LevelInput { std::string category, name; int order; std::string description; };
struct AlphabetInput {
    Script script;
    std::string glyph, romaji, groupName;
    int displayOrder;
    std::string note;
};
struct QuestionInput {
    LevelId levelId;
    std::optional<ResourceId> resourceId;
    std::string prompt;
    std::array<std::string,4> options;
    char correctOption;
    std::string explanation;
    QuestionStatus status;
};
struct AccountSummary { UserId id; std::string username; UserRole role; UserStatus status; };
using ResourceDetail = std::variant<Vocabulary, Grammar, Kanji>;
struct ReviewView {
    ResourceId resourceId;
    ResourceType type;
    std::string front;
    std::optional<std::string> back;
};
struct ReviewState {
    std::optional<ReviewView> current;
    std::size_t remaining, initialCount, ratingCount;
    bool finished;
};
struct QuizQuestionView {
    std::string prompt;
    std::array<std::string,4> options;
};
struct QuizState {
    std::vector<QuizQuestionView> questions;
    std::vector<char> selectedAnswers;
    bool submitted;
};
struct QuizAnswerDetail {
    QuestionId id;
    char selectedAnswer, correctAnswer;
    std::string explanation;
};

// Call sequentially. An open Application exclusively owns its local store.
// Returned values own their content.
class Application {
    struct Impl;
    std::unique_ptr<Impl> impl_;
  public:
    Application();
    explicit Application(std::uint32_t quizSeed);
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;
    // Filesystem path arguments are UTF-8 encoded on every platform.
    Status initialize(const std::string& emptyFolder, const std::string& adminName,
                      const std::string& password);
    Status load(const std::string& folder, UnsavedPolicy unsaved = UnsavedPolicy::REJECT);
    Status retrySave();
    bool loaded() const noexcept;
    bool dirty() const noexcept;
    Status close();
    void discardAndClose();
    Status login(const std::string& username, const std::string& password);
    void logout() noexcept;
    Result<AccountSummary> currentAccount() const;
    Result<std::vector<AccountSummary>> accounts() const;
    Result<UserId> createAccount(const std::string& username, const std::string& password,
                                 UserRole role, UserStatus status = UserStatus::ACTIVE);
    Status setAccountStatus(const UserId&, UserStatus);
    Status resetPassword(const UserId&, const std::string& password);
    Status changePassword(const std::string& currentPassword, const std::string& newPassword);
    Result<std::vector<Level>> levels() const;
    Result<LevelId> createLevel(const LevelInput&);
    Status updateLevel(const LevelId&, const LevelInput&);
    Status removeLevel(const LevelId&);
    Result<ResourceId> createVocabulary(const VocabularyInput&);
    Result<ResourceId> createGrammar(const GrammarInput&);
    Result<ResourceId> createKanji(const KanjiInput&);
    Status updateVocabulary(const ResourceId&, const VocabularyInput&);
    Status updateGrammar(const ResourceId&, const GrammarInput&);
    Status updateKanji(const ResourceId&, const KanjiInput&);
    Status removeResource(const ResourceId&);
    Result<ResourceDetail> resource(const ResourceId&) const;
    Result<std::vector<SearchRow>> search(const SearchOptions&) const;
    Result<std::vector<SearchRow>> submitSearch(const SearchOptions&);
    Result<ResourceStats> statistics(std::optional<LevelId> level = {}) const;
    Status addFavorite(const ResourceId&);
    Status removeFavorite(const ResourceId&);
    Result<std::vector<ResourceDetail>> favorites() const;
    Result<std::vector<SearchHistory>> history() const;
    Result<SearchReplay> searchAgain(const HistoryId&);
    Status clearHistory();
    Status startReview();
    Result<ReviewState> review() const;
    Status revealReview();
    Status rateReview(bool remembered);
    void stopReview() noexcept;
    Result<std::vector<AlphabetEntry>> alphabet(std::optional<Script> script = {},
                                               const std::string& group = {}) const;
    Result<AlphabetId> createAlphabet(const AlphabetInput&);
    Status updateAlphabet(const AlphabetId&, const AlphabetInput&);
    Status removeAlphabet(const AlphabetId&);
    Result<std::vector<Question>> questions(std::optional<LevelId> level = {},
                                            std::optional<QuestionStatus> status = {}) const;
    Result<std::size_t> activeQuestionCount(const LevelId& level) const;
    Result<Question> question(const QuestionId&) const;
    Result<QuestionId> createQuestion(const QuestionInput&);
    Status updateQuestion(const QuestionId&, const QuestionInput&);
    Status removeQuestion(const QuestionId&);
    Status setQuestionStatus(const QuestionId&, QuestionStatus);
    Status startQuiz(const LevelId&, std::size_t count);
    Result<QuizState> quiz() const;
    Status selectAnswer(std::size_t index, char answer);
    Result<QuizAttempt> submitQuiz();
    Result<std::vector<QuizAnswerDetail>> quizAnswers() const;
    Status retryWrong();
    void stopQuiz() noexcept;
    Result<std::vector<QuizAttempt>> attempts() const;
    Status importPackage(const std::string& folder);
    Result<PackageExportPlan> inspectPackageExport(const std::string& folder) const;
    // Pass the files approved for replacement after inspecting the destination.
    Status exportPackage(const std::string& folder,
                         const std::vector<std::string>& approvedFiles = {}) const;
};

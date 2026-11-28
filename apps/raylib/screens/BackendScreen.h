#pragma once

#include "pbl2/Application.h"
#include "UiInput.h"
#include "FolderField.h"
#include "Forms.h"
#include "Sections.h"
#include "raylib.h"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using BackendId = std::variant<std::monostate, LevelId, UserId, AlphabetId, QuestionId, ResourceId, HistoryId, AttemptId>;

struct BackendTableRow {
    BackendId id;
    std::vector<std::string> cells;
    std::optional<UserRole> role{};
};

struct BackendTableData {
    std::vector<std::string> headers;
    std::vector<BackendTableRow> rows;
};

struct BackendActivityState {
    std::optional<ResourceStats> resourceStats;
    std::vector<Level> quizLevels;
    std::optional<LevelId> selectedQuizLevel;
    std::size_t activeQuestionCount = 0;
    std::optional<QuizState> quiz;
    std::optional<QuizAttempt> quizResult;
    std::vector<QuizAnswerDetail> quizAnswers;
    std::optional<ReviewState> review;
    std::optional<ResourceDetail> reviewResource;
    bool reviewStopped = false;
};
FormFields adminFormFields(Section section);

struct BackendScreenEvent {
    enum class Type { NONE, REFRESH, REQUEST_CLEAR_HISTORY, CLEAR_HISTORY, REPLAY_HISTORY, REMOVE_FAVORITE,
        START_REVIEW, REVEAL_REVIEW, RATE_KNOWN, RATE_UNKNOWN, STOP_REVIEW,
        START_QUIZ, SELECT_QUIZ_LEVEL,
        ANSWER, SUBMIT_QUIZ, RETRY_WRONG, STOP_QUIZ,
        TOGGLE_ACCOUNT, TOGGLE_QUESTION, DELETE_ROW, EXPORT_PACKAGE, REPLACE_PACKAGE, IMPORT_PACKAGE,
        OPEN_CREATE, SAVE_FORM, EDIT_ROW, RESET_PASSWORD, PASSWORD_TOO_LONG, CANCEL_FORM, EDITED } type = Type::NONE;
    BackendId id;
    std::size_t questionIndex = 0;
    char answer = 'A';
    std::string text;
    std::vector<std::string> approvedFiles;
    BackendScreenEvent() = default;
    BackendScreenEvent(Type eventType, BackendId target = {}) : type(eventType), id(std::move(target)) {}
};

class BackendScreen {
  public:
    BackendScreenEvent update(Section section, UserRole role, const std::vector<AlphabetEntry>& alphabet,
                              const BackendTableData& table, const BackendActivityState& activity,
                              const std::string& message = {});
    void openEditForm(FormFields fields, BackendId id);
    void openResetPasswordForm(const UserId& id);
    void confirm(BackendScreenEvent event, std::string title, std::string message);
    void draw(Section section, UserRole role, const std::vector<AlphabetEntry>& alphabet,
              const BackendTableData& table, const std::string& message,
              bool messageError,
              const BackendActivityState& activity) const;
    const std::string& password() const noexcept { return password_.value(); }
    void clearPassword() noexcept { password_.clear(); }
    const FormFields& formFields() const noexcept { return formFields_; }
    void closeForm() noexcept;
    void navigate(Section section);
    void revalidate(const BackendTableData& table, const std::vector<AlphabetEntry>& alphabet);
    void reset() noexcept;
    bool modalOpen() const { return formOpen_ || confirmation_.has_value() || rowDetailsOpen_; }
    bool tab(Section section, UserRole role, const BackendTableData& table,
             const std::vector<AlphabetEntry>& alphabet, const BackendActivityState& activity,
             const std::string& message = {});
    void clearFocus() { focus_.id = UiInput::Focus::none; }
  private:
    friend struct RaylibStateTest;
    enum Control { Refresh, Roles, Rows, Hiragana, Katakana, PackagePath, PackageBrowse, Export, Import,
                   RowDetails, QuizLevel, QuizPrevious, QuizNext, QuizText, QuizA, QuizB, QuizC, QuizD, ActionBase };
    static int actionControl(BackendScreenEvent::Type event) { return ActionBase + static_cast<int>(event); }
    UiInput::Focus focus_, modalFocus_{0};
    UiInput::Cursor formCursor_;
    struct Position { BackendId selected; std::size_t scroll = 0; };
    std::array<Position, static_cast<std::size_t>(Section::Count)> positions_{};
    Section section_ = Section::Dashboard;
    BackendId selected_;
    std::size_t scrollOffset_ = 0;
    std::size_t alphabetScrollOffset_ = 0;
    std::size_t quizQuestionIndex_ = 0;
    float quizOffset_ = 0.0f, quizResultOffset_ = 0.0f;
    Vector2 quizViewportSize_{};
    float bannerOffset_ = 0.0f;
    bool rowDetailsOpen_ = false;
    float rowDetailsOffset_ = 0.0f;
    float reviewDetailsScrollOffset_ = 0.0f;
    std::size_t accountRoleFilter_ = 0;
    bool alphabetKatakana_ = false;
    FolderField packageFolder_{"outputs/resource-package"};
    struct Confirmation {
        BackendScreenEvent event;
        std::string title;
        std::string message;
    };
    std::optional<Confirmation> confirmation_;
    bool formOpen_ = false;
    FormFields formFields_;
    PasswordInput password_;
    bool editing_ = false;
    bool resetPasswordForm_ = false;
    BackendId editId_;
    bool hasSelection() const noexcept { return !std::holds_alternative<std::monostate>(selected_); }
    BackendScreenEvent updateConfirmation();
    BackendScreenEvent updateForm();
    void openCreateForm(Section section);
    void drawForm(Section section, const std::string& error) const;
    BackendScreenEvent updateReview(Rectangle list, float actionX, float actionY, const BackendActivityState& activity);
    BackendScreenEvent updateQuiz(Rectangle list, float actionX, float actionY, const BackendActivityState& activity);
    void addStudyControls(std::vector<int>& order, Section section, const BackendActivityState& activity) const;
    void revealQuizOption(Rectangle list, const BackendActivityState& activity);
    BackendScreenEvent updateActions(Section section, UserRole role, float actionX, float actionY);
    void drawActions(Rectangle content, Rectangle helper, float actionX, float actionY, Section section,
                     UserRole role, const std::string& message, bool messageError,
                     const BackendActivityState& activity) const;
    void drawStatisticsCards(Rectangle content, const BackendActivityState& activity) const;
    void drawStudyActions(Section section, float x, float y, const BackendActivityState& activity) const;
    void drawReviewPage(Rectangle list, const BackendActivityState& activity, float scrollOffset) const;
    void drawQuizPage(Rectangle list, const BackendActivityState& activity) const;
    void drawRowDetails(const BackendTableData& table) const;
    void drawTablePage(Rectangle content, Rectangle list, Section section,
                       const BackendTableData& table, const std::vector<std::size_t>* sourceRows,
                       bool statisticsPage) const;
    void drawAlphabetPage(Rectangle list, const std::vector<AlphabetEntry>& rows,
                          bool katakana, std::size_t scrollOffset) const;
    static bool click(Rectangle bounds);
    static void button(Rectangle bounds, const char* label, bool primary = false, bool enabled = true);
    void controlButton(int control, Rectangle bounds, const char* label, bool primary = false, bool enabled = true) const;
};

#pragma once

#include "pbl2/Application.h"
#include "../PasswordInput.h"
#include "Forms.h"
#include "Sections.h"
#include "raylib.h"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using BackendId = std::variant<std::monostate, LevelId, UserId, AlphabetId, QuestionId, ResourceId, HistoryId>;

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
    enum class Type { NONE, REFRESH, CLEAR_HISTORY, REPLAY_HISTORY, REMOVE_FAVORITE,
        START_REVIEW, REVEAL_REVIEW, RATE_KNOWN, RATE_UNKNOWN, STOP_REVIEW,
        START_QUIZ, SELECT_QUIZ_LEVEL,
        ANSWER, SUBMIT_QUIZ, RETRY_WRONG, STOP_QUIZ,
        TOGGLE_ACCOUNT, TOGGLE_QUESTION, DELETE_ROW, EXPORT_PACKAGE, IMPORT_PACKAGE,
        OPEN_CREATE, SAVE_FORM, EDIT_ROW, RESET_PASSWORD, PASSWORD_TOO_LONG, CANCEL_FORM } type = Type::NONE;
    BackendId id;
    std::size_t questionIndex = 0;
    char answer = 'A';
    std::string text;
    BackendScreenEvent() = default;
    BackendScreenEvent(Type eventType, BackendId target = {}) : type(eventType), id(std::move(target)) {}
};

class BackendScreen {
  public:
    BackendScreenEvent update(Section section, UserRole role, const std::vector<AlphabetEntry>& alphabet,
                              const BackendTableData& table, const BackendActivityState& activity);
    void openEditForm(FormFields fields, BackendId id);
    void openResetPasswordForm(const UserId& id);
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
  private:
    struct Position { BackendId selected; std::size_t scroll = 0; };
    std::array<Position, static_cast<std::size_t>(Section::Count)> positions_{};
    Section section_ = Section::Dashboard;
    BackendId selected_;
    std::size_t scrollOffset_ = 0;
    std::size_t alphabetScrollOffset_ = 0;
    std::size_t quizQuestionIndex_ = 0;
    float reviewDetailsScrollOffset_ = 0.0f;
    std::size_t accountRoleFilter_ = 0;
    bool alphabetKatakana_ = false;
    std::string packagePath_ = "outputs/resource-package";
    bool importConfirm_ = false;
    bool deleteConfirm_ = false;
    bool packagePathFocused_ = false;
    bool formOpen_ = false;
    std::size_t focusedField_ = 0;
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
    BackendScreenEvent updateActions(Section section, UserRole role, float actionX, float actionY);
    void drawActions(Rectangle content, Rectangle list, float actionX, float actionY, Section section,
                     UserRole role, const std::string& message, bool messageError,
                     const BackendActivityState& activity) const;
    void drawStatisticsCards(Rectangle content, const BackendActivityState& activity) const;
    void drawStudyActions(Section section, float x, float y, const BackendActivityState& activity) const;
    void drawReviewPage(Rectangle list, const BackendActivityState& activity, float scrollOffset) const;
    void drawQuizPage(Rectangle list, const BackendActivityState& activity, std::size_t questionIndex,
                      std::size_t scrollOffset) const;
    void drawTablePage(Rectangle content, Rectangle list, Section section,
                       const BackendTableData& table, const std::vector<std::size_t>* sourceRows,
                       bool statisticsPage) const;
    void drawAlphabetPage(Rectangle list, const std::vector<AlphabetEntry>& rows,
                          bool katakana, std::size_t scrollOffset) const;
    static bool click(Rectangle bounds);
    static void button(Rectangle bounds, const char* label, bool primary = false, bool enabled = true);
};

#pragma once

#include "Application.h"
#include "raylib.h"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct BackendTableData {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
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

struct BackendScreenEvent {
    enum class Type { NONE, REFRESH, SELECT, CLEAR_SELECTION, CLEAR_HISTORY, REPLAY_HISTORY, REMOVE_FAVORITE,
        START_REVIEW, REVEAL_REVIEW, RATE_KNOWN, RATE_UNKNOWN, STOP_REVIEW,
        START_QUIZ, SELECT_QUIZ_LEVEL, PREVIOUS_QUESTION, NEXT_QUESTION,
        ANSWER, SUBMIT_QUIZ, RETRY_WRONG, STOP_QUIZ,
        TOGGLE_ACCOUNT, TOGGLE_QUESTION, DELETE_ROW, EXPORT_PACKAGE, IMPORT_PACKAGE,
        CREATE_ROW, UPDATE_ROW, EDIT_ROW, RESET_PASSWORD, CANCEL_FORM } type = Type::NONE;
    std::size_t index = 0;
    char answer = 'A';
    std::string text;
    std::array<std::string, 12> fields{};
    BackendScreenEvent() = default;
    BackendScreenEvent(Type eventType, std::size_t row = 0, char selectedAnswer = 'A', std::string value = {})
        : type(eventType), index(row), answer(selectedAnswer), text(std::move(value)) {}
};

class BackendScreen {
  public:
    BackendScreenEvent update(std::size_t section, UserRole role, const std::vector<std::string>& rows,
                              const BackendTableData& table, const BackendActivityState& activity);
    void openEditForm(const std::array<std::string,12>& fields, const std::string& id);
    void openResetPasswordForm(const std::string& id);
    void draw(std::size_t section, UserRole role, const std::vector<std::string>& rows,
              const BackendTableData& table, std::size_t selected, const std::string& message,
              bool messageError,
              const BackendActivityState& activity) const;
    void reset() noexcept { selected_ = 0; scrollOffset_ = 0; alphabetScrollOffset_ = 0; quizQuestionIndex_ = 0; reviewDetailsScrollOffset_ = 0.0f; alphabetKatakana_ = false; accountRoleFilter_ = 0; hasSelected_ = false; importConfirm_ = false; deleteConfirm_ = false; }
  private:
    std::size_t selected_ = 0;
    std::size_t scrollOffset_ = 0;
    std::size_t alphabetScrollOffset_ = 0;
    std::size_t quizQuestionIndex_ = 0;
    float reviewDetailsScrollOffset_ = 0.0f;
    std::size_t accountRoleFilter_ = 0;
    bool alphabetKatakana_ = false;
    bool hasSelected_ = false;
    std::string packagePath_ = "outputs/resource-package";
    bool importConfirm_ = false;
    bool deleteConfirm_ = false;
    bool packagePathFocused_ = false;
    bool formOpen_ = false;
    std::size_t focusedField_ = 0;
    std::array<std::string, 12> formFields_{};
    bool editing_ = false;
    bool resetPasswordForm_ = false;
    std::string editId_;
    BackendScreenEvent updateConfirmation();
    BackendScreenEvent updateForm(std::size_t section);
    BackendScreenEvent updateReview(Rectangle list, float actionX, float actionY, const BackendActivityState& activity);
    BackendScreenEvent updateQuiz(Rectangle list, float actionX, float actionY, const BackendActivityState& activity);
    BackendScreenEvent updateActions(std::size_t section, UserRole role, float actionX, float actionY);
    void drawActions(Rectangle content, Rectangle list, float actionX, float actionY, std::size_t section,
                     UserRole role, const std::string& message, bool messageError,
                     const BackendActivityState& activity) const;
    void drawStatisticsCards(Rectangle content, const BackendActivityState& activity) const;
    void drawReviewPage(Rectangle list, const BackendActivityState& activity, float scrollOffset) const;
    void drawQuizPage(Rectangle list, const BackendActivityState& activity, std::size_t questionIndex,
                      std::size_t scrollOffset) const;
    void drawTablePage(Rectangle content, Rectangle list, std::size_t section, UserRole role,
                       const BackendTableData& table, const std::vector<std::size_t>& sourceRows,
                       std::size_t selected, bool statisticsPage) const;
    void drawAlphabetPage(Rectangle list, const std::vector<std::string>& rows, std::size_t selected,
                          bool katakana, std::size_t scrollOffset) const;
    void drawRowsPage(Rectangle list, const std::vector<std::string>& rows, std::size_t selected,
                      std::size_t scrollOffset, std::size_t visibleRows, bool importExport) const;
    static bool click(Rectangle bounds);
    static void button(Rectangle bounds, const char* label, bool primary = false, bool enabled = true);
};

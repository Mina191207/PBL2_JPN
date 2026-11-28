#pragma once

#include "pbl2/Application.h"
#include "Sections.h"
#include "raylib.h"

#include <cstddef>
#include <string>
#include <vector>

struct DashboardData {
    ResourceStats resources{};
    std::size_t accountCount = 0;
    std::size_t questionCount = 0;
    std::size_t favoriteCount = 0;
    std::size_t quizAttemptCount = 0;
    std::size_t learningStreak = 0;
    std::vector<std::string> quizActivityDatesUtc;
    std::string message;
    bool loaded = false;
};

struct MainScreenEvent {
    enum class Type { NONE, SELECT_SECTION, LOGOUT, REQUEST_CLOSE } type = Type::NONE;
    Section section = Section::Dashboard;
};

class MainScreen {
  public:
    MainScreenEvent update(UserRole role, Section selectedSection);
    void draw(const AccountSummary& account, Section selectedSection,
              const DashboardData& dashboard) const;
    void resetDashboardMonth() noexcept { calendarMonthOffset_ = 0; }

  private:
    int calendarMonthOffset_ = 0;
};

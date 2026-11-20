#pragma once

#include "Application.h"
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
    std::size_t section = 0;
};

class MainScreen {
  public:
    MainScreenEvent update(UserRole role, std::size_t selectedSection);
    void draw(const AccountSummary& account, std::size_t selectedSection,
              const DashboardData& dashboard) const;
    void resetDashboardMonth() noexcept { calendarMonthOffset_ = 0; }

    static const std::vector<std::string>& sections(UserRole role);
    static const std::string& sectionDescription(UserRole role, std::size_t section);

  private:
    int calendarMonthOffset_ = 0;
};

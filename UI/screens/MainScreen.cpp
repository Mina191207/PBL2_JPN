#include "MainScreen.h"
#include "../UITheme.h"
#include "../UiLayout.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <unordered_set>
#include <utility>

namespace {
constexpr Color kBackground = UITheme::background;
constexpr Color kSidebar = UITheme::sidebar;
constexpr Color kCard = UITheme::card;
constexpr Color kPrimary = UITheme::primary;
constexpr Color kPrimaryHover = UITheme::primaryHover;
constexpr Color kBorder = UITheme::border;
constexpr Color kText = UITheme::text;
constexpr Color kMuted = UITheme::muted;

const std::vector<std::string> kAdminSections = {
    "Dashboard", "Resources", "Levels", "Accounts", "Alphabet", "Question bank", "Import / export"
};
const std::vector<std::string> kLearnerSections = {
    "Dashboard", "Search library", "Favorites", "Search history", "Review", "Quiz", "Alphabet", "Statistics"
};
const std::vector<std::string> kAdminDescriptions = {
    "Your administrator workspace is ready.",
    "Search, inspect, and manage learning resources.",
    "Manage the learning levels available in the library.",
    "Manage learner accounts and access status.",
    "Browse and manage the Japanese alphabet entries.",
    "Manage questions used by quizzes.",
    "Import or export a resource package."
};
const std::vector<std::string> kLearnerDescriptions = {
    "Your Japanese learning workspace is ready.",
    "Search the Japanese learning library.",
    "Review learning resources you have saved.",
    "View and repeat recent searches.",
    "Practice with your saved learning resources.",
    "Answer questions and review quiz results.",
    "Browse Hiragana and Katakana entries.",
    "View your learning resource statistics."
};

Rectangle sectionBounds(std::size_t index, std::size_t count) {
    const float top = std::min(138.0f, std::max(112.0f, static_cast<float>(GetScreenHeight()) * 0.19f));
    const float buttonHeight = std::clamp(static_cast<float>(GetScreenHeight()) * 0.058f, 30.0f, 42.0f);
    constexpr float preferredGap = 8.0f;
    const float bottom = static_cast<float>(GetScreenHeight()) - 126.0f;
    const float available = std::max(0.0f, bottom - top - 4.0f);
    const float gap = count > 1
                          ? std::max(0.0f, std::min(preferredGap,
                                (available - buttonHeight * static_cast<float>(count)) /
                                    static_cast<float>(count - 1)))
                          : 0.0f;
    const float step = buttonHeight + gap;
    const float side = UiLayout::sidebarWidth();
    return {14.0f, top + static_cast<float>(index) * step, side - 28.0f, buttonHeight};
}
Rectangle logoutBounds() {
    return {14.0f, static_cast<float>(GetScreenHeight()) - 64.0f,
            UiLayout::sidebarWidth() - 28.0f, 42.0f};
}
Rectangle closeBounds() {
    return {static_cast<float>(GetScreenWidth() - 176), 28.0f, 144.0f, 46.0f};
}
bool clicked(Rectangle bounds) {
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), bounds);
}
void drawButton(Rectangle bounds, const char* label) {
    const bool hover = CheckCollisionPointRec(GetMousePosition(), bounds);
    DrawRectangleRounded(bounds, 0.24f, 8, hover ? Color{255, 232, 240, 255} : kCard);
    DrawRectangleRoundedLinesEx(bounds, 0.24f, 8, 1.0f, kBorder);
    UITheme::drawText(label, static_cast<int>(bounds.x + 18), static_cast<int>(bounds.y + 13), 17, kText);
}
void drawText(const char* text, float x, float y, float size, Color color) {
    UITheme::drawText(text, static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), color);
}

struct CalendarMonth { int year; int month; };

CalendarMonth currentUtcMonth(int offset) {
    const std::time_t now = std::time(nullptr);
    const std::tm* utc = std::gmtime(&now);
    int year = utc ? utc->tm_year + 1900 : 1970;
    int month = utc ? utc->tm_mon + 1 : 1;
    const int serial = year * 12 + month - 1 + offset;
    year = serial / 12;
    month = serial % 12 + 1;
    return {year, month};
}

int daysInMonth(int year, int month) {
    static constexpr int lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0))) return 29;
    return lengths[month - 1];
}

int weekdaySundayFirst(int year, int month, int day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned adjustedMonth = month > 2 ? static_cast<unsigned>(month - 3) : static_cast<unsigned>(month + 9);
    const unsigned dayOfYear = (153 * adjustedMonth + 2) / 5 + static_cast<unsigned>(day) - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    const int ordinal = era * 146097 + static_cast<int>(dayOfEra);
    return ((ordinal + 3) % 7 + 7) % 7;
}

Rectangle calendarPreviousButton(const Rectangle& panel) {
    return {panel.x + panel.width - 86.0f, panel.y + 18.0f, 32.0f, 32.0f};
}
Rectangle calendarNextButton(const Rectangle& panel) {
    return {panel.x + panel.width - 46.0f, panel.y + 18.0f, 32.0f, 32.0f};
}

void drawMetricCard(Rectangle bounds, const char* label, const std::string& value) {
    DrawRectangleRounded(bounds, 0.08f, 8, Color{255, 252, 253, 255});
    DrawRectangleRoundedLinesEx(bounds, 0.08f, 8, 1.0f, kBorder);
    UITheme::drawText(label, static_cast<int>(bounds.x + 14.0f), static_cast<int>(bounds.y + 12.0f), 15, kMuted);
    UITheme::drawText(value.c_str(), static_cast<int>(bounds.x + 14.0f), static_cast<int>(bounds.y + 34.0f), 25, kText);
}

void drawDashboard(const AccountSummary& account, const DashboardData& dashboard,
                   const Rectangle& content, int monthOffset) {
    const float x = content.x + 28.0f;
    const float width = content.width - 56.0f;
    drawText(account.role == UserRole::ADMIN ? "Welcome, administrator" : "Welcome back",
             x, content.y + 28.0f, 28.0f, kText);
    drawText(TextFormat("Signed in as %s  |  %s", account.username.c_str(),
                        account.role == UserRole::ADMIN ? "Admin" : "Learner"),
             x + 2.0f, content.y + 69.0f, 16.0f, kMuted);

    if (!dashboard.loaded) {
        UITheme::drawStatusBanner({x, content.y + 104.0f, width, 42.0f},
            dashboard.message.empty() ? "Dashboard data could not be loaded." : dashboard.message, true);
        return;
    }

    if (account.role == UserRole::ADMIN) {
        const std::size_t total = dashboard.resources.vocabulary + dashboard.resources.grammar + dashboard.resources.kanji;
        const std::array<std::pair<const char*, std::size_t>, 6> metrics = {{
            {"Total resources", total}, {"Vocabulary", dashboard.resources.vocabulary},
            {"Grammar", dashboard.resources.grammar}, {"Kanji", dashboard.resources.kanji},
            {"Accounts", dashboard.accountCount}, {"Questions", dashboard.questionCount}
        }};
        const int columns = width >= 720.0f ? 3 : 2;
        const float gap = 14.0f;
        const float cardWidth = (width - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);
        const float cardHeight = 90.0f;
        for (std::size_t i = 0; i < metrics.size(); ++i) {
            const int row = static_cast<int>(i) / columns;
            const int column = static_cast<int>(i) % columns;
            const Rectangle card{x + column * (cardWidth + gap), content.y + 128.0f + row * (cardHeight + gap), cardWidth, cardHeight};
            drawMetricCard(card, metrics[i].first, std::to_string(metrics[i].second));
        }
        drawText("Overview from the current library and account data.", x + 2.0f,
                 content.y + 128.0f + 2.0f * (cardHeight + gap) + 10.0f, 15.0f, kMuted);
        return;
    }

    const bool compact = content.height < 500.0f;
    const float summaryY = content.y + (compact ? 98.0f : 105.0f);
    const float summaryGap = 12.0f;
    const float summaryWidth = (width - summaryGap * 2.0f) / 3.0f;
    const float summaryHeight = compact ? 60.0f : 70.0f;
    drawMetricCard({x, summaryY, summaryWidth, summaryHeight}, "Favorites", std::to_string(dashboard.favoriteCount));
    drawMetricCard({x + summaryWidth + summaryGap, summaryY, summaryWidth, summaryHeight},
                   "Submitted quizzes", std::to_string(dashboard.quizAttemptCount));
    drawMetricCard({x + (summaryWidth + summaryGap) * 2.0f, summaryY, summaryWidth, summaryHeight},
                   "Quiz streak (UTC)", std::to_string(dashboard.learningStreak) +
                       (dashboard.learningStreak == 1 ? " day" : " days"));

    const Rectangle panel{x, content.y + (compact ? 166.0f : 195.0f), width,
                          content.height - (compact ? 186.0f : 215.0f)};
    DrawRectangleRounded(panel, 0.04f, 8, Color{255, 252, 253, 255});
    DrawRectangleRoundedLinesEx(panel, 0.04f, 8, 1.0f, kBorder);
    const CalendarMonth month = currentUtcMonth(monthOffset);
    static constexpr const char* monthNames[] = {"January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"};
    UITheme::drawText("Quiz activity calendar (UTC)", static_cast<int>(panel.x + 18.0f),
                      static_cast<int>(panel.y + 18.0f), 20, kText);
    const std::string monthLabel = std::string(monthNames[month.month - 1]) + " " + std::to_string(month.year);
    UITheme::drawText(monthLabel.c_str(), static_cast<int>(panel.x + 18.0f),
                      static_cast<int>(panel.y + 52.0f), 16, kMuted);
    UITheme::drawButton(calendarPreviousButton(panel), "<");
    UITheme::drawButton(calendarNextButton(panel), ">", false, monthOffset < 0);

    const float gridX = panel.x + 16.0f;
    const float gridY = panel.y + (compact ? 74.0f : 91.0f);
    const float gridWidth = panel.width - 32.0f;
    const float cellWidth = gridWidth / 7.0f;
    const float cellHeight = compact ? std::max(18.0f, (panel.height - 122.0f) / 6.0f)
                                     : std::clamp((panel.height - 145.0f) / 6.0f, 29.0f, 38.0f);
    static constexpr const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    for (int column = 0; column < 7; ++column)
        UITheme::drawText(weekdays[column], static_cast<int>(gridX + column * cellWidth + 5.0f),
                          static_cast<int>(gridY), compact ? 12 : 14, kMuted);

    std::unordered_set<std::string> activityDates(dashboard.quizActivityDatesUtc.begin(),
                                                   dashboard.quizActivityDatesUtc.end());
    const std::time_t now = std::time(nullptr);
    const std::tm* utc = std::gmtime(&now);
    const std::string today = utc ? TextFormat("%04d-%02d-%02d", utc->tm_year + 1900,
        utc->tm_mon + 1, utc->tm_mday) : "";
    const int firstWeekday = weekdaySundayFirst(month.year, month.month, 1);
    for (int day = 1; day <= daysInMonth(month.year, month.month); ++day) {
        const int cellIndex = firstWeekday + day - 1;
        const int row = cellIndex / 7;
        const int column = cellIndex % 7;
        const float cellX = gridX + column * cellWidth + 2.0f;
        const float cellY = gridY + (compact ? 17.0f : 22.0f) + row * cellHeight;
        char date[11]{};
        std::snprintf(date, sizeof(date), "%04d-%02d-%02d", month.year, month.month, day);
        const bool active = activityDates.count(date) != 0;
        const bool isToday = today == date;
        const Rectangle cell{cellX, cellY, cellWidth - 4.0f, std::max(16.0f, cellHeight - 3.0f)};
        if (active) DrawRectangleRounded(cell, 0.16f, 5, Color{255, 226, 236, 255});
        if (isToday) DrawRectangleRoundedLinesEx(cell, 0.16f, 5, 1.5f, UITheme::primary);
        const std::string dayText = std::to_string(day);
        const int fontSize = compact ? 14 : 16;
        const int dayWidth = UITheme::measureText(dayText.c_str(), fontSize);
        UITheme::drawText(dayText.c_str(), static_cast<int>(cell.x + (cell.width - dayWidth) * 0.5f),
                          static_cast<int>(cell.y + (cell.height - fontSize) * 0.5f), fontSize, kText);
    }
    const bool hasCurrentMonthActivity = std::any_of(activityDates.begin(), activityDates.end(), [&](const std::string& date) {
        return date.size() >= 7 && date.substr(0, 4) == std::to_string(month.year) &&
               date.substr(5, 2) == (month.month < 10 ? "0" : "") + std::to_string(month.month);
    });
    UITheme::drawWrapped(hasCurrentMonthActivity ? "Highlighted days have a submitted Quiz attempt."
                                                  : "No submitted Quiz attempts recorded for this month.",
                          static_cast<int>(panel.x + 18.0f), static_cast<int>(panel.y + panel.height - 30.0f),
                          static_cast<int>(panel.width - 36.0f), 14, kMuted, 2);
}
}

const std::vector<std::string>& MainScreen::sections(UserRole role) {
    return role == UserRole::ADMIN ? kAdminSections : kLearnerSections;
}

const std::string& MainScreen::sectionDescription(UserRole role, std::size_t section) {
    const auto& descriptions = role == UserRole::ADMIN ? kAdminDescriptions : kLearnerDescriptions;
    return descriptions[std::min(section, descriptions.size() - 1)];
}

MainScreenEvent MainScreen::update(UserRole role, std::size_t selectedSection) {
    if (clicked(closeBounds())) return {MainScreenEvent::Type::REQUEST_CLOSE, 0};
    if (clicked(logoutBounds())) return {MainScreenEvent::Type::LOGOUT, 0};
    if (role == UserRole::LEARNER && selectedSection == 0) {
        const Rectangle panel = UiLayout::content();
        const float x = panel.x + 28.0f;
        const bool compact = panel.height < 500.0f;
        const Rectangle calendar{x, panel.y + (compact ? 166.0f : 195.0f), panel.width - 56.0f,
                                 panel.height - (compact ? 186.0f : 215.0f)};
        if (clicked(calendarPreviousButton(calendar))) --calendarMonthOffset_;
        else if (calendarMonthOffset_ < 0 && clicked(calendarNextButton(calendar))) ++calendarMonthOffset_;
    }
    const auto& items = sections(role);
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (clicked(sectionBounds(i, items.size()))) return {MainScreenEvent::Type::SELECT_SECTION, i};
    }
    return {};
}

void MainScreen::draw(const AccountSummary& account, std::size_t selectedSection,
                      const DashboardData& dashboard) const {
    const float sidebarWidth = UiLayout::sidebarWidth();
    ClearBackground(kBackground);
    DrawRectangle(0, 0, static_cast<int>(sidebarWidth), GetScreenHeight(), kSidebar);
    DrawLine(static_cast<int>(sidebarWidth), 0, static_cast<int>(sidebarWidth), GetScreenHeight(), kBorder);
    drawText("PBL2", 24, 26, 23, kPrimaryHover);
    drawText("Japanese Learning", 24, 56, 16, kMuted);
    drawText("WORKSPACE", 26, 108, 15, kMuted);

    const auto& items = sections(account.role);
    for (std::size_t i = 0; i < items.size(); ++i) {
        const Rectangle bounds = sectionBounds(i, items.size());
        if (i == selectedSection) {
            DrawRectangleRounded(bounds, 0.22f, 8, kPrimary);
            UITheme::drawText(items[i].c_str(), static_cast<int>(bounds.x + 18),
                     static_cast<int>(bounds.y + (bounds.height - 18.0f) / 2.0f), 18, UITheme::strongText);
        } else {
            const bool hover = CheckCollisionPointRec(GetMousePosition(), bounds);
            if (hover) DrawRectangleRounded(bounds, 0.22f, 8, Color{255, 232, 240, 255});
            UITheme::drawText(items[i].c_str(), static_cast<int>(bounds.x + 18),
                     static_cast<int>(bounds.y + (bounds.height - 18.0f) / 2.0f), 18, kText);
        }
    }

    DrawLine(16, GetScreenHeight() - 120, static_cast<int>(sidebarWidth) - 16,
             GetScreenHeight() - 120, kBorder);
    BeginScissorMode(20, GetScreenHeight() - 116, static_cast<int>(sidebarWidth) - 40, 20);
    UITheme::drawText(account.username.c_str(), 20, GetScreenHeight() - 112, 16, kText);
    EndScissorMode();
    drawText(account.role == UserRole::ADMIN ? "Admin" : "Learner",
             20, GetScreenHeight() - 86, 15, kMuted);
    const Rectangle logout = logoutBounds();
    DrawRectangleRounded(logout, 0.24f, 8,
                         CheckCollisionPointRec(GetMousePosition(), logout) ? Color{255, 232, 240, 255} : kCard);
    DrawRectangleRoundedLinesEx(logout, 0.24f, 8, 1.0f, kBorder);
    const int signOutWidth = UITheme::measureText("Sign out", 18);
    const int signOutX = static_cast<int>(logout.x + (logout.width - signOutWidth) * 0.5f);
    const int signOutY = static_cast<int>(logout.y + (logout.height - 18.0f) * 0.5f);
    drawText("Sign out", static_cast<float>(signOutX), static_cast<float>(signOutY), 18, kPrimaryHover);

    DrawRectangle(static_cast<int>(sidebarWidth), 0, GetScreenWidth() - static_cast<int>(sidebarWidth),
                  94, kCard);
    DrawLine(static_cast<int>(sidebarWidth), 94, GetScreenWidth(), 94, kBorder);
    const std::string title = items[std::min(selectedSection, items.size() - 1)];
    drawText(title.c_str(), static_cast<int>(UiLayout::content().x), 25, 30, kText);
    const std::string roleText = account.role == UserRole::ADMIN ? "Admin" : "Learner";
    drawText(TextFormat("%s  |  %s", account.username.c_str(), roleText.c_str()), static_cast<int>(UiLayout::content().x), 61, 15, kMuted);
    drawButton(closeBounds(), "Close");

    const Rectangle content = UiLayout::content();
    DrawRectangleRounded(content, 0.035f, 12, kCard);
    DrawRectangleRoundedLinesEx(content, 0.035f, 12, 1.0f, kBorder);
    if (selectedSection == 0) drawDashboard(account, dashboard, content, calendarMonthOffset_);
}

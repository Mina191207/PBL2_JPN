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

Rectangle sectionBounds(std::size_t index, std::size_t count) {
    const float top = std::clamp(static_cast<float>(GetScreenHeight()) * 0.19f, 132.0f, 146.0f);
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
Rectangle logoutBounds(UserRole role) {
    return {14.0f, static_cast<float>(GetScreenHeight()) - 64.0f,
            role == UserRole::LEARNER ? 72.0f : UiLayout::sidebarWidth() - 28.0f,
            role == UserRole::LEARNER ? 48.0f : 42.0f};
}
Rectangle changePasswordBounds() {
    return {94.0f, static_cast<float>(GetScreenHeight()) - 64.0f, UiLayout::sidebarWidth() - 108.0f, 48.0f};
}
Rectangle closeBounds() {
    return {static_cast<float>(GetScreenWidth() - 176), 28.0f, 144.0f, 46.0f};
}
void drawButton(Rectangle bounds, const char* label, bool focused) {
    const bool hover = CheckCollisionPointRec(GetMousePosition(), bounds);
    DrawRectangleRounded(bounds, 0.24f, 8, hover ? Color{255, 232, 240, 255} : kCard);
    DrawRectangleRoundedLinesEx(bounds, 0.24f, 8, 1.0f, kBorder);
    UITheme::drawText(label, static_cast<int>(bounds.x + 18), static_cast<int>(bounds.y + 13), 17, kText);
    if (focused) UITheme::drawFocus(bounds);
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
                   const Rectangle& content, int monthOffset, bool previousFocused, bool nextFocused) {
    const float x = content.x + 28.0f;
    const float width = content.width - 56.0f;
    drawText(account.role == UserRole::ADMIN ? "Welcome, administrator" : "Welcome back",
             x, content.y + 28.0f, 28.0f, kText);
    drawText(TextFormat("Signed in as %s  |  %s", account.username.c_str(),
                        account.role == UserRole::ADMIN ? "Admin" : "Learner"),
             x + 2.0f, content.y + 69.0f, 16.0f, kMuted);

    if (!dashboard.loaded) {
        const std::string message = dashboard.message.empty() ? "Dashboard data could not be loaded." : dashboard.message;
        UITheme::drawStatusBanner({x, content.y + 104.0f, width, UITheme::bannerHeight(width, message)}, message, true);
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
        const std::size_t rows = (metrics.size() + columns - 1) / columns;
        const float cardsTop = content.y + 110.0f;
        const auto overview = UITheme::wrap("Overview from the current library and account data.", width - 4.0f, 15);
        const float cardHeight = std::min(90.0f,
            (content.height - 110.0f - (rows - 1) * gap - overview.height() - 22.0f) / rows);
        for (std::size_t i = 0; i < metrics.size(); ++i) {
            const int row = static_cast<int>(i) / columns;
            const int column = static_cast<int>(i) % columns;
            const Rectangle card{x + column * (cardWidth + gap), cardsTop + row * (cardHeight + gap), cardWidth, cardHeight};
            drawMetricCard(card, metrics[i].first, std::to_string(metrics[i].second));
        }
        overview.draw(static_cast<int>(x + 2.0f),
                      static_cast<int>(cardsTop + rows * cardHeight + (rows - 1) * gap + 10.0f), kMuted);
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
    UITheme::drawButton(calendarPreviousButton(panel), "<", false, true, false, previousFocused);
    UITheme::drawButton(calendarNextButton(panel), ">", false, monthOffset < 0, false, nextFocused);

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

bool MainScreen::tab(UserRole role, Section section) {
    std::vector<int> order;
    for (std::size_t i = 0; i < menu(role).size(); ++i) order.push_back(static_cast<int>(i));
    order.push_back(SignOut);
    if (role == UserRole::LEARNER) order.push_back(ChangePassword);
    order.push_back(Close);
    if (role == UserRole::LEARNER && section == Section::Dashboard) {
        order.push_back(PreviousMonth);
        if (calendarMonthOffset_ < 0) order.push_back(NextMonth);
    }
    return focus_.move(order, false);
}

MainScreenEvent MainScreen::update(UserRole role, Section selectedSection) {
    if (focus_.activate(Close, closeBounds())) return {MainScreenEvent::Type::REQUEST_CLOSE};
    if (focus_.activate(SignOut, logoutBounds(role))) return {MainScreenEvent::Type::LOGOUT};
    if (role == UserRole::LEARNER && focus_.activate(ChangePassword, changePasswordBounds()))
        return {MainScreenEvent::Type::CHANGE_PASSWORD};
    if (role == UserRole::LEARNER && selectedSection == Section::Dashboard) {
        const Rectangle panel = UiLayout::content();
        const float x = panel.x + 28.0f;
        const bool compact = panel.height < 500.0f;
        const Rectangle calendar{x, panel.y + (compact ? 166.0f : 195.0f), panel.width - 56.0f,
                                 panel.height - (compact ? 186.0f : 215.0f)};
        if (focus_.activate(PreviousMonth, calendarPreviousButton(calendar))) --calendarMonthOffset_;
        else if (focus_.activate(NextMonth, calendarNextButton(calendar), calendarMonthOffset_ < 0)) {
            ++calendarMonthOffset_;
            if (calendarMonthOffset_ == 0) focus_.id = PreviousMonth;
        }
    }
    const auto& items = menu(role);
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (focus_.activate(static_cast<int>(i), sectionBounds(i, items.size())))
            return {MainScreenEvent::Type::SELECT_SECTION, items.begin()[i].section};
    }
    return {};
}

void MainScreen::draw(const AccountSummary& account, Section selectedSection,
                      const DashboardData& dashboard, bool showFocus) const {
    const float sidebarWidth = UiLayout::sidebarWidth();
    ClearBackground(kBackground);
    DrawRectangle(0, 0, static_cast<int>(sidebarWidth), GetScreenHeight(), kSidebar);
    DrawLine(static_cast<int>(sidebarWidth), 0, static_cast<int>(sidebarWidth), GetScreenHeight(), kBorder);
    drawText("PBL2", 24, 26, 23, kPrimaryHover);
    drawText("Japanese Learning", 24, 56, 16, kMuted);
    drawText("WORKSPACE", 26, 108, 15, kMuted);

    const auto& items = menu(account.role);
    for (std::size_t i = 0; i < items.size(); ++i) {
        const Rectangle bounds = sectionBounds(i, items.size());
        if (items.begin()[i].section == selectedSection) {
            DrawRectangleRounded(bounds, 0.22f, 8, kPrimary);
            UITheme::drawText(items.begin()[i].label, static_cast<int>(bounds.x + 18),
                     static_cast<int>(bounds.y + (bounds.height - 18.0f) / 2.0f), 18, UITheme::strongText);
        } else {
            const bool hover = CheckCollisionPointRec(GetMousePosition(), bounds);
            if (hover) DrawRectangleRounded(bounds, 0.22f, 8, Color{255, 232, 240, 255});
            UITheme::drawText(items.begin()[i].label, static_cast<int>(bounds.x + 18),
                     static_cast<int>(bounds.y + (bounds.height - 18.0f) / 2.0f), 18, kText);
        }
        if (showFocus && focus_.on(static_cast<int>(i))) UITheme::drawFocus(bounds);
    }

    DrawLine(16, GetScreenHeight() - 120, static_cast<int>(sidebarWidth) - 16,
             GetScreenHeight() - 120, kBorder);
    BeginScissorMode(20, GetScreenHeight() - 116, static_cast<int>(sidebarWidth) - 40, 20);
    UITheme::drawText(account.username.c_str(), 20, GetScreenHeight() - 112, 16, kText);
    EndScissorMode();
    drawText(account.role == UserRole::ADMIN ? "Admin" : "Learner",
             20, GetScreenHeight() - 86, 15, kMuted);
    const Rectangle logout = logoutBounds(account.role);
    DrawRectangleRounded(logout, 0.24f, 8,
                         CheckCollisionPointRec(GetMousePosition(), logout) ? Color{255, 232, 240, 255} : kCard);
    DrawRectangleRoundedLinesEx(logout, 0.24f, 8, 1.0f, kBorder);
    const int signOutWidth = UITheme::measureText("Sign out", 18);
    const int signOutX = static_cast<int>(logout.x + (logout.width - signOutWidth) * 0.5f);
    const int signOutY = static_cast<int>(logout.y + (logout.height - 18.0f) * 0.5f);
    drawText("Sign out", static_cast<float>(signOutX), static_cast<float>(signOutY), 18, kPrimaryHover);
    if (showFocus && focus_.on(SignOut)) UITheme::drawFocus(logout);
    if (account.role == UserRole::LEARNER)
        UITheme::drawButton(changePasswordBounds(), "Change password", false, true, false,
                            showFocus && focus_.on(ChangePassword));

    DrawRectangle(static_cast<int>(sidebarWidth), 0, GetScreenWidth() - static_cast<int>(sidebarWidth),
                  94, kCard);
    DrawLine(static_cast<int>(sidebarWidth), 94, GetScreenWidth(), 94, kBorder);
    const char* title = sectionTitle(account.role, selectedSection);
    drawText(title, static_cast<int>(UiLayout::content().x), 25, 30, kText);
    const std::string roleText = account.role == UserRole::ADMIN ? "Admin" : "Learner";
    drawText(TextFormat("%s  |  %s", account.username.c_str(), roleText.c_str()), static_cast<int>(UiLayout::content().x), 61, 15, kMuted);
    drawButton(closeBounds(), "Close", showFocus && focus_.on(Close));

    const Rectangle content = UiLayout::content();
    DrawRectangleRounded(content, 0.035f, 12, kCard);
    DrawRectangleRoundedLinesEx(content, 0.035f, 12, 1.0f, kBorder);
    if (selectedSection == Section::Dashboard)
        drawDashboard(account, dashboard, content, calendarMonthOffset_,
                      showFocus && focus_.on(PreviousMonth), showFocus && focus_.on(NextMonth));
}

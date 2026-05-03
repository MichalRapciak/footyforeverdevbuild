#include "GameCalendar.h"

// --- GameDate Formatting ---
std::string GameDate::toString() const {
    const char* months[] = { "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    if (month >= 1 && month <= 12) {
        return std::to_string(day) + " " + months[month] + " " + std::to_string(year);
    }
    return "Unknown Date";
}

// --- GameCalendar Implementation ---

GameCalendar::GameCalendar() {
    // Default start of a typical European football season
    m_currentDate = GameDate(1, 8, 2026);
}

GameCalendar::~GameCalendar() {}

void GameCalendar::setStartDate(int day, int month, int year) {
    m_currentDate = GameDate(day, month, year);
}

void GameCalendar::addEvent(const CalendarEvent& newEvent) {
    m_schedule[newEvent.date].push_back(newEvent);
}

void GameCalendar::clearAllEvents() {
    m_schedule.clear();
}

std::vector<CalendarEvent> GameCalendar::getEventsForDate(const GameDate& targetDate) const {
    auto it = m_schedule.find(targetDate);
    if (it != m_schedule.end()) {
        return it->second;
    }
    return {}; // Return empty vector if no events
}

std::vector<CalendarEvent> GameCalendar::getUpcomingUserEvents(int limit) const {
    std::vector<CalendarEvent> upcoming;

    // Because std::map is automatically sorted by the key (GameDate),
    // we can use lower_bound to instantly jump to 'today' and read forward!
    auto it = m_schedule.lower_bound(m_currentDate);

    while (it != m_schedule.end() && upcoming.size() < limit) {
        for (const auto& ev : it->second) {
            if (ev.isUserInvolved) {
                upcoming.push_back(ev);
                if (upcoming.size() >= limit) break;
            }
        }
        ++it;
    }
    return upcoming;
}

// ==========================================
// --- THE TIME MACHINE ---
// ==========================================

void GameCalendar::advanceDay() {
    m_currentDate.day++;

    int daysInCurrentMonth = getDaysInMonth(m_currentDate.month, m_currentDate.year);

    if (m_currentDate.day > daysInCurrentMonth) {
        m_currentDate.day = 1;
        m_currentDate.month++;

        if (m_currentDate.month > 12) {
            m_currentDate.month = 1;
            m_currentDate.year++;
        }
    }
}

void GameCalendar::advanceToNextUserEvent() {
    // Keep advancing time (and simulating background games) until we hit a day 
    // where 'isUserInvolved' is true.
    bool foundUserEvent = false;

    while (!foundUserEvent) {
        advanceDay();

        std::vector<CalendarEvent> todaysEvents = getEventsForDate(m_currentDate);
        for (const auto& ev : todaysEvents) {
            if (ev.isUserInvolved) {
                foundUserEvent = true;
                break;
            }
        }

        // Safety check to prevent infinite loops if the schedule is empty
        if (m_currentDate.year > 2100) break;
    }
}

// --- Date Math Helpers ---

bool GameCalendar::isLeapYear(int year) const {
    if (year % 4 != 0) return false;
    if (year % 100 == 0 && year % 400 != 0) return false;
    return true;
}

int GameCalendar::getDaysInMonth(int month, int year) const {
    switch (month) {
    case 4: case 6: case 9: case 11:
        return 30;
    case 2:
        return isLeapYear(year) ? 29 : 28;
    default:
        return 31;
    }
}
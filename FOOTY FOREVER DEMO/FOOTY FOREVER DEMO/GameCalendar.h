#pragma once
#include <string>
#include <vector>
#include <map>
#include <tuple>

// ==========================================
// --- CORE DATE STRUCT ---
// ==========================================
struct GameDate {
    int day;
    int month;
    int year;

    GameDate(int d = 1, int m = 8, int y = 2026) : day(d), month(m), year(y) {}

    // Operator overloads make sorting and comparing dates incredibly easy
    bool operator==(const GameDate& other) const {
        return day == other.day && month == other.month && year == other.year;
    }
    bool operator<(const GameDate& other) const {
        if (year != other.year) return year < other.year;
        if (month != other.month) return month < other.month;
        return day < other.day;
    }
    bool operator>(const GameDate& other) const {
        return !(*this == other) && !(*this < other);
    }
    bool operator<=(const GameDate& other) const {
        return *this < other || *this == other;
    }

    // Helper to get a clean string: "15 Aug 2026"
    std::string toString() const;
};

// ==========================================
// --- CALENDAR EVENTS ---
// ==========================================
enum class EventType {
    MatchFixture,
    TransferWindowOpen,
    TransferWindowClose,
    TrainingCamp,
    SeasonEnd,
    Custom
};

struct CalendarEvent {
    std::string id;
    GameDate date;
    EventType type;
    std::string title;       // e.g., "Champions League Final"
    std::string referenceId; // E.g., The specific MatchID or CompID
    bool isUserInvolved = false; // Flags if the user must actively play/manage this event
};

// ==========================================
// --- THE CALENDAR SYSTEM ---
// ==========================================
class GameCalendar {
public:
    GameCalendar();
    ~GameCalendar();

    // Setup
    void setStartDate(int day, int month, int year);

    // Time Control
    void advanceDay();
    void advanceToNextUserEvent();

    // Event Management
    void addEvent(const CalendarEvent& newEvent);
    void clearAllEvents();

    // Retrieval
    GameDate getCurrentDate() const { return m_currentDate; }
    std::vector<CalendarEvent> getEventsForDate(const GameDate& targetDate) const;
    std::vector<CalendarEvent> getUpcomingUserEvents(int limit = 5) const;

private:
    GameDate m_currentDate;

    // Using a map sorted by GameDate makes retrieving today's schedule instant!
    // The key is the Date, the value is a list of all events happening that day.
    std::map<GameDate, std::vector<CalendarEvent>> m_schedule;

    int getDaysInMonth(int month, int year) const;
    bool isLeapYear(int year) const;
};
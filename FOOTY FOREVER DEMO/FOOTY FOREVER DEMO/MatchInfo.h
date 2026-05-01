#pragma once
#include <string>
#include <vector>
#include <map>

// ==========================================
// --- EVENT STRUCTS ---
// ==========================================
struct MatchGoalEvent {
    std::string scorerPlayerId; // Empty string if it was an own goal? (Or track OG)
    std::string assistPlayerId; // Empty string if unassisted
    std::string teamId;         // The team that scored
    int matchMinute;            // E.g., 45, 90
    bool isOwnGoal = false;
    bool isPenalty = false;
};

struct MatchCardEvent {
    std::string playerId;
    std::string teamId;
    int matchMinute;
    bool isRedCard; // True if straight red or second yellow
};

struct MatchAppearanceEvent {
    std::string playerId;
    std::string teamId;
    int minuteOn;  // 0 if started the match
    int minuteOff; // 90 if finished the match, or minute subbed/red carded
};

// ==========================================
// --- MATCH INFO CLASS ---
// ==========================================
class MatchInfo
{
public:
    MatchInfo();
    ~MatchInfo();

    // 1. Setup the Match
    void initMatch(const std::string& homeTeamId, const std::string& awayTeamId);

    // 2. Real-time Event Logging
    void recordGoal(const std::string& scorerId, const std::string& assistId, const std::string& teamId, int minute, bool isOwnGoal = false, bool isPenalty = false);
    void recordCard(const std::string& playerId, const std::string& teamId, int minute, bool isRed);

    // 3. Substitution / Appearance Tracking
    void recordAppearanceStart(const std::string& playerId, const std::string& teamId, int minute = 0);
    void recordAppearanceEnd(const std::string& playerId, int minute);

    // 4. Final Score / State
    int getHomeScore() const { return m_homeScore; }
    int getAwayScore() const { return m_awayScore; }
    const std::string& getHomeTeamId() const { return m_homeTeamId; }
    const std::string& getAwayTeamId() const { return m_awayTeamId; }

    const std::vector<MatchGoalEvent>& getGoals() const { return m_goals; }
    const std::vector<MatchCardEvent>& getCards() const { return m_cards; }
    const std::vector<MatchAppearanceEvent>& getAppearances() const { return m_appearances; }

private:
    std::string m_homeTeamId;
    std::string m_awayTeamId;

    int m_homeScore = 0;
    int m_awayScore = 0;

    std::vector<MatchGoalEvent> m_goals;
    std::vector<MatchCardEvent> m_cards;
    std::vector<MatchAppearanceEvent> m_appearances;
};
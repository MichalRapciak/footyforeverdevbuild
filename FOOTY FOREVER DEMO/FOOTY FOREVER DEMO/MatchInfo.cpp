#include "MatchInfo.h"
#include <algorithm>

MatchInfo::MatchInfo() {
}

MatchInfo::~MatchInfo() {
}

void MatchInfo::initMatch(const std::string& homeTeamId, const std::string& awayTeamId) {
    m_homeTeamId = homeTeamId;
    m_awayTeamId = awayTeamId;
    m_homeScore = 0;
    m_awayScore = 0;
    m_goals.clear();
    m_cards.clear();
    m_appearances.clear();
}

void MatchInfo::recordGoal(const std::string& scorerId, const std::string& assistId, const std::string& teamId, int minute, bool isOwnGoal, bool isPenalty) {

    // Add the event to our log
    MatchGoalEvent newGoal;
    newGoal.scorerPlayerId = scorerId;
    newGoal.assistPlayerId = assistId;
    newGoal.teamId = teamId;
    newGoal.matchMinute = minute;
    newGoal.isOwnGoal = isOwnGoal;
    newGoal.isPenalty = isPenalty;
    m_goals.push_back(newGoal);

    // Keep the internal scoreboard exactly synced
    if (teamId == m_homeTeamId) {
        m_homeScore++;
    }
    else if (teamId == m_awayTeamId) {
        m_awayScore++;
    }
}

void MatchInfo::recordCard(const std::string& playerId, const std::string& teamId, int minute, bool isRed) {
    MatchCardEvent newCard;
    newCard.playerId = playerId;
    newCard.teamId = teamId;
    newCard.matchMinute = minute;
    newCard.isRedCard = isRed;
    m_cards.push_back(newCard);

    // If it's a red card, their appearance ends here!
    if (isRed) {
        recordAppearanceEnd(playerId, minute);
    }
}

void MatchInfo::recordAppearanceStart(const std::string& playerId, const std::string& teamId, int minute) {
    // Check if they are already recorded (just in case)
    for (const auto& app : m_appearances) {
        if (app.playerId == playerId) return;
    }

    MatchAppearanceEvent newApp;
    newApp.playerId = playerId;
    newApp.teamId = teamId;
    newApp.minuteOn = minute;
    newApp.minuteOff = 90; // Default to full match length, will update if subbed/red carded
    m_appearances.push_back(newApp);
}

void MatchInfo::recordAppearanceEnd(const std::string& playerId, int minute) {
    for (auto& app : m_appearances) {
        if (app.playerId == playerId) {
            app.minuteOff = minute;
            break;
        }
    }
}

void MatchInfo::recordPass(const std::string& playerId, bool completed, bool isKeyPass) {
    if (playerId.empty()) return;
    m_playerStats[playerId].passesAttempted++;
    if (completed) m_playerStats[playerId].passesCompleted++;
    if (isKeyPass) m_playerStats[playerId].keyPasses++;
}

void MatchInfo::recordShot(const std::string& playerId, bool onTarget) {
    if (playerId.empty()) return;
    m_playerStats[playerId].shots++;
    if (onTarget) m_playerStats[playerId].shotsOnTarget++;
}

void MatchInfo::recordTackle(const std::string& playerId, bool won) {
    if (playerId.empty()) return;
    m_playerStats[playerId].tacklesAttempted++;
    if (won) m_playerStats[playerId].tacklesWon++;
}

void MatchInfo::recordInterception(const std::string& playerId) {
    if (!playerId.empty()) m_playerStats[playerId].interceptions++;
}

void MatchInfo::recordClearance(const std::string& playerId) {
    if (!playerId.empty()) m_playerStats[playerId].clearances++;
}

void MatchInfo::recordSave(const std::string& playerId) {
    if (!playerId.empty()) m_playerStats[playerId].saves++;
}

void MatchInfo::recordFoul(const std::string& playerId) {
    if (!playerId.empty()) m_playerStats[playerId].foulsCommitted++;
}

void MatchInfo::recordDribble(const std::string& playerId, bool won) {
    if (playerId.empty()) return;
    m_playerStats[playerId].dribblesAttempted++;
    if (won) m_playerStats[playerId].dribblesCompleted++;
}
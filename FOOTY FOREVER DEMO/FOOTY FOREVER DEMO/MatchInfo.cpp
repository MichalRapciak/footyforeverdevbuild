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
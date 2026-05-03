#include "MatchInfo.h"
#include "PositionRole.h"
#include "Player.h"
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

    m_goals.push_back({scorerId, assistId, teamId, minute, isOwnGoal, isPenalty});

    // 2. Update the Global Match Score
    if (teamId == m_homeTeamId) {
        m_homeScore++;
    } else if (teamId == m_awayTeamId) {
        m_awayScore++;
    }

    // ==========================================
    // --- THE FIX: INDIVIDUAL PLAYER STATS ---
    // ==========================================
    // We don't credit own goals to a striker's career tally!
    if (!isOwnGoal && !scorerId.empty()) {
        m_playerStats[scorerId].goals++;
    }
    
    if (!assistId.empty()) {
        m_playerStats[assistId].assists++;
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

void MatchInfo::recordInjury(const std::string& playerId, const std::string& teamId, int minute, InjurySeverity severity, const std::string& injuryName, int durationDays) {
    if (playerId.empty()) return;

    m_injuries.push_back({ playerId, teamId, minute, severity, injuryName, durationDays });
}

void MatchInfo::calculateFinalRatings(const std::vector<Player*>& allPlayers) {

    for (Player* p : allPlayers) {
        std::string pId = p->getId();

        // Skip players who never touched the ball or recorded a stat
        if (m_playerStats.find(pId) == m_playerStats.end()) continue;

        PlayerMatchStats& stats = m_playerStats[pId];

        // 1. Calculate Total Minutes Played
        int minsPlayed = 0;
        for (const auto& app : m_appearances) {
            if (app.playerId == pId) {
                minsPlayed += (app.minuteOff - app.minuteOn);
            }
        }

        // If they played less than 15 minutes and did nothing special, give them a blank/average rating
        if (minsPlayed < 15 && stats.goals == 0 && stats.assists == 0) {
            stats.matchRating =5.5f;
            continue;
        }

        float rating = 6.0f; // The Baseline

        PositionRole role = p->getPositionRole();
        bool isDefender = (role == PositionRole::CenterBack || role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);
        bool isMid = (role == PositionRole::CenterMid || role == PositionRole::DefensiveMid || role == PositionRole::AttackingMid || role == PositionRole::LeftMid || role == PositionRole::RightMid);
        bool isGK = (role == PositionRole::Goalkeeper);

        // Figure out how many goals they conceded while on the pitch
        bool isHomeTeam = (p->getTeam() == Team::Home);
        int goalsConceded = isHomeTeam ? m_awayScore : m_homeScore;

        // ==========================================
        // --- A. ATTACKING CONTRIBUTIONS ---
        // ==========================================
        // Defenders get a massive bonus for scoring. Attackers get the standard +1.0.
        rating += (stats.goals * (isDefender ? 1.5f : 1.0f));
        rating += (stats.assists * 0.7f);
        rating += (stats.keyPasses * 0.2f);
        rating += (stats.shotsOnTarget * 0.1f);
        rating += (stats.dribblesCompleted * 0.1f);

        // ==========================================
        // --- B. PASSING EFFICIENCY ---
        // ==========================================
        if (stats.passesAttempted > 5) {
            float passPct = static_cast<float>(stats.passesCompleted) / stats.passesAttempted;
            if (passPct > 0.90f) rating += 0.5f;
            else if (passPct > 0.80f) rating += 0.2f;
            else if (passPct < 0.60f) rating -= 0.4f;
        }

        // ==========================================
        // --- C. DEFENSIVE WORK ---
        // ==========================================
        rating += (stats.tacklesWon * 0.2f);
        rating += (stats.interceptions * 0.15f);
        rating += (stats.clearances * 0.05f);

        if (isGK) {
            rating += (stats.saves * 0.3f);
        }

        // ==========================================
        // --- D. CLEAN SHEETS & CONCEDING ---
        // ==========================================
        if (minsPlayed >= 60) {
            if (goalsConceded == 0) {
                if (isGK || isDefender) rating += 1.0f; // Massive boost for a perfect defense
                else if (isMid) rating += 0.3f;
            }
            else {
                if (isGK || isDefender) rating -= (goalsConceded * 0.3f);
            }
        }

        // ==========================================
        // --- E. PENALTIES & CARDS ---
        // ==========================================
        rating -= (stats.foulsCommitted * 0.1f);

        // Check if they got a card
        for (const auto& card : m_cards) {
            if (card.playerId == pId) {
                rating -= (card.isRedCard ? 1.5f : 0.5f);
            }
        }

        // Clamp the final score between 1.0 (Worst) and 10.0 (Perfect)
        stats.matchRating = std::clamp(rating, 1.0f, 10.0f);
    }
}
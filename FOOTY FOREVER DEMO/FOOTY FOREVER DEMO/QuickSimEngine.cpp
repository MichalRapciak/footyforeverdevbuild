#include "QuickSimEngine.h"
#include <iostream>
#include <cmath>
#include <algorithm>

MatchInfo QuickSimEngine::simulateMatch(GameDatabase& db, const std::string& homeTeamId, const std::string& awayTeamId) {
    MatchInfo result;
    result.initMatch(homeTeamId, awayTeamId); // Uses your actual setup function!

    TeamData* homeTeam = db.getTeam(homeTeamId);
    TeamData* awayTeam = db.getTeam(awayTeamId);

    if (!homeTeam || !awayTeam) return result;

    // ==========================================
    // PHASE 1: CALCULATE EFFECTIVE STRENGTHS
    // ==========================================
    TeamStrength homeStr = calculateBaseStrength(db, homeTeam);
    TeamStrength awayStr = calculateBaseStrength(db, awayTeam);

    applyTacticalMultipliers(homeStr, homeTeam->defaultTactics, homeTeam->teamChemistry);
    applyTacticalMultipliers(awayStr, awayTeam->defaultTactics, awayTeam->teamChemistry);

    // Home Advantage (+3% to all stats)
    homeStr.def *= 1.03f; homeStr.mid *= 1.03f; homeStr.att *= 1.03f; homeStr.gk *= 1.03f;

    // FOG Factor (Funny Old Game / RNG Chaos)
    // Applies a random -5% to +5% swing to the total performance
    float homeFog = 1.0f + (getRandomInt(-50, 50) / 1000.0f);
    float awayFog = 1.0f + (getRandomInt(-50, 50) / 1000.0f);

    homeStr.att *= homeFog; homeStr.def *= homeFog; homeStr.mid *= homeFog;
    awayStr.att *= awayFog; awayStr.def *= awayFog; awayStr.mid *= awayFog;

    // ==========================================
    // PHASE 2: GENERATE CHANCES
    // ==========================================
    int homeChances = 0, awayChances = 0;
    generateChances(homeStr, awayStr, homeChances, awayChances);

    // ==========================================
    // PHASE 3: RESOLVE CHANCES TO GOALS
    // ==========================================
    int homeGoals = 0, awayGoals = 0;

    // To score, Attacker's quality must beat GK's quality + RNG
    for (int i = 0; i < homeChances; ++i) {
        float shotQuality = homeStr.att + getRandomInt(0, 20);
        float saveQuality = awayStr.gk + getRandomInt(0, 25);
        if (shotQuality > saveQuality) homeGoals++;
    }

    for (int i = 0; i < awayChances; ++i) {
        float shotQuality = awayStr.att + getRandomInt(0, 20);
        float saveQuality = homeStr.gk + getRandomInt(0, 25);
        if (shotQuality > saveQuality) awayGoals++;
    }

    result.setScore(homeGoals, awayGoals);

    // ==========================================
    // PHASE 4: DISTRIBUTE EVENTS (WHO DID IT?)
    // ==========================================
    for (int i = 0; i < homeGoals; ++i) {
        std::string scorerId = pickPlayerForGoal(db, homeTeam);
        // 70% chance a goal has an assist
        std::string assistId = (getRandomInt(1, 100) <= 70) ? pickPlayerForAssist(db, homeTeam, scorerId) : "";
        result.recordGoal(scorerId, assistId, homeTeamId, getRandomInt(1, 90));
    }

    for (int i = 0; i < awayGoals; ++i) {
        std::string scorerId = pickPlayerForGoal(db, awayTeam);
        std::string assistId = (getRandomInt(1, 100) <= 70) ? pickPlayerForAssist(db, awayTeam, scorerId) : "";
        result.recordGoal(scorerId, assistId, awayTeamId, getRandomInt(1, 90));
    }

    // Process Cards (Average 1-4 cards a game)
    int totalCards = getRandomInt(1, 4);
    for (int i = 0; i < totalCards; ++i) {
        bool isHomeCard = getRandomInt(0, 1);
        TeamData* offendingTeam = isHomeCard ? homeTeam : awayTeam;
        std::string tId = isHomeCard ? homeTeamId : awayTeamId;
        std::string offenderId = pickPlayerForCard(db, offendingTeam);

        bool isRed = (getRandomInt(1, 100) <= 5); // 5% chance of a straight red
        result.recordCard(offenderId, tId, getRandomInt(1, 90), isRed);
    }

    // Process basic match ratings & appearances using your MatchInfo functions
    for (const auto& [slotId, pId] : homeTeam->defaultTactics.startingXI) {
        if (!pId.empty()) {
            result.recordAppearanceStart(pId, homeTeamId, 0); // Starter
            result.recordAppearanceEnd(pId, 90);
            result.getEditablePlayerStats()[pId].matchRating = static_cast<float>(getRandomInt(55, 95)) / 10.0f;
        }
    }
    for (const auto& [slotId, pId] : awayTeam->defaultTactics.startingXI) {
        if (!pId.empty()) {
            result.recordAppearanceStart(pId, awayTeamId, 0);
            result.recordAppearanceEnd(pId, 90);
            result.getEditablePlayerStats()[pId].matchRating = static_cast<float>(getRandomInt(55, 95)) / 10.0f;
        }
    }

    return result;
}

// ---------------------------------------------------------
// Helpers
// ---------------------------------------------------------

QuickSimEngine::TeamStrength QuickSimEngine::calculateBaseStrength(GameDatabase& db, TeamData* team) {
    TeamStrength ts;
    int defCount = 0, midCount = 0, attCount = 0, gkCount = 0;

    for (const auto& [slotId, pId] : team->defaultTactics.startingXI) {
        if (pId.empty()) continue;
        PlayerData* p = db.getPlayer(pId);
        if (!p) continue;

        // Ensure overall is up to date
        p->stats.calculateOverallRating(p->positionRole);
        float ovr = p->stats.overallRating;

        switch (p->positionRole) {
        case PositionRole::Goalkeeper:
            ts.gk += ovr; gkCount++; break;
        case PositionRole::LeftBack: case PositionRole::RightBack: case PositionRole::CenterBack:
        case PositionRole::LeftWingBack: case PositionRole::RightWingBack:
            ts.def += ovr; defCount++; break;
        case PositionRole::DefensiveMid: case PositionRole::CenterMid: case PositionRole::AttackingMid:
        case PositionRole::LeftMid: case PositionRole::RightMid:
            ts.mid += ovr; midCount++; break;
        case PositionRole::LeftWing: case PositionRole::RightWing: case PositionRole::CenterForward: case PositionRole::Striker:
            ts.att += ovr; attCount++; break;
        }
    }

    if (gkCount > 0) ts.gk /= gkCount;
    if (defCount > 0) ts.def /= defCount;
    if (midCount > 0) ts.mid /= midCount;
    if (attCount > 0) ts.att /= attCount;

    // Calculate bench strength
    int benchCount = 0;
    for (const auto& pId : team->defaultTactics.benchIds) {
        PlayerData* p = db.getPlayer(pId);
        if (p) { ts.bench += p->stats.overallRating; benchCount++; }
    }
    if (benchCount > 0) ts.bench /= benchCount;

    ts.overall = (ts.gk + ts.def + ts.mid + ts.att) / 4.0f;
    return ts;
}

void QuickSimEngine::applyTacticalMultipliers(TeamStrength& ts, const TeamTactics& tactics, float chemistry) {
    // 1. Chemistry Penalty/Bonus
    // 100 chem = 100% potential. 50 chem = players perform at 85% capacity.
    float chemModifier = 0.7f + ((chemistry / 100.0f) * 0.3f);
    ts.def *= chemModifier; ts.mid *= chemModifier; ts.att *= chemModifier; ts.gk *= chemModifier;

    // 2. Formation Weighting (All 11 Formations)
    const std::string& form = tactics.formationName;

    if (form == "4-4-2") {
        // The baseline. Perfectly balanced, no extreme modifiers.
    }
    else if (form == "4-3-3") {
        ts.att *= 1.05f; // Standard attacking boost
        ts.def *= 0.95f;
    }
    else if (form == "4-2-3-1") {
        ts.mid *= 1.05f; // Double pivot gives strong midfield control
        ts.att *= 0.95f;
    }
    else if (form == "4-1-4-1") {
        ts.def *= 1.05f; // Very solid defensively with a CDM block
        ts.mid *= 1.05f;
        ts.att *= 0.90f; // Can leave the lone striker isolated
    }
    else if (form == "4-1-2-1-2") { // Narrow Diamond
        ts.mid *= 1.08f; // Massive central dominance
        ts.att *= 0.97f;
        ts.def *= 0.95f; // Lacks wide defensive cover
    }
    else if (form == "4-2-4") {
        ts.att *= 1.15f; // All-out attack
        ts.mid *= 0.90f; // Empty midfield
        ts.def *= 0.95f;
    }
    else if (form == "3-4-3") {
        ts.att *= 1.10f; // Heavy forward presence
        ts.def *= 0.90f; // Exposed at the back
    }
    else if (form == "3-5-2") {
        ts.mid *= 1.10f; // Wins the possession battle
        ts.def *= 0.90f;
    }
    else if (form == "5-3-2") {
        ts.def *= 1.10f; // Solid back 5
        ts.att *= 1.05f; // Still has two strikers
        ts.mid *= 0.85f; // Concedes possession
    }
    else if (form == "5-2-3") {
        ts.def *= 1.10f; // Defensive counter-attack setup
        ts.mid *= 0.90f;
    }
    else if (form == "5-4-1") {
        ts.def *= 1.15f; // Park the bus
        ts.mid *= 1.00f;
        ts.att *= 0.85f; // Very little attacking threat
    }

    // 3. Tactical Sliders (0-100)
    // High defensive depth (+70) = Low Block -> better defense, worse attack
    if (tactics.defensiveDepth > 70) {
        ts.def *= 1.05f;
        ts.att *= 0.95f;
    }
    // Low defensive depth (<30) = High Line -> better attack, exposed defense
    else if (tactics.defensiveDepth < 30) {
        ts.def *= 0.95f;
        ts.att *= 1.05f;
    }

    // High pressing creates midfield turnovers/dominance
    if (tactics.pressingIntensity > 75) {
        ts.mid *= 1.05f;
    }
}

void QuickSimEngine::generateChances(const TeamStrength& home, const TeamStrength& away, int& homeChances, int& awayChances) {
    // 1. The Midfield Battle (Dictates Possession)
    float midDiff = home.mid - away.mid;

    // Base chances everyone gets
    homeChances = getRandomInt(2, 4);
    awayChances = getRandomInt(2, 4);

    if (midDiff > 5.0f) homeChances += getRandomInt(1, 3);
    else if (midDiff < -5.0f) awayChances += getRandomInt(1, 3);

    // 2. The Attack vs Defense Battle
    float homeAttDiff = home.att - away.def;
    float awayAttDiff = away.att - home.def;

    if (homeAttDiff > 10.0f) homeChances += getRandomInt(2, 4);
    else if (homeAttDiff > 3.0f) homeChances += getRandomInt(1, 2);
    else if (homeAttDiff < -5.0f) homeChances -= getRandomInt(0, 1);

    if (awayAttDiff > 10.0f) awayChances += getRandomInt(2, 4);
    else if (awayAttDiff > 3.0f) awayChances += getRandomInt(1, 2);
    else if (awayAttDiff < -5.0f) awayChances -= getRandomInt(0, 1);

    homeChances = static_cast<int>(clamp(static_cast<float>(homeChances), 0.0f, 12.0f));
    awayChances = static_cast<int>(clamp(static_cast<float>(awayChances), 0.0f, 12.0f));
}

std::string QuickSimEngine::pickPlayerForGoal(GameDatabase& db, TeamData* team) {
    std::vector<std::pair<std::string, float>> lottery;

    for (const auto& [slotId, pId] : team->defaultTactics.startingXI) {
        if (pId.empty()) continue;
        PlayerData* p = db.getPlayer(pId);
        if (!p) continue;

        float weight = p->stats.finishing + (p->stats.awareness * 0.5f);

        if (p->positionRole == PositionRole::Striker || p->positionRole == PositionRole::CenterForward) weight *= 3.0f;
        else if (p->positionRole == PositionRole::LeftWing || p->positionRole == PositionRole::RightWing) weight *= 2.0f;
        else if (p->positionRole == PositionRole::Goalkeeper) weight = 1.0f;

        lottery.push_back({ pId, weight });
    }

    if (lottery.empty()) return "";

    float totalWeight = 0.0f;
    for (auto& item : lottery) totalWeight += item.second;

    float randomVal = static_cast<float>(getRandomInt(0, static_cast<int>(totalWeight)));
    float currentSum = 0.0f;

    for (auto& item : lottery) {
        currentSum += item.second;
        if (randomVal <= currentSum) return item.first;
    }
    return lottery.front().first;
}

std::string QuickSimEngine::pickPlayerForAssist(GameDatabase& db, TeamData* team, const std::string& scorerId) {
    std::vector<std::pair<std::string, float>> lottery;

    for (const auto& [slotId, pId] : team->defaultTactics.startingXI) {
        if (pId.empty() || pId == scorerId) continue;

        PlayerData* p = db.getPlayer(pId);
        if (!p) continue;

        // Base assist potential
        float weight = p->stats.shortPassing + p->stats.awareness + (p->stats.longPassing * 0.5f);

        // --- POSITIONAL & PLAYSTYLE BIASES ---
        if (p->positionRole == PositionRole::AttackingMid || p->positionRole == PositionRole::CenterMid) {
            weight *= 2.5f; // Playmakers and central hubs get the most assists
        }
        else if (p->positionRole == PositionRole::LeftWing || p->positionRole == PositionRole::RightWing) {
            weight *= 2.0f; // Wingers naturally provide cutbacks and crosses
        }
        else if (p->positionRole == PositionRole::LeftBack || p->positionRole == PositionRole::RightBack ||
            p->positionRole == PositionRole::LeftWingBack || p->positionRole == PositionRole::RightWingBack) {

            // The Playstyle check for Wide Defenders
            switch (p->playstyle.type) {
            case PlaystyleType::TheCrosser:
                weight *= 1.8f; // Extremely high assist threat from wide areas
                break;
            case PlaystyleType::UpAndDown:
            case PlaystyleType::TheRoamerFB:
                weight *= 1.4f; // Gets forward often, moderate assist threat
                break;
            case PlaystyleType::DefensiveFB:
                weight *= 0.5f; // Stays back, very rarely assists
                break;
            default:
                weight *= 1.0f; // Baseline
                break;
            }
        }
        else if (p->positionRole == PositionRole::CenterBack) {
            // Give a tiny bump to ball-playing centerbacks
            if (p->playstyle.type == PlaystyleType::CalmAndCollected) weight *= 0.8f;
            else weight *= 0.3f; // Normal CBs rarely assist
        }

        lottery.push_back({ pId, weight });
    }

    if (lottery.empty()) return "";

    float totalWeight = 0.0f;
    for (auto& item : lottery) totalWeight += item.second;
    float randomVal = static_cast<float>(getRandomInt(0, static_cast<int>(totalWeight)));
    float currentSum = 0.0f;

    for (auto& item : lottery) {
        currentSum += item.second;
        if (randomVal <= currentSum) return item.first;
    }
    return lottery.front().first;
}

std::string QuickSimEngine::pickPlayerForCard(GameDatabase& db, TeamData* team) {
    std::vector<std::pair<std::string, float>> lottery;

    for (const auto& [slotId, pId] : team->defaultTactics.startingXI) {
        if (pId.empty()) continue;
        PlayerData* p = db.getPlayer(pId);
        if (!p) continue;

        float weight = p->stats.aggression + (100.0f - p->stats.blocking);

        if (p->positionRole == PositionRole::CenterBack || p->positionRole == PositionRole::DefensiveMid) weight *= 2.0f;

        lottery.push_back({ pId, weight });
    }

    if (lottery.empty()) return "";

    float totalWeight = 0.0f;
    for (auto& item : lottery) totalWeight += item.second;
    float randomVal = static_cast<float>(getRandomInt(0, static_cast<int>(totalWeight)));
    float currentSum = 0.0f;

    for (auto& item : lottery) {
        currentSum += item.second;
        if (randomVal <= currentSum) return item.first;
    }
    return lottery.front().first;
}

int QuickSimEngine::getRandomInt(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distr(min, max);
    return distr(gen);
}

float QuickSimEngine::clamp(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}
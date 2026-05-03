#pragma once
#include "GameDatabase.h"
#include "MatchInfo.h"
#include <random>
#include <string>
#include <vector>

class QuickSimEngine {
public:
    // The main entry point for simulating a game
    static MatchInfo simulateMatch(GameDatabase& db, const std::string& homeTeamId, const std::string& awayTeamId);

private:
    // A snapshot of a team's combat power on matchday
    struct TeamStrength {
        float gk = 0.0f;
        float def = 0.0f;
        float mid = 0.0f;
        float att = 0.0f;
        float bench = 0.0f;
        float overall = 0.0f;
    };

    // --- Core Phases ---
    static TeamStrength calculateBaseStrength(GameDatabase& db, TeamData* team);
    static void applyTacticalMultipliers(TeamStrength& ts, const TeamTactics& tactics, float chemistry);
    static void generateChances(const TeamStrength& home, const TeamStrength& away, int& homeChances, int& awayChances);

    // --- Event Distribution ---
    static std::string pickPlayerForGoal(GameDatabase& db, TeamData* team);
    static std::string pickPlayerForAssist(GameDatabase& db, TeamData* team, const std::string& scorerId);
    static std::string pickPlayerForCard(GameDatabase& db, TeamData* team);

    // --- Helpers ---
    static int getRandomInt(int min, int max);
    static float clamp(float val, float min, float max);
};
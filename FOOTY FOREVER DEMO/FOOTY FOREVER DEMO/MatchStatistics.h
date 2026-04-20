#pragma once
#include <vector>
#include <string>
#include <SFML/System.hpp>
#include "Team.h" // Wherever your Team::Home enum lives

struct TeamMatchStats {
    int goals = 0;
    std::vector<std::string> goalEvents; // Stores strings like "L. Messi 43'"

    float possessionTime = 0.f;

    int passesAttempted = 0;
    int passesCompleted = 0;

    int shotsOnTarget = 0;
    int shotsOffTarget = 0;

    int fouls = 0;

    // Helper Math
    float getPossessionPercent(float totalMatchPossession) const {
        if (totalMatchPossession <= 0.001f) return 50.0f;
        return (possessionTime / totalMatchPossession) * 100.0f;
    }

    float getPassCompletion() const {
        if (passesAttempted == 0) return 0.0f;
        float percent = (static_cast<float>(passesCompleted) / static_cast<float>(passesAttempted)) * 100.0f;
        return std::min(percent, 100.0f); // Hard cap at 100%
    }
};

class MatchStatistics {
public:
    TeamMatchStats home;
    TeamMatchStats away;

    void updatePossession(Team team, float dt) {
        if (team == Team::Home) home.possessionTime += dt;
        else away.possessionTime += dt;
    }

    void recordPassAttempt(Team team) {
        if (team == Team::Home) home.passesAttempted++;
        else away.passesAttempted++;
    }

    void recordPassComplete(Team team) {
        if (team == Team::Home) home.passesCompleted++;
        else away.passesCompleted++;
    }

    void recordShot(Team team, bool onTarget) {
        TeamMatchStats& stats = (team == Team::Home) ? home : away;
        if (onTarget) stats.shotsOnTarget++;
        else stats.shotsOffTarget++;
    }

    void recordFoul(Team team) {
        if (team == Team::Home) home.fouls++;
        else away.fouls++;
    }

    void recordGoal(Team team, const std::string& scorerName, int minute) {
        TeamMatchStats& stats = (team == Team::Home) ? home : away;
        stats.goals++;
        stats.goalEvents.push_back(scorerName + " " + std::to_string(minute) + "'");
    }

    float getTotalPossessionTime() const {
        return home.possessionTime + away.possessionTime;
    }

    void reset() {
        home = TeamMatchStats();
        away = TeamMatchStats();
    }
};
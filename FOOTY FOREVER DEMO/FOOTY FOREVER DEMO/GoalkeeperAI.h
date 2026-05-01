#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "NPCPlayer.h"
#include "MatchEnvironment.h"
#include "TeamAI.h"

class GoalkeeperAI {
public:
    static void handleGoalkeeping(NPCPlayer& npc, float dt, const TeamAI& teamAI, MatchEnvironment& env);
    static sf::Vector2f calculateGoaliePositioning(NPCPlayer& npc, sf::Vector2f goalCenter, MatchEnvironment& env);
    static bool shouldGoalieRush(NPCPlayer& npc, const TeamAI& teamAI, MatchEnvironment& env);
    static void attemptSave(NPCPlayer& npc, float dt, const TeamAI& teamAI, MatchEnvironment& env);
    static void triggerDive(NPCPlayer& npc, sf::Vector2f diveTarget, float jumpSpeed, float targetZ, const std::string& diveAnim);
    static void distributeBallAsGoalie(NPCPlayer& npc, const TeamAI& teamAI, MatchEnvironment& env);
};
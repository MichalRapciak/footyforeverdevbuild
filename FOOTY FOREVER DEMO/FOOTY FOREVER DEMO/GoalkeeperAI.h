#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "NPCPlayer.h"
#include "Ball.h"
#include "Pitch.h"
#include "TeamAI.h"
#include "SoundManager.h"

class GoalkeeperAI {
public:
    static void handleGoalkeeping(NPCPlayer& npc, Ball& ball, const Pitch& pitch, const std::vector<Player*>& team, const std::vector<Player*>& opposition, float dt, const TeamAI& teamAI, SoundManager& soundManager);
    static sf::Vector2f calculateGoaliePositioning(NPCPlayer& npc, sf::Vector2f ballPos, sf::Vector2f goalCenter, const Pitch& pitch);
    static bool shouldGoalieRush(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI);
    static void attemptSave(NPCPlayer& npc, Ball& ball, float dt, const TeamAI& teamAI);
    static void triggerDive(NPCPlayer& npc, sf::Vector2f diveTarget, float jumpSpeed, float targetZ, const std::string& diveAnim);
    static void distributeBallAsGoalie(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI, SoundManager& soundManager);
};
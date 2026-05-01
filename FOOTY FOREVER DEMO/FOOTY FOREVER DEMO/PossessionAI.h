#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "NPCPlayer.h"
#include "UserPlayer.h"
#include "MatchEnvironment.h"
#include "TeamAI.h"

enum class MatchState;

class PossessionAI {
public:
    static sf::Vector2f handlePossession(NPCPlayer& npc, UserPlayer* user, float dt, MatchState matchstate, const TeamAI& teamAI, MatchEnvironment& env);

    static Player* findBestPassOption(NPCPlayer& npc, UserPlayer* user, const TeamAI& teamAI, MatchEnvironment& env);
    static sf::Vector2f calculateDribbleDirection(NPCPlayer& npc, sf::Vector2f goalPos, const TeamAI& teamAI, MatchEnvironment& env);

    static void executePass(NPCPlayer& npc, Player* target, const TeamAI& teamAI, MatchEnvironment& env);
    static void executeShot(NPCPlayer& npc, sf::Vector2f goalPos, float dt, const TeamAI& teamAI, MatchEnvironment& env);
    static void executeThrowIn(NPCPlayer& npc, MatchEnvironment& env);

    // Kept Ball& since it's a pure math/physics check that doesn't need the environment!
    static void handleNPCJumpLogic(NPCPlayer& npc, Ball& ball);
    static bool tryNPCAerialStrike(NPCPlayer& npc, sf::Vector2f aimDir, bool isShot, MatchEnvironment& env);
    static void executeSetPiece(NPCPlayer& npc, MatchState state, const TeamAI& teamAI, MatchEnvironment& env);
};
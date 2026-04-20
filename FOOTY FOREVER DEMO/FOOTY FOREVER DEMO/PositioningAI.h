#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "NPCPlayer.h"
#include "Ball.h"
#include "Pitch.h"
#include "TeamAI.h"
#include "MatchReferee.h"

class SpatialGrid;

enum class AIUrgency {
    Recovery,      // Jogging back into formation (Low payoff)
    Pressing,      // Closing down the opponent (Medium payoff)
    AttackingRun,  // Making a run into space (High payoff)
    Critical       // Receiving a pass, loose ball, or emergency defending (Must sprint)
};

class PositioningAI {
public:
    static sf::Vector2f decideTargetPosition(NPCPlayer& npc, Ball& ball, const Pitch& pitch, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, Player* firstResponder, PositioningMask mask, const TeamAI& teamAI, const SpatialGrid& spatialGrid);

    static sf::Vector2f evaluateShapeAndSpace(NPCPlayer& npc, sf::Vector2f currentTarget, sf::Vector2f ballPos, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, const TeamAI& teamAI, const TacticalZone& zone, Ball& ball, const Pitch& pitch, const SpatialGrid& spatialGrid);

    static sf::Vector2f applyTacticalPositioning(NPCPlayer& npc, Ball& ball, sf::Vector2f homePos, sf::Vector2f ballPos, sf::Vector2f goalPos, TeamState state, TacticalZone zone, const Pitch& pitch, const std::vector<Player*>& team, const std::vector<Player*>& opposition, const TeamAI& teamAI, const SpatialGrid& spatialGrid);

    static bool evaluateSprintUrgency(NPCPlayer& npc, AIUrgency urgency, float distToTarget, float distToBall);
    static sf::Vector2f calculateInterceptionPoint(NPCPlayer& npc, Ball& ball);
    static sf::Vector2f calculateSeparation(NPCPlayer& npc, const std::vector<Player*>& team, const std::vector<Player*>& opponents, sf::Vector2f ballPos, const TeamAI& teamAI);

    static bool shouldEmergencyChase(NPCPlayer& npc, Player* firstResponder, float distToBall, const Pitch& pitch, Ball& ball, MatchState matchstate, const TeamAI& teamAI);
    static sf::Vector2f calculateMarkingPosition(NPCPlayer& npc, Player* threat, sf::Vector2f goalPos, const TacticalZone& zone, const TeamAI& teamAI);
    static sf::Vector2f clampToTacticalZone(sf::Vector2f target, sf::Vector2f homePos, const TacticalZone& zone, float distToBall, bool isHomeSide, const TeamAI& teamAI);
    static bool shouldRecoverPosition(const sf::Vector2f& npcPos, const sf::Vector2f& homePos, const TacticalZone& zone, float distToBall, bool isHomeSide, const TeamAI& teamAI);

    static Player* identifyTargetReceiver(Ball& ball, const std::vector<Player*>& team);
};
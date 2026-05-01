#pragma once
#include <SFML/Graphics.hpp>
#include "NPCPlayer.h"
#include "MatchEnvironment.h"
#include "MatchContext.h"
#include "TeamAI.h"

enum class AIUrgency {
    Recovery,      // Jogging back into formation (Low payoff)
    Pressing,      // Closing down the opponent (Medium payoff)
    AttackingRun,  // Making a run into space (High payoff)
    Critical       // Receiving a pass, loose ball, or emergency defending (Must sprint)
};

struct PositioningMask;

class PositioningAI {
public:
    static sf::Vector2f decideTargetPosition(NPCPlayer& npc, Player* firstResponder, PositioningMask mask, const TeamAI& teamAI, MatchEnvironment& env);

    static sf::Vector2f evaluateShapeAndSpace(NPCPlayer& npc, sf::Vector2f currentTarget, const TeamAI& teamAI, const TacticalZone& zone, MatchEnvironment& env);

    static sf::Vector2f applyTacticalPositioning(NPCPlayer& npc, sf::Vector2f homePos, sf::Vector2f goalPos, TacticalZone zone, const TeamAI& teamAI, MatchEnvironment& env);

    static bool evaluateSprintUrgency(NPCPlayer& npc, AIUrgency urgency, float distToTarget, float distToBall, TeamState state);
    static sf::Vector2f calculateInterceptionPoint(NPCPlayer& npc, MatchEnvironment& env);
    static sf::Vector2f calculateSeparation(NPCPlayer& npc, const TeamAI& teamAI, MatchEnvironment& env);

    static bool shouldEmergencyChase(NPCPlayer& npc, Player* firstResponder, float distToBall, const TeamAI& teamAI, MatchEnvironment& env);
    static sf::Vector2f calculateMarkingPosition(NPCPlayer& npc, Player* threat, sf::Vector2f goalPos, const TacticalZone& zone, const TeamAI& teamAI, MatchEnvironment& env);
    static sf::Vector2f clampToTacticalZone(sf::Vector2f target, sf::Vector2f homePos, const TacticalZone& zone, float distToBall, bool isHomeSide, const TeamAI& teamAI);
    static bool shouldRecoverPosition(const sf::Vector2f& npcPos, const sf::Vector2f& homePos, const TacticalZone& zone, float distToBall, bool isHomeSide, const TeamAI& teamAI);

    static Player* identifyTargetReceiver(MatchEnvironment& env);
private:
    static float getAverageDefensiveLineX(NPCPlayer& npc, MatchEnvironment& env);

    static sf::Vector2f evaluateAttackingShape(NPCPlayer& npc, sf::Vector2f currentTarget, const TeamAI& teamAI, const TacticalZone& zone, MatchEnvironment& env);
    static sf::Vector2f evaluateDefendingShape(NPCPlayer& npc, sf::Vector2f currentTarget, const TeamAI& teamAI, const TacticalZone& zone, float avgLineX, MatchEnvironment& env);

    static sf::Vector2f evaluateZonalShifts(NPCPlayer& npc, const TacticalZone& zone, float awareness, bool isMid, bool isDefender, MatchEnvironment& env);
    static sf::Vector2f evaluateRunnerTracking(NPCPlayer& npc, const TeamAI& teamAI, const TacticalZone& zone, float avgLineX, float awareness, bool isMid, bool isDefender, bool& outTrackingRunner, bool& outTriggerTrap, MatchEnvironment& env);
    static sf::Vector2f evaluateLineIntegrity(NPCPlayer& npc, const TacticalZone& zone, float avgLineX, float awareness, bool isMid, bool isForward, bool isDefender, bool trackingRunner, bool triggerOffsideTrap, MatchEnvironment& env);

    static sf::Vector2f calculateAttackingTarget(NPCPlayer& npc, sf::Vector2f tacticalTarget, TacticalZone zone, const TeamAI& teamAI, float enemyOffsideLineX, float ballProgress, MatchEnvironment& env);
    static sf::Vector2f calculateDefendingTarget(NPCPlayer& npc, sf::Vector2f tacticalTarget, TacticalZone zone, const TeamAI& teamAI, float ballProgress, float threatX, float myDefLineX, MatchEnvironment& env);
};
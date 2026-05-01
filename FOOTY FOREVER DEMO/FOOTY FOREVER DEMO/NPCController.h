#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "NPCPlayer.h"
#include "TacticalZone.h"
#include "MatchState.h"
#include "SoundManager.h"

class UserPlayer;
class Ball;
class Player;
struct Pitch;
class MatchReferee;
class TeamAI;
struct PositioningMask;
struct TacticalContext;
struct TeamState;
class SpatialGrid;
class MatchStatistics;
struct MatchEnvironment;

// Defines how an NPC should behave
enum class AIRole {
    SupportingTeammate,
    AggressiveChaser,   // Standard Opponent
    Goalie,
    Idle
};


class NPCController
{
public:
    NPCController();
    ~NPCController();

    // The main entry point for the AI brain
    void update(NPCPlayer& npc, UserPlayer* userPlayer, float dt, Player* firstResponder, const TeamAI& teamAI, MatchEnvironment& env);

private:
    // 3. Update the Physics signature (add TacticalContext at the end)
    void applyMovementPhysics(NPCPlayer& npc, sf::Vector2f directionInput, bool isSprinting,
        float dt, float distToTarget, Player* firstResponder,
        bool keeperBall, TacticalContext ctx, MatchEnvironment& env);

    void handleSetPiece(NPCPlayer& npc, UserPlayer* user, float dt, const TeamAI& teamAI, const TacticalContext& ctx, MatchEnvironment& env);

    bool handleAerialLogic(NPCPlayer& npc, UserPlayer* user, const TeamAI& teamAI, MatchEnvironment& env);

    void handleOutfieldActions(NPCPlayer& npc, UserPlayer* user, float dt, Player* firstResponder, const TeamAI& teamAI, const TacticalContext& ctx, const PositioningMask& mask, MatchEnvironment& env);

    void processDefensiveActions(NPCPlayer& npc, float dt, const TacticalContext& ctx, const TeamAI& teamAI, MatchEnvironment& env);

    // Constants for AI behavior tuning
    const float M_SUPPORT_DISTANCE = 400.f;
    const float M_TACKLE_THRESHOLD = 80.f;
};
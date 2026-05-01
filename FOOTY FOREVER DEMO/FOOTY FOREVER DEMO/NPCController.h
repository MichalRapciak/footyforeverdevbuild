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
    void update(NPCPlayer& npc, UserPlayer* user, Ball& ball,
        const std::vector<Player*>& team, const std::vector<Player*>& opposition,
        const Pitch& pitch, float dt, Player* firstResponder,
        const MatchReferee& referee, const TeamAI& teamAI, SoundManager& soundManager, const SpatialGrid& spatialGrid
        , MatchStatistics& stats);

private:
    // 3. Update the Physics signature (add TacticalContext at the end)
    void applyMovementPhysics(NPCPlayer& npc, sf::Vector2f directionInput, bool isSprinting,
        float dt, float distToTarget, Ball& ball, Player* firstResponder,
        const Pitch& pitch, bool keeperBall, TacticalContext ctx);

    void handleSetPiece(NPCPlayer& npc, UserPlayer* user, Ball& ball, const std::vector<Player*>& team,
        const std::vector<Player*>& opposition, const Pitch& pitch, float dt,
        const MatchReferee& referee, const TeamAI& teamAI, SoundManager& soundManager,
        MatchStatistics& stats, const TacticalContext& ctx);

    bool handleAerialLogic(NPCPlayer& npc, UserPlayer* user, Ball& ball, const std::vector<Player*>& team,
        const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI,
        SoundManager& soundManager, MatchStatistics& stats);

    void handleOutfieldActions(NPCPlayer& npc, UserPlayer* user, Ball& ball, const std::vector<Player*>& team,
        const std::vector<Player*>& opposition, const Pitch& pitch, float dt,
        Player* firstResponder, const TeamAI& teamAI, SoundManager& soundManager,
        const SpatialGrid& spatialGrid, MatchStatistics& stats, const TacticalContext& ctx,
        const PositioningMask& mask);

    void processDefensiveActions(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& opposition,
        const Pitch& pitch, float dt, const TacticalContext& ctx, const TeamAI& teamAI);

    // Constants for AI behavior tuning
    const float M_SUPPORT_DISTANCE = 400.f;
    const float M_TACKLE_THRESHOLD = 80.f;
};
#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "NPCPlayer.h"
#include "TacticalZone.h"
#include "MatchState.h"

class UserPlayer;
class Ball;
class Pitch;
class MatchReferee; // Forward declaration at the top
struct PositioningMask;
struct TacticalContext;

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
    void update(NPCPlayer& npc, UserPlayer& user, Ball& ball,
        const std::vector<Player*> team,
        const std::vector<Player*> opposition,
        const Pitch& pitch, TeamState teamState, float dt, Player* firstResponder, const MatchReferee& referee);
    /// Uses GkCatching: Determines if a successful save is held or parried away
    void resolveSaveOutcome(NPCPlayer& npc, Ball& ball);

private:
    // 1. TACTICAL BRAIN: Decides WHERE to go
    sf::Vector2f decideTargetPosition(NPCPlayer& npc, Ball& ball,
        const Pitch& pitch, TeamState state,
        const std::vector<Player*> opponents, Player* firstResponder, PositioningMask mask);

    sf::Vector2f applyTacticalPositioning(NPCPlayer& npc, sf::Vector2f homePos, sf::Vector2f ballPos, sf::Vector2f goalPos, TeamState state, TacticalZone zone, const Pitch& pitch, const std::vector<Player*> opposition);

    sf::Vector2f clampToTacticalZone(sf::Vector2f target, sf::Vector2f homePos,
        const TacticalZone& zone, float distToBall,
        bool isHomeSide);

    bool shouldRecoverPosition(const sf::Vector2f& npcPos, const sf::Vector2f& homePos, const TacticalZone& zone, float distToBall, bool isHomeSide);

    bool shouldEmergencyChase(NPCPlayer& npc, Player* firstResponder, float distToBall, const Pitch& pitch, Ball& ball, MatchState matchstate);

    Player* findBestThreat(const sf::Vector2f& npcPos, const std::vector<Player*>& opponents, const TacticalZone& zone, MatchState matchstate);

    sf::Vector2f calculateMarkingPosition(NPCPlayer& npc, Player* threat, sf::Vector2f goalPos, const TacticalZone& zone);

    // In NPCController.h
    sf::Vector2f handlePossession(NPCPlayer& npc, Ball& ball,
        const std::vector<Player*> teammates, const std::vector<Player*> opposition,
        UserPlayer& user, const Pitch& pitch, float dt, MatchState matchstate);

    void executeThrowIn(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates);

    Player* findBestPassOption(NPCPlayer& npc,
        const std::vector<Player*> team, const std::vector<Player*> opposition,
        UserPlayer& user);

    sf::Vector2f calculateDribbleDirection(NPCPlayer& npc, sf::Vector2f goalPos, const std::vector<Player*>& opposition);

    void executePass(NPCPlayer& npc, Ball& ball, Player* target, const std::vector<Player*>& opposition);

    void executeShot(NPCPlayer& npc, Ball& ball, sf::Vector2f goalPos, const std::vector<Player*>& opposition, float dt);

    // 3. Update the Physics signature (add TacticalContext at the end)
    void applyMovementPhysics(NPCPlayer& npc, sf::Vector2f directionInput, bool isSprinting,
        float dt, float distToTarget, Ball& ball, Player* firstResponder,
        const Pitch& pitch, bool keeperBall, TacticalContext ctx);

    sf::Vector2f calculateSeparation(NPCPlayer& npc, const std::vector<Player*> team, const std::vector<Player*> opponents, sf::Vector2f ballPos);

    sf::Vector2f calculateInterceptionPoint(NPCPlayer& npc, Ball& ball);

    void updateNPCAirPhysics(NPCPlayer& npc, float dt);
    void handleNPCJumpLogic(NPCPlayer& npc, Ball& ball);
    bool tryNPCAerialStrike(NPCPlayer& npc, Ball& ball, sf::Vector2f aimDir, bool isShot);

    // --------------------------------------------------------
    // 3. GOALKEEPING LOGIC (New Section)
    // --------------------------------------------------------

    /// Main update branch for the Goalie role
    void handleGoalkeeping(NPCPlayer& npc, Ball& ball, const Pitch& pitch,
        const std::vector<Player*>& team, const std::vector<Player*>& opposition, float dt);

    /// Uses GkCoverage: Calculates position on the arc between the ball and goal center
    sf::Vector2f calculateGoaliePositioning(NPCPlayer& npc, sf::Vector2f ballPos, sf::Vector2f goalCenter, const Pitch& pitch);

    /// Uses GkAwareness: Decides if the keeper should abandon the line to intercept a through-ball
    bool shouldGoalieRush(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& opposition, const Pitch& pitch);

    /// Uses GkReactions & GkBlocking: Triggers diving/saving physics when a shot is detected
    void attemptSave(NPCPlayer& npc, Ball& ball, float dt);


    /// Uses GkThrowing: Handles distribution once the keeper has possession
    void distributeBallAsGoalie(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates);

    void triggerDive(NPCPlayer& npc, sf::Vector2f diveTarget, float jumpSpeed);

    // 4. UTILITY FUNCTIONS
    //float getDistance(sf::Vector2f a, sf::Vector2f b);
    sf::Vector2f normalize(sf::Vector2f source);
    float dist(sf::Vector2f p1, sf::Vector2f p2);
    bool isBallInOurHalf(NPCPlayer& npc, Ball& ball, const Pitch& pitch);
    Player* findNearestOpponent(const sf::Vector2f& npcPos, const std::vector<Player*>& opponents);
    TacticalZone getZoneForRole(PositionRole role);
    float length(sf::Vector2f v) {
        return std::sqrt(v.x * v.x + v.y * v.y);
    }





    // Constants for AI behavior tuning
    const float M_SUPPORT_DISTANCE = 400.f;
    const float M_TACKLE_THRESHOLD = 80.f;
};
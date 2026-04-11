#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "MatchContext.h"
#include "TacticalZone.h"
#include "Direction.h"

class Player;
class NPCPlayer;
class UserPlayer;
class Ball;
struct Pitch;
class TeamAI;

enum class AIUrgency {
    Recovery,      // Jogging back into formation (Low payoff)
    Pressing,      // Closing down the opponent (Medium payoff)
    AttackingRun,  // Making a run into space (High payoff)
    Critical       // Receiving a pass, loose ball, or emergency defending (Must sprint)
};

class PlayerAI {
public:
    // ==========================================
    // --- OFF-BALL MOVEMENT (Where do I run?) ---
    // ==========================================
    static sf::Vector2f decideTargetPosition(NPCPlayer& npc, Ball& ball, const Pitch& pitch, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, Player* firstResponder, PositioningMask mask, const TeamAI& teamAI);

    static sf::Vector2f applyTacticalPositioning(NPCPlayer& npc, sf::Vector2f homePos, sf::Vector2f ballPos, sf::Vector2f goalPos, TeamState state, TacticalZone zone, const Pitch& pitch, const std::vector<Player*>& team, const std::vector<Player*>& opposition, const TeamAI& teamAI);

    static sf::Vector2f evaluateShapeAndSpace(NPCPlayer& npc, sf::Vector2f currentTarget, sf::Vector2f ballPos, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, const TeamAI& teamAI, const TacticalZone& zone);

    static bool evaluateSprintUrgency(NPCPlayer& npc, AIUrgency urgency, float distToTarget, float distToBall);
    static bool shouldEmergencyChase(NPCPlayer& npc, Player* firstResponder, float distToBall, const Pitch& pitch, Ball& ball, MatchState matchstate, const TeamAI& teamAI);
    static sf::Vector2f calculateInterceptionPoint(NPCPlayer& npc, Ball& ball);
    static sf::Vector2f calculateSeparation(NPCPlayer& npc, const std::vector<Player*>& team, const std::vector<Player*>& opponents, sf::Vector2f ballPos, const TeamAI& teamAI);
    static sf::Vector2f calculateMarkingPosition(NPCPlayer& npc, Player* threat, sf::Vector2f goalPos, const TacticalZone& zone, const TeamAI& teamAI);
    static bool shouldRecoverPosition(const sf::Vector2f& npcPos, const sf::Vector2f& homePos, const TacticalZone& zone, float distToBall, bool isHomeSide, const TeamAI& teamAI);
    static sf::Vector2f clampToTacticalZone(sf::Vector2f target, sf::Vector2f homePos, const TacticalZone& zone, float distToBall, bool isHomeSide, const TeamAI& teamAI);

    static Player* findBestThreat(const sf::Vector2f& npcPos, const std::vector<Player*>& opponents, const TacticalZone& zone, MatchState matchstate);
    static Player* findNearestOpponent(const sf::Vector2f& npcPos, const std::vector<Player*>& opponents);

    // ==========================================
    // --- ON-BALL DECISIONS (What do I do?) ---
    // ==========================================
    // Returns the vector the player should dribble towards.
    // NOTE: This will also automatically execute passes or shots if the AI decides to do so!
    static sf::Vector2f handlePossession(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates, const std::vector<Player*>& opposition, UserPlayer& user, const Pitch& pitch, float dt, MatchState matchstate, const TeamAI& teamAI);

    static Player* findBestPassOption(NPCPlayer& npc, const std::vector<Player*>& team, const std::vector<Player*>& opposition, UserPlayer& user, const TeamAI& teamAI, const Pitch& pitch);

    static sf::Vector2f calculateDribbleDirection(NPCPlayer& npc, sf::Vector2f goalPos, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI);



    // ==========================================
    // --- AERIAL LOGIC ---
    // ==========================================
    // Checks if the ball is at a hittable height and triggers a jump
    static void handleNPCJumpLogic(NPCPlayer& npc, Ball& ball);

    // Determines if the NPC should attempt a header or volley
    static bool tryNPCAerialStrike(NPCPlayer& npc, Ball& ball, sf::Vector2f aimDir, bool isShot);


    // ==========================================
    // --- SET PIECES ---
    // ==========================================
    // Executes a throw-in to the best available teammate
    static void executeThrowIn(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates);


    // ==========================================
    // --- GOALKEEPER BRAIN ---
    // ==========================================

    static void handleGoalkeeping(NPCPlayer& npc, Ball& ball, const Pitch& pitch, const std::vector<Player*>& team,
        const std::vector<Player*>& opposition, float dt, const TeamAI& teamAI);

    // Calculates the ideal "Step out" position on the goal line
    static sf::Vector2f calculateGoaliePositioning(NPCPlayer& npc, sf::Vector2f ballPos, sf::Vector2f goalCenter, const Pitch& pitch);

    // Logic to decide if the keeper should abandon the line to tackle an attacker
    static bool shouldGoalieRush(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI);

    // Scans for incoming shots and triggers a dive
    static void attemptSave(NPCPlayer& npc, Ball& ball, float dt, const TeamAI& teamAI);

    // Physically applies the diving impulse to the keeper
    static void triggerDive(NPCPlayer& npc, sf::Vector2f diveTarget, float jumpSpeed, float targetZ);

    // GK Specific distribution (long kicks/throws vs short rolls)
    static void distributeBallAsGoalie(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI);

    // Decision logic for NPC shooting (outward-facing for NPCController to call)
    static void executeShot(NPCPlayer& npc, Ball& ball, sf::Vector2f goalPos, const std::vector<Player*>& opposition, const Pitch& pitch, float dt);

    // Decision logic for NPC passing (outward-facing)
    static void executePass(NPCPlayer& npc, Ball& ball, Player* target, const std::vector<Player*>& opposition);

    // Helper math
    static float dist(sf::Vector2f p1, sf::Vector2f p2);
    static sf::Vector2f normalize(sf::Vector2f source);
    static float length(sf::Vector2f v) {
        return std::sqrt(v.x * v.x + v.y * v.y);
    }
    static float dot(sf::Vector2f v1, sf::Vector2f v2) {
        return (v1.x * v2.x) + (v1.y * v2.y);
    }
    static sf::Vector2f getFacingVec(Direction dir) {
        switch (dir) {
        case Direction::Up:        return { 1.f, 0.f };
        case Direction::Down:      return { -1.f, 0.f };
        case Direction::Left:      return { 0.f, -1.f };
        case Direction::Right:     return { 0.f, 1.f };
        case Direction::UpLeft:    return { 0.707f, -0.707f };
        case Direction::UpRight:   return { 0.707f, 0.707f };
        case Direction::DownLeft:  return { -0.707f, -0.707f };
        case Direction::DownRight: return { -0.707f, 0.707f };
        default:                   return { 1.f, 0.f };
        }
    }

private:

};
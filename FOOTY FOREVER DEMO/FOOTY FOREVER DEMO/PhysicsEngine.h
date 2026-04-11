#pragma once
#include <vector>
#include <SFML/Graphics.hpp>

// Forward declarations so we don't have to include massive headers
class Player;
class Ball;
struct Pitch;
struct Goal; // Forward declaration
class MatchReferee;
class AnimationServer;

class PhysicsEngine {
public:
    // ==========================================
    // 1. BALL PHYSICS
    // ==========================================
// Handles curve, lift from backspin, and air drag
    static void applyBallAerodynamics(Ball& ball, float dt);

    // Handles ground friction and the gradual decay of spin
    static void applyBallFrictionAndSpin(Ball& ball, float dt);

    // Handles X/Y movement and bouncing off the invisible pitch walls
    static void updateBallPositionAndBounds(Ball& ball, float dt, float pitchWidth, float pitchHeight);

    // Handles Z-axis falling and bouncing off the grass
    static void applyBallGravityAndBounce(Ball& ball, float dt);

    // ==========================================
    // 2. PLAYER PHYSICS
    // ==========================================
    static void updatePlayerAirPhysics(Player& player, float dt);
    static void applySlideTackleFriction(Player& player, float dt);
    // --- GROUND MOVEMENT PHYSICS ---
    // Smoothly slows the player down based on their agility when no input is given
    static void applyPlayerIdleFriction(Player& player, float dt);

    // Kills momentum perpendicular to the target direction (prevents orbiting/drifting)
    static void applyTangentialVelocityDamping(Player& player, sf::Vector2f targetDir, float dampingStrength, float dt);

    // Keeps players inside the stadium walls (using the Pitch dimensions)
    static void resolvePlayerPitchBoundaries(Player& player, const Pitch& pitch);

    // Applies acceleration, braking, and turning friction based on the player's stats
    // The 'maxSpeed' is dictated by the Controller (e.g. slowed down if jockeying)
    static void applyPlayerLocomotion(Player& player, sf::Vector2f inputDir, float maxSpeed, float dt);

    // --- GOALKEEPER PHYSICS ---
    static void applyKeeperDiveFriction(Player& keeper, float dt);

    // --- GOALKEEPER COLLISIONS ---
    // Checks if the ball intersects with any diving goalkeepers
    static void resolveGoalkeeperBallCollisions(Ball& ball, std::vector<Player*>& players);

    // The mathematical resolution of the ball hitting the keeper's gloves
    static void resolveGoalkeeperSave(Player& keeper, Ball& ball);

    // ==========================================
    // 3. COLLISION PHYSICS
    // ==========================================
    // Pushes players apart if they overlap
    static void resolvePlayerPlayerCollisions(
        std::vector<Player*>& players,
        Ball& ball,
        MatchReferee& referee,
        AnimationServer& animServer,
        const Pitch& pitch
    );

    // Checks if the ball hits the invisible walls around the pitch/net
    static void resolveBallPitchBoundaries(Ball& ball, const Pitch& pitch);

    // Bounces the ball off outfield players, applying player momentum to the deflection
    static void resolveBallPlayerCollisions(Ball& ball, std::vector<Player*>& players);

    // --- GOAL COLLISIONS ---
    // Handles the ball hitting the net, crossbar, and posts
    static void resolveBallGoalCollisions(Ball& ball, const Goal& goal);

    // Handles players running into the net or posts
    static void resolvePlayerGoalCollisions(Player& player, const Goal& goal);
};
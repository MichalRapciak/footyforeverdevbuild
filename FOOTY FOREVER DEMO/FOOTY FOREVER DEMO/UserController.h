#pragma once
#include <SFML/Graphics.hpp>
#include "MatchState.h"

class UserPlayer; // I call this class here so the PlayerController knows to expect a player when it's loaded, without having to load the class here.
class MatchEngine;
class Ball;
struct MatchEnvironment;
class Player;
struct Pitch;

/// <summary>
/// This class is in charge of all the calculations depending on player movement and aiming, letting the player class be less bloated
/// </summary>
class UserController
{
public:
	UserController(UserPlayer& player);
	~UserController();

	void inputHandler(const sf::Event t_event);
	void update(float dt, MatchEnvironment& env);
	void draw(sf::RenderWindow& window);
	void mouseAiming(sf::Vector2f t_mouseWorld, sf::RenderWindow& t_window, sf::View t_view);
	void playerMovement(float dt, MatchEnvironment& env);
	void playerShooting(float dt, MatchEnvironment& env);
	float getKickStrength() const { return kickStrength; }
	void updateTargetScanning(MatchEnvironment& env);

	Player* findBestDefensiveSwitch(Player& currentPlayer, const std::vector<Player*>& team, Ball& ball, const Pitch& pitch);



private:
	static float dist(sf::Vector2f p1, sf::Vector2f p2);
	static sf::Vector2f normalize(sf::Vector2f source);

	void executeKickRelease(MatchEnvironment& env);
	bool calculateAerialKick(MatchEnvironment& env, float& finalPower, float& vzPower, float& errorAngle, float& finalBackspin);
	void calculateGroundKick(float basePower, float& finalPower, float& vzPower, float& errorAngle, float& finalBackspin);
	void attemptSave(float dt, MatchEnvironment& env);

	UserPlayer& m_userPlayer;
	sf::Vector2f m_speedVector{ 0,0 }; // Current speed vector
	sf::Vector2f m_newPos{ 0,0 }; // New player position after speed vector is added on

	// Kick charge
	float kickStrength = 0.f;        // 0 -> 1
	bool charging = false;
	bool increasing = true;
	float kickSpeed = 2.0f;        // oscillation speed (cycles/sec)
	bool kickPressed = false;
	bool isPressing = false;

	bool justKicked = false;
	float kickCooldown = 0.5f; // seconds before you can possess again
	float kickCooldownTimer = 0.f;

	bool isMoving = 0; // Boolean to check if player is moving
	bool m_up = false;
	bool m_down = false;
	bool m_left = false;
	bool m_right = false;
	sf::Vector2f directionInput{ 0.f, 0.f };
	bool isSprinting = false;
	bool isShooting = false;
	bool isPressuring = false;
	bool switchPressed = false;
	float m_tapTimerA = 0.0f;
	float m_tapTimerD = 0.0f;
	bool m_wasAPressed = false;
	bool m_wasDPressed = false;
	bool triggerBargeLeft = false;
	bool triggerBargeRight = false;


	float sprintRatio = 0;
	float accelMultiplier = 0;
	float accel = 0;
	float length = 0;
	float forwardSpeed = 0;
	float decel = 0;
	float decelMultiplier = 0;
	float speed = 0;
	float decelAmount = 0;

	MatchState m_lastMatchState = MatchState::InPlay;
	bool m_hadPossessionLastFrame = false;

	void resetInputs();

	float m_preChargePower = 0.0f;
	float m_preChargeTimer = 0.0f;
	bool m_isSetPieceRunUp = false;

	bool isHighKick = false;
	bool isJockeying = false;
	bool isChasingLooseBall = false;

    Player* m_currentTarget = nullptr; // Track who we are currently aiming at
    sf::CircleShape m_targetHighlight; // The visual circle

	const int m_speedNearlyZero = 1; // This value determines when player's speed is considered "nearly zero" and is used in a statement to set player's speed to 0.
	float maxSpeed = 0;
	sf::Vector2f mousePos = { 0,0 };
	sf::Vector2f m_facingDirection = { 0,0 };
	float m_angleRadians = 0.0f;
	float m_angleDegrees = 0.0f;

};
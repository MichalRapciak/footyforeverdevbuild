#pragma once
#include "Entity.h"
#include "PlayerStats.h"
#include "PlayerState.h"
#include "PositionRole.h"
#include "Team.h"



class Player : public Entity {
public:
    Player();

    // Shared Physics Update (Inertia, Counter-steer, etc.)
    virtual void update(float dt);

    // Shared Combat/Sports Actions
    void startTackle(sf::Vector2f direction);

    // Common Getters/Setters
    sf::Vector2f getPosition() const override { return m_position; }
    void setPosition(sf::Vector2f t_position) { m_position = t_position; }
    virtual sf::Sprite getSprite() = 0;
    sf::FloatRect getBoundingBox() const override { if (m_positionRole == PositionRole::Goalkeeper) {return sf::FloatRect({ m_position.x - 30,m_position.y - 40 }, { 60,80 }); } else { return sf::FloatRect({ m_position.x - 20,m_position.y - 20 }, { 40,40 }); } }
    
    sf::Vector2f getVelocity() { return m_velocity; }
    void setVelocity(sf::Vector2f t_velocity) { m_velocity = t_velocity; }
    bool getBallPossession() { return hasPossession; }
    void setBallPossession(bool t) { hasPossession = t; }

    virtual sf::Vector2f getAimDirection() const = 0;

    float getFinishing() { return m_stats.getFinishing(); }
    float getDeadBall() { return m_stats.getDeadBall(); }
    float getHeading() { return m_stats.getHeading(); }

    float getCurl() { return m_stats.getCurl(); }
    float getBallControl() { return m_stats.getBallControl(); }
    float getBalancing() { return m_stats.getBalancing(); }

    float getShortPassing() { return m_stats.getShortPassing(); }
    float getLongPassing() { return m_stats.getLongPassing(); }

	float getAcceleration() { return m_stats.getAccel(); }
	float getTopSpeed() { return m_stats.getTopSpeed(); }
	float getAgility() { return m_stats.getAgility(); }

    float getBodyStrength() { return m_stats.getBodyStrength(); }
    float getKickPower() { return m_stats.getKickPower(); }
    float getJumpingStrength() { return m_stats.getJumpingStrength(); }

    float getAwareness() { return m_stats.getAwareness(); }
    float getAggression() { return m_stats.getAggression(); }
    float getBlocking() { return m_stats.getBlocking(); }

    float getGkThrowing() { return m_stats.getGkThrowing(); }
    float getGkCoverage() { return m_stats.getGkCoverage(); }
    float getGkReactions() { return m_stats.getGkReactions();}
    float getGkCatching() { return m_stats.getGkCatching(); }
    float getGkAwareness() { return m_stats.getGkAwareness(); }
    float getGkBlocking() { return m_stats.getGkBlocking(); }

    bool isTackling() { return m_currentState == PlayerState::Tackling; }
    void setStumbled(float duration) {
        // Only stumble if we aren't already in a more severe state
        if (m_currentState == PlayerState::Normal) {
            m_currentState = PlayerState::Stumbled;
            m_stumbleTimer = duration;

            if (this->getBallPossession()) {
                // If the stumble is severe (> 0.5s), they almost always lose it.
                // If it's a minor clip, they might keep it based on their stats.
                float spillChance = (duration / 0.8f) * (100.f - getBallControl());

                if (spillChance > 30.f) { // Arbitrary threshold
                    this->setBallPossession(false); // Make sure the ball gets a tiny "pop" velocity
                }
            }
        }
    }

    bool canTackle() const { return m_tackleCooldownTimer <= 0.0f; }
    void updateCooldown(float dt) {
        if (m_tackleCooldownTimer > 0.0f) m_tackleCooldownTimer -= dt; 
    }
    void startTackleCooldown() { m_tackleCooldownTimer = TACKLE_COOLDOWN_DURATION; }
    PlayerState getState() const { return m_currentState; }
    void setState(PlayerState state) { m_currentState = state; }
    float getCollisionRadius() const { return m_collisionRadius; }

    void resetTackleCooldown() { m_tackleTimer = m_tackleDuration; };
    virtual void move(sf::Vector2f t_move) { };
    bool isTeammate() { return m_team == Team::Home; }
    void setTeam(Team t_team) { m_team = t_team; }
    sf::FloatRect getTackleHitbox();
    Team getTeam() { return m_team; }

    void setPositionRole(PositionRole role) { m_positionRole = role; }
    PositionRole getPositionRole() const { return m_positionRole; }
    sf::Vector2f getBaseTacticalCoordinate(bool isHomeTeam) const;
    sf::Vector2f getHomePosition(bool isHomeTeam, TeamState teamState) const;
    float getWeight() { return weight; }

    bool usingRightFoot() { return rightFoot; }
    void changeFoot() { rightFoot = !rightFoot; }

    float height = 175.0f; // height range - 150 -> 210
    float weight = 75.0f; // weight range - 50 -> 120
    float m_possessionTimer = 0.0f;
protected:
    sf::Vector2f m_position;
    sf::Vector2f m_velocity;

    PositionRole m_positionRole = PositionRole::LCenterBack;
    PlayerStats m_stats;
    PlayerState m_currentState = PlayerState::Normal;
    Team m_team = Team::Home;

    float m_collisionRadius = 25.f; // Adjust based on your player size

    float m_stumbleTimer = 0.0f;
    bool rightFoot = false;
    bool hasPossession = false;
    float m_tackleTimer = 0.0f;
    float m_tackleDuration = 0.4f;
    const float TACKLE_COOLDOWN_DURATION = 2.0f; // 2 seconds
    float m_tackleCooldownTimer = 0.0f;
};
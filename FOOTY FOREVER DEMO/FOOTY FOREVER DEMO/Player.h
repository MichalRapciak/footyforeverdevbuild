#pragma once
#include "Entity.h"
#include "PlayerStats.h"
#include "PlayerState.h"
#include "PositionRole.h"
#include "Playstyle.h"
#include "Team.h"
#include "Animator.h"
#include "Direction.h"
#include <string>
#include <vector>
#include "InjuryData.h"
#include "KitLayer.h"

// Forward declare the database struct 
struct PlayerData;
struct TeamData;

class Player : public Entity {
    friend class PhysicsEngine;
public:
    Player();

    void loadFromData(const PlayerData& data, const TeamData& teamData);
    int getRoleProficiency(PositionRole role) const;
    void swapIdentityWith(Player* other);

    virtual void update(float dt);
    void applyPhysicalScale();

    void applySubstitution(const PlayerData& newPlayerData, const TeamData& teamData);
    void startTackle(sf::Vector2f direction);

    sf::Vector2f getPosition() const override { return m_position; }
    void setPosition(sf::Vector2f t_position) { m_position = t_position; m_sprite.setPosition(m_position); }
    sf::Sprite getSprite() { return m_sprite; }

    Animator& getAnimator() { return m_animator; }
    sf::FloatRect getBoundingBox() const override {
        if (m_matchRole == PositionRole::Goalkeeper && m_currentState == PlayerState::Diving) {
            return sf::FloatRect({ m_position.x - 40, m_position.y - (60 + (m_stats.getGkCoverage() * 0.8f)) }, { 80 + (m_stats.getGkCoverage()) * 0.8f, (60 + (m_stats.getGkCoverage() * 0.8f)) * 2 });
        }
        else {
            return sf::FloatRect({ m_position.x - 50, m_position.y - 20 }, { 100,40 });
        }
    }

    void setMatchTimeScale(float timeScale) { m_matchTimeScale = timeScale; }

    sf::Vector2f getVelocity() { return m_velocity; }
    void setVelocity(sf::Vector2f t_velocity) { m_velocity = t_velocity; }
    bool getBallPossession() { return hasPossession; }
    void setBallPossession(bool t) { hasPossession = t; }
    float getMatchTimeScale() { return m_matchTimeScale; }

    virtual sf::Vector2f getAimDirection() const = 0;

    std::string getId() const { return m_id; }
    std::string getName() const { return m_name; }
    int getSquadNumber() const { return m_squadNumber; }
    int getAge() const { return m_age; }
    std::string getPreferredFoot() const { return m_preferredFoot; }
    const std::vector<std::string>& getTraits() const { return m_traits; }
    bool hasTrait(const std::string& traitName) const;
    void triggerFallOver(sf::Vector2f impactVelocity);
    void checkInjury(float impactForce);
    void applyRandomInjury(float impactNorm);
    void recoverFromFall();

    float getFitness() { return m_stats.getFitness(); }
    int getWeakFootAccuracy() { return m_stats.getWeakFootAccuracy(); }
    int getInjuryResistance() { return m_stats.getInjuryResistance(); }
    float getTacticalFamiliarity() { return m_tacticalFamiliarity; }
    float getTeamChemistry() { return m_teamChemistry; }

    float getFinishing() { return m_stats.getFinishing() * getGeneralMultiplier(); }
    float getDeadBall() { return m_stats.getDeadBall() * getGeneralMultiplier(); }
    float getHeading() { return m_stats.getHeading() * getGeneralMultiplier(); }

    float getCurl() { return m_stats.getCurl() * getGeneralMultiplier(); }
    float getBallControl() { return m_stats.getBallControl() * getGeneralMultiplier(); }
    float getBalancing() { return m_stats.getBalancing() * getMovementMultiplier() * getInjuryDebuff(); }

    float getShortPassing() { return m_stats.getShortPassing() * getGeneralMultiplier() * getMentalMultiplier(); }
    float getLongPassing() { return m_stats.getLongPassing() * getGeneralMultiplier() * getInjuryDebuff() * getMentalMultiplier(); }

    float getAcceleration() { return m_stats.getAccel() * getMovementMultiplier() * getInjuryDebuff(); }
    float getTopSpeed() { return m_stats.getTopSpeed() * getMovementMultiplier() * getInjuryDebuff(); }
    float getAgility() { return m_stats.getAgility() * getMovementMultiplier() * getInjuryDebuff(); }

    float getBodyStrength() { return m_stats.getBodyStrength() * getInjuryDebuff(); }
    float getKickPower() { return m_stats.getKickPower() * getInjuryDebuff(); }
    float getJumpingStrength() { return m_stats.getJumpingStrength() * getMovementMultiplier() * getInjuryDebuff(); }

    float getAwareness() const;
    float getAggression() { return m_stats.getAggression() * getGeneralMultiplier() * getMentalMultiplier(); }
    float getBlocking() { return m_stats.getBlocking() * getGeneralMultiplier() * getMentalMultiplier(); }

    float getGkThrowing() { return m_stats.getGkThrowing() * getGeneralMultiplier() * getMentalMultiplier(); }
    float getGkCoverage() { return m_stats.getGkCoverage() * getGeneralMultiplier(); }
    float getGkReactions() { return m_stats.getGkReactions() * getGeneralMultiplier(); }
    float getGkCatching() { return m_stats.getGkCatching() * getGeneralMultiplier(); }
    float getGkAwareness() { return m_stats.getGkAwareness() * getGeneralMultiplier() * getMentalMultiplier(); }
    float getGkBlocking() { return m_stats.getGkBlocking() * getGeneralMultiplier(); }

    PlayerStats getStats() { return m_stats; }
    void setStats(PlayerStats stats) { m_stats = stats; }

    int getLoyalty() { return loyalty; }

    bool isTackling() { return m_currentState == PlayerState::Tackling; }
    void setStumbled(float duration) {
        if (m_currentState == PlayerState::Normal) {
            m_currentState = PlayerState::Stumbled;
            m_stumbleTimer = duration;

            if (this->getBallPossession()) {
                float spillChance = (duration / 0.8f) * (100.f - getBallControl());
                if (spillChance > 30.f) {
                    this->setBallPossession(false);
                }
            }
        }
    }

    float getInjuryDebuff() const {
        if (!isInjured) return 1.0f; // 100% healthy

        switch (currentInjurySeverity) {
        case InjurySeverity::Knock:
            return 0.85f;
        case InjurySeverity::Mild:
            return 0.60f;
        case InjurySeverity::Severe:
            return 0.0f;
        default: return 1.0f;
        }
    }

    Playstyle getPlaystyle() { return m_playstyle; }
    Direction get8WayDirection(sf::Vector2f targetVector);
    Direction getDirection() const { return m_currentDirection; }
    bool canTackle() const { return m_tackleCooldownTimer <= 0.0f; }
    void updateCooldown(float dt) {
        if (m_tackleCooldownTimer > 0.0f) m_tackleCooldownTimer -= dt;
    }
    void startTackleCooldown() { m_tackleCooldownTimer = TACKLE_COOLDOWN_DURATION; }

    PlayerState getState() const { return m_currentState; }
    void setState(PlayerState newState);
    float getCollisionRadius() const { return m_collisionRadius; }
    float getSortDepth() const override {
        float feetOffset = 400.0f * std::abs(m_sprite.getScale().x);
        return m_position.x - feetOffset;
    }

    void resetTackleCooldown() { m_tackleTimer = m_tackleDuration; };
    virtual void move(sf::Vector2f t_move) {};
    bool isTeammate() { return m_team == Team::Home; }
    void setTeam(Team t_team) { m_team = t_team; }
    sf::FloatRect getTackleHitbox();
    Team getTeam() const { return m_team; }

    void setPositionRole(PositionRole role) { m_matchRole = role; }
    PositionRole getPositionRole() const { return m_matchRole; }
    sf::Vector2f getBaseTacticalCoordinate(bool isHomeTeam, int slotId, const std::vector<std::vector<std::pair<int, PositionRole>>>& layout) const;
    void setBaseHomePosition(sf::Vector2f pos) { m_baseHomePosition = pos; }

    sf::Vector2f getHomePosition() const;

    float getWeight() { return weight; }

    bool usingRightFoot() { return rightFoot; }
    void changeFoot() { rightFoot = !rightFoot; }

    float height = 175.0f;
    float weight = 75.0f;
    float m_possessionTimer = 0.0f;

    int getYellowCards() const { return m_yellowCards; }
    bool isSentOff() const { return m_isSentOff; }

    void giveYellowCard() {
        m_yellowCards++;
        if (m_yellowCards >= 2) {
            m_isSentOff = true;
        }
    }
    void giveRedCard() {
        m_isSentOff = true;
    }

    float getBargeCooldown() const { return m_bargeCooldown; }
    bool executeShoulderBarge(Player* target);

    void initializeStamina(float naturalFitness, float matchSharpness);
    void setTeamChemistry(float chem) { m_teamChemistry = chem; }
    void updateStamina(float dt, bool isSprinting);
    void deductStaminaAction(float baseAmount);
    float getMentalMultiplier() const;
    float getCurrentStamina() const { return m_currentStamina; }
    float getMaxStamina() const { return m_maxStamina; }

    float getMovementMultiplier() const;
    float getGeneralMultiplier() const;

    const std::vector<KitLayer>& getKitLayers() const { return m_kitLayers; }
    sf::Color getSkinColor() const { return m_skinColor; }

protected:
    sf::Vector2f m_baseHomePosition;
    bool m_tackleAnimTriggered = false;
    sf::Vector2f m_position;
    sf::Vector2f m_velocity;

    float m_tacticalFamiliarity = 100.f;
    float m_teamChemistry = 100.f;
    int loyalty = 0;

    float m_naturalFitness = 100.f;
    float m_matchSharpness = 100.f;

    float m_maxStamina = 100.f;
    float m_currentStamina = 100.f;

    float m_matchTimeScale = 22.5f;

    int m_yellowCards = 0;
    bool m_isSentOff = false;

    // --- GRAPHICS COMPONENTS ---
    sf::Sprite m_sprite;
    Animator m_animator;
    Direction m_currentDirection = Direction::Down;

    std::vector<KitLayer> m_kitLayers;
    sf::Color m_skinColor;

    // --- IDENTITY & BIO DATA ---
    std::string m_id;
    std::string m_name;
    int m_squadNumber = 0;
    int m_age = 0;
    std::string m_preferredFoot = "Right";
    std::vector<std::string> m_traits;

    // --- TACTICAL & STATS ---
    PositionRole m_matchRole = PositionRole::CenterBack;
    std::map<PositionRole, int> m_familiarity;
    Playstyle m_playstyle;
    PlayerStats m_stats;
    PlayerState m_currentState = PlayerState::Normal;
    Team m_team = Team::Home;

    float m_collisionRadius = 25.f;
    float m_stumbleTimer = 0.0f;

    bool rightFoot = false;
    bool hasPossession = false;

    float m_bargeCooldown = 0.0f;
    float m_tackleTimer = 0.0f;
    float m_tackleDuration = 0.4f;
    const float TACKLE_COOLDOWN_DURATION = 2.0f;
    float m_tackleCooldownTimer = 0.0f;

    // injury info
    bool isInjured = false;
    std::string currentInjury = "";
    int injuryDaysRemaining = 0;
    InjurySeverity currentInjurySeverity = InjurySeverity::Knock;

};

// Logic for scaling 0-5 accuracy
inline float getWeakFootPenalty(int wfStars, float& powerMod, float& errorMod) {
    powerMod = 0.7f + (wfStars / 5.0f) * 0.3f;
    errorMod = 1.0f + (5.0f - wfStars) * 0.8f;
    float shankChance = (5.0f - wfStars) * 10.0f;
    if ((rand() % 100) < shankChance) {
        return 15.0f + (5.0f - wfStars) * 5.0f;
    }
    return 0.0f;
}
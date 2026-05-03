#pragma once
#include <string>
#include <vector>
#include <map>

#include "Entity.h"
#include "PlayerStats.h"
#include "PlayerState.h"
#include "PositionRole.h"
#include "Playstyle.h"
#include "Team.h"
#include "Animator.h"
#include "Direction.h"
#include "InjuryData.h"
#include "KitLayer.h"

// Forward declare the database structs 
struct PlayerData;
struct TeamData;
class Player;
struct MatchEnvironment;

struct PendingKick {
    bool isActive = false;
    sf::Vector2f aimDir;
    float power = 0.f;
    float spin = 0.f;
    float vz = 0.f;
    float backspin = 0.f;
    int targetFrame = 3;
    float failsafeTimer = 0.0f; // Prevents permanent freezing if an animation glitches

    bool isPassIntent = false;
    bool isShotIntent = false;
    bool isShotOnTarget = false;
    Player* assistCandidate = nullptr;
};

class Player : public Entity {
    friend class PhysicsEngine;

public:
    Player();

    // ==========================================
    // --- LIFECYCLE & INITIALIZATION ---
    // ==========================================
    virtual void update(float dt);
    void loadFromData(const PlayerData& data, const TeamData& teamData);
    void applySubstitution(const PlayerData& newPlayerData, const TeamData& teamData);
    void swapIdentityWith(Player* other);

    // ==========================================
    // --- IDENTITY & BIO ---
    // ==========================================
    std::string getId() const { return m_id; }
    std::string getName() const { return m_name; }
    int getSquadNumber() const { return m_squadNumber; }
    int getAge() const { return m_age; }
    std::string getPreferredFoot() const { return m_preferredFoot; }

    const std::vector<std::string>& getTraits() const { return m_traits; }
    bool hasTrait(const std::string& traitName) const;

    // ==========================================
    // --- MATCH STATE & CONTEXT ---
    // ==========================================
    Team getTeam() const { return m_team; }
    void setTeam(Team t_team) { m_team = t_team; }
    bool isTeammate() { return m_team == Team::Home; }

    PlayerState getState() const { return m_currentState; }
    void setState(PlayerState newState);

    bool getBallPossession() { return hasPossession; }
    void setBallPossession(bool t) { hasPossession = t; }

    void setMatchTimeScale(float timeScale) { m_matchTimeScale = timeScale; }
    float getMatchTimeScale() { return m_matchTimeScale; }
    void setIsUserOpponent(bool isOpponent) { m_isUserOpponent = isOpponent; }

    // --- Disciplinary ---
    int getFoulCount() const { return m_foulCount; }
    int incrementFouls() { return ++m_foulCount; }
    int getYellowCards() const { return m_yellowCards; }
    bool isSentOff() const { return m_isSentOff; }
    void giveYellowCard() {
        m_yellowCards++;
        if (m_yellowCards >= 2) m_isSentOff = true;
    }
    void giveRedCard() { m_isSentOff = true; }

    // ==========================================
    // --- TACTICS & POSITIONING ---
    // ==========================================
    PositionRole getPositionRole() const { return m_matchRole; }
    void setPositionRole(PositionRole role) { m_matchRole = role; }
    Playstyle getPlaystyle() { return m_playstyle; }
    int getRoleProficiency(PositionRole role) const;

    sf::Vector2f getHomePosition() const;
    void setBaseHomePosition(sf::Vector2f pos) { m_baseHomePosition = pos; }
    sf::Vector2f getBaseTacticalCoordinate(bool isHomeTeam, int slotId, const std::vector<std::vector<std::pair<int, PositionRole>>>& layout) const;

    bool isDefender() const {
        return m_matchRole == PositionRole::CenterBack || m_matchRole == PositionRole::LeftBack ||
            m_matchRole == PositionRole::RightBack || m_matchRole == PositionRole::LeftWingBack ||
            m_matchRole == PositionRole::RightWingBack;
    }
    bool isMidfielder() const {
        return m_matchRole == PositionRole::DefensiveMid || m_matchRole == PositionRole::CenterMid ||
            m_matchRole == PositionRole::LeftMid || m_matchRole == PositionRole::RightMid ||
            m_matchRole == PositionRole::AttackingMid;
    }

    // ==========================================
    // --- PHYSICS & TRANSFORM ---
    // ==========================================
    sf::Vector2f getPosition() const override { return m_position; }
    void setPosition(sf::Vector2f t_position) { m_position = t_position; m_sprite.setPosition(m_position); }
    virtual void move(sf::Vector2f t_move) {};

    sf::Vector2f getVelocity() { return m_velocity; }
    void setVelocity(sf::Vector2f t_velocity) { m_velocity = t_velocity; }

    float getSortDepth() const override {
        float feetOffset = 400.0f * std::abs(m_sprite.getScale().x);
        return m_position.x - feetOffset;
    }
    float getCollisionRadius() const { return m_collisionRadius; }
    float getWeight() { return weight; }

    sf::FloatRect getBoundingBox() const override {
        if (m_matchRole == PositionRole::Goalkeeper && m_currentState == PlayerState::Diving) {
            return sf::FloatRect({ m_position.x - 40, m_position.y - (60 + (m_stats.getGkCoverage() * 0.8f)) },
                { 80 + (m_stats.getGkCoverage()) * 0.8f, (60 + (m_stats.getGkCoverage() * 0.8f)) * 2 });
        }
        return sf::FloatRect({ m_position.x - 50, m_position.y - 20 }, { 100, 40 });
    }

    // ==========================================
    // --- ACTIONS & BEHAVIORS ---
    // ==========================================
    virtual sf::Vector2f getAimDirection() const = 0;

    // --- Tackling & Physicality ---
    bool isTackling() { return m_currentState == PlayerState::Tackling; }
    bool canTackle() const { return m_tackleCooldownTimer <= 0.0f; }
    void startTackle(sf::Vector2f direction);
    void resetTackleCooldown() { m_tackleTimer = m_tackleDuration; };
    void startTackleCooldown() { m_tackleCooldownTimer = TACKLE_COOLDOWN_DURATION; }
    sf::FloatRect getTackleHitbox();

    float getBargeCooldown() const { return m_bargeCooldown; }
    bool executeShoulderBarge(Player* target);

    // --- Interruptions ---
    void triggerFallOver(sf::Vector2f impactVelocity);
    void recoverFromFall();
    void setStumbled(float duration) {
        if (m_currentState == PlayerState::Normal) {
            m_currentState = PlayerState::Stumbled;
            m_stumbleTimer = duration;

            if (this->getBallPossession()) {
                float spillChance = (duration / 0.8f) * (100.f - getBallControl());
                if (spillChance > 30.f) this->setBallPossession(false);
            }
        }
    }

    void updateCooldown(float dt) {
        if (m_tackleCooldownTimer > 0.0f) m_tackleCooldownTimer -= dt;
    }

    // ==========================================
    // --- CORE STATS & FITNESS ---
    // ==========================================
    PlayerStats getStats() { return m_stats; }
    void setStats(PlayerStats stats) { m_stats = stats; }

    float getFitness() { return m_stats.getFitness(); }
    int getWeakFootAccuracy() { return m_stats.getWeakFootAccuracy(); }
    int getInjuryResistance() { return m_stats.getInjuryResistance(); }
    float getTacticalFamiliarity() { return m_tacticalFamiliarity; }
    float getTeamChemistry() { return m_teamChemistry; }
    int getLoyalty() { return loyalty; }

    void initializeStamina(float naturalFitness, float matchSharpness);
    void setTeamChemistry(float chem) { m_teamChemistry = chem; }
    void updateStamina(float dt, bool isSprinting);
    void deductStaminaAction(float baseAmount);
    float getCurrentStamina() const { return m_currentStamina; }
    float getMaxStamina() const { return m_maxStamina; }

    // --- Injuries ---
    void checkInjury(float impactForce, MatchEnvironment& env);
    void applyRandomInjury(float impactNorm, MatchEnvironment& env);
    float getInjuryDebuff() const {
        if (!isInjured) return 1.0f;
        switch (currentInjurySeverity) {
        case InjurySeverity::Knock: return 0.85f;
        case InjurySeverity::Mild:  return 0.60f;
        case InjurySeverity::Severe:return 0.0f;
        default: return 1.0f;
        }
    }

    // ==========================================
    // --- MODIFIED MATCH STATS ---
    // ==========================================
    float getMovementMultiplier() const;
    float getGeneralMultiplier() const;
    float getMentalMultiplier() const;
    float getDifficultySpeedMod() const {
        if (!m_isUserOpponent) return 1.0f;
        if (isDefender()) return 1.10f;
        if (isMidfielder()) return 1.05f;
        return 1.0f;
    }
    float getDifficultyDefMod() const {
        if (!m_isUserOpponent) return 1.0f;
        if (isDefender() || isMidfielder()) return 1.10f;
        return 1.0f;
    }

    // Outfield
    float getFinishing() { return m_stats.getFinishing() * getGeneralMultiplier(); }
    float getDeadBall() { return m_stats.getDeadBall() * getGeneralMultiplier(); }
    float getHeading() { return m_stats.getHeading() * getGeneralMultiplier(); }
    float getCurl() { return m_stats.getCurl() * getGeneralMultiplier(); }
    float getBallControl() { return m_stats.getBallControl() * getGeneralMultiplier(); }
    float getBalancing() { return m_stats.getBalancing() * getMovementMultiplier() * getInjuryDebuff(); }
    float getShortPassing() { return m_stats.getShortPassing() * getGeneralMultiplier() * getMentalMultiplier(); }
    float getLongPassing() { return m_stats.getLongPassing() * getGeneralMultiplier() * getInjuryDebuff() * getMentalMultiplier(); }
    float getAcceleration() { return m_stats.getAccel() * getMovementMultiplier() * getInjuryDebuff() * getDifficultySpeedMod(); }
    float getTopSpeed() { return m_stats.getTopSpeed() * getMovementMultiplier() * getInjuryDebuff() * getDifficultySpeedMod(); }
    float getAgility() { return m_stats.getAgility() * getMovementMultiplier() * getInjuryDebuff() * getDifficultySpeedMod(); }
    float getBodyStrength() { return m_stats.getBodyStrength() * getInjuryDebuff(); }
    float getKickPower() { return m_stats.getKickPower() * getInjuryDebuff(); }
    float getJumpingStrength() { return m_stats.getJumpingStrength() * getMovementMultiplier() * getInjuryDebuff(); }
    float getAwareness() const; // Implemented in Player.cpp
    float getAggression() { return m_stats.getAggression() * getGeneralMultiplier() * getMentalMultiplier() * getDifficultyDefMod(); }
    float getBlocking() { return m_stats.getBlocking() * getGeneralMultiplier() * getMentalMultiplier() * getDifficultyDefMod(); }

    // Goalkeeping
    float getGkThrowing() { return m_stats.getGkThrowing() * getGeneralMultiplier() * getMentalMultiplier(); }
    float getGkCoverage() { return m_stats.getGkCoverage() * getGeneralMultiplier(); }
    float getGkReactions() { return m_stats.getGkReactions() * getGeneralMultiplier(); }
    float getGkCatching() { return m_stats.getGkCatching() * getGeneralMultiplier(); }
    float getGkAwareness() { return m_stats.getGkAwareness() * getGeneralMultiplier() * getMentalMultiplier(); }
    float getGkBlocking() { return m_stats.getGkBlocking() * getGeneralMultiplier(); }

    // ==========================================
    // --- GRAPHICS & ANIMATION ---
    // ==========================================
    sf::Sprite getSprite() { return m_sprite; }
    Animator& getAnimator() { return m_animator; }
    void applyPhysicalScale();
    void setRotation(float t_degrees) { m_sprite.setRotation(sf::degrees(t_degrees)); }

    Direction getDirection() const { return m_currentDirection; }
    Direction get8WayDirection(sf::Vector2f targetVector);

    const std::string& getLastDiveDirection() const { return m_lastDiveDirection; }
    void setLastDiveDirection(const std::string& dir) { m_lastDiveDirection = dir; }

    bool usingRightFoot() { return rightFoot; }
    void changeFoot() { rightFoot = !rightFoot; }

    const std::vector<KitLayer>& getKitLayers() const { return m_kitLayers; }
    sf::Color getSkinColor() const { return m_skinColor; }


    // ==========================================
    // --- PUBLIC VARIABLES ---
    // ==========================================
    // Note: Migrating these to protected with getters/setters later is recommended!
    float height = 175.0f;
    float weight = 75.0f;
    float m_possessionTimer = 0.0f;
    bool isChargingAction = false;
    PendingKick m_pendingKick;

protected:
    // ==========================================
    // --- INTERNAL STATE & BIO ---
    // ==========================================
    std::string m_id;
    std::string m_name;
    int m_squadNumber = 0;
    int m_age = 0;
    std::string m_preferredFoot = "Right";
    std::vector<std::string> m_traits;

    // ==========================================
    // --- MATCH STATE & TACTICS ---
    // ==========================================
    Team m_team = Team::Home;
    PositionRole m_matchRole = PositionRole::CenterBack;
    PlayerState m_currentState = PlayerState::Normal;
    Playstyle m_playstyle;
    std::map<PositionRole, int> m_familiarity;

    bool m_isUserOpponent = false;
    int m_foulCount = 0;
    int m_yellowCards = 0;
    bool m_isSentOff = false;

    // ==========================================
    // --- PHYSICS & TIMERS ---
    // ==========================================
    sf::Vector2f m_position;
    sf::Vector2f m_velocity;
    sf::Vector2f m_baseHomePosition;

    float m_collisionRadius = 25.f;
    float m_matchTimeScale = 22.5f;

    bool hasPossession = false;
    bool rightFoot = false;

    float m_stumbleTimer = 0.0f;
    float m_bargeCooldown = 0.0f;

    float m_tackleTimer = 0.0f;
    float m_tackleDuration = 0.4f;
    float m_tackleCooldownTimer = 0.0f;
    const float TACKLE_COOLDOWN_DURATION = 2.0f;
    bool m_tackleAnimTriggered = false;

    // ==========================================
    // --- CORE ATTRIBUTES ---
    // ==========================================
    PlayerStats m_stats;
    int loyalty = 0;
    float m_tacticalFamiliarity = 100.f;
    float m_teamChemistry = 100.f;
    float m_naturalFitness = 100.f;
    float m_matchSharpness = 100.f;
    float m_maxStamina = 100.f;
    float m_currentStamina = 100.f;

    // ==========================================
    // --- GRAPHICS DATA ---
    // ==========================================
    sf::Sprite m_sprite;
    Animator m_animator;
    Direction m_currentDirection = Direction::Down;
    std::string m_lastDiveDirection = "Center";
    std::vector<KitLayer> m_kitLayers;
    sf::Color m_skinColor;

    // ==========================================
    // --- INJURY SYSTEM ---
    // ==========================================
    bool isInjured = false;
    std::string currentInjury = "";
    int injuryDaysRemaining = 0;
    InjurySeverity currentInjurySeverity = InjurySeverity::Knock;
};

// ==========================================
// --- GLOBAL HELPERS ---
// ==========================================
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
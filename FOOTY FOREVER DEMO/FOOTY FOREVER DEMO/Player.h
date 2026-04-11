#pragma once
#include "Entity.h"
#include "PlayerStats.h"
#include "PlayerState.h"
#include "PositionRole.h"
#include "Playstyle.h"
#include "Team.h"
#include "Animator.h"   // <-- NEW
#include "Direction.h"
#include <string>       // <-- NEW for Database strings
#include <vector>       // <-- NEW for Database traits

// Forward declare the database struct (Replace/include the actual header where PlayerData is defined)
struct PlayerData;

class Player : public Entity {
    friend class PhysicsEngine;
public:
    Player(const sf::Texture& texture);

    // --- NEW: INITIALIZATION ---
    // Pass the parsed JSON struct in to set up all stats, bio, and scales at once
    void loadFromData(const PlayerData& data);

    void swapIdentityWith(Player* other);

    // Shared Physics Update (Inertia, Counter-steer, etc.)
    virtual void update(float dt, AnimationServer& animServer);
    void applyPhysicalScale();

    // Shared Combat/Sports Actions
    void startTackle(sf::Vector2f direction);

    // Common Getters/Setters
    sf::Vector2f getPosition() const override { return m_position; }
    void setPosition(sf::Vector2f t_position) { m_position = t_position; }
    sf::Sprite getSprite() { return m_sprite; }

    Animator& getAnimator() { return m_animator; }
    sf::FloatRect getBoundingBox() const override {
        if (m_positionRole == PositionRole::Goalkeeper && m_currentState == PlayerState::Diving) {
            return sf::FloatRect({ m_position.x - 40, m_position.y - (60 + (m_stats.getGkCoverage() * 0.8f)) }, { 80 + (m_stats.getGkCoverage()) * 0.8f, (60 + (m_stats.getGkCoverage() * 0.8f)) * 2});
        }
        else {
            return sf::FloatRect({ m_position.x - 60, m_position.y - 40 }, { 120,80 });
        }
    }

    sf::Vector2f getVelocity() { return m_velocity; }
    void setVelocity(sf::Vector2f t_velocity) { m_velocity = t_velocity; }
    bool getBallPossession() { return hasPossession; }
    void setBallPossession(bool t) { hasPossession = t; }

    virtual sf::Vector2f getAimDirection() const = 0;

    // --- IDENTITY & BIO GETTERS (NEW) ---
    std::string getId() const { return m_id; }
    std::string getName() const { return m_name; }
    int getSquadNumber() const { return m_squadNumber; }
    int getAge() const { return m_age; }
    std::string getPreferredFoot() const { return m_preferredFoot; }
    const std::vector<std::string>& getTraits() const { return m_traits; }
    bool hasTrait(const std::string& traitName) const;
    void triggerFallOver(sf::Vector2f impactVelocity, AnimationServer& animServer);
    void recoverFromFall();

    // --- STAT GETTERS ---
    float getFitness() { return m_stats.getFitness(); }
    int getWeakFootAccuracy() { return m_stats.getWeakFootAccuracy(); }

    float getFinishing() { return m_stats.getFinishing() * getGeneralMultiplier(); }
    float getDeadBall() { return m_stats.getDeadBall() * getGeneralMultiplier(); }
    float getHeading() { return m_stats.getHeading() * getGeneralMultiplier(); }

    float getCurl() { return m_stats.getCurl() * getGeneralMultiplier(); }
    float getBallControl() { return m_stats.getBallControl() * getGeneralMultiplier(); }
    float getBalancing() { return m_stats.getBalancing() * getMovementMultiplier(); }

    float getShortPassing() { return m_stats.getShortPassing() * getGeneralMultiplier(); }
    float getLongPassing() { return m_stats.getLongPassing() * getGeneralMultiplier(); }

    float getAcceleration() { return m_stats.getAccel() * getMovementMultiplier(); }
    float getTopSpeed() { return m_stats.getTopSpeed() * getMovementMultiplier(); }
    float getAgility() { return m_stats.getAgility() * getMovementMultiplier(); }

    float getBodyStrength() { return m_stats.getBodyStrength(); }
    float getKickPower() { return m_stats.getKickPower(); }
    float getJumpingStrength() { return m_stats.getJumpingStrength() * getMovementMultiplier(); }

    float getAwareness() { return m_stats.getAwareness() * getGeneralMultiplier(); }
    float getAggression() { return m_stats.getAggression() * getGeneralMultiplier(); }
    float getBlocking() { return m_stats.getBlocking() * getGeneralMultiplier(); }

    float getGkThrowing() { return m_stats.getGkThrowing() * getGeneralMultiplier(); }
    float getGkCoverage() { return m_stats.getGkCoverage() * getGeneralMultiplier(); }
    float getGkReactions() { return m_stats.getGkReactions() * getGeneralMultiplier(); }
    float getGkCatching() { return m_stats.getGkCatching() * getGeneralMultiplier(); }
    float getGkAwareness() { return m_stats.getGkAwareness() * getGeneralMultiplier(); }
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

    Playstyle getPlaystyle() {  return m_playstyle; }
    Direction get8WayDirection(sf::Vector2f targetVector);
    Direction getDirection() const{ return m_currentDirection; }
    bool canTackle() const { return m_tackleCooldownTimer <= 0.0f; }
    void updateCooldown(float dt) {
        if (m_tackleCooldownTimer > 0.0f) m_tackleCooldownTimer -= dt;
    }
    void startTackleCooldown() { m_tackleCooldownTimer = TACKLE_COOLDOWN_DURATION; }

    PlayerState getState() const { return m_currentState; }
    void setState(PlayerState state) { m_currentState = state; }
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

    void setPositionRole(PositionRole role) { m_positionRole = role; }
    PositionRole getPositionRole() const { return m_positionRole; }
    sf::Vector2f getBaseTacticalCoordinate(bool isHomeTeam, int slotId, const std::vector<std::vector<std::pair<int, PositionRole>>>& layout) const;
    void setBaseHomePosition(sf::Vector2f pos) { m_baseHomePosition = pos; }

    // Update the getter signature (Remove the arguments!)
    sf::Vector2f getHomePosition() const;

    float getWeight() { return weight; }

    // Dynamic dribble tracking (Alternates left/right on touches)
    bool usingRightFoot() { return rightFoot; }
    void changeFoot() { rightFoot = !rightFoot; }

    // Physical footprint variables (Mapped from database in loadFromData)
    float height = 175.0f;
    float weight = 75.0f;
    float m_possessionTimer = 0.0f;

    int getYellowCards() const { return m_yellowCards; }
    bool isSentOff() const { return m_isSentOff; }

    void giveYellowCard() {
        m_yellowCards++;
        if (m_yellowCards >= 2) {
            m_isSentOff = true; // Second yellow equals a Red!
        }
    }
    void giveRedCard() {
        m_isSentOff = true;
    }

    // STAMINA FUNCTIONS
// Setup
    void initializeStamina(float naturalFitness, float matchSharpness);

    // Core Updates
    void updateStamina(float dt, bool isSprinting);
    void deductStaminaAction(float baseAmount); // For tackles, jumps, getting hit

    // Getters for Controllers/UI
    float getCurrentStamina() const { return m_currentStamina; }
    float getMaxStamina() const { return m_maxStamina; }

    // --- DEBUFF CALCULATORS ---
    // Returns a multiplier (e.g., 0.75 to 1.00) to multiply against base stats
    float getMovementMultiplier() const;
    float getGeneralMultiplier() const;

protected:
    sf::Vector2f m_baseHomePosition;
    bool m_tackleAnimTriggered = false;
    sf::Vector2f m_position;
    sf::Vector2f m_velocity;

    int loyalty = 0;

    float m_naturalFitness = 100.f;  // Base capacity (0-100)
    float m_matchSharpness = 100.f;  // Depletion rate modifier (0-100)

    float m_maxStamina = 100.f;      // Scaled directly from NaturalFitness
    float m_currentStamina = 100.f;  // The live gauge

    int m_yellowCards = 0;
    bool m_isSentOff = false;

    // --- GRAPHICS COMPONENTS ---
    sf::Sprite m_sprite;
    Animator m_animator;
    Direction m_currentDirection = Direction::Down;

    // --- IDENTITY & BIO DATA (NEW) ---
    std::string m_id;
    std::string m_name;
    int m_squadNumber = 0;
    int m_age = 0;
    std::string m_preferredFoot = "Right";
    std::vector<std::string> m_traits;

    // --- TACTICAL & STATS ---
    PositionRole m_positionRole = PositionRole::CenterBack;
    Playstyle m_playstyle;
    PlayerStats m_stats;
    PlayerState m_currentState = PlayerState::Normal;
    Team m_team = Team::Home;

    float m_collisionRadius = 25.f;
    float m_stumbleTimer = 0.0f;

    // Dynamic physics state variables
    bool rightFoot = false; // Alternates during dribbling "Step & Tap"
    bool hasPossession = false;

    float m_tackleTimer = 0.0f;
    float m_tackleDuration = 0.4f;
    const float TACKLE_COOLDOWN_DURATION = 2.0f;
    float m_tackleCooldownTimer = 0.0f;

};

// Logic for scaling 0-5 accuracy
inline float getWeakFootPenalty(int wfStars, float& powerMod, float& errorMod) {
    // Power Penalty: 0 stars = 70% power, 5 stars = 100% power
    powerMod = 0.7f + (wfStars / 5.0f) * 0.3f;

    // Accuracy Penalty: 0 stars = 5x error, 5 stars = 1x error
    errorMod = 1.0f + (5.0f - wfStars) * 0.8f;

    // The "Mess Up" Chance (The shank factor)
    float shankChance = (5.0f - wfStars) * 10.0f; // 0 stars = 50%, 5 stars = 0%
    if ((rand() % 100) < shankChance) {
        // WF 0-1 gets a massive extra error, WF 3 gets a medium one
        return 15.0f + (5.0f - wfStars) * 5.0f;
    }
    return 0.0f;
}
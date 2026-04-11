#include "Player.h"
#include <iostream>
#include <cmath>
#include "GameDatabase.h" 
#include "PlaystyleDatabase.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Player::Player(const sf::Texture& texture)
    : m_position(0.f, 0.f),
    m_velocity(0.f, 0.f),
    m_currentState(PlayerState::Normal),
    m_animator(m_sprite),
    m_sprite(texture)
{
    m_sprite.setScale({ 0.26f, 0.26f });
    m_sprite.setOrigin({ 250.f, 250.f });
    m_sprite.setRotation(sf::degrees(90.f));
}

void Player::loadFromData(const PlayerData& data)
{
    m_id = data.id;
    m_name = data.name;
    m_squadNumber = data.squadNumber;
    m_age = data.age;
    m_preferredFoot = data.preferredFoot;
    m_traits = data.traits;
    m_playstyle = PlaystyleDatabase::getPlaystyle(data.playstyle.type);

    height = static_cast<float>(data.heightCm);
    weight = static_cast<float>(data.weightKg);

    m_positionRole = data.positionRole;
    m_stats = data.stats;

    initializeStamina(data.stats.naturalFitness, data.sharpness);
    applyPhysicalScale();
}

bool Player::hasTrait(const std::string& traitName) const
{
    for (const auto& trait : m_traits) {
        if (trait == traitName) return true;
    }
    return false;
}

void Player::applyPhysicalScale()
{
    const float BASE_HEIGHT = 180.0f;
    const float BASE_WEIGHT = 70.0f;
    const float BASE_SCALE = 0.26f;

    float scaleX = BASE_SCALE * (height / BASE_HEIGHT);
    float scaleY = BASE_SCALE * (weight / BASE_WEIGHT);

    m_sprite.setScale(sf::Vector2f(scaleY, scaleX));
}

// ==========================================
// --- NEW: THE FALL OVER SYSTEM ---
// ==========================================
void Player::triggerFallOver(sf::Vector2f impactVelocity, AnimationServer& animServer)
{
    m_currentState = PlayerState::FallOver;
    m_velocity = impactVelocity * 0.8f; // Transfer impact momentum
    m_stumbleTimer = 1.5f; // Re-use stumble timer for the floor duration

    deductStaminaAction(3.0f);
    // Ensure we start from their true physical size so we don't double-shrink them
    applyPhysicalScale();
    sf::Vector2f baseScale = m_sprite.getScale();

    // Determine fall direction
    float len = std::sqrt(impactVelocity.x * impactVelocity.x + impactVelocity.y * impactVelocity.y);
    sf::Vector2f fallDir = (len > 0.f) ? impactVelocity / len : sf::Vector2f(1.f, 0.f);

    float absX = std::abs(fallDir.x);
    float absY = std::abs(fallDir.y);

    // --- PURE VERTICAL (UP / DOWN) ---
    if (absX > 0.85f) {
        if (fallDir.x > 0) {
            // Falling UP
            m_currentDirection = Direction::Up;
            m_sprite.setRotation(sf::degrees(90.f)); // Base 90
            m_sprite.setScale(sf::Vector2f(baseScale.x, baseScale.y * 0.8f));
        }
        else {
            // Falling DOWN
            m_currentDirection = Direction::Up; // Use 'Up' sprite, but rotate it to face down!
            m_sprite.setRotation(sf::degrees(270.f)); // 180 + 90
            m_sprite.setScale(sf::Vector2f(baseScale.x, baseScale.y * 0.8f));
        }
    }
    // --- PURE HORIZONTAL (LEFT / RIGHT) ---
    else if (absY > 0.85f) {
        if (fallDir.y > 0) {
            // Falling RIGHT
            m_currentDirection = Direction::Right;
            m_sprite.setRotation(sf::degrees(180.f)); // 90 + 90
            m_sprite.setScale(sf::Vector2f(baseScale.x * 0.8f, baseScale.y));
        }
        else {
            // Falling LEFT
            m_currentDirection = Direction::Left;
            m_sprite.setRotation(sf::degrees(0.f)); // -90 + 90
            m_sprite.setScale(sf::Vector2f(baseScale.x * 0.8f, baseScale.y));
        }
    }
    // --- DIAGONALS ---
    else {
        if (fallDir.x > 0 && fallDir.y > 0) {
            // Falling UP-RIGHT
            m_currentDirection = Direction::UpRight;
            m_sprite.setRotation(sf::degrees(135.f)); // 45 + 90
            m_sprite.setScale(sf::Vector2f(baseScale.x * 0.9f, baseScale.y * 0.8f));
        }
        else if (fallDir.x > 0 && fallDir.y < 0) {
            // Falling UP-LEFT
            m_currentDirection = Direction::UpLeft;
            m_sprite.setRotation(sf::degrees(45.f)); // -45 + 90
            m_sprite.setScale(sf::Vector2f(baseScale.x * 0.9f, baseScale.y * 0.8f));
        }
        else if (fallDir.x < 0 && fallDir.y > 0) {
            // Falling DOWN-RIGHT
            m_currentDirection = Direction::DownRight;
            m_sprite.setRotation(sf::degrees(225.f)); // 135 + 90
            m_sprite.setScale(sf::Vector2f(baseScale.x * 0.9f, baseScale.y * 0.8f));
        }
        else {
            // Falling DOWN-LEFT
            m_currentDirection = Direction::DownLeft;
            m_sprite.setRotation(sf::degrees(-45.f)); // -135 + 90
            m_sprite.setScale(sf::Vector2f(baseScale.x * 0.9f, baseScale.y * 0.8f));
        }
    }

    // Lock to Frame 1 of the running cycle so they look like they are sprawled out
    const Animation* runAnim = &animServer.getRunningAnimation(m_currentDirection);
    if (runAnim) {
        m_animator.playAnimation(runAnim, false, false, 1);
    }
}

void Player::recoverFromFall()
{
    m_currentState = PlayerState::Normal;
    m_stumbleTimer = 0.f;

    // Snap their rotation back to the default setup
    m_sprite.setRotation(sf::degrees(90.f));

    // Re-apply their genetics to unsquash them!
    applyPhysicalScale();
}


void Player::update(float dt, AnimationServer& animServer)
{
    updateCooldown(dt);

    // ==========================================
    // --- FALL OVER INTERCEPT ---
    // ==========================================
    if (m_currentState == PlayerState::FallOver)
    {
        m_stumbleTimer -= dt;
        m_velocity *= 0.90f; // Slide to a halt in the grass

        // Keep updating position so they physically slide
        m_position += m_velocity * dt;
        m_sprite.setPosition(m_position);

        if (m_stumbleTimer <= 0.f) {
            recoverFromFall();
        }

        // CRITICAL: Return immediately! Do not update normal animations.
        return;
    }

    if (m_currentState == PlayerState::Stumbled)
    {
        m_stumbleTimer -= dt;
        m_velocity *= 0.92f;
        if (m_stumbleTimer <= 0.f)
        {
            m_currentState = PlayerState::Normal;
            m_stumbleTimer = 0.f;
        }
    }

    if (m_currentState == PlayerState::Stunned)
    {
        m_tackleTimer -= dt;
        m_velocity *= 0.80f;
        if (m_tackleTimer <= 0.f)
        {
            m_currentState = PlayerState::Normal;
            m_tackleTimer = 0.f;
        }
    }

    if (m_currentState == PlayerState::Diving)
    {
        m_velocity *= 0.98f;
    }

    if (m_team != Team::Home)
    {
        m_sprite.setColor(sf::Color::Blue);
    }
    if (m_positionRole == PositionRole::Goalkeeper)
    {
        m_sprite.setColor(sf::Color::Green);
    }

    // --- APPLY MOVEMENT ---
    m_position += m_velocity * dt;
    m_sprite.setPosition(m_position);

    float speed = std::sqrt((m_velocity.x * m_velocity.x) + (m_velocity.y * m_velocity.y));
    float BASE_RUN_SPEED = 400.f;

    float animSpeedMultiplier = 1.0f;
    if (speed > 10.f) {
        animSpeedMultiplier = speed / BASE_RUN_SPEED;
        animSpeedMultiplier = std::clamp(animSpeedMultiplier, 0.6f, 2.5f);
    }

    // ==========================================
    // --- TACKLE ANIMATION STATE MACHINE ---
    // ==========================================
    if (m_currentState == PlayerState::Tackling)
    {
        m_tackleTimer -= dt;
        m_velocity *= 0.98f;

        if (!m_tackleAnimTriggered) {
            m_sprite.setTexture(animServer.getTackleTexture());

            int runFrame = m_animator.getCurrentFrameIndex();
            const Animation& tackleAnim = animServer.getTackleAnimation(m_currentDirection, runFrame);

            m_animator.playAnimation(&tackleAnim, false, false, 1);
            m_tackleAnimTriggered = true;
        }

        if (speed < 50.f) {
            m_animator.releaseHold();
        }

        if (m_animator.isFinished()) {
            m_currentState = PlayerState::Normal;
            m_sprite.setTexture(animServer.getPlayerTexture());
            m_tackleTimer = 0.f;
        }

        m_animator.update(sf::seconds(dt), 1.0f);
    }
    else
    {
        if (m_tackleAnimTriggered) {
            m_sprite.setTexture(animServer.getPlayerTexture());
            m_tackleAnimTriggered = false;
            m_tackleTimer = 0.f;
        }

        m_animator.releaseHold();
        m_animator.update(sf::seconds(dt), animSpeedMultiplier);
    }
}

void Player::startTackle(sf::Vector2f direction)
{
    if (m_currentState == PlayerState::Tackling) return;

    m_currentState = PlayerState::Tackling;
    m_tackleAnimTriggered = false;
    m_tackleTimer = m_tackleDuration;
    startTackleCooldown();

    float len = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (len > 0.f) direction /= len;

    float currentSpeed = std::sqrt(m_velocity.x * m_velocity.x + m_velocity.y * m_velocity.y);
    float lungeForce = std::max(currentSpeed * 2.0f, m_stats.getTopSpeed() * 1.5f);

    m_velocity = direction * lungeForce;
    deductStaminaAction(1.5f);
}

sf::FloatRect Player::getTackleHitbox()
{
    sf::Vector2f pos = m_position;
    float reach = 60.0f;
    float width = 40.0f;

    return sf::FloatRect({ pos.x - (width / 2), pos.y - (width / 2) }, { width + reach, width + reach });
}

sf::Vector2f Player::getBaseTacticalCoordinate(bool isHomeTeam, int slotId, const std::vector<std::vector<std::pair<int, PositionRole>>>& layout) const
{
    float pitchCenterY = 3500.f;

    // Y-Axis Offsets
    float cbOffset = 800.f;
    float fullbackOffset = 1800.f;
    float wingbackOffset = 1900.f;
    float wideMidOffset = 2000.f;
    float wingOffset = 2200.f;

    sf::Vector2f pos;

    // ==========================================
    // 1. SET X-AXIS (DEPTH)
    // ==========================================
    switch (m_positionRole) {
    case PositionRole::Goalkeeper:    pos.x = 700.f;  break;

    case PositionRole::CenterBack:    pos.x = 2000.f; break;
    case PositionRole::LeftBack:
    case PositionRole::RightBack:     pos.x = 2500.f; break;
    case PositionRole::LeftWingBack:
    case PositionRole::RightWingBack: pos.x = 3000.f; break;

    case PositionRole::DefensiveMid:  pos.x = 2500.f; break;
    case PositionRole::CenterMid:     pos.x = 3200.f; break;
    case PositionRole::LeftMid:
    case PositionRole::RightMid:      pos.x = 3500.f; break;
    case PositionRole::AttackingMid:  pos.x = 3900.f; break;

    case PositionRole::CenterForward: pos.x = 4150.f; break;
    case PositionRole::Striker:       pos.x = 4400.f; break;
    case PositionRole::LeftWing:
    case PositionRole::RightWing:     pos.x = 4400.f; break;

    default:                          pos.x = 5000.f; break;
    }

    // ==========================================
    // 2. SET Y-AXIS (WIDTH)
    // ==========================================
    // A. Explicitly Wide Roles
    if (m_positionRole == PositionRole::RightBack)          pos.y = pitchCenterY + fullbackOffset;
    else if (m_positionRole == PositionRole::LeftBack)      pos.y = pitchCenterY - fullbackOffset;
    else if (m_positionRole == PositionRole::RightWingBack) pos.y = pitchCenterY + wingbackOffset;
    else if (m_positionRole == PositionRole::LeftWingBack)  pos.y = pitchCenterY - wingbackOffset;
    else if (m_positionRole == PositionRole::RightMid)      pos.y = pitchCenterY + wideMidOffset;
    else if (m_positionRole == PositionRole::LeftMid)       pos.y = pitchCenterY - wideMidOffset;
    else if (m_positionRole == PositionRole::RightWing)     pos.y = pitchCenterY + wingOffset;
    else if (m_positionRole == PositionRole::LeftWing)      pos.y = pitchCenterY - wingOffset;
    else if (m_positionRole == PositionRole::Goalkeeper)    pos.y = pitchCenterY;

    // B. Central Roles (Dynamically spaced based on Formation Layout)
    else {
        int roleCount = 0;
        int myIndex = 0;

        // Count how many players share this exact role, and find our index among them
        for (const auto& line : layout) {
            for (const auto& slot : line) {
                if (slot.second == m_positionRole) {
                    if (slot.first == slotId) myIndex = roleCount;
                    roleCount++;
                }
            }
        }

        if (roleCount <= 1) {
            // Exactly one player (e.g., solo Striker or solo DM) sits dead center
            pos.y = pitchCenterY;
        }
        else {
            // If there are multiple, spread them out symmetrically!
            float spreadSpacing = 1400.f; // Default gap

            if (m_positionRole == PositionRole::CenterBack) spreadSpacing = cbOffset * 2.f; // 1600 gap
            else if (m_positionRole == PositionRole::Striker) spreadSpacing = 1200.f; // Strikers stay closer together

            float totalWidth = (roleCount - 1) * spreadSpacing;
            float startY = pitchCenterY + (totalWidth / 2.f); // Start on the Right side (Positive Y)

            // Our layout arrays process Right-to-Left, so index 0 is Right, index 1 is Left
            pos.y = startY - (myIndex * spreadSpacing);
        }
    }

    // ==========================================
    // 3. MIRROR FOR AWAY TEAM
    // ==========================================
    if (!isHomeTeam) {
        // Mirror the Depth
        pos.x = 10000.f - pos.x;

        // BUG FIX: You must mirror the Width (Y-axis) too! 
        // A "Right Back" is always on the right side from the Goalkeeper's perspective.
        // If the away team is defending the right side of the screen (X=10000), 
        // their Right Back needs to be at the top of the screen (Y-), not the bottom (Y+)!
        pos.y = 7000.f - pos.y;
    }

    return pos;
}

// Replace your old getHomePosition with this:
sf::Vector2f Player::getHomePosition() const {
    sf::Vector2f pos = m_baseHomePosition;

    // Keep your pitch bounds clamping!
    pos.x = std::clamp(pos.x, 600.f, 9400.f);
    pos.y = std::clamp(pos.y, 600.f, 6400.f);

    return pos;
}

Direction Player::get8WayDirection(sf::Vector2f targetVector)
{
    if (std::abs(targetVector.x) < 0.1f && std::abs(targetVector.y) < 0.1f) {
        return m_currentDirection;
    }

    float angle = std::atan2(targetVector.y, targetVector.x) * 180.f / M_PI;
    if (angle < 0) angle += 360.f;

    if (angle >= 337.5f || angle < 22.5f)  return Direction::Right;
    if (angle >= 22.5f && angle < 67.5f)  return Direction::DownRight;
    if (angle >= 67.5f && angle < 112.5f) return Direction::Down;
    if (angle >= 112.5f && angle < 157.5f) return Direction::DownLeft;
    if (angle >= 157.5f && angle < 202.5f) return Direction::Left;
    if (angle >= 202.5f && angle < 247.5f) return Direction::UpLeft;
    if (angle >= 247.5f && angle < 292.5f) return Direction::Up;
    if (angle >= 292.5f && angle < 337.5f) return Direction::UpRight;

    return m_currentDirection;
}

void Player::initializeStamina(float naturalFitness, float matchSharpness) {
    m_naturalFitness = naturalFitness;
    m_matchSharpness = matchSharpness;

    // Natural Fitness dictates the absolute size of the gas tank
    m_maxStamina = m_naturalFitness;
    m_currentStamina = m_maxStamina;
}

void Player::updateStamina(float dt, bool isSprinting) {
    // 100 Sharpness = 1.0x penalty. 0 Sharpness = 2.5x penalty.
    float sharpnessPenalty = 1.0f + ((100.f - m_matchSharpness) / 100.f) * 1.5f;

    if (isSprinting) {
        // Continuous drain while sprinting
        float sprintCostPerSec = 1.0f; // Tune this: 4.0 means ~25 secs of pure sprinting for 100 fitness
        m_currentStamina -= (sprintCostPerSec * sharpnessPenalty) * dt;
    }
    else if (getState() != PlayerState::Diving && getState() != PlayerState::FallOver) {
        // Passive recovery while jogging/idle (Ignores sharpness, so recovery is steady)
        float regenPerSec = 1.0f * (m_matchSharpness / 100.f);
        m_currentStamina += regenPerSec * dt;
    }

    m_currentStamina = std::clamp(m_currentStamina, 0.f, m_maxStamina);
}

void Player::deductStaminaAction(float baseAmount) {
    float sharpnessPenalty = 1.0f + ((100.f - m_matchSharpness) / 100.f) * 1.5f;
    m_currentStamina -= (baseAmount * sharpnessPenalty);
    m_currentStamina = std::max(0.f, m_currentStamina);
}

float Player::getMovementMultiplier() const {
    // 0.0 (Full Stamina) to 1.0 (Completely Exhausted)
    float exhaustion = 1.0f - (m_currentStamina / m_maxStamina);

    // Max 25% debuff -> Exhaustion * 0.25
    return 1.0f - (exhaustion * 0.25f);
}

float Player::getGeneralMultiplier() const {
    float exhaustion = 1.0f - (m_currentStamina / m_maxStamina);

    // Max 10% debuff -> Exhaustion * 0.10
    return 1.0f - (exhaustion * 0.10f);
}

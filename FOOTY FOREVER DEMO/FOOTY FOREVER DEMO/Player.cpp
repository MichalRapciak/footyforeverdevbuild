#include "Player.h"
#include <iostream>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 1. CONSTRUCTOR - This fixes the 'unresolved external' error
Player::Player(const sf::Texture& texture)
    : m_position(0.f, 0.f),
    m_velocity(0.f, 0.f),
    m_currentState(PlayerState::Normal),
    m_animator(m_sprite),
    m_sprite(texture)
{
    m_sprite.setScale({ 0.13f, 0.13f });
    m_sprite.setOrigin({ 500.f, 500.f });
    m_sprite.setRotation(sf::degrees(90.f));

    // The specific texture loading usually happens in the Child classes 
    // (UserPlayer/NPCTeammate) so they can have different kits.
}

void Player::applyPhysicalScale()
{
    // The baseline "default" human in your engine
    const float BASE_HEIGHT = 175.0f;
    const float BASE_WEIGHT = 75.0f;
    const float BASE_SCALE = 0.13f;

    // 1. HEIGHT (X-Axis)
    // If height is 210, ratio is 1.2 (120%). Scale becomes 0.156f.
    // If height is 150, ratio is 0.85 (85%). Scale becomes 0.111f.
    float scaleX = BASE_SCALE * (height / BASE_HEIGHT);

    // 2. WEIGHT / GIRTH (Y-Axis)
    // If weight is 120, ratio is 1.6 (160%). Scale becomes 0.208f.
    // If weight is 50, ratio is 0.66 (66%). Scale becomes 0.086f.
    float scaleY = BASE_SCALE * (weight / BASE_WEIGHT);

    // 3. Apply it to the sprite!
    // Since you mentioned your game's "Up" is the X-axis, X gets height and Y gets weight.
    m_sprite.setScale(sf::Vector2f(scaleX, scaleY));
}

void Player::update(float dt, AnimationServer& animServer)
{
    updateCooldown(dt);

    if (m_currentState == PlayerState::Stumbled)
    {
        // 1. Tick down a specific stumble timer (usually short, 0.4s to 0.7s)
        m_stumbleTimer -= dt;

        // 2. Heavy friction: The player is stumbling, so they lose speed fast
        m_velocity *= 0.92f;

        // 3. Transition back to Normal
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
    // Multiply by dt for frame-rate independent movement
    m_position += m_velocity * dt;
    m_sprite.setPosition(m_position);

    // 1. Calculate physical speed
    float speed = std::sqrt((m_velocity.x * m_velocity.x) + (m_velocity.y * m_velocity.y));
    float BASE_RUN_SPEED = 400.f;

    // 2. Create the multiplier
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
        m_velocity *= 0.98f; // Adjust this float to make the slide go further or stop shorter
        // 1. Swap the texture and start the specific slide sequence
        if (!m_tackleAnimTriggered) {
            m_sprite.setTexture(animServer.getTackleTexture());

            // Figure out which sequence to play based on our current run stride
            int runFrame = m_animator.getCurrentFrameIndex();
            const Animation& tackleAnim = animServer.getTackleAnimation(m_currentDirection, runFrame);

            // Play it, DON'T maintain frame, DON'T loop, HOLD at local frame 1 (the slide)
            m_animator.playAnimation(&tackleAnim, false, false, 1);
            m_tackleAnimTriggered = true;
        }

        // 2. Release the animation hold when they slow down significantly
        if (speed < 50.f) {
            m_animator.releaseHold();
        }

        // 3. When the recovery frames (standing up) finish, return to normal!
        if (m_animator.isFinished()) {
            m_currentState = PlayerState::Normal;
            m_sprite.setTexture(animServer.getPlayerTexture()); // Swap back to running sheet
            m_tackleTimer = 0.f; // Reset physics timer just in case
        }

        // Tick the animator at a fixed 1.0x speed so the tackle animation doesn't fast-forward
        m_animator.update(sf::seconds(dt), 1.0f);
    }
    else
    {
        // SAFEGUARD: If a tackle was forcibly interrupted by a match state change, 
                // we need to clean up the texture and flags!
        if (m_tackleAnimTriggered) {
            m_sprite.setTexture(animServer.getPlayerTexture());
            m_tackleAnimTriggered = false;
            m_tackleTimer = 0.f;
        }

        m_animator.releaseHold();

        // --- NORMAL ANIMATION ---
        // Let the child classes (UserPlayer/NPCPlayer) pick the running/idle animations,
        // but we tick the actual clock forward here in the base class!
        m_animator.update(sf::seconds(dt), animSpeedMultiplier);
    }
}

void Player::startTackle(sf::Vector2f direction)
{
    if (m_currentState == PlayerState::Tackling) return;

    m_currentState = PlayerState::Tackling;
    m_tackleAnimTriggered = false; // Reset flag so the animator picks it up!
    m_tackleTimer = m_tackleDuration;
    startTackleCooldown();

    // Normalize direction
    float len = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (len > 0.f) direction /= len;

    // Lunge Force: Boost current speed or use a minimum burst
    float currentSpeed = std::sqrt(m_velocity.x * m_velocity.x + m_velocity.y * m_velocity.y);
    float lungeForce = std::max(currentSpeed * 1.5f, m_stats.getTopSpeed() * 1.1f);

    m_velocity = direction * lungeForce;
}

sf::FloatRect Player::getTackleHitbox()
{
    sf::Vector2f pos = m_position;
    float reach = 60.0f; // How far the legs extend
    float width = 40.0f; // Width of the slide path

    // Create a box centered on the player that extends in the movement direction
    return sf::FloatRect({ pos.x - (width / 2), pos.y - (width / 2) }, { width + reach, width + reach });
}

sf::Vector2f Player::getBaseTacticalCoordinate(bool isHomeTeam) const
{
    // Basic Pitch Measurements
    float pitchCenterY = 3500.f;
    float cbOffset = 800.f;
    float wingOffset = 2200.f;    // Distance from center to wings
    float fullbackOffset = 1800.f; // Distance from center to fullbacks

    sf::Vector2f pos;

    switch (m_positionRole) {
    case PositionRole::Goalkeeper:   pos = { 700.f,  pitchCenterY }; break;

    case PositionRole::LeftBack:     pos = { 2500.f, pitchCenterY - fullbackOffset }; break;
    case PositionRole::LCenterBack:   pos = { 2000.f, pitchCenterY - cbOffset }; break;
    case PositionRole::RCenterBack:   pos = { 2000.f, pitchCenterY + cbOffset }; break;
    case PositionRole::RightBack:    pos = { 2500.f, pitchCenterY + fullbackOffset }; break;

    case PositionRole::DefensiveMid: pos = { 2500.f, pitchCenterY }; break;
    case PositionRole::CenterMid:    pos = { 3200.f, pitchCenterY }; break;
    case PositionRole::AttackingMid: pos = { 3900.f, pitchCenterY }; break;

    case PositionRole::LeftWing:     pos = { 4400.f, pitchCenterY - wingOffset }; break;
    case PositionRole::RightWing:    pos = { 4400.f, pitchCenterY + wingOffset }; break;
    case PositionRole::Striker:      pos = { 4400.f, pitchCenterY }; break;

    default: pos = { 5000.f, pitchCenterY }; break;
    }

    // If this is the Away team, flip the X-axis so they play right-to-left
    if (!isHomeTeam) {
        pos.x = 10000.f - pos.x;
    }
    return pos;
}

sf::Vector2f Player::getHomePosition(bool isHomeTeam, TeamState teamState) const {
    // 1. Get the standard tactical coordinate for the role
    sf::Vector2f pos = getBaseTacticalCoordinate(isHomeTeam);

    // Safety Clamps: Don't let defenders shift off the pitch or behind the goal
    pos.x = std::clamp(pos.x, 600.f, 9400.f);
    pos.y = std::clamp(pos.y, 600.f, 6400.f);

    return pos;
}

Direction Player::get8WayDirection(sf::Vector2f targetVector)
{
    // If the vector is essentially zero, just keep facing the current direction
    if (std::abs(targetVector.x) < 0.1f && std::abs(targetVector.y) < 0.1f) {
        return m_currentDirection;
    }

    // atan2 gives us the angle in radians, we convert to degrees (0 to 360)
    float angle = std::atan2(targetVector.y, targetVector.x) * 180.f / M_PI;
    if (angle < 0) angle += 360.f;

    // Slice the 360 degrees into 8 chunks
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
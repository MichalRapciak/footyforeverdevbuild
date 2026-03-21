#include "Player.h"
#include <iostream>
#include <cmath>

// 1. CONSTRUCTOR - This fixes the 'unresolved external' error
Player::Player()
    : m_position(0.f, 0.f),
    m_velocity(0.f, 0.f),
    m_currentState(PlayerState::Normal)
{
    // The specific texture loading usually happens in the Child classes 
    // (UserPlayer/NPCTeammate) so they can have different kits.
}

void Player::update(float dt)
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
    if (m_currentState == PlayerState::Tackling)
    {
        // 1. Tick down the timer
        m_tackleTimer -= dt;

        // 2. Transition back to Normal
        // We check if the timer is up OR if the player has slowed down significantly
        float speedSq = m_velocity.x * m_velocity.x + m_velocity.y * m_velocity.y;

        if (m_tackleTimer <= 0.f || speedSq < 5.f) // 100.f is speed 10 squared
        {
            m_currentState = PlayerState::Normal;
            m_tackleTimer = 0.f;

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
    // --- APPLY MOVEMENT ---
    // Multiply by dt for frame-rate independent movement
    m_position += m_velocity * dt;
    //m_sprite.setPosition(m_position);
}

void Player::startTackle(sf::Vector2f direction)
{
    if (m_currentState == PlayerState::Tackling) return;

    m_currentState = PlayerState::Tackling;
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
    // If you have a rotation, you'd use sin/cos here, 
    // but a simple bounding box expansion works for starters:
    return sf::FloatRect({ pos.x - (width / 2), pos.y - (width / 2) }, { width + reach, width + reach });
}

sf::Vector2f Player::getBaseTacticalCoordinate(bool isHomeTeam) const
{
    // Basic Pitch Measurements
    float pitchCenterY = 3500.f;
    float cbOffset = 800.f;
    float wingOffset = 2200.f;    // Distance from center to wings
    float fullbackOffset = 1800.f; // Distance from center to fullbacks

    // We'll define coordinates for a "Home" team (playing left-to-right)
    // Then we flip them if the player is on the "Away" team.
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

    /*
    // 2. Calculate the "Push" or "Pull" based on state
    // Attacking: +15m (1500px), Defending: -10m (1000px)
    float shiftAmount = 0.f;
    if (teamState == TeamState::Attacking) shiftAmount = 1500.f;
    else if (teamState == TeamState::Defending) shiftAmount = -1000.f;

    // 3. Apply the shift based on which way the team is playing
    // Home team plays towards +X, Away team plays towards -X
    pos.x += isHomeTeam ? shiftAmount : -shiftAmount;
    */

    // 4. Safety Clamps: Don't let defenders shift off the pitch or behind the goal
    pos.x = std::clamp(pos.x, 600.f, 9400.f);
    pos.y = std::clamp(pos.y, 600.f, 6400.f);

    return pos;
}
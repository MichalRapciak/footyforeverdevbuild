#include "Player.h"
#include <iostream>
#include <cmath>
#include "GameDatabase.h" 
#include "PlaystyleDatabase.h"
#include "AnimationServer.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Player::Player()
    : m_position(0.f, 0.f),
    m_velocity(0.f, 0.f),
    m_currentState(PlayerState::Normal),
    m_animator(m_sprite),
    m_sprite(AnimationServer::getSkinTexture())
{
    m_sprite.setScale({ 0.26f, 0.26f });
    m_sprite.setOrigin({ 250.f, 250.f });
    m_sprite.setRotation(sf::degrees(90.f));
}

void Player::loadFromData(const PlayerData& data, const TeamData& teamData)
{
    m_id = data.id;
    m_name = data.name;
    m_squadNumber = data.squadNumber;
    m_age = data.age;
    m_preferredFoot = data.preferredFoot;
    m_traits = data.traits;
    m_playstyle = PlaystyleDatabase::getPlaystyle(data.playstyle.type);
    isInjured = data.isInjured;
    currentInjury = data.currentInjury;
    injuryDaysRemaining = data.injuryDaysRemaining;
    currentInjurySeverity = data.currentInjurySeverity;

    height = static_cast<float>(data.heightCm);
    weight = static_cast<float>(data.weightKg);

    m_matchRole = data.positionRole;
    m_familiarity = data.positionFamiliarity;
    m_tacticalFamiliarity = data.tacticalFamiliarity;
    m_stats = data.stats;

    // ==========================================
    // --- BUILD THE KIT STACK (Match Engine) ---
    // ==========================================
    m_skinColor = data.graphics.skinColor;
    m_kitLayers.clear();

    // 1. Face
    if (!data.graphics.faceType.empty() && data.graphics.faceType != "None") {
        m_kitLayers.push_back({ data.graphics.faceType, sf::Color::White });
    }

    // 2. Beard
    if (!data.graphics.beardType.empty() && data.graphics.beardType != "None") {
        m_kitLayers.push_back({ data.graphics.beardType, data.graphics.beardColor });
    }

    // 3. Hair
    if (!data.graphics.hairType.empty() && data.graphics.hairType != "None") {
        m_kitLayers.push_back({ data.graphics.hairType, data.graphics.hairColor });
    }

    // 4. Kits (Socks -> Shorts -> Shirt)
    for (const auto& layer : teamData.socksLayers) m_kitLayers.push_back(layer);
    for (const auto& layer : teamData.shortsLayers) m_kitLayers.push_back(layer);
    for (const auto& layer : teamData.shirtLayers) {
        KitLayer shirtLayer = layer;
        if (m_matchRole == PositionRole::Goalkeeper) {
            shirtLayer.color = sf::Color(50, 200, 50); // GK Override
        }
        m_kitLayers.push_back(shirtLayer);
    }

    // Bind the shared static skin texture just to establish bounds! 
    m_sprite.setTexture(AnimationServer::getSkinTexture());

    m_currentDirection = Direction::Down;
    const Animation& startAnim = AnimationServer::getRunningAnimation(m_currentDirection);
    m_animator.playAnimation(&startAnim, false, true);
    m_sprite.setTextureRect(startAnim.frames[0]);

    initializeStamina(data.stats.naturalFitness, data.sharpness);
    applyPhysicalScale();
}

void Player::swapIdentityWith(Player* other) {
    if (!other || other == this) return;

    // 1. Spatial & Physics Data
    std::swap(this->m_position, other->m_position);
    std::swap(this->m_velocity, other->m_velocity);
    std::swap(this->z, other->z);
    std::swap(this->vz, other->vz);
    std::swap(this->m_baseHomePosition, other->m_baseHomePosition);
    std::swap(this->m_currentDirection, other->m_currentDirection);
    std::swap(this->m_currentState, other->m_currentState);

    // 2. Timers & Dynamic States
    std::swap(this->m_stumbleTimer, other->m_stumbleTimer);
    std::swap(this->m_tackleTimer, other->m_tackleTimer);
    std::swap(this->m_tackleCooldownTimer, other->m_tackleCooldownTimer);
    std::swap(this->m_tackleAnimTriggered, other->m_tackleAnimTriggered);
    std::swap(this->hasPossession, other->hasPossession);
    std::swap(this->rightFoot, other->rightFoot);
    std::swap(this->m_possessionTimer, other->m_possessionTimer);

    std::swap(this->m_tackleAnimTriggered, other->m_tackleAnimTriggered);
    if (this->m_currentState == PlayerState::Tackling) {
        this->m_tackleAnimTriggered = false;
    }
    else {
        this->m_tackleAnimTriggered = true;
    }
    if (other->m_currentState == PlayerState::Tackling) {
        other->m_tackleAnimTriggered = false;
    }
    else {
        other->m_tackleAnimTriggered = true;
    }

    // 3. Bio & DNA
    std::swap(this->m_id, other->m_id);
    std::swap(this->m_name, other->m_name);
    std::swap(this->m_squadNumber, other->m_squadNumber);
    std::swap(this->m_age, other->m_age);
    std::swap(this->m_preferredFoot, other->m_preferredFoot);
    std::swap(this->m_traits, other->m_traits);
    std::swap(this->height, other->height);
    std::swap(this->weight, other->weight);

    // 4. Tactical & Stats
    std::swap(this->m_matchRole, other->m_matchRole);
    std::swap(this->m_playstyle, other->m_playstyle);
    std::swap(this->m_stats, other->m_stats);
    std::swap(this->m_team, other->m_team);
    std::swap(this->m_tacticalFamiliarity, other->m_tacticalFamiliarity);

    // 5. Stamina & Match Condition
    std::swap(this->m_naturalFitness, other->m_naturalFitness);
    std::swap(this->m_matchSharpness, other->m_matchSharpness);
    std::swap(this->m_maxStamina, other->m_maxStamina);
    std::swap(this->m_currentStamina, other->m_currentStamina);
    std::swap(this->m_yellowCards, other->m_yellowCards);
    std::swap(this->m_isSentOff, other->m_isSentOff);

    // Injury info
    std::swap(this->isInjured, other->isInjured);
    std::swap(this->currentInjury, other->currentInjury);
    std::swap(this->injuryDaysRemaining, other->injuryDaysRemaining);
    std::swap(this->currentInjurySeverity, other->currentInjurySeverity);


    // 6. Graphics Sync (Apply the new heights and visually snap the sprites)

    std::swap(this->m_skinColor, other->m_skinColor);
    std::swap(this->m_kitLayers, other->m_kitLayers);

    this->m_sprite.setTexture(AnimationServer::getSkinTexture());
    other->m_sprite.setTexture(AnimationServer::getSkinTexture());

    this->applyPhysicalScale();
    other->applyPhysicalScale();

    this->m_sprite.setPosition(this->m_position);
    other->m_sprite.setPosition(other->m_position);

    // 7. Rebind the Animators!
    if (this->m_currentState != PlayerState::Tackling) {
        this->m_animator.playAnimation(&AnimationServer::getRunningAnimation(this->m_currentDirection), true, true);
    }
    if (other->m_currentState != PlayerState::Tackling) {
        other->m_animator.playAnimation(&AnimationServer::getRunningAnimation(other->m_currentDirection), true, true);
    }
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
    const float BASE_WEIGHT = 80.0f;
    const float BASE_SCALE = 0.26f;

    float scaleX = BASE_SCALE * (height / BASE_HEIGHT);
    float scaleY = BASE_SCALE * (weight / BASE_WEIGHT);

    m_sprite.setScale(sf::Vector2f(scaleY, scaleX));
}

// ==========================================
// --- NEW: THE FALL OVER SYSTEM ---
// ==========================================
void Player::triggerFallOver(sf::Vector2f impactVelocity)
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
    const Animation* runAnim = &AnimationServer::getRunningAnimation(m_currentDirection);
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


void Player::update(float dt)
{
    updateCooldown(dt);

    if (m_bargeCooldown > 0.0f) {
        m_bargeCooldown -= dt;
    }

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

    // --- APPLY MOVEMENT ---
    m_position += m_velocity * dt;
    m_sprite.setPosition(m_position);

    float speed = std::sqrt((m_velocity.x * m_velocity.x) + (m_velocity.y * m_velocity.y));
    float BASE_RUN_SPEED = 400.f;

    float animSpeedMultiplier = 1.0f;

    // ==========================================
    // --- THE FIX: ANIMATION DEADZONE ---
    // ==========================================
    if (speed < 10.f && m_currentState != PlayerState::Tackling) {
        // Player is practically stationary. 
        // Stop the animation clock entirely so they don't jitter!
        animSpeedMultiplier = 0.0f;

        // Force the animator to reset to the standing frame (Frame 0)
        m_animator.setFrame(0);

        // Force the sprite to draw the idle frame!
        const Animation& idleAnim = AnimationServer::getRunningAnimation(m_currentDirection);
        m_sprite.setTextureRect(idleAnim.frames[0]);
    }
    else {
        // Player is moving. Scale the animation speed dynamically!
        animSpeedMultiplier = speed / BASE_RUN_SPEED;
        animSpeedMultiplier = std::clamp(animSpeedMultiplier, 0.8f, 2.0f);
    }

    // ==========================================
    // --- TACKLE ANIMATION STATE MACHINE ---
    // ==========================================
    if (m_currentState == PlayerState::Tackling)
    {
        m_tackleTimer -= dt;
        m_velocity *= 0.985f;

        if (!m_tackleAnimTriggered) {
            m_sprite.setTexture(AnimationServer::getTackleTexture());

            int runFrame = m_animator.getCurrentFrameIndex();
            const Animation& tackleAnim = AnimationServer::getTackleAnimation(m_currentDirection, runFrame);

            m_animator.playAnimation(&tackleAnim, false, false, 1);
            m_tackleAnimTriggered = true;
        }

        if (speed < 50.f) {
            m_animator.releaseHold();
        }

        if (m_animator.isFinished()) {
            m_currentState = PlayerState::Normal;
            m_sprite.setTexture(AnimationServer::getSkinTexture());
            m_tackleTimer = 0.f;
        }

        m_animator.update(sf::seconds(dt), 1.0f);
    }
    else
    {
        if (m_tackleAnimTriggered) {
            m_sprite.setTexture(AnimationServer::getSkinTexture());
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
    float lungeForce = std::max(currentSpeed * 1.5f, m_stats.getTopSpeed() * 1.50f);

    m_velocity = direction * lungeForce;
    deductStaminaAction(1.5f);
}

sf::FloatRect Player::getTackleHitbox()
{
    sf::Vector2f pos = m_position;
    sf::Vector2f vel = getVelocity(); // Read the raw physics velocity

    // 1. Dynamic Reach: Base 40px + (Height * 0.2). 
    // A 175cm player gets 75px reach. A 195cm player gets 79px reach.
    float reach = 40.0f + (height * 0.2f);

    // 2. The thickness of the player's body sliding on the grass
    float thickness = 40.0f;

    // Default bounds
    float left = pos.x;
    float top = pos.y;
    float boxWidth = thickness;
    float boxHeight = thickness;

    // ==========================================
    // --- VELOCITY-BASED DIRECTION OVERRIDE ---
    // ==========================================
    // Default to where they are looking if they are standing completely still
    Direction tackleDir = m_currentDirection;

    float speedSq = vel.x * vel.x + vel.y * vel.y;

    // If the player has actual momentum, calculate the true physics direction!
    if (speedSq > 2.0f) {
        // atan2 gives us the angle in radians. Convert to degrees (0 to 360)
        float angle = std::atan2(vel.y, vel.x) * 180.f / 3.14159f;
        if (angle < 0.f) angle += 360.f;

        // Map the 360 degree circle into 8 exact 45-degree slices based on your World Axes
        if (angle >= 337.5f || angle < 22.5f)  tackleDir = Direction::Up;         // +X
        else if (angle >= 22.5f && angle < 67.5f)  tackleDir = Direction::UpRight;    // +X, +Y
        else if (angle >= 67.5f && angle < 112.5f) tackleDir = Direction::Right;      // +Y
        else if (angle >= 112.5f && angle < 157.5f) tackleDir = Direction::DownRight;  // -X, +Y
        else if (angle >= 157.5f && angle < 202.5f) tackleDir = Direction::Down;       // -X
        else if (angle >= 202.5f && angle < 247.5f) tackleDir = Direction::DownLeft;   // -X, -Y
        else if (angle >= 247.5f && angle < 292.5f) tackleDir = Direction::Left;       // -Y
        else if (angle >= 292.5f && angle < 337.5f) tackleDir = Direction::UpLeft;     // +X, -Y
    }

    // 3. Shift the box explicitly in the 8 directions using the calculated momentum!
    // NOTE: Because the camera is rotated 90 degrees clockwise, 
    // Visual UP = World +X (Right)
    // Visual RIGHT = World +Y (Down)
    // Visual DOWN = World -X (Left)
    // Visual LEFT = World -Y (Up)

    switch (tackleDir) {
    case Direction::Up: // Visual Up -> World Right (+X)
        left = pos.x;
        top = pos.y - (thickness / 2.f);
        boxWidth = reach;
        boxHeight = thickness;
        break;

    case Direction::Right: // Visual Right -> World Down (+Y)
        left = pos.x - (thickness / 2.f);
        top = pos.y;
        boxWidth = thickness;
        boxHeight = reach;
        break;

    case Direction::Down: // Visual Down -> World Left (-X)
        left = pos.x - reach;
        top = pos.y - (thickness / 2.f);
        boxWidth = reach;
        boxHeight = thickness;
        break;

    case Direction::Left: // Visual Left -> World Up (-Y)
        left = pos.x - (thickness / 2.f);
        top = pos.y - reach;
        boxWidth = thickness;
        boxHeight = reach;
        break;

        // DIAGONALS (Multiplier 0.85f to prevent oversized corners)
    case Direction::UpRight: // Visual UpRight -> World Right & Down (+X, +Y)
        left = pos.x;
        top = pos.y;
        boxWidth = reach * 0.85f;
        boxHeight = reach * 0.85f;
        break;

    case Direction::DownRight: // Visual DownRight -> World Left & Down (-X, +Y)
        left = pos.x - (reach * 0.85f);
        top = pos.y;
        boxWidth = reach * 0.85f;
        boxHeight = reach * 0.85f;
        break;

    case Direction::DownLeft: // Visual DownLeft -> World Left & Up (-X, -Y)
        left = pos.x - (reach * 0.85f);
        top = pos.y - (reach * 0.85f);
        boxWidth = reach * 0.85f;
        boxHeight = reach * 0.85f;
        break;

    case Direction::UpLeft: // Visual UpLeft -> World Right & Up (+X, -Y)
        left = pos.x;
        top = pos.y - (reach * 0.85f);
        boxWidth = reach * 0.85f;
        boxHeight = reach * 0.85f;
        break;
    }

    // Return the correctly constructed SFML 3 FloatRect
    return sf::FloatRect({ left, top }, { boxWidth, boxHeight });
}

bool Player::executeShoulderBarge(Player* target) {
    if (m_bargeCooldown > 0.0f || !target) return false;

    sf::Vector2f toTarget = target->getPosition() - getPosition();
    float distSq = toTarget.x * toTarget.x + toTarget.y * toTarget.y;

    // Must be within close physical proximity (e.g., 150 pixels / 1.5m)
    if (distSq > 22500.f) return false;

    float dist = std::sqrt(distSq);
    sf::Vector2f bargeDir = toTarget / dist;

    // ==========================================
    // --- STAT-DRIVEN PHYSICS ---
    // ==========================================
    // Base power + (Body Strength * 3) + (Aggression * 1.5)
    // A strong, aggressive CB will literally launch a weak winger.
    float power = 250.f + (m_stats.bodyStrength * 3.0f) + (m_stats.aggression * 1.5f);

    // Apply the sudden impulse to OUR velocity. 
    // The existing collision engine will transfer this momentum to the target!
    m_velocity += bargeDir * power;

    // Lock out the ability so they can't spam it and fly off the pitch
    m_bargeCooldown = 1.5f;

    // Optional: Barging is exhausting! Tax their stamina slightly.
    m_currentStamina = std::max(0.0f, m_currentStamina - 2.0f);

    return true;
}

void Player::checkInjury(float impactForce) {
    // If they are already seriously injured, don't overwrite it with a minor knock
    if (isInjured && currentInjurySeverity == InjurySeverity::Severe) return;

    float resNorm = getInjuryResistance() / 100.0f;
    float stamNorm = getCurrentStamina() / getMaxStamina();

    // 1. Normalize the impact force (assuming 1000.f is a massive, bone-crunching tackle)
    float impactNorm = std::clamp(impactForce / 1000.f, 0.0f, 1.0f);

    // 2. The Fatigue Multiplier
    // A player at 100% stamina has a 1.0x risk. A player at 0% stamina has a 2.5x risk!
    float fatigueMultiplier = 1.0f + ((1.0f - stamNorm) * 1.5f);

    // 3. The Resistance Shield
    // 99 Injury Resistance blocks 80% of all injury math. 
    // 1 Injury Resistance blocks almost nothing.
    float resistanceShield = 1.0f - (resNorm * 0.8f);

    // 4. Calculate Final Risk Percentage
    // Max theoretical risk here is ~15% on a single massive tackle when completely exhausted with 0 resistance.
    float injuryRiskPercent = (impactNorm * fatigueMultiplier * resistanceShield) * 15.0f;

    // Roll the dice (0.0 to 100.0)
    float roll = (rand() % 10000) / 100.f;

    if (roll < injuryRiskPercent) {
        applyRandomInjury(impactNorm);
    }
}

void Player::applyRandomInjury(float impactNorm) {
    std::vector<InjuryType> validInjuries;

    // Filter injuries based on the tackle impact. 
    // You can't tear your ACL from a tiny 100.f bump!
    for (const auto& inj : InjuryDatabase) {
        if (inj.severity == InjurySeverity::Severe && impactNorm < 0.6f) continue;
        validInjuries.push_back(inj);
    }

    if (validInjuries.empty()) validInjuries = InjuryDatabase; // Fallback

    // Pick a random injury from the valid pool
    int idx = rand() % validInjuries.size();
    InjuryType selected = validInjuries[idx];

    // Apply to PlayerData
    isInjured = true;
    currentInjury = selected.name;
    currentInjurySeverity = selected.severity;

    // Calculate randomized duration within the bounds
    int durationRange = selected.maxDays - selected.minDays;
    injuryDaysRemaining = selected.minDays + (durationRange > 0 ? (rand() % durationRange) : 0);

    // ==========================================
        // --- THE SEVERE INJURY TRIGGER ---
        // ==========================================
    if (selected.severity == InjurySeverity::Severe) {
        // 1. Play the horrific scream audio


        // 2. Lock them in the injured state so they writhe on the floor
        setState(PlayerState::Injured);

        // 3. Kill all their momentum so they don't slide around while screaming
        setVelocity({ 0.f, 0.f });
    }
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
    switch (m_matchRole) {
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
    if (m_matchRole == PositionRole::RightBack)          pos.y = pitchCenterY + fullbackOffset;
    else if (m_matchRole == PositionRole::LeftBack)      pos.y = pitchCenterY - fullbackOffset;
    else if (m_matchRole == PositionRole::RightWingBack) pos.y = pitchCenterY + wingbackOffset;
    else if (m_matchRole == PositionRole::LeftWingBack)  pos.y = pitchCenterY - wingbackOffset;
    else if (m_matchRole == PositionRole::RightMid)      pos.y = pitchCenterY + wideMidOffset;
    else if (m_matchRole == PositionRole::LeftMid)       pos.y = pitchCenterY - wideMidOffset;
    else if (m_matchRole == PositionRole::RightWing)     pos.y = pitchCenterY + wingOffset;
    else if (m_matchRole == PositionRole::LeftWing)      pos.y = pitchCenterY - wingOffset;
    else if (m_matchRole == PositionRole::Goalkeeper)    pos.y = pitchCenterY;

    // B. Central Roles (Dynamically spaced based on Formation Layout)
    else {
        int roleCount = 0;
        int myIndex = 0;

        // Count how many players share this exact role, and find our index among them
        for (const auto& line : layout) {
            for (const auto& slot : line) {
                if (slot.second == m_matchRole) {
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

            if (m_matchRole == PositionRole::CenterBack) spreadSpacing = cbOffset * 2.f; // 1600 gap
            else if (m_matchRole == PositionRole::Striker) spreadSpacing = 1200.f; // Strikers stay closer together

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

    // Use the cached time scale!
    float timeScaleMod = m_matchTimeScale / 22.5f;

    if (isSprinting) {
        // Continuous drain while sprinting
        float sprintCostPerSec = 1.0f * timeScaleMod;
        m_currentStamina -= (sprintCostPerSec * sharpnessPenalty) * dt;
    }
    else if (getState() != PlayerState::Diving && getState() != PlayerState::FallOver) {
        // Passive recovery while jogging/idle
        float regenPerSec = 1.0f * timeScaleMod * (m_matchSharpness / 100.f);
        m_currentStamina += regenPerSec * dt;
    }

    m_currentStamina = std::clamp(m_currentStamina, 0.f, m_maxStamina);
}

void Player::deductStaminaAction(float baseAmount) {
    float sharpnessPenalty = 1.0f + ((100.f - m_matchSharpness) / 100.f) * 1.5f;

    // Use the cached time scale!
    float timeScaleMod = m_matchTimeScale / 22.5f;

    m_currentStamina -= (baseAmount * timeScaleMod * sharpnessPenalty);
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

void Player::setState(PlayerState newState)
{
    // If the state isn't actually changing, do nothing
    if (m_currentState == newState) return;

    // ==========================================
    // --- 1. EXIT STATE LOGIC (Cleanup) ---
    // ==========================================
    if (m_currentState == PlayerState::FallOver)
    {
        // We are being forced out of a fall prematurely (e.g. Referee blew the whistle).
        // Manually trigger the visual recovery so we don't get stuck sideways!
        m_sprite.setRotation(sf::degrees(90.f));
        applyPhysicalScale();
        m_stumbleTimer = 0.f;
    }
    else if (m_currentState == PlayerState::Tackling)
    {
        // If the referee teleports us while we are mid-slide tackle, 
        // force the update loop to instantly reset our texture back to running/standing.
        m_tackleAnimTriggered = true;
        m_tackleTimer = 0.f;
    }

    // ==========================================
    // --- 2. APPLY NEW STATE ---
    // ==========================================
    m_currentState = newState;
}

int Player::getRoleProficiency(PositionRole role) const {
    auto it = m_familiarity.find(role);
    if (it != m_familiarity.end()) {
        return it->second;
    }
    // If the position isn't in their map at all, they have no idea what they are doing!
    return 1;
}

float Player::getAwareness() const {
    float finalAwareness = m_stats.getAwareness();
    int proficiency = getRoleProficiency(m_matchRole);

    switch (proficiency) {
    case 4: break; // Mastered: 0% Penalty
    case 3: finalAwareness *= 0.80f; break; // Mostly Knows: 20% Penalty
    case 2: finalAwareness *= 0.60f; break; // Rough Idea: 40% Penalty
    case 1: finalAwareness *= 0.40f; break; // Completely Unable: 60% Penalty
    }

    return finalAwareness * getGeneralMultiplier() * getMentalMultiplier();
}

// ==========================================
// --- MID-MATCH SUBSTITUTION (RE-SKINNING) ---
// ==========================================
void Player::applySubstitution(const PlayerData& newPlayerData, const TeamData& teamData)
{
    // 1. Re-use your existing data loader to completely overwrite their DNA!
    // This updates name, stats, traits, injury status, height, weight, and resets max stamina.
    loadFromData(newPlayerData, teamData);

    // 2. Scrub the Physics Engine States
    m_currentState = PlayerState::Normal;
    m_velocity = { 0.f, 0.f };
    z = 0.f;
    vz = 0.f;
    hasPossession = false;

    // 3. Scrub the Timers
    m_stumbleTimer = 0.f;
    m_tackleTimer = 0.f;
    m_tackleCooldownTimer = 0.f;
    m_possessionTimer = 0.f;

    // 4. Scrub the Match Discipline
    m_yellowCards = 0;
    m_isSentOff = false;

    // 5. Visual Safety Resets
    // If the outgoing player was writhing on the floor injured, or mid-slide tackle,
    // we need to make sure the incoming sub is standing upright and rendering normally!
    m_sprite.setRotation(sf::degrees(90.f));
    m_tackleAnimTriggered = false;
}

float Player::getMentalMultiplier() const {
    // Base is 1.0. 
    // Low familiarity penalizes stats by up to 10%.
    // High chemistry boosts stats by up to 5%.
    float familiarityPenalty = (100.f - m_tacticalFamiliarity) / 100.f * 0.10f;
    float chemistryBonus = (m_teamChemistry / 100.f) * 0.10f;

    return 1.0f - familiarityPenalty + chemistryBonus;
}
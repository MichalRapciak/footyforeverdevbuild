#include "Ball.h"
#include "Player.h"
#include "UserPlayer.h"

Ball::Ball() : sprite(texture)
{
    if (!texture.loadFromFile("Assets/images/ball.png", false, sf::IntRect({ 0,0 }, { 100, 100 }))) // if texture doesnt load, output text
    {
        std::cout << "Texture not loaded." << std::endl;
    }
    sprite.setTexture(texture);
    sprite.setTextureRect(sf::IntRect({ 0,0 }, { 100,100 }));
    sprite.setOrigin({ 50,50 });
    sprite.setScale({ 0.24f,0.24f });
    sprite.setPosition({ 5000,3000 });
    shape.setRadius(12.f);
    shape.setFillColor(sf::Color::White);
    shape.setOrigin({ 12.f,12.f });
    shape.setPosition({ 5000,3500 });

    shadow.setRadius(12.f);
    shadow.setFillColor(sf::Color(0, 0, 0, 150)); // semi-transparent black
    shadow.setOrigin({ 12.f,12.f });
    shape.setPosition({ 5000,3500 });

    velocity = { 0.f, 0.f };
    z = 0.f;
    vz = 0.f;
}

void Ball::update(float dt)
{
    // Route the logic based on possession
    if (owner != nullptr) {
        updateDribbling(dt);
    }
    else {
        updateFreePhysics(dt);
    }
}

void Ball::updateDribbling(float dt)
{
    // --- 1. PLAYER STATE & DIRECTION ---
    sf::Vector2f playerPos = owner->getPosition();
    sf::Vector2f playerVel = owner->getVelocity();
    float playerSpeed = std::sqrt(playerVel.x * playerVel.x + playerVel.y * playerVel.y);
    float topSpeed = owner->getTopSpeed() * 10.0f;
    float speedFactor = std::clamp(playerSpeed / topSpeed, 0.0f, 1.0f);

    sf::Vector2f forward;
    if (playerSpeed > 5.0f) {
        forward = playerVel / playerSpeed;
    }
    else {
        forward = owner->getAimDirection();
    }
    sf::Vector2f lateral = { -forward.y, forward.x };

    float bcNorm = owner->getBallControl() / 100.0f;
    float errorFactor = 1.0f - bcNorm; 

    // --- 2. THE DRIBBLE LEASH ---
    sf::Vector2f toBall = shape.getPosition() - playerPos;
    float dist = std::sqrt(toBall.x * toBall.x + toBall.y * toBall.y);
    sf::Vector2f toBallNorm = (dist > 0.f) ? toBall / dist : sf::Vector2f{ 0.f, 0.f };
    float cosAngle = forward.x * toBallNorm.x + forward.y * toBallNorm.y;

    float frontMax = 120.f + (bcNorm * 60.f);
    float backMax = 60.f + (bcNorm * 40.f);
    float maxDist = backMax + (frontMax - backMax) * ((cosAngle + 1.f) / 2.f);

    // Added a 40px buffer to the leash so temporary physics stretching doesn't break possession
    if (dist > maxDist + 40.f) {
        release();
        return;
    }

    // --- 3. THE "STEP & TAP" CYCLE ---
    float stepInterval = 0.5f - (speedFactor * 0.25f);
    static sf::Vector2f stepError = { 0.f, 0.f };

    footTimer += dt;

    if (footTimer >= stepInterval) {
        footTimer = 0.f;          

        if (playerSpeed > 50.f) {
            float randAngle = ((rand() % 360) * 3.14159f) / 180.f;
            float maxErrorDist = 35.f * errorFactor; 
            float randMag = (rand() % 100) / 100.f * maxErrorDist;
            stepError = sf::Vector2f(std::cos(randAngle) * randMag, std::sin(randAngle) * randMag);
            
            // Removed the harsh horizontal velocity dampening that was fighting the player's momentum
        }
        else {
            stepError = { 0.f, 0.f }; 
        }
    }

    // --- 4. CALCULATING THE DYNAMIC OFFSET ---
    float stepProgress = footTimer / stepInterval;

    float basePush = 15.f; 
    float maxPushExt = 20.f + (errorFactor * 50.f); 

    float dynamicForwardPush = basePush + (maxPushExt * speedFactor * (1.0f - stepProgress));

    float footSpread = (playerSpeed < 50.f) ? 12.f : 22.f;
    float sideOffset = owner->usingRightFoot() ? footSpread : -footSpread;

    sf::Vector2f desiredPos = playerPos + (forward * dynamicForwardPush) + (lateral * sideOffset) + stepError;

    // ==========================================
    // --- 5. FIXED SPRING PHYSICS ---
    // ==========================================
    sf::Vector2f toTarget = desiredPos - shape.getPosition();
    dist = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);

    float controlMultiplier = 0.6f + (bcNorm * 1.4f);
    float springStrength = 12.0f * controlMultiplier; 

    // The target velocity is simply the player's speed PLUS the spring trying to close the gap
    sf::Vector2f targetVel = playerVel + (toTarget * springStrength);
    
    // Smoothly interpolate the ball's current velocity toward the target velocity
    float damping = 0.85f; // Higher damping = smoother dribbling
    velocity = (velocity * damping) + (targetVel * (1.0f - damping));

    // --- 6. APPLY MOVEMENT & AIR PHYSICS ---
    shape.move(velocity * dt);

    if (z > 0.f || vz != 0.f) {
        vz -= gravity * dt;
        z += vz * dt;

        if (z < 0.f) {
            z = 0.f;
            vz = 0.f;
            velocity *= 0.85f; 
        }
    }

    // --- VISUAL SCALING ---
    float t = std::min(z / 300.f, 1.f);
    float scale = minScale + (maxScale - minScale) * t;
    shape.setScale({ scale, scale });
    shadow.setPosition(shape.getPosition());

    // THE SHADOW FIX: Shrink the shadow along the X-axis instead of Y
    shadow.setScale({ 1.f - (t * 0.5f), 1.f });
}

// =========================================================
// --- PRIVATE HELPER: FREE PHYSICS ---
// =========================================================
void Ball::updateFreePhysics(float dt)
{
    float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    float dynamicGravity = gravity;

    // --- 1. THE MAGNUS EFFECT (AIR ONLY) ---
    if (z > 5.f && speed > 50.f && std::abs(spin) > 0.1f)
    {
        sf::Vector2f perpendicular(-velocity.y, velocity.x);
        perpendicular /= speed;

        float spinStrength = 12.0f * (1.0f + z / 100.f);
        velocity += perpendicular * spin * spinStrength * dt;

        float newSpeed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
        velocity = (velocity / newSpeed) * speed;
    }

    // ==========================================
    // FIX 1: THE LIFT NERF
    // ==========================================
    if (z > 5.f && speed > 100.f && bs > 0.1f)
    {
        // Lowered the multiplier by 10x so it requires immense spin to lift
        float liftForce = (bs * speed * 0.0005f);

        // Cap the lift at 25% of gravity (Max 245px/s lift)
        // This ensures the ball ALWAYS feels 75% of Earth's gravity, dropping naturally!
        liftForce = std::min(liftForce, gravity * 0.25f);

        vz += liftForce * dt;
    }

    // ==========================================
    // FIX 2: REALISTIC AERODYNAMIC DRAG (THE DRAG CRISIS)
    // ==========================================
    // We calculate the true 3D speed of the ball (X, Y, and Z axes combined)
    float trueSpeed = std::sqrt((speed * speed) + (vz * vz));

    if (z > 0.f && trueSpeed > 5.f)
    {
        // 1. Calculate Dynamic Drag Coefficient (Cd)
        // Laminar Flow (Slow ball): Cd is high (~0.25)
        // Turbulent Flow (Fast ball): Cd drops significantly (~0.12)
        float Cd = 0.25f;

        // The transition happens between roughly 12m/s (1200px) and 18m/s (1800px)
        if (trueSpeed > 1200.f) {
            float t = std::clamp((trueSpeed - 1200.f) / 600.f, 0.0f, 1.0f);
            // Smoothly interpolate from 0.25 down to 0.12 as speed increases
            Cd = 0.25f - (0.13f * t);
        }

        // 2. Calculate Aerodynamic Drag Force
        // Formula: F = 0.5 * AirDensity * Cd * Area * Velocity^2
        // We bake the constant physics properties (density, area, mass) into a single game-feel multiplier (0.0003f)
        float dragDecel = (trueSpeed * trueSpeed) * Cd * 0.0003f;

        // 3. Apply Deceleration Proportionally to 3D Vectors
        float newTrueSpeed = std::max(0.f, trueSpeed - dragDecel * dt);

        if (trueSpeed > 0.1f) {
            float dragRatio = newTrueSpeed / trueSpeed;

            // Slow down the horizontal speed
            speed *= dragRatio;

            // Slow down the vertical speed (This naturally acts as terminal velocity!)
            vz *= dragRatio;

            // Re-apply the slowed speed to the X/Y velocity vector
            float currentLen = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
            if (currentLen > 0.1f) {
                velocity = (velocity / currentLen) * speed;
            }
        }
    }

    // --- 2. FRICTION & SPIN DECAY ---
    float currentSpinFriction = (z <= 0.f) ? 150.0f : 0.5f;

    // Decay side spin
    if (spin > 0) spin = std::max(0.f, spin - currentSpinFriction * dt);
    else if (spin < 0) spin = std::min(0.f, spin + currentSpinFriction * dt);

    // Decay backspin (it lasts longer in the air than on the ground)
    float bsDecay = (z <= 0.f) ? 200.f : 10.f;
    bs = std::max(0.f, bs - bsDecay * dt);

    if (z <= 0.f && speed > 0.f) {
        float decel = friction * dt;
        speed = std::max(0.f, speed - decel);
        velocity = (velocity / std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y)) * speed;
    }

    // --- 3. MOVEMENT & PITCH BOUNDARIES ---
    shape.move(velocity * dt);

    const float pitchWidth = 10000.f;
    const float pitchHeight = 7000.f;
    const float bounciness = 0.5f;
    const float radius = 12.f;

    if (shape.getPosition().x - radius < 0.f) {
        shape.setPosition({ radius, shape.getPosition().y });
        velocity.x = -velocity.x * bounciness;
    }
    else if (shape.getPosition().x + radius > pitchWidth) {
        shape.setPosition({ pitchWidth - radius, shape.getPosition().y });
        velocity.x = -velocity.x * bounciness;
    }

    if (shape.getPosition().y - radius < 0.f) {
        shape.setPosition({ shape.getPosition().x, radius });
        velocity.y = -velocity.y * bounciness;
    }
    else if (shape.getPosition().y + radius > pitchHeight) {
        shape.setPosition({ shape.getPosition().x , pitchHeight - radius });
        velocity.y = -velocity.y * bounciness;
    }

    // --- 4. AIR PHYSICS & BOUNCING ---
    if (z > 0.f || vz != 0.f) {

        // REMOVED: The old "currentGravity += speed * 50" hack is gone!
        // Our new 3D air drag handles vertical slowing perfectly and naturally.

        vz -= gravity * dt;
        z += vz * dt;

        if (z < 0.f) {
            z = 0.f;
            spin = 0.f;
            // --- THE BACKSPIN BRAKE ---
            if (bs > 20.f && speed > 100.f) {
                float biteAmount = (bs / 100.f) * 0.3f;
                velocity *= (1.0f - biteAmount);
                vz = -vz * (0.05f + (bs / 200.f));
            }
            else {
                vz = -vz * 0.35f;
                velocity *= 0.8f;
            }

            if (vz < 15.f) vz = 0.f;
        }
    }

    // --- VISUAL SCALING ---
    float t = std::min(z / 300.f, 1.f);
    float scale = minScale + (maxScale - minScale) * t;
    shape.setScale({ scale, scale });
    shadow.setPosition(shape.getPosition());

    // THE SHADOW FIX: Shrink the shadow along the X-axis instead of Y
    shadow.setScale({ 1.f - (t * 0.5f), 1.f });
}

void Ball::draw(sf::RenderWindow& window)
{
    // 1. Draw shadow at the ACTUAL ground position
        // No need to setPosition here if you already set it in update()
    window.draw(shadow);

    // 2. Calculate the visual (elevated) position for the ball
    // We use a local variable so we don't mess up the ball's real position
    sf::Vector2f groundPos = shape.getPosition();
    sf::Vector2f visualPos = { groundPos.x + z, groundPos.y  };

    // 3. Move the VISUALS only
    sprite.setPosition(visualPos);
    sprite.setScale(shape.getScale() * 0.24f);

    // If you use 'shape' as a placeholder/glow:
    shape.setPosition(visualPos);

    window.draw(shape);
    window.draw(sprite);

    // 4. IMPORTANT: Reset shape to ground so update() logic stays correct
    shape.setPosition(groundPos);
}

void Ball::possess(Player* player)
{
    if (player->isTackling() == false && z <= 40 && player->getState() != PlayerState::Stunned)
    {
        owner = player;
        owner->setBallPossession(true);
        owner->changeFoot();
        footTimer = 0.f;
        velocity = { 0,0 };
    }
}

void Ball::release()
{
    if (owner)
    {
        velocity = owner->getVelocity();
        owner->setBallPossession(false);
        lastOwner = owner;
        owner = nullptr;
    }
    // THE EXPONENTIAL SPEED BOMB IS DELETED.
    // No more velocity *= 1.4f; here!
}

void Ball::shoot(const sf::Vector2f& direction, float power, float kickSpin, float v0z, float backspin)
{
    // 1. Release the owner if there is one
    if (owner)
    {
        owner->setBallPossession(false);
        lastOwner = owner;
        owner = nullptr;
    }

    // 2. APPLY PHYSICS EVEN IF THERE IS NO OWNER (For loose-ball volleys!)
    velocity = direction * (power * 52);
    spin = kickSpin;
    vz = (v0z < 100.f) ? 100.f : v0z;
    bs = backspin;

    // 3. FIX: Do NOT set z = 0 here! That ruins aerial shots.
    // Instead, just give it a microscopic bump so it registers as "in the air" and escapes grass friction.
}

bool Ball::hasOwner() const
{
    return owner != nullptr;
}

Player* Ball::getOwner() const
{
    return owner;
}

sf::Vector2f Ball::getPosition() const
{
    return shape.getPosition();
}

void Ball::setPosition(const sf::Vector2f& pos)
{
    shape.setPosition(pos);
}

void Ball::applyImpulse(sf::Vector2f force)
{
    float forceSpeed = std::sqrt(force.x * force.x + force.y * force.y);

    // 1. THE FLICKER SHIELD
    // Only strip possession if the collision is violent (>300 force). 
    // This stops the dribbler's own body from ripping the ball away from their feet!
    if (owner && forceSpeed > 300.f)
    {
        if (owner->getPositionRole() != PositionRole::Goalkeeper) {
            owner->setBallPossession(false);
            lastOwner = owner;
            owner = nullptr;
        }
    }

    // If someone is still safely dribbling, ignore this bump entirely!
    if (owner) return;

    // 2. NORMAL PHYSICS
    velocity += force / 2.f;

    // Only pop up on the Z-axis if the ball is on the ground AND the hit was hard
    if (z < 15.f && forceSpeed > 300.f) {
        vz = 50.0f + (forceSpeed / 3.0f);
    }

    // 3. HARD SPEED LIMIT
    float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    float maxBallSpeed = 1300.f;

    if (speed > maxBallSpeed)
    {
        velocity = (velocity / speed) * maxBallSpeed;
    }
}

sf::Vector2f Ball::reflect(const sf::Vector2f& velocity, const sf::Vector2f& normal)
{
    float dot = (velocity.x * normal.x) + (velocity.y * normal.y);

    // 1. THE PING-PONG TRAP SHIELD
    // If dot >= 0, the ball is ALREADY moving away from the object!
    // Do NOT reflect it again, or it will reverse straight back into the wall.
    if (dot >= 0.f) {
        return velocity;
    }

    // 2. THE ENERGY SINK (Restitution)
    // Hitting a rigid object like a crossbar or a player absorbs kinetic energy.
    float restitution = 0.45f; // Retain only 45% of the speed

    // Apply the perfect reflection formula, then instantly dampen it
    return (velocity - 2.f * dot * normal) * restitution;
}
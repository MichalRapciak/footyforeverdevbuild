#include "Ball.h"
#include "Player.h"
#include "NPCPlayer.h"
#include "UserPlayer.h"
#include "PhysicsEngine.h"

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
        if (m_isSetPiece) {
            // DEAD BALL: The ball is glued to the ground, not the foot.
            velocity = { 0.f, 0.f };
            shadow.setPosition(shape.getPosition());

            // FIX: Use minScale instead of hardcoding 1.0f to prevent visual popping
            shape.setScale({ minScale, minScale });
            shadow.setScale({ 1.0f, 1.0f });

            // FIX: Explicitly kill Z-height during dead balls
            z = 0.f;
            vz = 0.f;
        }
        else {
            updateDribbling(dt);
        }
    }
    else {
        updateFreePhysics(dt);
    }

    float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    if (speed > maxSpeed)
    {
        velocity /= speed;
        speed = maxSpeed;
        velocity *= speed; 
    }

}

void Ball::updateDribbling(float dt)
{
    if (!owner) return;

    // ==========================================
    // --- THE FIX 2: THE COOLDOWN LOCK (NPC SAFE) ---
    // ==========================================
    // Safely cast the generic Player pointer to an NPCPlayer.
    // If it's a UserPlayer, npcOwner will be nullptr and it skips the check!
    if (NPCPlayer* npcOwner = dynamic_cast<NPCPlayer*>(owner)) {
        if (npcOwner->getKickCooldown() > 0.0f) {
            return; // Lock the dribble motor while the NPC is kicking
        }
    }

    // --- 1. PLAYER STATE & DIRECTION ---
    sf::Vector2f playerCenter = owner->getPosition();

    // Grab the exact current scale of this specific player (e.g., 0.13 * height)
    sf::Vector2f playerScale = owner->getSprite().getScale();

    // Start at the center (500, 500)
    sf::Vector2f feetPos = playerCenter;

    // Shift 200 raw pixels down the sprite's local vertical axis (Y) to hit the boots,
    // perfectly multiplied by whatever their dynamic height scale is!
    feetPos.x -= 150.0f * std::abs(playerScale.x);

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
    // Use feetPos instead of the center!
    sf::Vector2f toBall = shape.getPosition() - feetPos;
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
        }
        else {
            stepError = { 0.f, 0.f };
        }
    }

    // --- 4. CALCULATING THE DYNAMIC OFFSET ---
    float stepProgress = footTimer / stepInterval;

    float basePush = 25.f;
    float maxPushExt = 40.f + (errorFactor * 50.f);

    float dynamicForwardPush = basePush + (maxPushExt * speedFactor * (1.0f - stepProgress));

    float footSpread = (playerSpeed < 50.f) ? 12.f : 22.f;
    float sideOffset = owner->usingRightFoot() ? footSpread : -footSpread;

    // ANCHOR TO THE FEET!
    sf::Vector2f desiredPos = feetPos + (forward * dynamicForwardPush) + (lateral * sideOffset) + stepError;

    // ==========================================
        // --- 5. SMOOTH "TAP AND ROLL" PHYSICS ---
        // ==========================================
    sf::Vector2f toTarget = desiredPos - shape.getPosition();
    dist = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);

    float controlMultiplier = 0.8f + (bcNorm * 1.2f);

    // 1. THE GOLDILOCKS SPRING
    // Strong enough to follow the player, soft enough to absorb the "teleporting" footstep target
    float springStrength = 18.0f * controlMultiplier;

    // The ball matches the player's base speed, plus the pull of the spring
    sf::Vector2f targetVel = playerVel + (toTarget * springStrength);

    // ==========================================
    // --- THE FIX 3: SPEED LIMITER ---
    // ==========================================
    // The ball should never travel significantly faster than the player's top speed 
    // while it is attached to their feet!
    float targetSpeed = std::sqrt(targetVel.x * targetVel.x + targetVel.y * targetVel.y);
    float absoluteMaxSpeed = topSpeed * 1.5f; // Cap dribble snaps at 150% of player sprint speed

    if (targetSpeed > absoluteMaxSpeed) {
        targetVel = (targetVel / targetSpeed) * absoluteMaxSpeed;
    }

    // 2. THE SMOOTHING DAMPING
    // 0.75f gives us enough "slide" to make the ball roll smoothly between touches,
    // but prevents it from feeling laggy or getting left behind.
    float damping = 0.75f;

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
    const float PITCH_WIDTH = 10000.f;
    const float PITCH_HEIGHT = 7000.f;

    // Ask the physics engine to calculate the 4 stages of free-flight!
    PhysicsEngine::applyBallAerodynamics(*this, dt);
    PhysicsEngine::applyBallFrictionAndSpin(*this, dt);
    PhysicsEngine::updateBallPositionAndBounds(*this, dt, PITCH_WIDTH, PITCH_HEIGHT);
    PhysicsEngine::applyBallGravityAndBounce(*this, dt);

    // --- VISUAL SCALING (Keep this here, since scaling is purely visual for the Ball class!) ---
    float t = std::min(z / 600.f, 1.f);
    float scale = minScale + (maxScale - minScale) * t;
    shape.setScale({ scale, scale });
    shadow.setPosition(shape.getPosition());
    shadow.setScale({ 1.f - (t * 0.5f), 1.f });
}

void Ball::draw(sf::RenderWindow& window)
{
    sf::Vector2f groundPos = shape.getPosition();

    // --- 1. DYNAMIC FLOODLIGHT SHADOWS ---
    float zRatio = std::min(z / 2000.f, 1.f);
    float airFade = std::max(0.f, 1.0f - (z / 150.f));
    float shadowScale = 1.f - (zRatio * 0.5f);
    float currentRadius = 12.f * shadowScale; // Ball radius is 12

    if (airFade > 0.01f)
    {
        sf::Vector2f lights[4] = {
            {-500.f, -500.f},
            {-500.f, 7500.f},
            {10500.f, -500.f},
            {10500.f, 7500.f}
        };

        const float lightHeight = 6000.f;
        const float ballHeight = 24.f; // The physical diameter of the ball
        const float maxLightDist = 12500.f;

        for (int i = 0; i < 4; ++i)
        {
            sf::Vector2f toBall = groundPos - lights[i];
            float distXY = std::sqrt(toBall.x * toBall.x + toBall.y * toBall.y);

            if (distXY > 0.1f) {
                sf::Vector2f dir = toBall / distXY;
                sf::Vector2f normal(-dir.y, dir.x);

                float totalHeight = ballHeight + z;
                float length = totalHeight * (distXY / lightHeight);
                length = std::max(8.f, length);

                float normalizedDist = std::min(distXY / maxLightDist, 1.0f);
                float intensity = std::pow(1.0f - normalizedDist, 2.0f);

                // Ball cast shadows are slightly fainter than player shadows
                std::uint8_t alpha = static_cast<std::uint8_t>(45 * intensity * airFade);

                if (alpha < 2) continue;

                sf::Color baseColor(0, 0, 0, alpha);
                sf::Color tipColor(0, 0, 0, 0);

                float diffusion = 1.2f + (normalizedDist * 3.0f);
                float width = 8.f * shadowScale; // Narrower shadow for the ball

                sf::Vector2f start = groundPos + (dir * (currentRadius - 2.f));

                sf::VertexArray floodShadow(sf::PrimitiveType::TriangleStrip, 4);
                floodShadow[0].position = start + normal * width;
                floodShadow[0].color = baseColor;
                floodShadow[1].position = start - normal * width;
                floodShadow[1].color = baseColor;
                floodShadow[2].position = start + (dir * length) + normal * (width * diffusion);
                floodShadow[2].color = tipColor;
                floodShadow[3].position = start + (dir * length) - normal * (width * diffusion);
                floodShadow[3].color = tipColor;

                window.draw(floodShadow);
            }
        }
    }

    // 2. Draw ambient shadow at the ACTUAL ground position
    window.draw(shadow);

    // 3. Calculate the visual (elevated) position for the ball
    sf::Vector2f visualPos = { groundPos.x + (z / 2.25f), groundPos.y };
    // 4. Move the VISUALS only
    sprite.setPosition(visualPos);
    sprite.setScale(shape.getScale() * 0.24f);
    shape.setPosition(visualPos);

    window.draw(shape);
    window.draw(sprite);

    // 5. IMPORTANT: Reset shape to ground so update() logic stays correct
    shape.setPosition(groundPos);
}

void Ball::possess(Player* player)
{
    // Valid possession check
    if (!player) {
        // If we passed nullptr, we treat it as releasing the ball
        owner = nullptr;
        return;
    }

    if (player->isTackling() == false && z <= 40 && player->getState() != PlayerState::Stunned)
    {
        if (owner != nullptr && owner != player) {
            owner->setBallPossession(false);
            lastOwner = owner;
        }

        owner = player;
        owner->setBallPossession(true);

        if (lastTouch != nullptr && lastTouch->getTeam() == player->getTeam() && lastTouch != player) {
            passCompletedEvent = true;
        }

        lastTouch = player;

        // --- SET INITIAL FOOT STATE ---
        bool prefersRight = (owner->getPreferredFoot() == "Right");
        if (!prefersRight && owner->usingRightFoot()) {
            owner->changeFoot();
        }
        else if (prefersRight && !owner->usingRightFoot()) {
            owner->changeFoot();
        }

        // ==========================================
        // --- THE FIX 1: SNAP TO FEET ---
        // ==========================================
        // Prevent the "Rubber-Band" acceleration bug by instantly snapping 
        // the ball to the player's feet the moment they gain possession.
        sf::Vector2f playerScale = owner->getSprite().getScale();
        sf::Vector2f feetPos = owner->getPosition();
        feetPos.x -= 150.0f * std::abs(playerScale.x); // Shift down to boots

        shape.setPosition(feetPos);

        // Reset physics
        footTimer = 0.f;
        velocity = { 0, 0 };
        vz = 0.f;
        z = 0.f;
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
    m_isSetPiece = false;
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
    z = 1.0f; // <--- You wrote the comment but forgot to write this line!
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
    if (m_isSetPiece) return;
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
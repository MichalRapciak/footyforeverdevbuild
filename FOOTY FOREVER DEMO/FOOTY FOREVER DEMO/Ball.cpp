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

    if (NPCPlayer* npcOwner = dynamic_cast<NPCPlayer*>(owner)) {
        if (npcOwner->getKickCooldown() > 0.0f) {
            return;
        }
    }

    // --- 1. PLAYER STATE & DIRECTION ---
    sf::Vector2f playerCenter = owner->getPosition();
    sf::Vector2f playerScale = owner->getSprite().getScale();
    sf::Vector2f feetPos = playerCenter;
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

    // --- 2. THE LEASH (Fixed Turnovers) ---
    sf::Vector2f toBall = shape.getPosition() - feetPos;
    float dist = std::sqrt(toBall.x * toBall.x + toBall.y * toBall.y);
    float cosAngle = (dist > 0.f) ? ((toBall.x / dist) * forward.x + (toBall.y / dist) * forward.y) : 0.f;

    float frontMax = 140.f + (bcNorm * 60.f);
    float backMax = 80.f + (bcNorm * 40.f);
    float maxDist = backMax + (frontMax - backMax) * ((cosAngle + 1.f) / 2.f);

    float leashLimit = std::max(220.f, maxDist + 100.f);
    if (dist > leashLimit) {
        release();
        return;
    }

    // ==========================================
    // --- 3. ACTIVE FOOT TAP LOGIC ---
    // ==========================================
    int currentFrame = owner->getAnimator().getCurrentFrameIndex();
    bool isRightFoot = owner->usingRightFoot();
    bool isTapFrame = false;

    if (isRightFoot && currentFrame == 7) {
        isTapFrame = true;
    }
    else if (!isRightFoot && (currentFrame == 3 || currentFrame == 11)) {
        isTapFrame = true;
    }

    if (currentFrame == 0) m_lastDribbleFrame = -1;

    if (isTapFrame && currentFrame != m_lastDribbleFrame) {
        m_lastDribbleFrame = currentFrame;

        // ==========================================
        // THE FIX 1: THE PHYSICAL WHIFF
        // ==========================================
        // If the ball has swung too far wide during a sharp cut, 
        // the foot swing misses completely! 
        float maxLegReach = 65.f + (bcNorm * 40.f); // 65px to 105px reach
        if (m_isFirstTouch) maxLegReach += 30.f; // Lunge allowance

        if (dist > maxLegReach) {
            // WHIFF! Let the ball coast smoothly on its current trajectory
            velocity *= 0.965f;
            m_isFirstTouch = false;
        }
        else {
            sf::Vector2f stepError = { 0.f, 0.f };
            if (playerSpeed > 50.f) {
                float randAngle = ((rand() % 360) * 3.14159f) / 180.f;
                float maxErrorDist = 20.f * (errorFactor * errorFactor);
                float randMag = (rand() % 100) / 100.f * maxErrorDist;
                stepError = sf::Vector2f(std::cos(randAngle) * randMag, std::sin(randAngle) * randMag);
            }

            float basePush = 14.f + (12.f * errorFactor);
            float sprintPush = 40.f * speedFactor * (1.0f + errorFactor);
            float dynamicForwardPush = basePush + sprintPush;

            float footSpread = (playerSpeed < 50.f) ? 8.f : 16.f;
            float sideOffset = isRightFoot ? footSpread : -footSpread;

            float turnDot = (m_lastDribbleDir.x * forward.x) + (m_lastDribbleDir.y * forward.y);
            float turnSeverity = std::max(0.0f, 1.0f - turnDot);

            if (m_isFirstTouch) {
                dynamicForwardPush = 14.f + (25.f * speedFactor);
                sideOffset *= 0.2f;
                m_isFirstTouch = false;
            }
            else {
                if (turnSeverity > 0.15f) {
                    dynamicForwardPush *= (1.0f + (turnSeverity * 0.4f));
                }
            }
            m_lastDribbleDir = forward;

            sf::Vector2f desiredPos = feetPos + (forward * dynamicForwardPush) + (lateral * sideOffset) + stepError;
            sf::Vector2f targetDiff = desiredPos - shape.getPosition();

            // ==========================================
            // THE FIX 2: THE GLANCING BLOW
            // ==========================================
            // If they are stretching to reach a ball that is wide, they can't put full 
            // power into redirecting it. The touch becomes a weak "glancing blow".
            float diffLen = std::sqrt(targetDiff.x * targetDiff.x + targetDiff.y * targetDiff.y);
            float kickSnap = 8.0f + (8.0f * speedFactor);

            if (diffLen > 45.f) {
                // Reduce snap strength drastically if the adjustment is huge
                kickSnap *= (45.f / diffLen);
            }

            sf::Vector2f pureKickVel = playerVel + (targetDiff * kickSnap);

            // ==========================================
            // THE FIX 3: MOMENTUM BLENDING
            // ==========================================
            // Prevent telepathy by retaining the ball's existing rolling momentum based on turn severity.
            // Straight lines = keep 15%. Sharp cuts = keep up to 85% of the old rolling speed!
            float momentumResistance = 0.7f - (bcNorm * 0.4f); // Good players kill momentum better
            float oldMomentumWeight = 0.15f + (turnSeverity * momentumResistance);
            oldMomentumWeight = std::clamp(oldMomentumWeight, 0.15f, 0.85f);

            velocity = (velocity * oldMomentumWeight) + (pureKickVel * (1.0f - oldMomentumWeight));
        }
    }
    else {
        // --- 3.5. THE ANTI-GLUE MICRO-BUMP ---
        if (playerSpeed > 10.f) {
            float footSpread = (playerSpeed < 50.f) ? 8.f : 16.f;
            float sideOffset = owner->usingRightFoot() ? footSpread : -footSpread;

            if (cosAngle < -0.05f || dist < 20.f) {
                sf::Vector2f microTarget = feetPos + (forward * 22.f) + (lateral * sideOffset);
                sf::Vector2f toMicro = microTarget - shape.getPosition();
                velocity = playerVel + (toMicro * 10.0f);
            }
            else {
                velocity *= 0.965f;
            }
        }
        else {
            float footSpread = 10.f;
            float sideOffset = owner->usingRightFoot() ? footSpread : -footSpread;
            sf::Vector2f idleTarget = feetPos + (forward * 14.f) + (lateral * sideOffset);

            sf::Vector2f pull = idleTarget - shape.getPosition();
            velocity += pull * 15.0f * dt;
            velocity *= 0.85f;
        }
    }

    // --- 4. APPLY MOVEMENT ---
    float targetSpeed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    float absoluteMaxSpeed = topSpeed * 2.5f;

    if (targetSpeed > absoluteMaxSpeed) {
        velocity = (velocity / targetSpeed) * absoluteMaxSpeed;
    }

    shape.move(velocity * dt);

    // --- 5. AIR PHYSICS & VISUALS ---
    if (z > 0.f || vz != 0.f) {
        vz -= gravity * dt;
        z += vz * dt;

        if (z < 0.f) {
            z = 0.f;
            vz = 0.f;
            velocity *= 0.85f;
        }
    }

    float t = std::min(z / 300.f, 1.f);
    float scale = minScale + (maxScale - minScale) * t;
    shape.setScale({ scale, scale });
    shadow.setPosition(shape.getPosition());
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

        // ==========================================
                // --- NEW: DYNAMIC RECEIVING FOOT ---
                // ==========================================
                // Figure out which side the ball is physically arriving on
        sf::Vector2f forward = owner->getAimDirection();
        sf::Vector2f lateral = { -forward.y, forward.x };
        sf::Vector2f toBall = shape.getPosition() - owner->getPosition();

        bool ballArrivingOnRight = (toBall.x * lateral.x + toBall.y * lateral.y) > 0;

        // Force the player to trap it with the foot closest to the ball!
        if (ballArrivingOnRight && !owner->usingRightFoot()) {
            owner->changeFoot();
        }
        else if (!ballArrivingOnRight && owner->usingRightFoot()) {
            owner->changeFoot();
        }

        // ==========================================
        // --- SNAP TO FEET ---
        // ==========================================
        sf::Vector2f playerScale = owner->getSprite().getScale();
        sf::Vector2f feetPos = owner->getPosition();
        feetPos.x -= 150.0f * std::abs(playerScale.x);

        m_isLooseControl = false;
        shape.setPosition(feetPos);

        // --- THE FIX: SETUP FIRST TOUCH ---
        m_isFirstTouch = true;
        m_lastDribbleDir = owner->getAimDirection();

        // Reset physics
        m_lastDribbleFrame = -1;
        velocity = owner->getVelocity();
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
    m_isLooseControl = false;
    // THE EXPONENTIAL SPEED BOMB IS DELETED.
    // No more velocity *= 1.4f; here!
}

void Ball::shoot(const sf::Vector2f& direction, float power, float kickSpin, float v0z, float backspin)
{
    // 1. Release the owner if there is one (WITH PHYSICAL REACH CHECK)
    if (owner)
    {
        // --- PHYSICAL REACH & ANGLE CHECK ---
        sf::Vector2f playerScale = owner->getSprite().getScale();
        sf::Vector2f feetPos = owner->getPosition();
        feetPos.x -= 150.0f * std::abs(playerScale.x);

        sf::Vector2f toBall = shape.getPosition() - feetPos;
        float distSq = toBall.x * toBall.x + toBall.y * toBall.y;

        float maxReachSq = 120.f * 120.f;
        bool whiffed = false;

        // Check Distance & Angle
        if (distSq > maxReachSq) {
            whiffed = true;
        }
        else if (distSq > 0.001f) {
            float dist = std::sqrt(distSq);
            sf::Vector2f toBallNorm = toBall / dist;
            sf::Vector2f forward = owner->getAimDirection();

            float dot = (toBallNorm.x * forward.x) + (toBallNorm.y * forward.y);
            if (dot < -0.1f) whiffed = true;
        }

        // ==========================================
        // THE FIX: GUARANTEED POSSESSION STRIP
        // ==========================================
        // Whether they hit it clean or swung at thin air, they lose possession!
        owner->setBallPossession(false);
        lastOwner = owner;
        owner = nullptr;
        m_isLooseControl = false;

        // If they whiffed, ABORT the kick physics! 
        // The ball will smoothly continue its previous momentum in free physics.
        if (whiffed) return;
    }

    // Only wake the ball up if the kick actually connected!
    m_isSetPiece = false;

    // 2. APPLY PHYSICS 
    velocity = direction * (power * 52);
    spin = kickSpin;
    vz = (v0z < 100.f) ? 100.f : v0z;
    bs = backspin;

    // 3. AIR ESCAPE
    z = 1.0f;
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
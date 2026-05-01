#include "PhysicsEngine.h"
#include "Player.h"
#include "Ball.h"
#include "NPCPlayer.h"
#include "Pitch.h"
#include <cmath>
#include <algorithm>
#include "MatchReferee.h"
#include "AnimationServer.h"
#include "SoundManager.h"
#include "MatchStatistics.h"

// ==========================================
// 1. BALL PHYSICS
// ==========================================

void PhysicsEngine::applyBallAerodynamics(Ball& ball, float dt) {
    float speed = std::sqrt(ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y);
    float trueSpeed = std::sqrt((speed * speed) + (ball.vz * ball.vz));

    // 1. THE MAGNUS EFFECT (Curve)
        // THE FIX: Removed "ball.z > 5.f" so the ball can grip the grass and curve on the ground!
    if (speed > 50.f && std::abs(ball.spin) > 0.1f) {
        sf::Vector2f perpendicular(-ball.velocity.y, ball.velocity.x);
        perpendicular /= speed;

        float heightFactor = std::clamp(ball.z / 400.f, 0.0f, 1.0f);
        float altitudeDampener = 1.0f - (heightFactor * 0.8f);
        float speedFactor = std::clamp(speed / 1000.f, 0.2f, 1.0f);

        // Ground passes grip the grass and curve slightly more aggressively!
        float gripMultiplier = (ball.z <= 5.f) ? 1.35f : 1.0f;
        float spinStrength = 15.0f * altitudeDampener * speedFactor * gripMultiplier;

        ball.velocity += perpendicular * ball.spin * spinStrength * dt;

        float newSpeed = std::sqrt(ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y);
        ball.velocity = (ball.velocity / newSpeed) * speed;
    }

    // 2. THE LIFT NERF (Backspin lift)
    if (ball.z > 5.f && speed > 100.f && ball.bs > 0.1f) {
        float liftForce = (ball.bs * speed * 0.0005f);
        liftForce = std::min(liftForce, ball.gravity * 0.40f);
        ball.vz += liftForce * dt;
    }

    // 3. AERODYNAMIC DRAG
    if (ball.z > 0.f && trueSpeed > 5.f) {
        float Cd = 0.25f;
        if (trueSpeed > 1200.f) {
            float t = std::clamp((trueSpeed - 1200.f) / 600.f, 0.0f, 1.0f);
            Cd = 0.25f - (0.13f * t);
        }

        float dragDecel = (trueSpeed * trueSpeed) * Cd * 0.0003f;
        float newTrueSpeed = std::max(0.f, trueSpeed - dragDecel * dt);

        if (trueSpeed > 0.1f) {
            float dragRatio = newTrueSpeed / trueSpeed;
            speed *= dragRatio;

            float currentLen = std::sqrt(ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y);
            if (currentLen > 0.1f) {
                ball.velocity = (ball.velocity / currentLen) * speed;
            }
        }
    }
}

void PhysicsEngine::applyBallFrictionAndSpin(Ball& ball, float dt) {
    float speed = std::sqrt(ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y);

    // Decay Side Spin
    // THE FIX: Reduced ground spin friction from 150.f to 35.f! 
    // Now a spin of 90 will last for ~2.5 seconds, allowing long ground passes to bend.
    float currentSpinFriction = (ball.z <= 0.f) ? 35.0f : 0.5f;
    if (ball.spin > 0) ball.spin = std::max(0.f, ball.spin - currentSpinFriction * dt);
    else if (ball.spin < 0) ball.spin = std::min(0.f, ball.spin + currentSpinFriction * dt);

    // Decay Backspin
    float bsDecay = (ball.z <= 0.f) ? 200.f : 10.f;
    ball.bs = std::max(0.f, ball.bs - bsDecay * dt);

    // Ground Friction
    if (ball.z <= 0.f && speed > 0.f) {
        float decel = ball.friction * dt;
        speed = std::max(0.f, speed - decel);
        if (speed > 0.f) {
            ball.velocity = (ball.velocity / std::sqrt(ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y)) * speed;
        }
        else {
            ball.velocity = { 0.f, 0.f };
        }
    }
}

void PhysicsEngine::updateBallPositionAndBounds(Ball& ball, float dt, float pitchWidth, float pitchHeight) {
    ball.shape.move(ball.velocity * dt);

    const float bounciness = 0.5f;
    const float radius = 12.f;
    sf::Vector2f pos = ball.shape.getPosition();

    if (pos.x - radius < 0.f) {
        pos.x = radius;
        ball.velocity.x = -ball.velocity.x * bounciness;
    }
    else if (pos.x + radius > pitchWidth) {
        pos.x = pitchWidth - radius;
        ball.velocity.x = -ball.velocity.x * bounciness;
    }

    if (pos.y - radius < 0.f) {
        pos.y = radius;
        ball.velocity.y = -ball.velocity.y * bounciness;
    }
    else if (pos.y + radius > pitchHeight) {
        pos.y = pitchHeight - radius;
        ball.velocity.y = -ball.velocity.y * bounciness;
    }

    ball.shape.setPosition(pos);
}

void PhysicsEngine::applyBallGravityAndBounce(Ball& ball, float dt) {
    if (ball.z > 0.f || ball.vz != 0.f) {
        ball.vz -= ball.gravity * dt;
        ball.z += ball.vz * dt;

        if (ball.z < 0.f) {
            ball.z = 0.f;
            ball.spin = 0.f;

            float speed = std::sqrt(ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y);

            // The Backspin Brake
            if (ball.bs > 20.f && speed > 100.f) {
                float biteAmount = (ball.bs / 100.f) * 0.3f;
                ball.velocity *= (1.0f - biteAmount);
                ball.vz = -ball.vz * (0.05f + (ball.bs / 200.f));
            }
            else {
                ball.vz = -ball.vz * 0.35f;
                ball.velocity *= 0.8f;
            }

            if (ball.vz < 15.f) ball.vz = 0.f;
        }
    }
}

// ==========================================
// 2. PLAYER PHYSICS
// ==========================================

void PhysicsEngine::updatePlayerAirPhysics(Player& player, float dt) {
    // If the player is in the air, or has upward velocity
    if (player.z > 0.0f || player.vz != 0.0f) {
        // Apply Gravity (9.8m/s^2 = 980px/s^2)
        player.vz -= 980.f * dt;
        player.z += player.vz * dt;

        // Hit the ground
        if (player.z <= 0.0f) {
            player.z = 0.0f;
            player.vz = 0.0f;
            player.setVelocity(player.getVelocity() * 0.60f); // Impact friction

            // Return to normal state (unless they were tackling/stunned)
            if (player.getState() == PlayerState::Jumping) {
                player.setState(PlayerState::Normal);
            }
        }
    }
}

void PhysicsEngine::applySlideTackleFriction(Player& player, float dt) {
    if (player.getState() == PlayerState::Tackling) {
        // Standardized friction for all players (averaged between your old 1500 and 2400)
        float slideDecel = 2000.f;

        sf::Vector2f vel = player.getVelocity();
        float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);

        if (speed > 0.f) {
            float newSpeed = std::max(0.f, speed - (slideDecel * dt));
            player.setVelocity((vel / speed) * newSpeed);
        }
    }
}

void PhysicsEngine::applyPlayerIdleFriction(Player& player, float dt) {
    if (player.getState() == PlayerState::Tackling) return; // Slide tackles use their own friction!

    sf::Vector2f vel = player.getVelocity();
    float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);

    if (speed > 0.1f) {
        float speedRatio = std::clamp(speed / (player.getTopSpeed() * 10.0f), 0.0f, 1.0f);
        
        // At low speeds, we want high friction to "snap" to a halt.
        float momentumFactor = 1.0f + (1.0f - speedRatio) * 2.0f; 
        
        // BUFFED: Multiplied by 15 for realistic grass grip!
        float totalDecel = (player.getAgility() * 15.f) * momentumFactor; 
        
        float newSpeed = std::max(0.f, speed - (totalDecel * dt));
        player.setVelocity((vel / speed) * newSpeed);
    } else {
        player.setVelocity({0.f, 0.f});
    }
}

void PhysicsEngine::applyTangentialVelocityDamping(Player& player, sf::Vector2f targetDir, float dampingStrength, float dt) {
    sf::Vector2f vel = player.getVelocity();
    float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);

    if (speed > 10.f) {
        // Calculate how much of our speed is going EXACTLY toward the target
        float velocityTowardTarget = (vel.x * targetDir.x + vel.y * targetDir.y);

        // Isolate the sideways "drifting" speed
        sf::Vector2f tangentialVel = vel - (targetDir * velocityTowardTarget);

        // Erase the drift!
        vel -= tangentialVel * dampingStrength * dt;
        player.setVelocity(vel);
    }
}

void PhysicsEngine::applyPlayerLocomotion(Player& player, sf::Vector2f inputDir, float maxSpeed, float dt) {
    if (player.getState() == PlayerState::Tackling) return;
    if (player.getState() == PlayerState::Injured) return;

    // Check for a "Micro-Input" threshold
    float inputLenSq = inputDir.x * inputDir.x + inputDir.y * inputDir.y;
    if (inputLenSq < 0.001f) {
        applyPlayerIdleFriction(player, dt);
        return;
    }

    // Ensure the input is normalized so we don't move slower just because the target is close
    sf::Vector2f normInput = (inputLenSq > 1.f) ? inputDir / std::sqrt(inputLenSq) : inputDir;

    sf::Vector2f vel = player.getVelocity();
    float currentSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y);

    // ==========================================
    // --- THE FIX: REAL WORLD SPEED MAPPING ---
    // ==========================================
    // 100px = 1 meter. 
    // A pace stat of 1 now gives ~20km/h (550px/s).
    // A pace stat of 99 gives ~36km/h (995px/s).
    float absoluteTopSpeed = 550.f + (player.getTopSpeed() * 4.5f);

    // The AI and User Controllers pass in 'maxSpeed' based on the old curve (stat * 10.0f).
    // We convert that into an "effort percentage" to seamlessly map it to the new physics curve!
    float intendedEffort = std::clamp(maxSpeed / std::max(1.f, player.getTopSpeed() * 10.f), 0.1f, 1.2f);
    float mappedMaxSpeed = absoluteTopSpeed * intendedEffort;

    // ==========================================
    // --- THE FIX 1: THE COMMITTED STRIDE ---
    // ==========================================
    if (player.isChargingAction || player.getState() == PlayerState::Kicking) {
        // Drop their top speed to ~25% of normal. They are planting their feet!
        mappedMaxSpeed *= 0.55f;
    }

    if (player.getState() == PlayerState::Kicking) {
        // Once they commit to the kick, they cannot change direction mid-stride!
        // Lock their movement vector to their physical facing direction.
        normInput = player.getAimDirection();
    }

    bool hasBall = player.getBallPossession();
    float bcNorm = player.getBallControl() / 100.f;
    float agilityNorm = player.getAgility() / 100.f;

    bool isAI = (dynamic_cast<NPCPlayer*>(&player) != nullptr);

    // ==========================================
    // --- EXAGGERATED GALLOP PHYSICS ---
    // ==========================================
    int currentFrame = player.getAnimator().getCurrentFrameIndex();

    // Planted to catch weight, or stepping down
    bool isPlantedOrDown = (currentFrame == 0 || currentFrame == 1 || currentFrame == 4 ||
        currentFrame == 5 || currentFrame == 8 || currentFrame == 9);

    // Actively pushing off the turf
    bool isPushing = (currentFrame == 2 || currentFrame == 3 || currentFrame == 6 ||
        currentFrame == 7 || currentFrame == 10 || currentFrame == 11);

    float frameAccelMultiplier = 1.0f;

    // "First Step" bypass to break out of the Idle Lock
    if (currentSpeed > 15.f) {
        if (isPlantedOrDown) {
            frameAccelMultiplier = 0.0f;
        }
        else if (isPushing) {
            frameAccelMultiplier = 2.2f;
        }
    }

    // ==========================================
    // 1. DYNAMIC TURN SHARPNESS & REDIRECTION
    // ==========================================
    sf::Vector2f velDir = (currentSpeed > 5.f) ? (vel / currentSpeed) : normInput;
    float moveTurnDot = (velDir.x * normInput.x) + (velDir.y * normInput.y);
    float speedRatio = std::clamp(currentSpeed / absoluteTopSpeed, 0.0f, 1.0f);

    float turnMultiplier = 1.8f;
    bool isDrifting = false;

    // If turning sharper than ~75 degrees (dot < 0.25) at over 75% speed!
    if (moveTurnDot < 0.25f && speedRatio > 0.75f) {
        if (((float)(rand() % 100) / 100.f) > agilityNorm) isDrifting = true;
    }

    if (moveTurnDot < 0.85f) {
        float misalignment = 1.0f - moveTurnDot; // Ranges from 0.15 to 2.0

        if (isDrifting) {
            // ICE SKATES! They lost their footing.
            float driftBrake = player.getAgility() * 2.0f * dt;
            currentSpeed = std::max(0.0f, currentSpeed - driftBrake);
            vel = velDir * currentSpeed;
            turnMultiplier = 0.0f;
        }
        else if (hasBall && isAI) {
            // AI-ONLY VECTOR SNAP
            float pivotRate = (3.0f + (agilityNorm * 7.0f)) * dt;
            pivotRate = std::clamp(pivotRate, 0.0f, 1.0f);

            // Mathematically carve the trajectory smoothly
            velDir = normalize((velDir * (1.0f - pivotRate)) + (normInput * pivotRate));

            // The U-Turn Tax
            float baseFriction = player.getAgility() * 5.0f * misalignment * dt;
            if (misalignment > 1.0f) baseFriction *= 1.8f;

            currentSpeed = std::max(0.0f, currentSpeed - baseFriction);
            vel = velDir * currentSpeed;

            float minTurnAccel = 0.4f + (bcNorm * 0.4f);
            float penalty = misalignment * (0.3f + (speedRatio * 0.4f));
            turnMultiplier = std::max(minTurnAccel, 1.2f - penalty);
        }
        else {
            // STANDARD BRAKING (Off-Ball AND Human Dribblers)
            float hardBrakeFriction = player.getAgility() * 15.0f * misalignment * dt;
            currentSpeed = std::max(0.0f, currentSpeed - hardBrakeFriction);
            vel = velDir * currentSpeed;

            float turnPenaltyIntensity = 0.6f + (speedRatio * 1.5f);
            turnMultiplier = std::max(0.1f, 1.5f - (misalignment * turnPenaltyIntensity));
        }
    }

    // ==========================================
    // 2. ACCELERATION (Burst from standing still)
    // ==========================================
    float explosionFactor = std::pow(1.0f - speedRatio, 3.0f);
    float burstMultiplier = 1.f + (explosionFactor * 25.f);

    if (isDrifting) burstMultiplier = 0.2f;

    sf::Vector2f accelVec(0.f, 0.f);

    // ==========================================
    // 3. OMNI-DIRECTIONAL vs TANK CONTROLS
    // ==========================================
    if (hasBall && isAI) {
        // AI-ONLY OMNI-DIRECTIONAL BURST
        float fwdAccel = player.getAcceleration() * burstMultiplier * turnMultiplier;

        // Elite AI dribblers get an injection of base acceleration and top speed
        fwdAccel *= 0.9f + (bcNorm * 0.5f) + (agilityNorm * 0.2f);
        mappedMaxSpeed *= 0.9f + (bcNorm * 0.3f) + (agilityNorm * 0.1f);

        // --- APPLY THE FRAME MULTIPLIER ---
        fwdAccel *= frameAccelMultiplier;

        accelVec = normInput * fwdAccel; // Drive directly into the input!
    }
    else {
        // TANK CONTROLS (Off-Ball AND Human Dribblers)
        sf::Vector2f forwardDir = player.getAimDirection();
        sf::Vector2f rightDir(-forwardDir.y, forwardDir.x);

        float currentFwdSpeed = (vel.x * forwardDir.x + vel.y * forwardDir.y);
        float currentSideSpeed = (vel.x * rightDir.x + vel.y * rightDir.y);
        float inputForward = (normInput.x * forwardDir.x + normInput.y * forwardDir.y);
        float inputRight = (normInput.x * rightDir.x + normInput.y * rightDir.y);

        // Braking against momentum
        float brakeForce = player.getAgility() * 4.0f;
        if ((currentFwdSpeed > 0 && inputForward < -0.1f) || (currentFwdSpeed < 0 && inputForward > 0.1f)) {
            float fwdBrake = brakeForce * 1.5f * std::abs(inputForward) * dt;
            if (std::abs(currentFwdSpeed) < fwdBrake) vel -= forwardDir * currentFwdSpeed;
            else vel -= forwardDir * (currentFwdSpeed > 0 ? fwdBrake : -fwdBrake);
        }

        if (std::abs(currentSideSpeed) > 10.f) {
            float sideAlignment = (inputRight > 0) == (currentSideSpeed > 0) ? std::abs(inputRight) : -std::abs(inputRight);
            if (sideAlignment < 0.5f) {
                float sideBrake = brakeForce * 4.5f * dt;
                if (std::abs(currentSideSpeed) < sideBrake) vel -= rightDir * currentSideSpeed;
                else vel -= rightDir * (currentSideSpeed > 0 ? sideBrake : -sideBrake);
            }
        }

        float fwdAccel = player.getAcceleration() * burstMultiplier * turnMultiplier;
        float sideAccel = player.getAgility() * (burstMultiplier * 0.5f + 0.4f);

        if (inputForward < 0.f) {
            fwdAccel *= 0.6f;
            mappedMaxSpeed *= 0.7f;
        }

        // HUMAN DRIBBLING PENALTY
        if (hasBall && !isAI) {
            fwdAccel *= 0.5f + (bcNorm * 0.25f);
            mappedMaxSpeed *= 0.75f + (bcNorm * 0.15f);
        }

        // --- APPLY THE FRAME MULTIPLIER ---
        fwdAccel *= frameAccelMultiplier;
        sideAccel *= frameAccelMultiplier;

        accelVec = (forwardDir * inputForward * fwdAccel) + (rightDir * inputRight * sideAccel);
    }

    // ==========================================
    // 4. APPLY ACCEL & SPEED LIMIT
    // ==========================================
    float forwardSpeedAfterBrake = (vel.x * normInput.x + vel.y * normInput.y);
    if (forwardSpeedAfterBrake < mappedMaxSpeed) {
        vel += accelVec * dt;
    }

    float finalSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
    if (finalSpeed > mappedMaxSpeed && finalSpeed > 0.1f) {

        // ==========================================
        // --- THE FIX: SMOOTH STRIDE BRAKING ---
        // ==========================================
        if (player.isChargingAction || player.getState() == PlayerState::Kicking) {
            // Apply heavy friction to organically slow down into the planted step
            float brakeForce = 2500.f * dt;
            vel -= (vel / finalSpeed) * std::min(brakeForce, finalSpeed - mappedMaxSpeed);
        }
        else {
            // Normal hard cap
            vel = (vel / finalSpeed) * mappedMaxSpeed;
        }
    }

    player.setVelocity(vel);
}

void PhysicsEngine::resolvePlayerPitchBoundaries(Player& player, const Pitch& pitch) {
    sf::Vector2f pos = player.getPosition();
    sf::Vector2f vel = player.getVelocity();
    float radius = player.getCollisionRadius(); // Ensure we bounce from the edge of the player
    bool bounced = false;

    // The restitution (bounciness). 
    // 0.4f means they lose 60% of their speed but definitely get pushed back.
    const float adboardRestitution = 0.40f;

    // ==========================================
    // --- X-AXIS (Goal Ends / Corner Boards) ---
    // ==========================================
    if (pos.x < radius) {
        pos.x = radius;
        vel.x = -vel.x * adboardRestitution;
        bounced = true;
    }
    else if (pos.x > pitch.totalWidth - radius) {
        pos.x = pitch.totalWidth - radius;
        vel.x = -vel.x * adboardRestitution;
        bounced = true;
    }

    // ==========================================
    // --- Y-AXIS (Touchlines / Long Adboards) ---
    // ==========================================
    if (pos.y < radius) {
        pos.y = radius;
        vel.y = -vel.y * adboardRestitution;
        bounced = true;
    }
    else if (pos.y > pitch.totalHeight - radius) {
        pos.y = pitch.totalHeight - radius;
        vel.y = -vel.y * adboardRestitution;
        bounced = true;
    }

    if (bounced) {
        // Snap to the surface of the adboard
        player.setPosition(pos);

        // If they hit the wall hard enough, they should lose balance slightly
        float impactSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        if (impactSpeed > 400.f && player.getState() == PlayerState::Normal) {
            // Optional: trigger a small stumble or 0.1s stun so they can't 
            // instantly turn and sprint away after hitting a wall at full tilt
            player.setStumbled(0.15f);
        }

        player.setVelocity(vel);
    }
}

// ==========================================
// --- GOALKEEPER PHYSICS & COLLISIONS ---
// ==========================================

void PhysicsEngine::applyKeeperDiveFriction(Player& keeper, float dt) {
    if (keeper.getState() == PlayerState::Diving) {

        std::string diveAnim = keeper.getLastDiveDirection();
        sf::Vector2f vel = keeper.getVelocity();

        bool isAerialDive = (diveAnim == "Up" || diveAnim == "UpLeft" || diveAnim == "UpRight" || diveAnim == "Center");

        if (isAerialDive && keeper.z > 5.0f) {
            vel -= vel * 0.5f * dt;
        }
        else {
            vel -= vel * 4.0f * dt;
        }

        keeper.setVelocity(vel);

        // ==========================================
        // --- THE FIX: RESTORE THE +90 OFFSET ---
        // ==========================================
        if (diveAnim != "Center" && diveAnim != "Up" && diveAnim != "Down") {
            if (std::abs(vel.x) > 0.1f || std::abs(vel.y) > 0.1f) {
                float angle = std::atan2(vel.y, vel.x) * 180.f / 3.14159f;

                // atan2(Y, 0) = 90. We add 90 to lay them flat at 180!
                keeper.setRotation(angle + 90.f);
            }
        }

        float speedSq = (vel.x * vel.x) + (vel.y * vel.y);

        if (speedSq < 150.0f && keeper.z < 10.0f) {
            keeper.setState(PlayerState::Normal);
            keeper.setRotation(90.f); // Stand them back up!
        }
    }
}

void PhysicsEngine::resolveGoalkeeperBallCollisions(Ball& ball, std::vector<Player*>& players) {
    if (ball.getOwner() != nullptr) return;

    for (Player* p : players) {
        if (p->getPositionRole() == PositionRole::Goalkeeper && p->getState() == PlayerState::Diving) {

            sf::FloatRect gkBox = p->getBoundingBox();
            std::string diveAnim = p->getLastDiveDirection();
            bool isHomeSide = (p->getTeam() == Team::Home);

            // ==========================================
            // --- THE FIX: SFML 3 VOLUMETRIC WALL ---
            // ==========================================
            float armSpan = 60.f;

            if (diveAnim == "Center" || diveAnim == "Down" || diveAnim == "Up") {
                gkBox.position.y -= armSpan;
                gkBox.size.y += (armSpan * 2.f);

                gkBox.position.x -= 30.f;
                gkBox.size.x += 60.f;
            }
            else if (diveAnim == "Left" || diveAnim == "DownLeft" || diveAnim == "UpLeft") {
                if (isHomeSide) {
                    gkBox.position.y -= armSpan;
                    gkBox.size.y += armSpan;
                }
                else {
                    gkBox.size.y += armSpan;
                }
            }
            else if (diveAnim == "Right" || diveAnim == "DownRight" || diveAnim == "UpRight") {
                if (isHomeSide) {
                    gkBox.size.y += armSpan;
                }
                else {
                    gkBox.position.y -= armSpan;
                    gkBox.size.y += armSpan;
                }
            }

            // --- Z-VOLUME HITBOX ---
            float zMin = p->z - 20.f;
            float zMax = p->z + 140.f;

            if (diveAnim == "Down" || diveAnim == "DownLeft" || diveAnim == "DownRight") {
                zMin = -10.f;
                zMax = p->z + 80.f;
            }
            else if (diveAnim == "Up" || diveAnim == "UpLeft" || diveAnim == "UpRight") {
                zMin = p->z + 40.f;
                zMax = p->z + 240.f;
            }

            bool zOverlap = (ball.z >= zMin) && (ball.z <= zMax);

            // SFML 3 .contains() still works the same way!
            if (gkBox.contains(ball.getPosition()) && zOverlap) {
                ball.lastTouch = p;

                resolveGoalkeeperSave(*p, ball, diveAnim);
                return;
            }
        }
    }
}

void PhysicsEngine::resolveGoalkeeperSave(Player& keeper, Ball& ball, const std::string& diveAnim) {
    sf::Vector2f incomingVel = ball.velocity;
    float ballSpeed = std::sqrt(incomingVel.x * incomingVel.x + incomingVel.y * incomingVel.y + ball.vz * ball.vz);

    float catchingStat = keeper.getGkCatching();
    float speedPenalty = (ballSpeed / 3000.0f) * 30.0f;
    float catchChance = std::clamp(catchingStat - speedPenalty, 15.0f, 98.0f);

    if (diveAnim == "Up" || diveAnim == "UpLeft" || diveAnim == "UpRight") {
        catchChance = 0.0f;
    }
    else if (diveAnim == "Down" || diveAnim == "DownLeft" || diveAnim == "DownRight") {
        catchChance *= 0.6f;
    }

    if ((rand() % 100) <= catchChance) {
        // --- OUTCOME: CLEAN CATCH ---
        keeper.setVelocity({ 0.f, 0.f });
        keeper.setState(PlayerState::Normal);

        // ==========================================
        // --- THE FIX: STAND UP AFTER SAVE ---
        // ==========================================
        keeper.setRotation(90.f);

        ball.possess(&keeper);
    }
    else {
        // --- OUTCOME: PARRY / REBOUND ---
        float dampenFactor = 0.22f - ((catchingStat / 100.0f) * 0.18f);

        sf::Vector2f parryVel;

        if (diveAnim == "Up") {
            dampenFactor = 0.4f;
            parryVel = sf::Vector2f(incomingVel.x * dampenFactor, incomingVel.y * dampenFactor);
        }
        else {
            parryVel = sf::Vector2f(-incomingVel.x * dampenFactor, -incomingVel.y * dampenFactor);
        }

        float randomDeviation = ((rand() % 100) - 50) / 100.0f;
        float angleOffset = randomDeviation * 45.0f;

        float radOffset = angleOffset * 3.14159f / 180.0f;
        float cosA = std::cos(radOffset);
        float sinA = std::sin(radOffset);

        ball.velocity.x = parryVel.x * cosA - parryVel.y * sinA;
        ball.velocity.y = parryVel.x * sinA + parryVel.y * cosA;

        if (diveAnim == "Up") {
            ball.vz = 450.0f;
        }
        else {
            ball.vz = 80.0f + (std::abs(randomDeviation) * 50.0f);
        }

        keeper.setVelocity({ 0.f, 0.f });
        keeper.setState(PlayerState::Normal);

        // ==========================================
        // --- THE FIX: STAND UP AFTER SAVE ---
        // ==========================================
        keeper.setRotation(90.f);
    }
}

// ==========================================
// 3. COLLISION PHYSICS
// ==========================================

void PhysicsEngine::resolvePlayerPlayerCollisions(std::vector<Player*>& players, Ball& ball, MatchReferee& referee, const Pitch& pitch, SoundManager& soundManager, MatchStatistics& stats)
{
    // Quick helper lambda for normalization
    auto normalize = [](sf::Vector2f v) {
        float len = std::sqrt(v.x * v.x + v.y * v.y);
        return len > 0.0001f ? v / len : sf::Vector2f(0.f, 0.f);
        };

    // ==========================================
    // --- PLAYER-TO-PLAYER MATRIX ---
    // ==========================================
    for (size_t i = 0; i < players.size(); ++i)
    {
        for (size_t j = i + 1; j < players.size(); ++j)
        {
            Player* p1 = players[i];
            Player* p2 = players[j];

            bool isSameTeam = (p1->getTeam() == p2->getTeam());

            // ---------------------------------------------------------
            // A. TACKLE LOGIC & FOULS
            // ---------------------------------------------------------
            auto processTackleHit = [&](Player* tackler, Player* victim)
                {
                    if (!tackler->isTackling()) return;

                    if (tackler->getTackleHitbox().findIntersection(victim->getBoundingBox()))
                    {
                        if (ball.getOwner() == victim) {
                            sf::Vector2f toBall = ball.getPosition() - tackler->getPosition();
                            float distToBall = std::sqrt(toBall.x * toBall.x + toBall.y * toBall.y);

                            if (distToBall > 110.f) {
                                // --- WIPEOUT ---
                                sf::Vector2f impactDir = victim->getPosition() - tackler->getPosition();
                                if (impactDir.x == 0 && impactDir.y == 0) impactDir.x = 1.f;
                                float impactForce = isSameTeam ? 600.f : 800.f;
                                victim->triggerFallOver(normalize(impactDir) * (impactForce));
                                victim->checkInjury(impactForce);
                                tackler->resetTackleCooldown();
                                tackler->setState(PlayerState::Normal);
                                tackler->setRotation(90.f);
                                tackler->setVelocity({ 0.f, 0.f });

                                if (!isSameTeam) {
                                    // ==========================================
                                    // --- THE FIX: ANGLE-BASED REFEREEING ---
                                    // ==========================================
                                    // 1. Calculate Victim's Facing Direction
                                    sf::Vector2f vVel = victim->getVelocity();
                                    float vSpeed = std::sqrt(vVel.x * vVel.x + vVel.y * vVel.y);
                                    sf::Vector2f vFacing = (vSpeed > 10.f) ? (vVel / vSpeed) : normalize(ball.getPosition() - victim->getPosition());
                                    if (vFacing.x == 0.f && vFacing.y == 0.f) vFacing = { 1.f, 0.f };

                                    // 2. Calculate Tackler's Approach Angle
                                    sf::Vector2f toTackler = normalize(tackler->getPosition() - victim->getPosition());

                                    // 1.0 = Front, 0.0 = Side, -1.0 = Behind
                                    float approachDot = vFacing.x * toTackler.x + vFacing.y * toTackler.y;

                                    int roll = rand() % 100;
                                    bool callFoul = false;
                                    FoulType fType = FoulType::Obstruction; // Base free kick, no card

                                    // BEHIND: ~30 degree tight cone (15 degrees each side -> cos(165) = -0.965)
                                    if (approachDot < -0.965f) {
                                        callFoul = true;
                                        if (roll < 10) fType = FoulType::Violent;     // 10% Straight Red
                                        else fType = FoulType::Sliding;               // 90% Yellow
                                    }
                                    // FRONT: ~120 degree wide cone (60 degrees each side -> cos(60) = 0.5)
                                    else if (approachDot > 0.5f) {
                                        if (roll < 5) { callFoul = true; fType = FoulType::Sliding; } // 5% Yellow
                                        else if (roll < 15) { callFoul = true; fType = FoulType::Obstruction; }  // 15% Foul
                                        // Remaining 80%: Play on! (Wipeout occurs, but no whistle)
                                    }
                                    // SIDE: Everything else in between
                                    else {
                                        if (roll < 1) { callFoul = true; fType = FoulType::Violent; }       // 1% Straight Red
                                        else if (roll < 21) { callFoul = true; fType = FoulType::Sliding; } // 20% Yellow
                                        else if (roll < 31) { callFoul = true; fType = FoulType::Obstruction; }  // 30% Foul
                                        // Remaining 49%: Play on!
                                    }

                                    if (callFoul) {
                                        FoulEvent foul;
                                        foul.location = victim->getPosition();
                                        foul.offender = tackler;
                                        foul.type = fType;
                                        referee.awardFoul(foul, pitch, ball, players, victim, soundManager, stats);
                                    }
                                }
                                else {
                                    tackler->triggerFallOver(normalize(-impactDir) * 400.f);
                                }
                            }
                            else {
                                // --- CLEAN TACKLE ---
                                victim->setStumbled(0.8f);

                                // 1. Force the victim to drop the ball!
                                ball.release();

                                // 2. Knock the ball away based on the tackler's momentum
                                sf::Vector2f tackleImpulse = tackler->getVelocity() * 1.2f;
                                ball.applyImpulse(tackleImpulse);
                                ball.lastTouch = tackler;
                                // 3. Slow the tackler down so they don't slide forever
                                tackler->setVelocity(tackler->getVelocity() * 0.5f);

                                // 4. End the tackle state
                                tackler->setState(PlayerState::Normal);
                                tackler->setRotation(90.f);
                                tackler->startTackleCooldown();
                            }
                        }
                    }
                };

            processTackleHit(p1, p2);
            processTackleHit(p2, p1);

            // ---------------------------------------------------------
            // B. PHYSICAL BODY RESOLUTION (The "Un-Sticker" & Bumping)
            // ---------------------------------------------------------
            sf::Vector2f delta = p1->getPosition() - p2->getPosition();
            float distanceSq = delta.x * delta.x + delta.y * delta.y;
            float combinedRadius = p1->getCollisionRadius() + p2->getCollisionRadius();

            // If they are overlapping
            if (distanceSq < combinedRadius * combinedRadius)
            {
                float distance = std::sqrt(distanceSq);
                sf::Vector2f normal;

                // ANTI-SUCTION: If distance is 0, push them apart arbitrarily.
                if (distance < 0.01f) {
                    distance = 0.01f;
                    normal = { 1.f, 0.f };
                }
                else {
                    normal = delta / distance;
                }

                float overlap = combinedRadius - distance;

                // Weight/Mass calculations
                float m1 = p1->getWeight() * (1.0f + p1->getBalancing() / 100.f);
                float m2 = p2->getWeight() * (1.0f + p2->getBalancing() / 100.f);
                float invM1 = 1.0f / m1;
                float invM2 = 1.0f / m2;
                float sumInvMass = invM1 + invM2;

                // 1. STATIC RESOLUTION (Physically separate them)
                p1->move(normal * (overlap * (invM1 / sumInvMass)));
                p2->move(-normal * (overlap * (invM2 / sumInvMass)));

                // 2. DYNAMIC RESOLUTION (Bounce/Impulse)
                sf::Vector2f relativeVel = p1->getVelocity() - p2->getVelocity();
                float velAlongNormal = (relativeVel.x * normal.x + relativeVel.y * normal.y);

                if (velAlongNormal < 0)
                {
                    float restitution = 0.15f;
                    float strengthFactor = 1.0f + ((p1->getBodyStrength() + p2->getBodyStrength()) / 200.f);
                    float j = -(1.0f + restitution) * velAlongNormal;
                    j /= sumInvMass;
                    j *= strengthFactor;

                    sf::Vector2f impulse = j * normal;
                    p1->setVelocity(p1->getVelocity() + (invM1 * impulse));
                    p2->setVelocity(p2->getVelocity() - (invM2 * impulse));

                    // Check for massive clatters (Knockdowns based on balance)
                    float deltaV1 = std::sqrt((invM1 * impulse).x * (invM1 * impulse).x + (invM1 * impulse).y * (invM1 * impulse).y);
                    float deltaV2 = std::sqrt((invM2 * impulse).x * (invM2 * impulse).x + (invM2 * impulse).y * (invM2 * impulse).y);
                    float thresh1 = 450.f * (1.0f + p1->getBalancing() / 100.f);
                    float thresh2 = 450.f * (1.0f + p2->getBalancing() / 100.f);

                    if (deltaV2 > thresh2 * 8.0f) {
                        p2->triggerFallOver(normalize(p2->getPosition() - p1->getPosition()) * 600.f);
                        p2->checkInjury(deltaV2); // THE FIX: Big collision impact!
                    }
                    if (deltaV1 > thresh1 * 8.0f) {
                        p1->triggerFallOver(normalize(p1->getPosition() - p2->getPosition()) * 600.f);
                        p1->checkInjury(deltaV1); // THE FIX: Big collision impact!
                    }
                }
            }
        }
    }
}

void PhysicsEngine::resolveBallPitchBoundaries(Ball& ball, const Pitch& pitch, SoundManager& soundManager) {
    sf::Vector2f pos = ball.getPosition();
    sf::Vector2f vel = ball.getVelocity();
    bool bounced = false;

    // Bounce off left/right invisible walls
    if (pos.x < 0.f) { pos.x = 0.f; vel.x = -vel.x * 0.8f; bounced = true; }
    if (pos.x > pitch.totalWidth) { pos.x = pitch.totalWidth; vel.x = -vel.x * 0.8f; bounced = true; }

    // Bounce off top/bottom invisible walls
    if (pos.y < 0.f) { pos.y = 0.f; vel.y = -vel.y * 0.8f; bounced = true; }
    if (pos.y > pitch.totalHeight) { pos.y = pitch.totalHeight; vel.y = -vel.y * 0.8f; bounced = true; }

    if (bounced) {
        ball.setPosition(pos);
        ball.setVelocity(vel);
    }

    // ==========================================
    // --- THE FLOOR AUDIO FIX ---
    // ==========================================
    // Raised threshold from -50.f to -150.f to prevent micro-bounces from 
    // constantly triggering and stealing audio channels.
    if (ball.z <= 0.0f && ball.vz < -150.f) {
        float bounceVol = std::min(100.f, std::abs(ball.vz) / 10.f);
        soundManager.playSound("ball_bounce", bounceVol, 0.1f);
    }
}

void PhysicsEngine::resolveBallPlayerCollisions(Ball& ball, std::vector<Player*>& players) {
    // Only check outfield collisions if the ball is low enough
    if (ball.z >= 80.f) return;

    for (Player* p : players) {
        if (p->getBallPossession()) continue;

        if (std::abs(p->z - ball.z) < p->height) {
            sf::Vector2f ballPos = ball.shape.getPosition();
            sf::Vector2f playerPos = p->getPosition();
            sf::Vector2f delta = ballPos - playerPos;

            float distSq = delta.x * delta.x + delta.y * delta.y;
            // Use standard ball radius (12.f) so we don't rely on shadow scale
            float combineRadius = 12.f + p->getCollisionRadius();

            if (distSq < combineRadius * combineRadius && distSq > 0.0001f) {

                // ==========================================
                // --- THE FIX 3: KICKER IMMUNITY ---
                // ==========================================
                // Safely cast to NPCPlayer to check the kick cooldown!
                if (p == ball.lastTouch) {
                    if (NPCPlayer* npc = dynamic_cast<NPCPlayer*>(p)) {
                        if (npc->getKickCooldown() > 0.0f) {
                            continue; // Phase cleanly through their body!
                        }
                    }
                }

                float distance = std::sqrt(distSq);
                sf::Vector2f normal = delta / distance;

                // --- 1. OVERLAP RESOLUTION ---
                float overlap = combineRadius - distance;
                ball.shape.setPosition(ballPos + normal * (overlap + 1.0f));

                // --- 2. RELATIVE VELOCITY ---
                sf::Vector2f currentBallVel = ball.velocity;
                sf::Vector2f playerVel = p->getVelocity();
                sf::Vector2f relVel = currentBallVel - playerVel;

                float dot = relVel.x * normal.x + relVel.y * normal.y;

                if (dot < 0) {
                    ball.lastTouch = p;

                    // ==========================================
                    // --- THE FIX: HUMAN "SANDBAG" PHYSICS ---
                    // ==========================================
                    // Humans are squishy and do not bounce.
                    float restitution = 0.05f; // Extremely low bounciness

                    sf::Vector2f reflection = relVel - (1.0f + restitution) * dot * normal;

                    // Absorb 85% of the kinetic energy into the player's body!
                    sf::Vector2f finalVel = playerVel + (reflection * 0.25f);

                    float finalSpeed = std::sqrt(finalVel.x * finalVel.x + finalVel.y * finalVel.y);

                    // Hard cap the absolute maximum bounce speed to a jog pace (350px/s), 
                    // preventing the 1000px/s "Pinball Rocket" effect!
                    if (finalSpeed > 350.f) {
                        finalVel = (finalVel / finalSpeed) * 350.f;
                    }

                    ball.velocity = finalVel;

                    // We still stun the player if they took a 2000px/s missile to the chest
                    float originalSpeed = std::sqrt(currentBallVel.x * currentBallVel.x + currentBallVel.y * currentBallVel.y);
                    if (originalSpeed > 1500.f) {
                        p->setState(PlayerState::Stunned);
                    }
                }
            }
        }
    }
}

void PhysicsEngine::resolveBallGoalCollisions(Ball& ball, const Goal& goal, SoundManager& soundManager) {
    sf::Vector2f bPos = ball.getPosition();
    sf::Vector2f velocity = ball.velocity;
    float ballZ = ball.z;
    float bRadius = 12.f;

    float crossbarZ = 244.f;
    float postRadius = 6.f;
    float goalX = goal.center.x;
    float topY = goal.center.y - 366.f;
    float bottomY = goal.center.y + 366.f;
    float netDepth = 225.f;
    float backX = goal.isHomeGoal ? (goalX - netDepth) : (goalX + netDepth);

    // ==========================================
    // --- 1. THE CROSSBAR (3D Cylinder in X-Z) ---
    // ==========================================
    if (bPos.y > topY - bRadius && bPos.y < bottomY + bRadius) {
        float dx = bPos.x - goalX;
        float dz = ballZ - crossbarZ;
        float distXZSq = (dx * dx) + (dz * dz);
        float minDist = bRadius + postRadius;

        if (distXZSq < minDist * minDist && distXZSq > 0.001f) {
            float distXZ = std::sqrt(distXZSq);
            float nx = dx / distXZ;
            float nz = dz / distXZ;

            float dot = velocity.x * nx + ball.vz * nz;
            if (dot < 0.f) {
                float hitSpeed = std::sqrt(velocity.x * velocity.x + ball.vz * ball.vz);

                float vol = std::clamp(hitSpeed / 12.f, 50.f, 100.f);
                if (hitSpeed > 1100.f) soundManager.playSound("crossbar_strong", vol);
                else soundManager.playSound("crossbar", vol);

                velocity.x = (velocity.x - 2.f * dot * nx) * 0.6f;
                ball.vz = (ball.vz - 2.f * dot * nz) * 0.6f;

                bPos.x = goalX + nx * minDist;
                ballZ = crossbarZ + nz * minDist;
            }
        }
    }

    // ==========================================
    // --- 2. THE POSTS (3D Cylinders in X-Y) ---
    // ==========================================
    auto checkPost = [&](const sf::CircleShape& post) {
        if (ballZ > crossbarZ + bRadius) return;

        sf::Vector2f pPos = post.getPosition();
        sf::Vector2f diff = bPos - pPos;
        float distSq = (diff.x * diff.x) + (diff.y * diff.y);
        float minDist = bRadius + postRadius;

        if (distSq < minDist * minDist && distSq > 0.001f) {
            float dist = std::sqrt(distSq);
            sf::Vector2f normal = diff / dist;

            float dot = velocity.x * normal.x + velocity.y * normal.y;
            if (dot < 0.f) {
                float hitSpeed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);

                float vol = std::clamp(hitSpeed / 12.f, 50.f, 100.f);
                if (hitSpeed > 1100.f) {
                    std::string soundId = (rand() % 100 < 50) ? "post_strong" : "post_strong2";
                    soundManager.playSound(soundId, vol);
                }
                else {
                    soundManager.playSound("post", vol);
                }

                velocity.x = (velocity.x - 2.f * dot * normal.x) * 0.6f;
                velocity.y = (velocity.y - 2.f * dot * normal.y) * 0.6f;
            }
            bPos = pPos + normal * minDist;
        }
        };
    checkPost(goal.topPost);
    checkPost(goal.bottomPost);

    // ==========================================
    // --- 3. VOLUMETRIC NET COLLISIONS ---
    // ==========================================
    float minX = goal.isHomeGoal ? backX : goalX;
    float maxX = goal.isHomeGoal ? goalX : backX;

    // Extrapolate previous position to track trajectory
    float dtApprox = 0.016f;
    sf::Vector2f prevPos = bPos - velocity * dtApprox;
    float prevZ = ballZ - ball.vz * dtApprox;

    // Check if the ball was ALREADY completely inside the net volume last frame
    bool wasInsideX = (prevPos.x > minX && prevPos.x < maxX);
    bool wasInsideY = (prevPos.y > topY && prevPos.y < bottomY);
    bool wasInsideZ = (prevZ < crossbarZ);
    bool wasInsideVolume = (wasInsideX && wasInsideY && wasInsideZ);

    // Check if the ball literally JUST walked through the front door (the goal line)
    bool enteredThroughFront = false;
    if (goal.isHomeGoal) {
        if (prevPos.x >= goalX && bPos.x < goalX && bPos.y > topY && bPos.y < bottomY && ballZ < crossbarZ) enteredThroughFront = true;
    }
    else {
        if (prevPos.x <= goalX && bPos.x > goalX && bPos.y > topY && bPos.y < bottomY && ballZ < crossbarZ) enteredThroughFront = true;
    }

    // If the ball is overlapping the bounding box of the net...
    bool inBoundsX = (bPos.x > minX - bRadius && bPos.x < maxX + bRadius);
    bool inBoundsY = (bPos.y > topY - bRadius && bPos.y < bottomY + bRadius);
    bool inBoundsZ = (ballZ < crossbarZ + bRadius);

    if (inBoundsX && inBoundsY && inBoundsZ) {

        // If it came through the front door, it is a legitimate goal. TRAP IT.
        if (wasInsideVolume || enteredThroughFront) {

            bool touchedFabric = false;

            // A. Clamp to Back Net
            if (goal.isHomeGoal && bPos.x < backX + bRadius) {
                bPos.x = backX + bRadius;
                velocity.x = std::max(0.f, velocity.x); // Kill negative momentum
                touchedFabric = true;
            }
            else if (!goal.isHomeGoal && bPos.x > backX - bRadius) {
                bPos.x = backX - bRadius;
                velocity.x = std::min(0.f, velocity.x); // Kill positive momentum
                touchedFabric = true;
            }

            // B. Clamp to Side Nets
            if (bPos.y < topY + bRadius) {
                bPos.y = topY + bRadius;
                velocity.y = std::max(0.f, velocity.y);
                touchedFabric = true;
            }
            else if (bPos.y > bottomY - bRadius) {
                bPos.y = bottomY - bRadius;
                velocity.y = std::min(0.f, velocity.y);
                touchedFabric = true;
            }

            // C. Clamp to Roof
            if (ballZ > crossbarZ - bRadius) {
                ballZ = crossbarZ - bRadius;
                ball.vz = std::min(0.f, ball.vz);
                touchedFabric = true;
            }

            // Simulate the net tangling and dragging the ball
            velocity.x *= 0.94f;
            velocity.y *= 0.94f;

            if (touchedFabric) {
                float hitSpeed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + ball.vz * ball.vz);
                if (hitSpeed > 100.f) {
                    float netVol = std::clamp(hitSpeed / 10.f, 30.f, 90.f);
                    soundManager.playRandomSound("net", 3, netVol, 0.1f);
                }

                // Heavily kill momentum upon striking the canvas
                velocity.x *= 0.5f;
                velocity.y *= 0.5f;
                ball.vz *= 0.5f;
            }
        }
        else {
            // It did NOT enter through the front. It is clipping from the OUTSIDE. REPEL IT!
            // Calculate penetration depth for all 4 external faces (Roof, Back, Top-Side, Bottom-Side)
            float dRoof = (crossbarZ + bRadius) - ballZ;
            float dBack = goal.isHomeGoal ? (bPos.x - (backX - bRadius)) : ((backX + bRadius) - bPos.x);
            float dTopSide = (bPos.y - (topY - bRadius));
            float dBotSide = ((bottomY + bRadius) - bPos.y);

            // Find the face it penetrated the least (MTV - Minimum Translation Vector)
            float minPen = std::min({ dRoof, dBack, dTopSide, dBotSide });

            if (minPen == dRoof) {
                ballZ = crossbarZ + bRadius;
                if (ball.vz < 0.f) ball.vz = -ball.vz * 0.4f;
                velocity.x *= 0.8f;
                velocity.y *= 0.8f;
            }
            else if (minPen == dBack) {
                bPos.x = goal.isHomeGoal ? (backX - bRadius) : (backX + bRadius);
                if (goal.isHomeGoal && velocity.x > 0.f) velocity.x = -velocity.x * 0.4f;
                else if (!goal.isHomeGoal && velocity.x < 0.f) velocity.x = -velocity.x * 0.4f;
            }
            else if (minPen == dTopSide) {
                bPos.y = topY - bRadius;
                if (velocity.y > 0.f) velocity.y = -velocity.y * 0.4f;
            }
            else if (minPen == dBotSide) {
                bPos.y = bottomY + bRadius;
                if (velocity.y < 0.f) velocity.y = -velocity.y * 0.4f;
            }
        }
    }

    // Save strictly bounded states back to the ball!
    ball.shape.setPosition(bPos);
    ball.velocity = velocity;
    ball.z = ballZ;
}

void PhysicsEngine::resolvePlayerGoalCollisions(Player& player, const Goal& goal) {
    sf::Vector2f pPos = player.getPosition();
    sf::Vector2f pVel = player.getVelocity();
    float pRadius = 25.f;

    float netDepth = 225.f;
    float goalWidth = 732.f;
    float topY = goal.center.y - (goalWidth / 2.f);
    float bottomY = goal.center.y + (goalWidth / 2.f);
    float goalX = goal.center.x;
    float backX = goal.isHomeGoal ? (goalX - netDepth) : (goalX + netDepth);

    // --- 1. SIDE WALLS ---
    bool isInsideX = goal.isHomeGoal ? (pPos.x < goalX + pRadius && pPos.x > backX - pRadius) : (pPos.x > goalX - pRadius && pPos.x < backX + pRadius);

    if (isInsideX) {
        if (std::abs(pPos.y - topY) < pRadius) {
            float pushDir = (pPos.y < topY) ? -1.0f : 1.0f;
            player.setPosition({ pPos.x, topY + (pushDir * pRadius) });
            player.setVelocity({ pVel.x * 0.5f, -pVel.y * 0.2f });
        }
        else if (std::abs(pPos.y - bottomY) < pRadius) {
            float pushDir = (pPos.y > bottomY) ? 1.0f : -1.0f;
            player.setPosition({ pPos.x, bottomY + (pushDir * pRadius) });
            player.setVelocity({ pVel.x * 0.5f, -pVel.y * 0.2f });
        }
    }

    // --- 2. BACK WALL ---
    if (pPos.y > topY - pRadius && pPos.y < bottomY + pRadius) {
        bool hittingBack = std::abs(pPos.x - backX) < pRadius;
        if (hittingBack) {
            float pushDir;
            if (goal.isHomeGoal) pushDir = (pPos.x < backX) ? -1.0f : 1.0f;
            else pushDir = (pPos.x > backX) ? 1.0f : -1.0f;

            player.setPosition({ backX + (pushDir * pRadius), pPos.y });
            player.setVelocity({ -pVel.x * 0.2f, pVel.y * 0.5f });
        }
    }

    // --- 3. POSTS ---
    auto collideWithPost = [&](const sf::CircleShape& post) {
        sf::Vector2f postPos = post.getPosition();
        sf::Vector2f diff = pPos - postPos;
        float distSq = (diff.x * diff.x) + (diff.y * diff.y);
        float minDist = pRadius + post.getRadius();

        if (distSq < minDist * minDist) {
            float dist = std::sqrt(distSq);
            sf::Vector2f normal = diff / dist;
            player.setPosition(postPos + normal * (minDist + 1.f));

            float dot = pVel.x * normal.x + pVel.y * normal.y;
            if (dot < 0.f) {
                sf::Vector2f reflection = pVel - 2.f * dot * normal;
                player.setVelocity(reflection * 0.3f);
            }
        }
        };
    collideWithPost(goal.topPost);
    collideWithPost(goal.bottomPost);
}

sf::Vector2f PhysicsEngine::normalize(sf::Vector2f source) {
    float length = std::sqrt(source.x * source.x + source.y * source.y);
    if (length != 0) return source / length;
    return source;
}
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
    float absoluteTopSpeed = player.getTopSpeed() * 10.f;

    bool hasBall = player.getBallPossession();
    float bcNorm = player.getBallControl() / 100.f;
    float agilityNorm = player.getAgility() / 100.f;

    // ==========================================
    // --- THE ASYMMETRICAL AI LOCK ---
    // ==========================================
    // Replace this with however your engine checks for AI vs User!
    // e.g., bool isAI = (dynamic_cast<NPCPlayer*>(&player) != nullptr);
    bool isAI = (dynamic_cast<NPCPlayer*>(&player) != nullptr);

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
            turnMultiplier = 0.0f; // Cannot cut into the new direction while sliding
        }
        else if (hasBall && isAI) {
            // ==========================================
            // THE FIX 1: AI-ONLY VECTOR SNAP
            // ==========================================
            // AI dribblers instantly bend their momentum toward the input!
            float pivotRate = (3.0f + (agilityNorm * 7.0f)) * dt;
            pivotRate = std::clamp(pivotRate, 0.0f, 1.0f);

            // Mathematically carve the trajectory smoothly
            velDir = normalize((velDir * (1.0f - pivotRate)) + (normInput * pivotRate));

            // The U-Turn Tax
            float baseFriction = player.getAgility() * 5.0f * misalignment * dt;
            if (misalignment > 1.0f) baseFriction *= 1.8f;

            currentSpeed = std::max(0.0f, currentSpeed - baseFriction);
            vel = velDir * currentSpeed;

            // High BC AI players keep a massive chunk of their burst
            float minTurnAccel = 0.4f + (bcNorm * 0.4f);
            float penalty = misalignment * (0.3f + (speedRatio * 0.4f));
            turnMultiplier = std::max(minTurnAccel, 1.2f - penalty);
        }
        else {
            // ==========================================
            // STANDARD BRAKING (Off-Ball AND Human Dribblers)
            // ==========================================
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
        // ==========================================
        // THE FIX 2: AI-ONLY OMNI-DIRECTIONAL BURST
        // ==========================================
        float fwdAccel = player.getAcceleration() * burstMultiplier * turnMultiplier;

        // Elite AI dribblers get an injection of base acceleration and top speed
        fwdAccel *= 0.9f + (bcNorm * 0.5f) + (agilityNorm * 0.2f);
        maxSpeed *= 0.9f + (bcNorm * 0.3f) + (agilityNorm * 0.1f);

        accelVec = normInput * fwdAccel; // Drive directly into the input!
    }
    else {
        // ==========================================
        // TANK CONTROLS (Off-Ball AND Human Dribblers)
        // ==========================================
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
            maxSpeed *= 0.7f;
        }

        // ==========================================
        // HUMAN DRIBBLING PENALTY
        // ==========================================
        // The human player uses Tank Controls AND receives a realistic 
        // physics penalty to their acceleration when carrying the ball!
        if (hasBall && !isAI) {
            fwdAccel *= 0.5f + (bcNorm * 0.25f);
            maxSpeed *= 0.85f + (bcNorm * 0.15f);
        }

        accelVec = (forwardDir * inputForward * fwdAccel) + (rightDir * inputRight * sideAccel);
    }

    // ==========================================
    // 4. APPLY ACCEL & SPEED LIMIT
    // ==========================================
    float forwardSpeedAfterBrake = (vel.x * normInput.x + vel.y * normInput.y);
    if (forwardSpeedAfterBrake < maxSpeed) {
        vel += accelVec * dt;
    }

    float finalSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
    if (finalSpeed > maxSpeed && finalSpeed > 0.1f) {
        vel = (vel / finalSpeed) * maxSpeed;
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
        sf::Vector2f vel = keeper.getVelocity();
        vel -= vel * 4.0f * dt; // Apply drag so they slide to a stop
        keeper.setVelocity(vel);

        float speedSq = (vel.x * vel.x) + (vel.y * vel.y);
        // If they have slowed down enough, they stand back up
        if (speedSq < 150.0f) {
            keeper.setState(PlayerState::Normal);
        }
    }
}

void PhysicsEngine::resolveGoalkeeperBallCollisions(Ball& ball, std::vector<Player*>& players) {
    if (ball.getOwner() != nullptr) return; // Ball is already possessed

    for (Player* p : players) {
        // Only check players who are actually diving
        if (p->getPositionRole() == PositionRole::Goalkeeper && p->getState() == PlayerState::Diving) {

            sf::FloatRect gkBox = p->getBoundingBox();

            // ==========================================
            // --- THE LOW SHOT FIX: Z-VOLUME HITBOX ---
            // ==========================================
            // Instead of a strict 100px difference, we create a realistic physical "wall".
            // A diving keeper covers from just below their center, up to about 140px (shoulder width).
            bool zOverlap = (ball.z >= p->z - 20.f) && (ball.z <= p->z + 140.f);

            // Check if the 2D ball position is inside the dynamic rectangle AND hits the Z-wall
            if (gkBox.contains(ball.getPosition()) && zOverlap) {
                resolveGoalkeeperSave(*p, ball);
                return; // Only one person can save it, break the loop!
            }
        }
    }
}

void PhysicsEngine::resolveGoalkeeperSave(Player& keeper, Ball& ball) {
    sf::Vector2f incomingVel = ball.velocity;
    float ballSpeed = std::sqrt(incomingVel.x * incomingVel.x + incomingVel.y * incomingVel.y + ball.vz * ball.vz);

    // BUFF 1: Catching Penalty reduced. A 100km/h shot only reduces catch chance by 30%, not 50%.
    float catchingStat = keeper.getGkCatching();
    float speedPenalty = (ballSpeed / 3000.0f) * 30.0f;
    float catchChance = std::clamp(catchingStat - speedPenalty, 15.0f, 98.0f);

    if ((rand() % 100) <= catchChance) {
        // --- OUTCOME: CLEAN CATCH ---
        keeper.setVelocity({ 0.f, 0.f });
        keeper.setState(PlayerState::Normal);
        ball.possess(&keeper);
    }
    else {
        // --- OUTCOME: PARRY / REBOUND ---
        // BUFF 2: Stronger wrists. Parries dampen the ball much more to prevent crazy rebounds.
        float dampenFactor = 0.35f - ((catchingStat / 100.0f) * 0.25f);
        sf::Vector2f parryVel(-incomingVel.x * dampenFactor, -incomingVel.y * dampenFactor);

        float randomDeviation = ((rand() % 100) - 50) / 100.0f;
        float angleOffset = randomDeviation * 45.0f;

        float radOffset = angleOffset * 3.14159f / 180.0f;
        float cosA = std::cos(radOffset);
        float sinA = std::sin(radOffset);

        ball.velocity.x = parryVel.x * cosA - parryVel.y * sinA;
        ball.velocity.y = parryVel.x * sinA + parryVel.y * cosA;

        // BUFF 3: Keep the parry low to the ground so it's harder to head in on the rebound.
        ball.vz = 150.0f + (std::abs(randomDeviation) * 100.0f);

        keeper.setVelocity({ 0.f, 0.f });
        keeper.setState(PlayerState::Normal);
    }
}
// ==========================================
// 3. COLLISION PHYSICS
// ==========================================

void PhysicsEngine::resolvePlayerPlayerCollisions(std::vector<Player*>& players, Ball& ball, MatchReferee& referee, AnimationServer& animServer, const Pitch& pitch, SoundManager& soundManager)
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
                                // --- FOUL / WIPEOUT ---
                                sf::Vector2f impactDir = victim->getPosition() - tackler->getPosition();
                                if (impactDir.x == 0 && impactDir.y == 0) impactDir.x = 1.f;
                                float impactForce = isSameTeam ? 600.f : 800.f;
                                victim->triggerFallOver(normalize(impactDir) * (impactForce), animServer);
                                victim->checkInjury(impactForce);
                                tackler->resetTackleCooldown();
                                tackler->setState(PlayerState::Normal);
                                tackler->setVelocity({ 0.f, 0.f });

                                if (!isSameTeam) {
                                    FoulEvent foul;
                                    foul.location = victim->getPosition();
                                    foul.offender = tackler;
                                    foul.type = (rand() % 100 < 15) ? FoulType::Violent : FoulType::Sliding;
                                    referee.awardFoul(foul, pitch, ball, players, victim, soundManager);
                                }
                                else {
                                    tackler->triggerFallOver(normalize(-impactDir) * 400.f, animServer);
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

                                // 3. Slow the tackler down so they don't slide forever
                                tackler->setVelocity(tackler->getVelocity() * 0.5f);

                                // 4. End the tackle state
                                tackler->setState(PlayerState::Normal);
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
                        p2->triggerFallOver(normalize(p2->getPosition() - p1->getPosition()) * 600.f, animServer);
                        p2->checkInjury(deltaV2); // THE FIX: Big collision impact!
                    }
                    if (deltaV1 > thresh1 * 8.0f) {
                        p1->triggerFallOver(normalize(p1->getPosition() - p2->getPosition()) * 600.f, animServer);
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
                    float relSpeed = std::sqrt(relVel.x * relVel.x + relVel.y * relVel.y);
                    float restitution = (relSpeed > 800.f) ? 0.25f : 0.05f;

                    sf::Vector2f reflection = relVel - (1.0f + restitution) * dot * normal;
                    sf::Vector2f finalVel = playerVel + reflection;

                    float finalSpeed = std::sqrt(finalVel.x * finalVel.x + finalVel.y * finalVel.y);
                    if (finalSpeed > 1000.f) {
                        finalVel = (finalVel / finalSpeed) * 1000.f;
                    }

                    ball.velocity = finalVel;

                    float originalSpeed = std::sqrt(currentBallVel.x * currentBallVel.x + currentBallVel.y * currentBallVel.y);
                    if (originalSpeed > 2000.f) {
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
       // --- 1. THE CROSSBAR (3D Cylinder in X-Z Plane) ---
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

                // THE FIX: Dynamic volume and lowered threshold for the "strong" sound
                float vol = std::clamp(hitSpeed / 12.f, 50.f, 100.f);
                if (hitSpeed > 1100.f) soundManager.playSound("crossbar_strong", vol);
                else soundManager.playSound("crossbar", vol);

                ball.velocity.x = (velocity.x - 2.f * dot * nx) * 0.6f;
                ball.vz = (ball.vz - 2.f * dot * nz) * 0.6f;

                ball.shape.setPosition({ goalX + nx * minDist, bPos.y });
                ball.z = crossbarZ + nz * minDist;

                bPos = ball.getPosition();
                velocity = ball.velocity;
                ballZ = ball.z;
            }
        }
    }

    // ==========================================
    // --- 2. THE POSTS (3D Cylinders in X-Y Plane) ---
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

                // THE FIX: Exact string matching for the IDs you loaded!
                float vol = std::clamp(hitSpeed / 12.f, 50.f, 100.f);
                if (hitSpeed > 1100.f) {
                    std::string soundId = (rand() % 100 < 50) ? "post_strong" : "post_strong2";
                    soundManager.playSound(soundId, vol);
                }
                else {
                    soundManager.playSound("post", vol);
                }

                ball.velocity.x = (velocity.x - 2.f * dot * normal.x) * 0.6f;
                ball.velocity.y = (velocity.y - 2.f * dot * normal.y) * 0.6f;
            }
            ball.shape.setPosition(pPos + normal * minDist);

            bPos = ball.getPosition();
            velocity = ball.velocity;
        }
        };
    checkPost(goal.topPost);
    checkPost(goal.bottomPost);

    // ==========================================
    // --- 3. THE ROOF & NET WALLS ---
    // ==========================================
    float minX = goal.isHomeGoal ? backX : goalX;
    float maxX = goal.isHomeGoal ? goalX : backX;
    float catchBuffer = 100.f;
    bool insideGoalMouth = false;

    if (goal.isHomeGoal) {
        if (bPos.x < goalX && bPos.x > backX - catchBuffer && bPos.y > topY && bPos.y < bottomY) insideGoalMouth = true;
    }
    else {
        if (bPos.x > goalX && bPos.x < backX + catchBuffer && bPos.y > topY && bPos.y < bottomY) insideGoalMouth = true;
    }

    if (insideGoalMouth) {
        bool hitNetFabric = false;

        // A. ROOF (Hitting the ceiling from Inside)
        if (ballZ > crossbarZ - bRadius) {
            ball.z = crossbarZ - bRadius;
            if (ball.vz > 0.f) {
                ball.vz = -ball.vz * 0.2f;
                hitNetFabric = true;
            }
        }

        // B. BACK NET
        if (goal.isHomeGoal && bPos.x < backX + bRadius) {
            bPos.x = backX + bRadius;
            if (velocity.x < 0.f) { ball.velocity.x = -velocity.x * 0.1f; hitNetFabric = true; }
        }
        else if (!goal.isHomeGoal && bPos.x > backX - bRadius) {
            bPos.x = backX - bRadius;
            if (velocity.x > 0.f) { ball.velocity.x = -velocity.x * 0.1f; hitNetFabric = true; }
        }

        // C. SIDE NETS (From Inside)
        if (bPos.y < topY + bRadius) {
            bPos.y = topY + bRadius;
            if (velocity.y < 0.f) { ball.velocity.y = -velocity.y * 0.1f; hitNetFabric = true; }
        }
        else if (bPos.y > bottomY - bRadius) {
            bPos.y = bottomY - bRadius;
            if (velocity.y > 0.f) { ball.velocity.y = -velocity.y * 0.1f; hitNetFabric = true; }
        }

        // ==========================================
        // --- THE AUDIO FIX ---
        // ==========================================
        // The sound is now strictly locked behind the physical collision check!
        if (hitNetFabric) {
            float hitSpeed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + ball.vz * ball.vz);
            if (hitSpeed > 300.f) { // Added a minimum threshold to ignore resting touches
                float netVol = std::clamp(hitSpeed / 15.f, 30.f, 90.f);
                soundManager.playRandomSound("net", 3, netVol, 0.1f);
            }
        }

        ball.shape.setPosition(bPos);
        ball.velocity.x *= 0.95f;
        ball.velocity.y *= 0.95f;
    }
    else {
        // THE BALL IS OUTSIDE THE NET.
        bool inNetX = (bPos.x > minX && bPos.x < maxX);
        bool inNetY = (bPos.y > topY && bPos.y < bottomY);

        // A. ROOF (Landing on top of the net from the outside)
        if (inNetX && inNetY && ballZ < crossbarZ + bRadius && ballZ > crossbarZ) {
            ball.z = crossbarZ + bRadius;
            if (ball.vz < 0.f) ball.vz = -ball.vz * 0.3f;
            ball.velocity.x *= 0.8f;
            ball.velocity.y *= 0.8f;
        }

        // B. SIDE NETS (Hitting from the Outside)
        if (ballZ < crossbarZ && inNetX) {
            if (bPos.y > topY - bRadius && bPos.y <= topY) {
                bPos.y = topY - bRadius;
                if (velocity.y > 0.f) ball.velocity.y = -velocity.y * 0.3f;
                ball.shape.setPosition(bPos);
            }
            else if (bPos.y < bottomY + bRadius && bPos.y >= bottomY) {
                bPos.y = bottomY + bRadius;
                if (velocity.y < 0.f) ball.velocity.y = -velocity.y * 0.3f;
                ball.shape.setPosition(bPos);
            }
        }

        // C. BACK NET (Hitting from the Outside)
        if (ballZ < crossbarZ && inNetY) {
            if (goal.isHomeGoal && bPos.x > backX - bRadius && bPos.x <= backX) {
                bPos.x = backX - bRadius;
                if (velocity.x > 0.f) ball.velocity.x = -velocity.x * 0.3f;
                ball.shape.setPosition(bPos);
            }
            else if (!goal.isHomeGoal && bPos.x < backX + bRadius && bPos.x >= backX) {
                bPos.x = backX + bRadius;
                if (velocity.x < 0.f) ball.velocity.x = -velocity.x * 0.3f;
                ball.shape.setPosition(bPos);
            }
        }
    }
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
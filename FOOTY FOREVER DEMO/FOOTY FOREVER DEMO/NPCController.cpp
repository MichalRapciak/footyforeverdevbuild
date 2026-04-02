#include "NPCController.h"
#include "Ball.h"
#include "UserPlayer.h"
#include "Pitch.h"
#include "MatchContext.h"
#include "MatchReferee.h"
#include <cmath>

NPCController::NPCController() {}
NPCController::~NPCController() {}

void NPCController::update(NPCPlayer& npc, UserPlayer& user, Ball& ball,
    const std::vector<Player*> team, const std::vector<Player*> opposition,
    const Pitch& pitch, TeamState teamState, float dt, Player* firstResponder,
    const MatchReferee& referee)
{
    npc.updateCooldown(dt);
    updateNPCAirPhysics(npc, dt);

    bool isTaker = (&npc == referee.getSetPieceTaker());
    TacticalContext ctx = referee.getTacticalContext(npc.getTeam(), isTaker);
    PositioningMask mask = referee.getPositioningMask(npc.getTeam(), npc.getPositionRole(), pitch);

    // ==========================================
    // 3. MATCH PAUSED & CELEBRATION STATES
    // ==========================================
    if (ctx.state == MatchState::HalfTime || ctx.state == MatchState::FullTime || ctx.state == MatchState::GoalScored)
    {
        // Let the NPC naturally decelerate or walk slowly to their tactical spots.
        // We don't 'return' here so they can still execute their off-ball positioning, 
        // but ctx.ballInfluence is 0.0f and maxSpeed is capped in MatchReferee.
    }
    // ==========================================
    // 4. DEAD BALL STATE HANDLING (Set Pieces)
    // ==========================================
    else if (ctx.state != MatchState::InPlay)
    {
        if (isTaker) {
            // 1. KEEP THEM FROZEN ON THE SPOT
            npc.setVelocity({ 0.f, 0.f });

            if (referee.isWhistleBlown() && npc.getKickCooldown() <= 0.0f) {

                // --- THROW IN ---
                if (ctx.state == MatchState::ThrowIn) {
                    executeThrowIn(npc, ball, team);
                }
                // --- GOAL KICK ---
                else if (ctx.state == MatchState::GoalKick) {
                    distributeBallAsGoalie(npc, ball, team);
                }
                // --- KICK OFF ---
                else if (ctx.state == MatchState::KickOff) {
                    Player* bestTarget = findBestPassOption(npc, team, opposition, user);
                    if (bestTarget) {
                        executePass(npc, ball, bestTarget, opposition);
                    }
                    else {
                        // Safe tap backward if no one is open
                        float dirX = (npc.getTeam() == Team::Home) ? -1.f : 1.f;
                        ball.shoot({ dirX, 0.f }, 30.f, 0.f, 0.f, 0.f);
                        npc.resetKickCooldown();
                    }
                }
                // --- PENALTY ---
                else if (ctx.state == MatchState::Penalty) {
                    bool isHome = (npc.getTeam() == Team::Home);
                    sf::Vector2f goalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);
                    executeShot(npc, ball, goalPos, opposition, dt);
                }
                // --- FREE KICK (Direct vs Pass) ---
                else if (ctx.state == MatchState::FreeKick) {
                    bool isHome = (npc.getTeam() == Team::Home);
                    sf::Vector2f goalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);
                    float distToGoal = dist(npc.getPosition(), goalPos);

                    // A. Calculate the player's Free Kick Threat 
                    float fkSkill = (npc.getDeadBall() * 0.5f) + (npc.getFinishing() * 0.3f) + (npc.getCurl() * 0.2f);
                    float requiredSkill = 65.f + ((distToGoal - 2000.f) / 1500.f) * 20.f;

                    if (distToGoal < 3800.f && fkSkill >= requiredSkill) {
                        // DIRECT SHOT (Over the wall!)
                        float targetY = (npc.getPosition().y < 3500.f) ? 3250.f : 3750.f;
                        sf::Vector2f cornerAim = normalize(sf::Vector2f(goalPos.x, targetY) - npc.getPosition());

                        float power = std::min(npc.getKickPower() * 0.95f, 100.f);
                        float spinDirection = (targetY < 3500.f) ? npc.getCurl() : -npc.getCurl();
                        float loft = 450.f + (npc.getDeadBall() * 1.5f);
                        float dipSpin = npc.getDeadBall() * 0.7f;

                        ball.shoot(cornerAim, power, spinDirection, loft, dipSpin);
                        npc.resetKickCooldown();
                    }
                    else {
                        // INDIRECT PASS
                        Player* bestTarget = findBestPassOption(npc, team, opposition, user);
                        if (bestTarget) {
                            executePass(npc, ball, bestTarget, opposition);
                        }
                        else {
                            sf::Vector2f boxPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin - 800.f, 3500.f) : sf::Vector2f(pitch.margin + 800.f, 3500.f);
                            ball.shoot(normalize(boxPos - npc.getPosition()), 85.f, 0.f, 600.f, 20.f);
                            npc.resetKickCooldown();
                        }
                    }
                }
                // --- CORNER KICK ---
                else if (ctx.state == MatchState::Corner) {
                    Player* bestTarget = findBestPassOption(npc, team, opposition, user);
                    if (bestTarget) {
                        executePass(npc, ball, bestTarget, opposition);
                    }
                    else {
                        bool isHome = (npc.getTeam() == Team::Home);
                        sf::Vector2f boxPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin - 800.f, 3500.f) : sf::Vector2f(pitch.margin + 800.f, 3500.f);
                        float curl = (npc.getPosition().y < 3500.f) ? npc.getCurl() : -npc.getCurl();

                        ball.shoot(normalize(boxPos - npc.getPosition()), 80.f, curl, 750.f, 30.f);
                        npc.resetKickCooldown();
                    }
                }
            }

            // Aim at the center of the pitch while waiting
            npc.setRotationToward(sf::Vector2f(pitch.totalWidth / 2.f, pitch.totalHeight / 2.f));
            return;
        }

        // Non-takers skip straight to off-ball movement in the block below.
    }

    // ==========================================
    // 5. NORMAL OPEN PLAY LOGIC (InPlay)
    // ==========================================
    if (ball.z > 40.f) {
        handleNPCJumpLogic(npc, ball);

        bool isHome = (npc.getTeam() == Team::Home);
        sf::Vector2f oppGoalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);
        sf::Vector2f myGoalPos = isHome ? sf::Vector2f(pitch.margin, 3500.f) : sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f);

        float distToOppGoal = dist(npc.getPosition(), oppGoalPos);
        float distToMyGoal = dist(npc.getPosition(), myGoalPos);

        float relativeHeight = ball.z - npc.z;
        bool isVolley = (relativeHeight >= 40.f && relativeHeight < 140.f);

        // META NERF: Banning Midfield Volleys
        bool allowStrike = true;
        if (isVolley && distToOppGoal > 2500.f && distToMyGoal > 2500.f) {
            allowStrike = false;
        }

        if (allowStrike) {
            // Set isShot = true ONLY if they are near the opponent's goal!
            bool isShooting = (distToOppGoal < 2500.f);
            if (tryNPCAerialStrike(npc, ball, normalize(oppGoalPos - npc.getPosition()), isShooting)) return;
        }
    }

    if (npc.getPositionRole() == PositionRole::Goalkeeper) {
        handleGoalkeeping(npc, ball, pitch, team, opposition, dt);
    }
    else {
        sf::Vector2f ballPos = ball.getPosition();
        sf::Vector2f npcPos = npc.getPosition();
        float distToBall = dist(npcPos, ballPos);

        sf::Vector2f finalDirection(0.f, 0.f);
        bool isSprinting = false;
        float distToTarget = 0.f;

        if (ball.getOwner() == &npc) {
            // --- ON THE BALL ---
            finalDirection = handlePossession(npc, ball, team, opposition, user, pitch, dt, ctx.state);
            distToTarget = 500.f;

            float closestDist = 9999.f;
            for (auto* opp : opposition) {
                float d = dist(npcPos, opp->getPosition());
                if (d < closestDist) closestDist = d;
            }
            isSprinting = (closestDist > 600.f);
        }
        else {
            // --- OFF THE BALL ---
            sf::Vector2f targetPos = decideTargetPosition(npc, ball, pitch, teamState, opposition, firstResponder, mask);

            if (ctx.state == MatchState::KickOff && !referee.isWhistleBlown()) {
                npc.setPosition(targetPos);
                npc.setVelocity({ 0.f, 0.f });
                npc.setRotationToward(ball.getPosition());
                return; // Skip the rest of the physics!
            }

            sf::Vector2f toTarget = targetPos - npcPos;
            distToTarget = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
            sf::Vector2f separation = calculateSeparation(npc, team, opposition, ballPos);
            finalDirection = normalize(normalize(toTarget) + (separation * 0.8f));

            float awarenessReach = 200.0f + ((npc.getAwareness() / 100.0f) * 800.0f);
            isSprinting = (distToTarget > 600.f || (distToBall < awarenessReach && !ball.hasOwner()));

            // Auto-Possess
            if (!ball.hasOwner() && distToBall < 70.f && ctx.canPossess) {
                if (npc.getState() != PlayerState::Tackling &&
                    npc.getState() != PlayerState::Stunned &&
                    ball.z < 40.f &&
                    npc.getKickCooldown() <= 0.0f) {
                    ball.possess(&npc);
                }
            }

            // Tackle Logic
            if (ball.hasOwner() && ball.getOwner()->getTeam() != npc.getTeam() && distToBall < 250.f && ctx.canTackle) {
                Player* attacker = ball.getOwner();

                if (attacker->getPositionRole() != PositionRole::Goalkeeper) {

                    float distToAttacker = dist(npcPos, attacker->getPosition());
                    float awareness = npc.getAwareness();

                    // --- SMART FOUL AVOIDANCE ---
                    // If we are significantly closer to the attacker's body than the ball itself (e.g. trailing from behind)
                    // High awareness players will refuse to slide tackle!
                    bool trailingBehind = (distToAttacker < distToBall - 30.f);

                    bool safeToTackle = true;
                    if (trailingBehind) {
                        // 99 Awareness = 1% chance to make a stupid tackle from behind.
                        // 20 Awareness = 80% chance to hack them down.
                        if ((rand() % 100) < awareness) {
                            safeToTackle = false;
                        }
                    }

                    // We also only commit to the slide if we are within realistic striking distance (120px)
                    if (safeToTackle && distToBall < 120.f) {
                        sf::Vector2f futureBallPos = ballPos + (ball.getVelocity() * 0.24f);
                        npc.startTackle(normalize(futureBallPos - npcPos));
                    }
                }
            }
        }

        if (npc.getState() != PlayerState::Stunned && npc.getState() != PlayerState::Stumbled) {
            // Check if the opponent keeper is holding it
            bool isKeeperBall = (ball.hasOwner() && ball.getOwner()->getPositionRole() == PositionRole::Goalkeeper);

            applyMovementPhysics(npc, finalDirection, isSprinting, dt, distToTarget, ball, firstResponder, pitch, isKeeperBall, ctx);
        }
    }

    // Only force rotation toward the ball if we are standing still AND we don't own it
    if (std::sqrt(npc.getVelocity().x * npc.getVelocity().x + npc.getVelocity().y * npc.getVelocity().y) < 2.f) {
        if (ball.getOwner() != &npc) {
            npc.setRotationToward(ball.getPosition());
        }
    }
}

void NPCController::applyMovementPhysics(NPCPlayer& npc, sf::Vector2f directionInput, bool isSprinting,
    float dt, float distToTarget, Ball& ball, Player* firstResponder,
    const Pitch& pitch, bool keeperBall, TacticalContext ctx)
{
    sf::Vector2f vel = npc.getVelocity();
    float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
    float sprintSpeed = npc.getTopSpeed() * 10.f;
    sf::Vector2f npcPos = npc.getPosition();

    // --- 1. ARRIVAL & SPEED LIMITS ---
    float maxSpeed = isSprinting ? sprintSpeed : sprintSpeed * 0.5f;

    // CONTEXT SPEED LIMIT
    maxSpeed = std::min(maxSpeed, ctx.maxSpeedLimit);

    float slowingRadius = 600.f;
    float stopRadius = 200.f;
    float distToBall = dist(npcPos, ball.getPosition());

    bool isChasingBall = !keeperBall && ctx.ballInfluence > 0.0f && (&npc == firstResponder || (distToTarget < 450.f && distToBall < 300.f && npc.getState() != PlayerState::Tackling));

    if (isChasingBall) {
        maxSpeed = std::min(sprintSpeed, ctx.maxSpeedLimit);

        // --- THE "HONE IN" LOGIC ---
        sf::Vector2f toBall = ball.getPosition() - npcPos;
        float dBall = std::sqrt(toBall.x * toBall.x + toBall.y * toBall.y);

        if (dBall > 10.f) {
            sf::Vector2f ballDir = toBall / dBall;
            float velocityTowardBall = (vel.x * ballDir.x + vel.y * ballDir.y);
            sf::Vector2f tangentialVel = vel - (ballDir * velocityTowardBall);

            float dampingStrength = 6.0f;
            vel -= tangentialVel * dampingStrength * dt;

            float pullStrength = 0.55f * ctx.ballInfluence;
            if (directionInput.x == 0.f && directionInput.y == 0.f) {
                directionInput = ballDir;
            }
            else {
                directionInput = (directionInput * (1.0f - pullStrength)) + (ballDir * pullStrength);
            }
        }
    }
    else {
        // --- AGGRESSION: PRESSING / JOCKEYING ---
        float aggressionFactor = npc.getAggression() / 100.0f;
        float jockeyClamp = 0.60f + (aggressionFactor * 0.30f);

        maxSpeed *= jockeyClamp;

        // ==========================================
        // NEW: TACTICAL SLOWDOWN BYPASS 
        // ==========================================
        // If we are defending and the ball is in play, we NEVER want to slow down 
        // as we approach our defensive mark. We want snappy, aggressive micro-adjustments!
        if (ctx.state == MatchState::InPlay && !ctx.canPossess) {
            slowingRadius = 50.f; // Almost no slowdown radius!
            stopRadius = 10.f;
        }

        if (distToTarget < slowingRadius && directionInput != sf::Vector2f(0, 0)) {
            float ramp = (distToTarget - stopRadius) / (slowingRadius - stopRadius);
            ramp = std::max(0.f, std::min(ramp, 1.f));
            maxSpeed *= ramp;
        }
    }

    // --- 2. TACKLE LUNGE ---
    if (npc.getState() == PlayerState::Tackling) {
        float slideDecel = 2400.f;
        if (speed > 0.f) {
            float newSpeed = std::max(0.f, speed - (slideDecel * dt));
            vel = (vel / speed) * newSpeed;
        }
        npc.setVelocity(vel);
        return;
    }

    // --- 3. DIRECTIONAL CALCS ---
    if (directionInput != sf::Vector2f(0.f, 0.f) && (distToTarget > stopRadius || isChasingBall)) {

        sf::Vector2f forwardDir = npc.getAimDirection();
        sf::Vector2f rightDir = sf::Vector2f(-forwardDir.y, forwardDir.x);

        float currentFwdSpeed = (vel.x * forwardDir.x + vel.y * forwardDir.y);
        float currentSideSpeed = (vel.x * rightDir.x + vel.y * rightDir.y);
        float inputForward = (directionInput.x * forwardDir.x + directionInput.y * forwardDir.y);
        float inputRight = (directionInput.x * rightDir.x + directionInput.y * rightDir.y);

        // --- BRAKING ---
        float brakeForce = npc.getAgility() * 2.0f;
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

        // --- 4. DYNAMIC TURN SHARPNESS ---
        float turnDotProduct = (forwardDir.x * directionInput.x) + (forwardDir.y * directionInput.y);
        float speedRatio = std::clamp(speed / sprintSpeed, 0.0f, 1.0f);
        float turnPenaltyIntensity = 0.2f + (speedRatio * 1.0f);
        float turnMultiplier = 1.8f;

        if (turnDotProduct < 0.95f) {
            turnMultiplier = std::max(0.15f, 1.5f - (std::abs(1.0f - turnDotProduct) * turnPenaltyIntensity));
        }

        // HARD TURN / U-TURN BRAKING
        if (turnDotProduct < 0.0f)
        {
            float sharpnessMultiplier = std::abs(turnDotProduct);
            float hardBrakeFriction = npc.getAgility() * 15.0f * sharpnessMultiplier * dt;
            float newSpeed = std::max(0.0f, speed - hardBrakeFriction);
            if (speed > 0.0f)
            {
                vel = forwardDir * newSpeed;
                speed = newSpeed;
            }
        }

        // --- 5. ACCELERATION ---
        float sprintRatio = speed / maxSpeed;
        float burstMultiplier = 1.f + (1.f - std::min(sprintRatio, 1.f)) * 12.f;
        float distanceMultiplier = (distToTarget < 500.f) ? (1.f + (1.f - (distToTarget / 500.f))) : 1.0f;
        float finalMultiplier = burstMultiplier * distanceMultiplier;

        float fwdAccel = npc.getAcceleration() * finalMultiplier * turnMultiplier;
        float sideAccel = npc.getAgility() * (finalMultiplier + 0.4f);

        if (isChasingBall) { fwdAccel *= 1.1f; sideAccel *= 1.1f; }

        sf::Vector2f accelVec = (forwardDir * inputForward * fwdAccel) + (rightDir * inputRight * sideAccel);
        float forwardSpeedAfterBrake = (vel.x * directionInput.x + vel.y * directionInput.y);

        if (forwardSpeedAfterBrake < maxSpeed || isChasingBall) {
            vel += accelVec * dt;
        }
    }

    // --- 6. PITCH BOUNDARIES ---
    float px = npcPos.x; float py = npcPos.y;
    if (px < pitch.margin || px > pitch.totalWidth - pitch.margin || py < pitch.margin || py > pitch.totalHeight - pitch.margin) {
        vel *= 0.85f; maxSpeed *= 0.5f;
    }

    speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
    if (speed > maxSpeed && speed > 0.1f) vel = (vel / speed) * maxSpeed;
    npc.setVelocity(vel);
}

sf::Vector2f NPCController::calculateSeparation(NPCPlayer& npc, const std::vector<Player*> team, const std::vector<Player*> opponents, sf::Vector2f ballPos)
{
    sf::Vector2f force(0.f, 0.f);
    sf::Vector2f npcPos = npc.getPosition();

    // 1. Who is this NPC focused on?
    Player* myTarget = findNearestOpponent(npcPos, opponents);

    for (auto& teammate : team) {
        if (teammate == &npc) continue;

        sf::Vector2f teamPos = teammate->getPosition();
        sf::Vector2f diff = npcPos - teamPos;
        float distSq = diff.x * diff.x + diff.y * diff.y;

        // 2. Identify Context
        // Are we both marking the same guy? 
        Player* teammateTarget = findNearestOpponent(teamPos, opponents);
        bool markingSameGuy = (myTarget != nullptr && myTarget == teammateTarget);

        // 2m (200px) if double-teaming, 5m (500px) if just wandering
        float currentBubble = markingSameGuy ? 300.f : 600.f;

        if (distSq < currentBubble * currentBubble && distSq > 0.1f) {
            float d = std::sqrt(distSq);
            force += (diff / d) * ((currentBubble - d) / currentBubble);
        }
    }
    return force;
}

// Helper: Normalize
sf::Vector2f NPCController::normalize(sf::Vector2f source) {
    float length = std::sqrt(source.x * source.x + source.y * source.y);
    if (length != 0) return source / length;
    return source;
}

float NPCController::dist(sf::Vector2f p1, sf::Vector2f p2) {
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    return std::sqrt(dx * dx + dy * dy);
}

sf::Vector2f NPCController::decideTargetPosition(NPCPlayer& npc, Ball& ball,
    const Pitch& pitch, TeamState state,
    const std::vector<Player*> opponents, Player* firstResponder, PositioningMask mask)
{
    // ==========================================
    // 1. MASK OVERRIDE (Strict Set Pieces)
    // ==========================================
    if (mask.useManualTarget) {
        return mask.manualTarget;
    }

    // --- DATA GATHERING ---
    bool isHomeSide = (npc.getTeam() == Team::Home);
    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f npcPos = npc.getPosition();
    float distToBall = dist(npcPos, ballPos);
    sf::Vector2f homePos = npc.getHomePosition(isHomeSide, state);
    sf::Vector2f goalPos = isHomeSide ? sf::Vector2f(pitch.margin, 3500.f) : sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f);
    TacticalZone zone = getZoneForRole(npc.getPositionRole());

    // ==========================================
    // 2. APPLY POSITIONING MASK (Dynamic Pitch Warping)
    // ==========================================
    homePos += mask.homeOffset;

    if (mask.lateralSqueeze > 0.0f) {
        homePos.y = homePos.y + ((ballPos.y - homePos.y) * mask.lateralSqueeze);
    }

    zone.forwardLeash *= mask.forwardLeashMod;
    zone.backwardLeash *= mask.backwardLeashMod;

    // Fetch our new stats (Normalized 0.0 to 1.0)
    float aggressionNorm = npc.getAggression() / 100.0f;
    float blockingNorm = npc.getBlocking() / 100.0f;
    sf::Vector2f finalTarget;

    // --- KEEPER CLEARANCE STANDOFF ---
    Player* owner = ball.getOwner();
    if (owner && owner->getTeam() != npc.getTeam() && owner->getPositionRole() == PositionRole::Goalkeeper) {
        sf::Vector2f retreatPos = applyTacticalPositioning(npc, homePos, ballPos, goalPos, TeamState::Defending, zone, pitch, opponents);
        float standoff = 2200.f;
        bool oppIsHome = (owner->getTeam() == Team::Home);
        float oppGoalX = oppIsHome ? pitch.margin : pitch.totalWidth - pitch.margin;

        if (oppIsHome) retreatPos.x = std::max(retreatPos.x, oppGoalX + standoff);
        else retreatPos.x = std::min(retreatPos.x, oppGoalX - standoff);

        retreatPos.y = homePos.y + (ballPos.y - homePos.y) * 0.05f;
        return retreatPos;
    }

    // --- EMERGENCY CHECK ---
    // (Note: If shouldEmergencyChase requires MatchState, you can just pass MatchState::InPlay here 
    // since dead balls are caught by the update() function before this runs anyway!)
    if (shouldEmergencyChase(npc, firstResponder, distToBall, pitch, ball, MatchState::InPlay))
    {
        if (ball.hasOwner() && ball.getOwner()->getTeam() != npc.getTeam())
        {
            float pressDistance = 250.f - (aggressionNorm * 100.f);
            sf::Vector2f toGoal = normalize(goalPos - ballPos);
            finalTarget = ballPos + (toGoal * pressDistance);
        }
        else {
            finalTarget = calculateInterceptionPoint(npc, ball);
        }
    }
    else
    {
        // --- CALCULATE SHAPE POSITION ---
        sf::Vector2f shapePos = applyTacticalPositioning(npc, homePos, ballPos, goalPos, state, zone, pitch, opponents);
        finalTarget = shapePos;

        // --- CALCULATE MARKING POSITION ---
        if (state == TeamState::Defending) {
            Player* threat = findBestThreat(npcPos, opponents, zone, MatchState::InPlay);
            if (threat) {
                sf::Vector2f markingPos = calculateMarkingPosition(npc, threat, goalPos, zone);

                float baseWeightWithBall = 0.5f + (aggressionNorm * 0.5f);
                float baseWeightNoBall = 0.1f + (aggressionNorm * 0.4f);
                float markingWeight = threat->getBallPossession() ? baseWeightWithBall : baseWeightNoBall;

                float dynamicLeashStretch = 400.0f + (aggressionNorm * 600.0f);
                float distToHome = dist(markingPos, homePos);

                if (distToHome > zone.forwardLeash + dynamicLeashStretch) markingWeight = 0.0f;

                finalTarget = (shapePos * (1.0f - markingWeight)) + (markingPos * markingWeight);

                float distToGoal = dist(threat->getPosition(), goalPos);
                if (threat->getBallPossession() && distToGoal < 2500.0f)
                {
                    sf::Vector2f shotLaneDir = normalize(goalPos - ballPos);
                    sf::Vector2f optimalBlockPos = ballPos + (shotLaneDir * 250.0f);
                    finalTarget = (finalTarget * (1.0f - blockingNorm)) + (optimalBlockPos * blockingNorm);
                }
            }
        }
    }

    // 4. APPLY CONSTRAINTS
    return clampToTacticalZone(finalTarget, homePos, zone, distToBall, isHomeSide);
}

sf::Vector2f NPCController::handlePossession(NPCPlayer& npc, Ball& ball,
    const std::vector<Player*> teammates, const std::vector<Player*> opposition,
    UserPlayer& user, const Pitch& pitch, float dt, MatchState matchstate)
{
    npc.updateCooldown(dt);
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);

    // --- Stats ---
    float bc = npc.getBallControl() / 100.f;
    float awareness = npc.getAwareness() / 100.f;
    float passingSkill = (npc.getShortPassing() + npc.getLongPassing()) / 200.f;
    float dribbleSkill = (npc.getBallControl() * 0.6f + npc.getAgility() * 0.4f) / 100.f;
    float finishingSkill = npc.getFinishing() / 100.f;

    // --- Sense Pressure ---
    float closestOppDist = 9999.f;
    Player* nearestOpp = findNearestOpponent(npcPos, opposition);
    if (nearestOpp) closestOppDist = dist(npcPos, nearestOpp->getPosition());

    bool isUnderPressure = (closestOppDist < 600.f);
    bool isCrammed = (closestOppDist < (250.f - (bc * 100.f)));

    sf::Vector2f goalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);

    // ==========================================
    // 1. SHOOTING LOGIC
    // ==========================================
    float distToGoal = dist(npcPos, goalPos);
    // 99 Finishing = shoots from ~30m. 0 Finishing = shoots from ~18m.
    float shotRange = 1800.f + (finishingSkill * 1200.f);

    if (distToGoal < shotRange && npc.getKickCooldown() <= 0.0f && matchstate == MatchState::InPlay) {
        // If we are crammed, only shoot if we are VERY close (inside the box). 
        // Otherwise, prioritize passing out of pressure.
        if (!isCrammed || distToGoal < 1650.f) {
            executeShot(npc, ball, goalPos, opposition, dt);
            return { 0.f, 0.f };
        }
    }

    // ==========================================
    // 2. THE INSTANT SCAN (Passing)
    // ==========================================
    npc.m_passTimer += dt;
    float scanFrequency = isUnderPressure ? 0.05f : 0.3f;

    if (npc.getKickCooldown() <= 0.0f && npc.m_passTimer > scanFrequency) {
        npc.m_passTimer = 0.f;
        Player* bestTarget = findBestPassOption(npc, teammates, opposition, user);

        if (bestTarget) {
            float targetOppDist = 9999.f;
            Player* tOpp = findNearestOpponent(bestTarget->getPosition(), opposition);
            if (tOpp) targetOppDist = dist(bestTarget->getPosition(), tOpp->getPosition());

            bool targetSafer = (targetOppDist > closestOppDist + 200.f);
            bool skillCheck = (passingSkill > (dribbleSkill * 1.2f));

            if (isCrammed || (isUnderPressure && targetSafer) || (isUnderPressure && skillCheck)) {
                executePass(npc, ball, bestTarget, opposition);
                return { 0.f, 0.f };
            }
        }
    }

    // ==========================================
    // 3. DRIBBLE
    // ==========================================
    if (matchstate == MatchState::InPlay) {
        sf::Vector2f dribbleDir = calculateDribbleDirection(npc, goalPos, opposition);

        float speedMult = 0.9f + (awareness * 0.1f);
        if (closestOppDist < 200.f) {
            speedMult = 0.75f;
        }

        sf::Vector2f vel = npc.getVelocity();
        float velLen = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        if (velLen > 10.f) {
            float turnDot = (dribbleDir.x * (vel.x / velLen) + dribbleDir.y * (vel.y / velLen));
            if (turnDot < 0.7f) speedMult *= 0.85f;
        }

        return dribbleDir * speedMult;
    }

    return { 0.f, 0.f };
}

Player* NPCController::findBestPassOption(NPCPlayer& npc,
    const std::vector<Player*> team, const std::vector<Player*> opposition,
    UserPlayer& user)
{
    Player* bestOption = nullptr;
    float bestScore = -1e9f;
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);

    const float friction = 800.f;
    const float engineMultiplier = 52.0f;

    // --- 1. FETCH STATS ---
    float shortPassNorm = npc.getShortPassing() / 100.f;
    float longPassNorm = npc.getLongPassing() / 100.f;
    float curlNorm = npc.getCurl() / 100.f;
    float visionNorm = npc.getAwareness() / 100.f;

    // Pitch Context
    bool inOwnHalf = isHome ? (npcPos.x < 5000.f) : (npcPos.x > 5000.f);
    bool inOwnDeepBox = isHome ? (npcPos.x < 2500.f) : (npcPos.x > 7500.f);
    float riskMultiplier = inOwnHalf ? 2.5f : 0.8f;

    // --- 2. PRESSURE SCAN ---
    Player* closestPresser = findNearestOpponent(npcPos, opposition);
    float presserDist = closestPresser ? dist(npcPos, closestPresser->getPosition()) : 9999.f;
    bool isCrammed = presserDist < 350.f;

    std::vector<Player*> potentialReceivers;
    for (auto& t : team) if (t != &npc) potentialReceivers.push_back(t);
    if (npc.getTeam() == user.getTeam()) potentialReceivers.push_back(&user);

    for (Player* target : potentialReceivers) {
        sf::Vector2f targetPos = target->getPosition();
        float distToTarget = dist(npcPos, targetPos);
        bool isGoalkeeper = (target->getPositionRole() == PositionRole::Goalkeeper);

        // Physics Prediction
        float arrivalSpeed = 500.f - (std::clamp(distToTarget / 4000.f, 0.f, 1.f) * 300.f);
        float requiredV0 = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * friction * distToTarget));
        if (requiredV0 / engineMultiplier > npc.getKickPower()) continue;

        sf::Vector2f predictedPos = targetPos + (target->getVelocity() * (distToTarget / requiredV0));
        float predictedDist = dist(npcPos, predictedPos);
        sf::Vector2f passDir = normalize(predictedPos - npcPos);

        // --- 3. BASE SCORING ---
        float progress = isHome ? (predictedPos.x - npcPos.x) : (npcPos.x - predictedPos.x);

        // POSSESSION BIAS & PROGRESS BUFF: 
        // Increased progress multiplier from 0.4 to 0.65 so forward passes score MUCH higher.
        float score = 600.f + (progress * 0.65f);

        // --- 4. TRAJECTORY CONSTRAINTS ---
        bool directLaneBlocked = false;
        bool canCurlAround = false;

        for (auto* opp : opposition) {
            sf::Vector2f toOpp = opp->getPosition() - npcPos;
            float dOpp = dist(npcPos, opp->getPosition());
            if (dOpp < predictedDist && dOpp > 100.f) {
                float alignment = (passDir.x * (toOpp.x / dOpp) + passDir.y * (toOpp.y / dOpp));
                if (alignment > 0.92f) {
                    directLaneBlocked = true;
                    if (alignment < 0.97f && curlNorm > 0.6f) canCurlAround = true;
                }
            }
        }

        // ==========================================
        // RULE 1: ANTI-CHIP 
        // ==========================================
        bool wantHigh = (distToTarget > 2200.f || (directLaneBlocked && !canCurlAround));

        if (wantHigh && distToTarget < 1500.f) {
            score -= 5000.f;
        }

        // ==========================================
        // RULE 2: PRESSURE HANDLING (The "Outlet" Logic)
        // ==========================================
        float pressureFactor = std::clamp((600.f - presserDist) / 400.f, 0.f, 1.f);

        if (pressureFactor > 0.1f) {
            if (wantHigh && distToTarget < 2000.f) {
                score -= 4000.f * pressureFactor;
            }

            if (distToTarget < 1500.f) {
                float desperationBonus = 3500.f * pressureFactor;
                if (directLaneBlocked) desperationBonus *= 0.5f;
                score += desperationBonus;
            }
        }

        // ==========================================
        // RULE 3: EMERGENCY CLEARANCE
        // ==========================================
        if (inOwnDeepBox && isCrammed) {
            if (isGoalkeeper && !directLaneBlocked) {
                score += 6000.f;
            }
            else if (progress > 1500.f) {
                score += 4000.f;
            }
        }

        // --- 5. STAT-BASED RISK & LONG PASSING BUFF ---
        Player* marker = findNearestOpponent(predictedPos, opposition);
        float distToMarker = marker ? dist(predictedPos, marker->getPosition()) : 9999.f;

        if (wantHigh) {
            // BRAVERY BUFF: Halved the penalty for low long-passing stats (from 1500 to 700)
            score -= (1.0f - longPassNorm) * 700.f * riskMultiplier;

            if (distToMarker < 400.f) {
                // Halved marking penalty for high passes (from 2000 to 1000)
                score -= 1000.f;
            }
            else if (distToTarget > 2500.f && distToMarker > 800.f) {
                // HOLLYWOOD PASS BONUS: If target is far away AND open, reward vision and long passing!
                score += 1500.f * visionNorm * longPassNorm;
            }

        }
        else if (directLaneBlocked && canCurlAround) {
            score -= (1.0f - curlNorm) * 1000.f * riskMultiplier;
        }
        else if (!directLaneBlocked) {
            score += 400.f * shortPassNorm;
        }

        // Receiver marking penalty (Ground)
        if (!wantHigh && distToMarker < 250.f) {
            score -= 1000.f * riskMultiplier;
        }

        if (score > bestScore) {
            bestScore = score;
            bestOption = target;
        }
    }

    float finalThreshold = isCrammed ? -2000.f : -400.f;
    return (bestScore > finalThreshold) ? bestOption : nullptr;
}

sf::Vector2f NPCController::calculateDribbleDirection(NPCPlayer& npc, sf::Vector2f goalPos, const std::vector<Player*>& opposition)
{
    sf::Vector2f npcPos = npc.getPosition();
    sf::Vector2f baseDir = normalize(goalPos - npcPos);
    sf::Vector2f bestDir = baseDir;
    float bestScore = -1e9f;

    // --- PLAYER IDENTITY (Stats) ---
    float bcNorm = npc.getBallControl() / 100.f;
    float agilityNorm = npc.getAgility() / 100.f;
    float speedNorm = npc.getTopSpeed() / 10.f; // getTopSpeed returns 1-10

    // Determine Dribbling Style
    bool isSpeedster = (speedNorm > 0.85f && speedNorm > bcNorm);
    bool isTrickster = (bcNorm > 0.8f && agilityNorm > 0.8f);

    const int numSamples = 16; // Increased resolution for sharper turns
    sf::Vector2f currentVel = npc.getVelocity();
    float currentSpeed = std::sqrt(currentVel.x * currentVel.x + currentVel.y * currentVel.y);
    sf::Vector2f currentDir = (currentSpeed > 10.f) ? (currentVel / currentSpeed) : baseDir;
    sf::Vector2f lastDribbleDir = npc.getDribbleTargetDir();

    float overallMinOppDist = 9999.f;
    Player* closestOpp = nullptr;
    for (auto* opp : opposition) {
        float d = dist(npcPos, opp->getPosition());
        if (d < overallMinOppDist) {
            overallMinOppDist = d;
            closestOpp = opp;
        }
    }

    for (int i = 0; i < numSamples; ++i) {
        float angleDeg = -135.f + (i * (270.f / (numSamples - 1))); // Test a wider 270-degree arc!
        float rad = angleDeg * 3.14159f / 180.f;

        sf::Vector2f testDir(
            baseDir.x * std::cos(rad) - baseDir.y * std::sin(rad),
            baseDir.x * std::sin(rad) + baseDir.y * std::cos(rad)
        );

        float score = 0.f;

        // 1. BASE ATTACK BIAS
        score += 350.f * (testDir.x * baseDir.x + testDir.y * baseDir.y);

        // 2. STYLE-SPECIFIC BEHAVIORS
        if (isTrickster && overallMinOppDist < 300.f) {
            // THE TRICKSTER: Loves sharp 90-degree cuts when defenders get close
            float turnAngle = std::abs(angleDeg);
            if (turnAngle > 70.f && turnAngle < 110.f) {
                score += 500.f * bcNorm * agilityNorm; // Huge reward for sharp cuts
            }
            // Penalty for maintaining the exact same direction (predictable)
            float predictability = (testDir.x * lastDribbleDir.x + testDir.y * lastDribbleDir.y);
            score -= predictability * 200.f;
        }
        else if (isSpeedster && overallMinOppDist < 400.f) {
            // THE SPEEDSTER: Wants to knock it past the defender into open space
            // Finds the angle that goes *around* the defender but maintains forward momentum
            if (closestOpp) {
                sf::Vector2f toOpp = normalize(closestOpp->getPosition() - npcPos);
                float oppAlignment = (testDir.x * toOpp.x + testDir.y * toOpp.y);

                // If the angle completely dodges the defender (-0.2 to 0.5 alignment)
                if (oppAlignment < 0.5f && oppAlignment > -0.2f) {
                    score += 600.f * speedNorm;
                }
            }
        }
        else {
            // STANDARD DRIBBLE: Wants momentum and stickiness
            float turnAlignment = (testDir.x * currentDir.x + testDir.y * currentDir.y);
            score -= (1.0f - turnAlignment) * 300.f * (1.0f - agilityNorm);

            float stickiness = (testDir.x * lastDribbleDir.x + testDir.y * lastDribbleDir.y);
            if (stickiness > 0.90f) score += 200.f;
        }

        // 3. SPATIAL AWARENESS (Avoid running straight into people)
        for (auto* opp : opposition) {
            sf::Vector2f toOppVec = opp->getPosition() - npcPos;
            float d = dist(npcPos, opp->getPosition());

            if (d < 500.f) {
                float dNorm = d / 500.f;
                sf::Vector2f toOppDir = toOppVec / d;
                float oppAlignment = (testDir.x * toOppDir.x + testDir.y * toOppDir.y);

                if (oppAlignment > 0.3f) {
                    score -= (1.0f - dNorm) * 800.f * oppAlignment;
                }
            }
        }

        if (score > bestScore) {
            bestScore = score;
            bestDir = testDir;
        }
    }

    npc.setDribbleTargetDir(bestDir);
    return bestDir;
}

void NPCController::executePass(NPCPlayer& npc, Ball& ball, Player* target, const std::vector<Player*>& opposition) {
    sf::Vector2f npcPos = npc.getPosition();
    sf::Vector2f targetPos = target->getPosition();
    sf::Vector2f targetVel = target->getVelocity();

    const float friction = 800.f;
    const float engineMultiplier = 52.0f;

    // --- 1. DYNAMIC ARRIVAL SPEED & PREDICTION ---
    float distToTarget = dist(npcPos, targetPos);
    float arrivalSpeed = 500.f - (std::clamp(distToTarget / 4000.f, 0.f, 1.f) * 300.f);

    float v0_est = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * friction * distToTarget));
    float travelTime = (distToTarget > 1200.f) ? (distToTarget / v0_est) + 0.3f : distToTarget / ((v0_est + arrivalSpeed) * 0.5f);

    sf::Vector2f predictedPlayerPos = targetPos + (targetVel * travelTime);
    sf::Vector2f dirToTarget = normalize(predictedPlayerPos - npcPos);

    // Lead the pass slightly
    sf::Vector2f aimSpot = predictedPlayerPos + (dirToTarget * 150.f);
    float finalDist = dist(npcPos, aimSpot);
    sf::Vector2f directDir = normalize(aimSpot - npcPos);

    // --- 2. TRAJECTORY SELECTION: GROUND, CURL, OR HIGH? ---
    // Remove "Panic Chip"—AI now makes tactical choices based on space.
    bool goHigh = (distToTarget > 2000.f);
    bool needsCurl = false;
    float curlSide = 0.f;

    float curlNorm = npc.getCurl() / 100.f;

    for (auto* opp : opposition) {
        sf::Vector2f toOpp = opp->getPosition() - npcPos;
        float dOpp = dist(npcPos, opp->getPosition());

        if (dOpp < finalDist && dOpp > 100.f) {
            float alignment = (directDir.x * (toOpp.x / dOpp) + directDir.y * (toOpp.y / dOpp));

            // If the lane is blocked...
            if (alignment > 0.90f) {
                // If it's a short/medium pass and we have curl skill, use Curl instead of High Pass.
                if (distToTarget < 2500.f && curlNorm > 0.5f) {
                    needsCurl = true;
                    goHigh = false;
                    float cross = (directDir.x * toOpp.y - directDir.y * toOpp.x);
                    curlSide = (cross > 0) ? -1.0f : 1.0f;
                }
                else {
                    goHigh = true; // Distance too great or skill too low, must go over.
                }
            }
        }
    }

    // --- 3. POWER & STAT CALCULATION ---
    float finalRequiredV0_World = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * friction * finalDist));

    // Driven ground passes move faster than lofts
    float aiPowerBuff = goHigh ? 0.95f : 1.25f;
    finalRequiredV0_World *= aiPowerBuff;

    float passingStat = goHigh ? npc.getLongPassing() : npc.getShortPassing();
    float rawStatNeeded = finalRequiredV0_World / engineMultiplier;

    // Apply stat-based weight error (Elite players are more consistent)
    float weightErrorFactor = (1.0f - (passingStat / 100.f)) * 0.12f;
    float randomWeight = 1.0f + (((rand() % 200) - 100) / 100.f) * weightErrorFactor;

    float finalStatUsed = std::min(rawStatNeeded * randomWeight, npc.getKickPower());
    float kickStrength = std::clamp(finalStatUsed / 100.f, 0.0f, 1.0f);

    // --- 4. TRAJECTORY (VZ) ---
    float requiredVz = 5.f;
    float finalBackspin = 0.f;

    if (goHigh) {
        // Dynamic loft: shorter lobs stay lower (max 650), long crosses go higher (max 850)
        float maxLoft = std::clamp(300.f + (distToTarget * 0.15f), 400.f, 850.f);
        requiredVz = maxLoft - (kickStrength * 150.f) - ((passingStat / 100.f) * 80.f);
        finalBackspin = (passingStat * 0.5f) + (kickStrength * 45.f);
        requiredVz = std::max(requiredVz, 220.f);
    }

    // --- 5. EXECUTE CURL ---
    float finalSpin = 0.f;
    sf::Vector2f finalDir = directDir;

    if (needsCurl && !goHigh) {
        bool isLeftFoot = !npc.usingRightFoot();
        float multiplier = (1.1f + kickStrength / 2.f);

        if (curlSide < 0) {
            finalSpin = isLeftFoot ? (npc.getCurl() * multiplier) : (-(npc.getCurl() / 2.f) * multiplier);
        }
        else {
            finalSpin = isLeftFoot ? ((npc.getCurl() / 2.f) * multiplier) : (-npc.getCurl() * multiplier);
        }

        // Adjust aim so it curves back into the target
        float offsetRad = -((finalSpin / 100.f) * 14.f) * (3.14159f / 180.f);
        finalDir = sf::Vector2f(
            directDir.x * std::cos(offsetRad) - directDir.y * std::sin(offsetRad),
            directDir.x * std::sin(offsetRad) + directDir.y * std::cos(offsetRad)
        );
    }
    // --- 6. LOG & SHOOT ---
    ball.shoot(finalDir, finalStatUsed, finalSpin, requiredVz, finalBackspin);
    npc.resetKickCooldown();
}
void NPCController::executeShot(NPCPlayer& npc, Ball& ball, sf::Vector2f goalPos, const std::vector<Player*>& opposition, float dt) {
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);

    // ==========================================
    // 1. SIMULATED CHARGE & STATS
    // ==========================================
    // Give the AI a random charge between 0.6 and 1.0 to mirror human charge times
    float simulatedCharge = 0.6f + ((rand() % 41) / 100.f);
    float finishingStat = npc.getFinishing();
    float finishingNorm = finishingStat / 100.0f;

    // ==========================================
    // 2. THE STRONG FINISHING AIMBOT 
    // ==========================================
    float topPostY = 3500.f - 366.f;
    float bottomPostY = 3500.f + 366.f;

    // A. The "Raw Aim" (NPC just aims generally at the net)
    float rawTargetY = topPostY + (static_cast<float>(rand()) / RAND_MAX * 732.f);
    sf::Vector2f rawDir = normalize(sf::Vector2f(goalPos.x, rawTargetY) - npcPos);

    // B. The "Perfect Corner" (Inside the post)
    float perfectTargetY = (rawTargetY < 3500.f) ? (topPostY + 40.f) : (bottomPostY - 40.f);
    sf::Vector2f perfectDir = normalize(sf::Vector2f(goalPos.x, perfectTargetY) - npcPos);

    // C. Magnetism Strength
    // AI gets a very strong pull. 99 Finishing = 85% lock onto the perfect corner pixel.
    // 20 Finishing = only a 17% nudge toward the corner.
    float magnetism = finishingNorm * 0.85f;

    // D. Blend the raw aim with the perfect corner aim
    sf::Vector2f directDir = (rawDir * (1.0f - magnetism)) + (perfectDir * magnetism);
    directDir = normalize(directDir);

    // We store the perfectTargetY so the Curl logic knows which way to bend it
    float targetY = perfectTargetY;

    // ==========================================
    // 2.5. GOALKEEPER POSITION CHECK (THE CHIP)
    // ==========================================
    bool tryChip = false;
    for (auto* opp : opposition) {
        if (opp->getPositionRole() == PositionRole::Goalkeeper) {
            // Check how far the keeper has strayed from their goal line
            float gkDistFromLine = std::abs(opp->getPosition().x - goalPos.x);

            // If the keeper is more than ~400 units off their line
            if (gkDistFromLine > 400.f) {
                // Higher finishing stat = more likely to spot the chip opportunity
                if ((rand() % 100) < (finishingStat * 0.8f)) {
                    tryChip = true;
                }
            }
            break;
        }
    }

    // ==========================================
    // 3. INTENTIONAL BEND & OBSTACLE DODGING
    // ==========================================
    float curlDirection = 0.f;
    bool needsCurl = false;

    // A. Obstacle Dodging (Priority 1)
    for (auto* opp : opposition) {
        sf::Vector2f toOpp = opp->getPosition() - npcPos;
        float dOpp = std::sqrt(toOpp.x * toOpp.x + toOpp.y * toOpp.y);

        if (dOpp < 800.f && dOpp > 50.f) {
            sf::Vector2f normToOpp = toOpp / dOpp;
            float alignment = (directDir.x * normToOpp.x + directDir.y * normToOpp.y);

            if (alignment > 0.92f) { // Slightly wider cone for shot blocking
                needsCurl = true;
                float cross = (directDir.x * toOpp.y - directDir.y * toOpp.x);
                curlDirection = (cross > 0) ? -1.0f : 1.0f;
                break;
            }
        }
    }

    // B. Intentional Corner Bending (If the lane is open)
    // High finishing players intentionally start the ball wide and bend it into the post away from the keeper!
    // NOTE: Don't violently bend a delicate chip!
    if (!needsCurl && finishingNorm > 0.6f && !tryChip) {
        needsCurl = true;
        // If aiming at the top post, curl it inward (-1). Bottom post hooks inward (+1).
        curlDirection = (targetY < 3500.f) ? -1.0f : 1.0f;
    }

    float finalSpin = 0.f;
    sf::Vector2f aimDir = directDir;

    if (needsCurl) {
        float rawSpin = npc.getCurl() * (0.8f + finishingNorm * 0.4f);
        // Spin intensity scales with their simulated charge
        rawSpin *= (1.1f + simulatedCharge / 2.f);

        finalSpin = curlDirection * rawSpin;

        // Offset the initial aim so the Magnus effect pulls it precisely into the corner
        float offsetAngle = -curlDirection * (15.0f * (npc.getCurl() / 100.f)) * (3.14159f / 180.f);
        float cosA = std::cos(offsetAngle);
        float sinA = std::sin(offsetAngle);
        aimDir = sf::Vector2f(directDir.x * cosA - directDir.y * sinA, directDir.x * sinA + directDir.y * cosA);
    }

    // ==========================================
    // 4. SHOT TRAJECTORY (High vs Low Driven vs Chip)
    // ==========================================
    float totalPower = npc.getKickPower() * simulatedCharge;
    float verticalPower = 0.f;
    float finalBackspin = 0.f;

    if (tryChip) {
        // --- THE CHIP SHOT ---
        float distToGoal = std::sqrt(std::pow(goalPos.x - npcPos.x, 2) + std::pow(goalPos.y - npcPos.y, 2));

        // Calculate a delicate power just strong enough to reach the goal line
        float chipPowerNeeded = (distToGoal / 52.0f); // Engine multiplier approximation
        totalPower = std::min(chipPowerNeeded * 0.85f, npc.getKickPower() * 0.6f); // Cap horizontal power so it drops in time

        // Massive vertical pop to get over the keeper's reach
        verticalPower = 700.f + (finishingNorm * 150.f);

        // Heavy backspin makes the ball "bite" and drop rapidly
        finalBackspin = 60.f + (finishingNorm * 40.f);
    }
    else {
        // 50/50 chance for a high corner shot vs a low driven shot
        bool isHighShot = (rand() % 100 > 50);

        if (isHighShot) {
            // --- HIGH SHOT (Inverted Logic Parity) ---
            float maxLoft = 850.f;
            float heightInversionFactor = 200.f;

            verticalPower = maxLoft - (simulatedCharge * heightInversionFactor);
            verticalPower -= (finishingNorm * 80.f); // Stat dampening keeps it flatter

            // Drops off faster with higher power
            float spinIntensity = 120.f;
            finalBackspin = (finishingStat * 0.8f) + (spinIntensity * (1.0f - simulatedCharge));
        }
        else {
            // --- LOW DRIVEN SHOT ---
            verticalPower = 5.f + (simulatedCharge * 50.f); // Tiny pop to escape friction
            totalPower *= 1.5f; // Driven shots move 50% faster horizontally
            finalBackspin = 0.f;
        }
    }

    // ==========================================
    // 5. ERROR CALCULATION
    // ==========================================
    float errorAngle = (1.0f - finishingNorm) * 5.0f;
    if (tryChip) errorAngle *= 1.5f; // Chips are inherently harder to hit perfectly straight

    float randError = ((rand() % 100) / 100.f - 0.5f) * errorAngle;
    float rad = randError * 3.14159f / 180.f;
    sf::Vector2f finalDir(
        aimDir.x * std::cos(rad) - aimDir.y * std::sin(rad),
        aimDir.x * std::sin(rad) + aimDir.y * std::cos(rad)
    );

    // Cap the final power at physical limits (bypass this limit if chipping so we don't accidentally overdrive it)
    if (!tryChip) {
        totalPower = std::min(totalPower, npc.getKickPower() * (verticalPower > 100.f ? 1.0f : 1.5f));
    }

    // ==========================================
    // 6. EXECUTE
    // ==========================================
    ball.shoot(finalDir, totalPower, finalSpin, verticalPower, finalBackspin);
    npc.resetKickCooldown();
}

TacticalZone NPCController::getZoneForRole(PositionRole role) {
    switch (role) {
    case PositionRole::Goalkeeper:
        // Very strict: stays on the line, barely marks anyone.
        return { 350.f, 200.f, 800.f, 0.75f, 400.f }; // FORWARD LEASH / BACK LEASH / SIDE LEASH / BALL CHASING / MARKING

    case PositionRole::LCenterBack:
    case PositionRole::RCenterBack:
        // Defensive Anchor: Low ball attraction, high marking range.
        // Doesn't like to push forward, but stays tight to opponents.
        return { 3500.f, 1500.f, 800.f, 0.2f, 1500.f };

    case PositionRole::LeftBack:
    case PositionRole::RightBack:
        // Fullbacks: Can push up a bit more than CBs, but watch their wings.
        return { 4200.f, 500.f, 1000.f, 0.35f, 1200.f };

    case PositionRole::DefensiveMid:
        // The "Pivot": Balanced movement, shields the defense.
        return { 3700.f, 1200.f, 1500.f, 0.5f, 1000.f };

    case PositionRole::CenterMid:
        return { 3000.f, 1500.f, 2000.f, 0.7f, 800.f };
    case PositionRole::AttackingMid:
        // Box-to-Box: Large zones, follows play closely.
        return { 3500.f, 1000.f, 1500.f, 0.5f, 600.f };

    case PositionRole::LeftWing:
    case PositionRole::RightWing:
        // Pure Width: High forward leash, very narrow lateral (Y) leash.
        return { 4000.f, 2000.f, 800.f, 0.7f, 600.f };

    case PositionRole::Striker:
        // The Target: High forward leash, high ball influence.
        // Doesn't mark much (defending isn't their priority).
        return { 4000.f, 500.f, 1500.f, 0.7f, 200.f };

    default:
        // Default "Average" settings
        return { 1000.f, 1000.f, 1000.f, 0.5f, 800.f };
    }
}

bool NPCController::isBallInOurHalf(NPCPlayer& npc, Ball& ball, const Pitch& pitch) {
    bool isHome = (npc.getTeam() == Team::Home);
    float ballX = ball.getPosition().x;
    return isHome ? (ballX < pitch.totalWidth / 2.f) : (ballX > pitch.totalWidth / 2.f);
}

sf::Vector2f NPCController::applyTacticalPositioning(NPCPlayer& npc, sf::Vector2f homePos, sf::Vector2f ballPos, sf::Vector2f goalPos, TeamState state, TacticalZone zone, const Pitch& pitch, const std::vector<Player*> opposition) {
    sf::Vector2f tacticalTarget = homePos;
    bool isHomeSide = (npc.getTeam() == Team::Home);
    sf::Vector2f npcPos = npc.getPosition();

    // 1. CALCULATE BALL PROGRESS (0.0 at our end, 1.0 at their end)
    float ballX = ballPos.x;
    float ballProgress = isHomeSide ? (ballX / pitch.totalWidth) : (1.0f - (ballX / pitch.totalWidth));
    ballProgress = std::clamp(ballProgress, 0.0f, 1.0f);

    // 2. FETCH MENTAL STATS & ROLES
    float awarenessNorm = npc.getAwareness() / 100.0f;
    float attackPosNorm = npc.getAwareness() / 100.0f; 

    bool isForward = (npc.getPositionRole() == PositionRole::Striker ||
        npc.getPositionRole() == PositionRole::LeftWing ||
        npc.getPositionRole() == PositionRole::RightWing);
    
    bool isMid = (npc.getPositionRole() == PositionRole::CenterMid ||
        npc.getPositionRole() == PositionRole::AttackingMid);
    
    // We treat CDMs, Fullbacks, and CBs all as defensive buildup players
    bool isDefender = (!isForward && !isMid); 

    // Pitch Context for this specific player
    bool inOwnHalf = isHomeSide ? (tacticalTarget.x < pitch.totalWidth / 2.f) : (tacticalTarget.x > pitch.totalWidth / 2.f);

    if (state == TeamState::Attacking) {
        // --- 3. THE DYNAMIC PUSH ---
        float pushMagnitude = zone.forwardLeash * ballProgress;
        sf::Vector2f pushDir = isHomeSide ? sf::Vector2f(1.f, 0.f) : sf::Vector2f(-1.f, 0.f);
        tacticalTarget = homePos + (pushDir * pushMagnitude);

        // --- 4. ROLE-BASED ATTACKING INTELLIGENCE ---
        if (isForward || isMid) {

            // A. THE "RAUMDEUTER" ESCAPE (Space Invader)
            Player* nearestOpp = findNearestOpponent(tacticalTarget, opposition);
            if (nearestOpp) {
                float distToOpp = dist(tacticalTarget, nearestOpp->getPosition());
                if (distToOpp < 500.f) {
                    sf::Vector2f escapeDir = normalize(tacticalTarget - nearestOpp->getPosition());
                    float spaceCreation = 100.f + (attackPosNorm * 300.f);
                    escapeDir.x = isHomeSide ? std::abs(escapeDir.x) : -std::abs(escapeDir.x);
                    tacticalTarget += escapeDir * spaceCreation;
                }
            }

            if (isForward) {
                // B. PENETRATING BOX RUNS (Strikers/Wingers)
                if (ballProgress > 0.65f) {
                    float boxPenetration = attackPosNorm * 800.f;
                    tacticalTarget.x += isHomeSide ? boxPenetration : -boxPenetration;
                    float postPeel = (tacticalTarget.y < pitch.totalHeight / 2.0f) ? -300.f : 300.f;
                    tacticalTarget.y += postPeel * attackPosNorm;
                }

                // C. OFFENSIVE AWARENESS (The Offside Trap)
                float offsideLineX = isHomeSide ? 0.0f : pitch.totalWidth;
                for (Player* opp : opposition) {
                    if (opp->getPositionRole() == PositionRole::Goalkeeper) continue;
                    if (isHomeSide && opp->getPosition().x > offsideLineX) offsideLineX = opp->getPosition().x;
                    else if (!isHomeSide && opp->getPosition().x < offsideLineX) offsideLineX = opp->getPosition().x;
                }

                float awarenessError = (1.0f - awarenessNorm) * 400.f;
                if (isHomeSide && tacticalTarget.x > offsideLineX + awarenessError) {
                    tacticalTarget.x = offsideLineX + awarenessError - 50.f;
                }
                else if (!isHomeSide && tacticalTarget.x < offsideLineX - awarenessError) {
                    tacticalTarget.x = offsideLineX - awarenessError + 50.f;
                }
            }
            else if (isMid) {
                // D. MIDFIELD AWARENESS (The Transition Reader)
                if (ballProgress > 0.5f) {
                    float midfieldSupportPush = (awarenessNorm * 800.f) * ((ballProgress - 0.5f) * 2.0f);
                    tacticalTarget.x += isHomeSide ? midfieldSupportPush : -midfieldSupportPush;
                }
            }

            // Shadows the ball's Y-axis aggressively
            tacticalTarget.y += (ballPos.y - tacticalTarget.y) * zone.ballInfluence;
        }
        else {
            // E. ATTACKING DISCIPLINE (Defenders)
            tacticalTarget.y += (ballPos.y - tacticalTarget.y) * (zone.ballInfluence * 0.3f);
        }

        // ========================================================
        // --- F. DYNAMIC PASSING LANES (SHOWING FOR THE BALL) ---
        // ========================================================
        float distToBall = dist(tacticalTarget, ballPos);

        // Only create lanes if we are a viable passing option (not too close, not miles away)
        if (distToBall > 200.f && distToBall < 1500.f) {

            // 1. Check if Ball Carrier is Pressured
            bool carrierPressured = false;
            for (auto* opp : opposition) {
                if (dist(ballPos, opp->getPosition()) < 450.f) {
                    carrierPressured = true;
                    break;
                }
            }

            // 2. Check if our Lane is Blocked (Cover Shadow)
            sf::Vector2f toBall = ballPos - tacticalTarget;
            sf::Vector2f dirToBall = normalize(toBall);
            Player* laneBlocker = nullptr;

            for (auto* opp : opposition) {
                float dOpp = dist(tacticalTarget, opp->getPosition());
                // If opponent is physically between us and the ball
                if (dOpp < distToBall - 50.f && dOpp > 50.f) {
                    float alignment = (dirToBall.x * ((opp->getPosition().x - tacticalTarget.x) / dOpp) +
                                       dirToBall.y * ((opp->getPosition().y - tacticalTarget.y) / dOpp));
                    // 0.94 is a tight cone. If they are in it, the direct pass is blocked.
                    if (alignment > 0.94f) {
                        laneBlocker = opp; 
                        break;
                    }
                }
            }

            // 3. Execution Based on Pitch Area and Role
            if (inOwnHalf) {
                // --- OWN HALF: Safety and Spacing ---
                if (isDefender) {
                    if (carrierPressured) {
                        // Drop deeper and spread wider to offer a safe back-pass (Safety Valve)
                        tacticalTarget.x += isHomeSide ? -300.f : 300.f;
                        float stretchY = (tacticalTarget.y > pitch.totalHeight / 2.f) ? 250.f : -250.f;
                        tacticalTarget.y += stretchY * awarenessNorm;
                    }
                    if (laneBlocker) {
                        // Shift laterally to open the lane
                        sf::Vector2f perp(-dirToBall.y, dirToBall.x);
                        sf::Vector2f toBlocker = laneBlocker->getPosition() - tacticalTarget;
                        if (perp.x * toBlocker.x + perp.y * toBlocker.y > 0) perp = -perp; // Step away from blocker
                        tacticalTarget += perp * (350.f * awarenessNorm);
                    }
                }
                else if (isMid) {
                    // Midfielders drop into pockets to help build up
                    if (carrierPressured) {
                        tacticalTarget += dirToBall * (450.f * awarenessNorm); // Show to feet
                    }
                    if (laneBlocker) {
                        sf::Vector2f perp(-dirToBall.y, dirToBall.x);
                        sf::Vector2f toBlocker = laneBlocker->getPosition() - tacticalTarget;
                        if (perp.x * toBlocker.x + perp.y * toBlocker.y > 0) perp = -perp;
                        tacticalTarget += perp * (200.f * awarenessNorm);
                    }
                }
                else if (isForward && carrierPressured) {
                    // Strikers abandon the offside line and drop deep as a Target Man when trapped
                    tacticalTarget += dirToBall * (600.f * awarenessNorm);
                }
            }
            else {
                // --- OPPONENT HALF: Penetration and Pockets ---
                if (isMid) {
                    // Midfielders constantly seek pockets of space between defenders (Tiki-Taka)
                    if (laneBlocker) {
                        sf::Vector2f perp(-dirToBall.y, dirToBall.x);
                        sf::Vector2f toBlocker = laneBlocker->getPosition() - tacticalTarget;
                        if (perp.x * toBlocker.x + perp.y * toBlocker.y > 0) perp = -perp;
                        tacticalTarget += perp * (250.f * awarenessNorm);
                    }
                    // Only show back to feet if the carrier is desperate and we are far away
                    if (carrierPressured && distToBall > 1200.f) {
                        tacticalTarget += dirToBall * (250.f * awarenessNorm);
                    }
                }
                else if (isForward) {
                    // Forwards mostly do penetrating runs, but micro-adjust to stay out of cover shadows
                    if (laneBlocker && distToBall < 2000.f) {
                        sf::Vector2f perp(-dirToBall.y, dirToBall.x);
                        sf::Vector2f toBlocker = laneBlocker->getPosition() - tacticalTarget;
                        if (perp.x * toBlocker.x + perp.y * toBlocker.y > 0) perp = -perp;
                        tacticalTarget += perp * (150.f * awarenessNorm);
                    }
                }
            }
        }
    }
    else {
        // --- 5. DEFENSIVE BLOCK LOGIC ---
        float dropIntensity = 1.0f - ballProgress;
        float dropDist = zone.backwardLeash * dropIntensity;

        sf::Vector2f dropDir = isHomeSide ? sf::Vector2f(-1.f, 0.f) : sf::Vector2f(1.f, 0.f);
        tacticalTarget = homePos + (dropDir * dropDist);

        // Lateral Compression: The team "squeezes" toward the Y-axis of the ball
        float sideSqueeze = (ballPos.y - tacticalTarget.y) * (zone.ballInfluence * 0.7f);
        tacticalTarget.y += sideSqueeze;

        // --- 6. DEFENSIVE AWARENESS (Holding the Line) ---
        if (isDefender) {
            // The Penalty Box line is roughly 16.5 meters (1650px) from the goal line
            float penaltyBoxEdgeX = isHomeSide ? pitch.margin + 1650.f : pitch.totalWidth - pitch.margin - 1650.f;
            bool ballOutsideBox = isHomeSide ? (ballPos.x > penaltyBoxEdgeX) : (ballPos.x < penaltyBoxEdgeX);

            if (ballOutsideBox) {
                float disciplineError = (1.0f - awarenessNorm) * 300.f;

                if (isHomeSide && tacticalTarget.x < penaltyBoxEdgeX - disciplineError) {
                    tacticalTarget.x = penaltyBoxEdgeX - disciplineError;
                }
                else if (!isHomeSide && tacticalTarget.x > penaltyBoxEdgeX + disciplineError) {
                    tacticalTarget.x = penaltyBoxEdgeX + disciplineError;
                }
            }
        }
        else {
            // Midfielders/Attackers shade goal-side
            sf::Vector2f toGoal = normalize(goalPos - tacticalTarget);
            tacticalTarget += toGoal * 150.f;
        }
    }

    return tacticalTarget;
}

sf::Vector2f NPCController::clampToTacticalZone(sf::Vector2f target, sf::Vector2f homePos,
    const TacticalZone& zone, float distToBall,
    bool isHomeSide) 
{
    // 1. Calculate standard boundaries
    float minX = isHomeSide ? homePos.x - zone.backwardLeash : homePos.x - zone.forwardLeash;
    float maxX = isHomeSide ? homePos.x + zone.forwardLeash : homePos.x + zone.backwardLeash;
    float minY = homePos.y - zone.lateralLeash;
    float maxY = homePos.y + zone.lateralLeash;

    // 2. Emergency Relax: Let them move an extra 200px if the ball is nearby
    if (distToBall < 600.f) {
        minX -= 200.f;
        maxX += 200.f;
        // Optional: Relax Y as well if you want them to chase into corners
        minY -= 100.f;
        maxY += 100.f;
    }

    // 3. Apply the limits
    target.x = std::clamp(target.x, minX, maxX);
    target.y = std::clamp(target.y, minY, maxY);

    return target;
}

void NPCController::updateNPCAirPhysics(NPCPlayer& npc, float dt) {
    if (npc.z > 0.0f || npc.vz != 0.0f) {
        npc.vz -= 980.f * dt; // Gravity
        npc.z += npc.vz * dt;

        if (npc.z <= 0.0f) {
            npc.z = 0.0f;
            npc.vz = 0.0f;
            npc.setVelocity(npc.getVelocity() * 0.60f); // Friction on landing
            if (npc.getState() == PlayerState::Jumping) {
                npc.setState(PlayerState::Normal);
            }
        }
    }
}

void NPCController::handleNPCJumpLogic(NPCPlayer& npc, Ball& ball) {
    if (npc.z > 0.0f || npc.getState() == PlayerState::Tackling) return;

    float d = dist(npc.getPosition(), ball.getPosition());

    // 1. Is the ball in a "header" zone? 
    // We check if the ball is high (z > 150) and potentially dropping toward us
    if (d < 350.f && ball.z > 150.f && ball.vz < 0.f) {

        // 2. Awareness Check: Timing the jump
        // High awareness (99) jumps almost perfectly. 
        // Low awareness (20) might jump while the ball is still 4 meters high.
        float awarenessNorm = npc.getAwareness() / 100.f;
        float idealInterceptZ = 170.f; // Aiming to hit at head height
        float error = (1.0f - awarenessNorm) * 100.f;

        if (ball.z < (idealInterceptZ + error)) {
            float jumpingNorm = npc.getJumpingStrength() / 100.0f;
            float jumpVz = 240.f + (jumpingNorm * 160.f);
            sf::Vector2f futureBallPos = ball.getPosition() + ball.getVelocity();
            sf::Vector2f jumpDir = normalize(futureBallPos - npc.getPosition());
            float speed = std::sqrt(npc.getVelocity().x * npc.getVelocity().x + npc.getVelocity().y * npc.getVelocity().y);
            npc.setVelocity(jumpDir * speed);
            npc.vz = jumpVz;
            npc.setState(PlayerState::Jumping);
        }
    }
}

bool NPCController::tryNPCAerialStrike(NPCPlayer& npc, Ball& ball, sf::Vector2f aimDir, bool isShot) {
    // ==========================================
        // 1. THE MACHINE-GUN SHIELD
        // ==========================================
    if (npc.getKickCooldown() > 0.0f) return false;

    // ==========================================
    // 2. THE JUGGLING EXPLOIT BAN
    // ==========================================
    Player* lastOwner = ball.getLastOwner();
    if (lastOwner != nullptr) {
        // A. The Self-Pass Ban
        // You cannot volley your own chip! (Unless it bounced off a keeper/post, 
        // which would change the lastOwner anyway).
        if (lastOwner == &npc) {
            return false;
        }

        // B. The Short-Pass Trap Mandate
        // If a teammate chips the ball to you from close range, you MUST let it drop 
        // to the grass and trap it, unless you are taking a direct shot on goal.
        if (!isShot && lastOwner->getTeam() == npc.getTeam()) {
            float distFromPasser = dist(npc.getPosition(), lastOwner->getPosition());

            // If the pass traveled less than 15 meters (1500px), refuse to hit it in the air
            if (distFromPasser < 1500.f) {
                return false;
            }
        }
    }

    float d = dist(npc.getPosition(), ball.getPosition());
    if (d > 120.f) return false;

    float relativeHeight = ball.z - npc.z;
    bool isHeader = (relativeHeight >= 140.f && relativeHeight <= 220.f);
    bool isVolley = (relativeHeight >= 40.f && relativeHeight < 140.f);

    if (!isHeader && !isVolley) return false;

    // Stats
    float headingStat = npc.getHeading();
    float finishingStat = npc.getFinishing();
    float activeStat = isHeader ? headingStat : finishingStat;
    float skillNorm = activeStat / 100.f;

    // ==========================================
    // NEW: THE "WHIFF & SCUFF" TIMING ENGINE
    // ==========================================
    sf::Vector2f bVel = ball.getVelocity();
    // Calculate true 3D incoming speed
    float incomingSpeed = std::sqrt(bVel.x * bVel.x + bVel.y * bVel.y + ball.vz * ball.vz);

    // Volleys are much harder to time than headers
    float difficultyMultiplier = isVolley ? 1.85f : 1.0f;

    // A solid cross is around 1500px/s (15m/s). Anything faster is a bullet.
    float speedPenalty = (incomingSpeed / 1500.f) * difficultyMultiplier;

    // The Whiff Check (Swing and miss!)
    // If the speed penalty greatly outweighs their skill, they miss the timing completely.
    float whiffChance = (speedPenalty - skillNorm) * 35.f;

    if (whiffChance > 0.f && (rand() % 100) < whiffChance) {
        // They jumped early/late or swung at air! 
        // Put them on cooldown so they can't try again instantly.
        npc.resetKickCooldown();
        return false;
    }

    // The Scuff Check (Bad contact!)
    // If it's a fast ball, their maximum potential charge drops drastically.
    // Elite players (skillNorm ~ 1.0) ignore a lot of this penalty.
    float timingQuality = std::clamp(1.0f - (speedPenalty * (1.0f - skillNorm)), 0.15f, 1.0f);

    float maxCharge = 0.8f * timingQuality;
    float minCharge = 0.5f * timingQuality;
    float simulatedCharge = minCharge + ((rand() % 100) / 100.f) * (maxCharge - minCharge);

    // ==========================================
    // PHYSICS & TRAJECTORY EXECUTION
    // ==========================================
    float basePower = 0.f;
    float vzOut = 0.f;
    float finalBackspin = 0.f;

    if (isHeader) {
        // Nerfed base power, relying heavily on the (now potentially scuffed) charge
        basePower = (30.f + (headingStat * 0.5f)) * std::max(0.3f, simulatedCharge);

        if (isShot) {
            vzOut = 100.f - (headingStat * 3.0f); // Driven spike into the ground
        }
        else {
            vzOut = 150.f + (headingStat * 1.5f); // Defensive clearance/looped pass
        }
        finalBackspin = 10.f;
    }
    else {
        // Volley power relies heavily on good timing charge
        basePower = npc.getKickPower() * simulatedCharge * 1.1f;

        // Bad timing absolutely skyrockets the ball into the stands
        float techniqueError = (1.0f - skillNorm);
        vzOut = 100.f + (techniqueError * 350.f) + ((1.0f - timingQuality) * 200.f);

        finalBackspin = 30.f;
    }

    // ==========================================
    // ACCURACY ERROR
    // ==========================================
    float errorMagnitude = (1.0f - skillNorm);

    // Scuffed timing drastically increases the wildness of the shot
    errorMagnitude += (1.0f - timingQuality) * 0.5f;

    float maxErrorAngle = isHeader ? 15.f : 12.f;
    float radOffset = (errorMagnitude * maxErrorAngle * (((rand() % 200) - 100) / 100.f)) * 3.14159f / 180.f;

    float cosA = std::cos(radOffset);
    float sinA = std::sin(radOffset);
    sf::Vector2f finalDir = sf::Vector2f(
        aimDir.x * cosA - aimDir.y * sinA,
        aimDir.x * sinA + aimDir.y * cosA
    );

    ball.shoot(finalDir, basePower, 0.0f, vzOut, finalBackspin);
    npc.resetKickCooldown();
    return true;
}

bool NPCController::shouldRecoverPosition(const sf::Vector2f& npcPos, const sf::Vector2f& homePos, const TacticalZone& zone, float distToBall, bool isHomeSide) {
    float currentDX = std::abs(npcPos.x - homePos.x);
    float currentDY = std::abs(npcPos.y - homePos.y);

    // Determine which leash to use based on which way we are facing
    float maxReachX = (npcPos.x > homePos.x == isHomeSide) ? zone.forwardLeash : zone.backwardLeash;

    // We only force a recovery if they are significantly out of bounds (+200px)
    // and the ball isn't close enough to be an immediate concern.
    return ((currentDX > maxReachX + 200.f || currentDY > zone.lateralLeash + 200.f) && distToBall > 400.f);
}

bool NPCController::shouldEmergencyChase(NPCPlayer& npc, Player* firstResponder, float distToBall, const Pitch& pitch, Ball& ball, MatchState matchstate) {
    if (&npc != firstResponder) return false;

    Player* owner = ball.getOwner();

    // --- 1. THE KEEPER PROTECTION RULE ---
    if ((owner != nullptr &&
        owner->getTeam() != npc.getTeam() &&
        owner->getPositionRole() == PositionRole::Goalkeeper) ||
        matchstate != MatchState::InPlay)
    {
        return false;
    }

    sf::Vector2f home = npc.getHomePosition(npc.getTeam() == Team::Home, TeamState::Defending);
    sf::Vector2f ballPos = ball.getPosition();

    // --- 2. STATS & ZONES ---
    TacticalZone zone = getZoneForRole(npc.getPositionRole());
    float aggressionNorm = npc.getAggression() / 100.0f;

    // Aggression expands how far they are willing to stretch their tactical shape
    float xBuffer = 200.f + (aggressionNorm * 400.f);
    float yBuffer = 200.f + (aggressionNorm * 300.f);

    float dx = std::abs(home.x - ballPos.x);
    float dy = std::abs(home.y - ballPos.y);

    bool isHomeTeam = (npc.getTeam() == Team::Home);
    bool ballIsForward = isHomeTeam ? (ballPos.x > home.x) : (ballPos.x < home.x);
    float currentXLeash = ballIsForward ? zone.forwardLeash : zone.backwardLeash;

    // --- 3. STRICT TACTICAL BOUNDARIES ---
    // Instead of a massive circular radius, we check X and Y separately.
    // This stops a Right Back from chasing a winger all the way to the left side of the pitch!
    if (dx > currentXLeash + xBuffer || dy > zone.lateralLeash + yBuffer) {
        return false;
    }

    // --- 4. THE "LOST CAUSE" CHECK (Anti-Cat-and-Mouse) ---
    if (owner && owner->getTeam() != npc.getTeam()) {
        sf::Vector2f npcPos = npc.getPosition();

        // If the defender is trailing behind by more than ~8 meters (800px)
        if (distToBall > 800.f) {
            sf::Vector2f toBall = ballPos - npcPos;
            sf::Vector2f ownerVel = owner->getVelocity();

            // Calculate the dot product to see if the attacker is moving AWAY from the defender
            float dot = (toBall.x * ownerVel.x + toBall.y * ownerVel.y);

            if (dot > 0.f) {
                // The attacker is faster, moving away, and we are already far behind.
                // Give up the chase! This allows the AI to fall back into shape and 
                // seamlessly passes the "firstResponder" baton to the next defender in line.
                return false;
            }
        }

        // --- 5. CENTER BACK DISCIPLINE ---
        // Center Backs are the last line of defense. If they step out and miss the tackle, 
        // they should immediately drop off rather than chasing the guy from behind.
        if (npc.getPositionRole() == PositionRole::LCenterBack || npc.getPositionRole() == PositionRole::RCenterBack) {
            if (distToBall > 450.f) {
                return false;
            }
        }
    }

    return true; // The chase is on!
}

sf::Vector2f NPCController::calculateMarkingPosition(NPCPlayer& npc, Player* threat, sf::Vector2f goalPos, const TacticalZone& zone) {
    sf::Vector2f threatPos = threat->getPosition();
    sf::Vector2f threatVel = threat->getVelocity();
    float threatSpeed = std::sqrt(threatVel.x * threatVel.x + threatVel.y * threatVel.y);
    float distToThreat = dist(npc.getPosition(), threatPos);

    // Normalize stats (0.0 to 1.0)
    float aggressionNorm = npc.getAggression() / 100.0f;
    float awarenessNorm = npc.getAwareness() / 100.0f;

    // Direction from the THREAT to our GOAL (The defensive line)
    sf::Vector2f toGoal = normalize(goalPos - threatPos);

    if (threat->getBallPossession()) {
        // ==========================================
        // CASE A: THREAT HAS THE BALL (Jockeying)
        // ==========================================

        // 1. BASE STANDOFF
        // Default to a 200px gap. 99 Aggression pushes to 150px. 0 Aggression drops to 250px.
        float jockeyBuffer = 250.f - (aggressionNorm * 100.f);

        // 2. SPEED COMPENSATION (The Cushion)
        // If the opponent is sprinting at us, we must drop deeper to avoid getting blown past.
        // A 1000px/s sprint adds 400px of extra defensive depth.
        float speedCompensation = threatSpeed * 0.8f;
        float totalStandoff = jockeyBuffer + speedCompensation;

        // The ideal point exactly on the line between the ball and the goal
        sf::Vector2f baseDefendPos = threatPos + (toGoal * totalStandoff);

        // 3. VELOCITY SHIFT (The 20% Rule)
        // We shift our target 20% in the direction the attacker is moving.
        // This makes the defender run parallel and block the path rather than chasing their shadow!
        sf::Vector2f velocityOffset = threatVel * 0.8f;
        sf::Vector2f tacticalTarget = baseDefendPos + velocityOffset;

        // 4. THE TACKLE COMMITMENT
        float commitThreshold = 80.f + ((1.0f - awarenessNorm) * 60.f);

        if (distToThreat < commitThreshold && npc.canTackle()) {
            // THE SHOULDER BARGE
            // If the defender has high strength and awareness, they step ACROSS the attacker's path
            float strengthNorm = npc.getBodyStrength() / 100.f;

            if (awarenessNorm > 0.6f && strengthNorm > 0.6f) {
                sf::Vector2f threatDir = (threatSpeed > 10.f) ? (threatVel / threatSpeed) : toGoal;
                sf::Vector2f bargePoint = threatPos + (threatDir * 80.f); // Step into their path!
                return bargePoint;
            }
            else {
                // Standard tackle step
                float tackleStep = 80.f - (aggressionNorm * 20.f);
                return threatPos + (toGoal * tackleStep);
            }
        }
        else {
            // Return our dynamically calculated jockeying position
            return tacticalTarget;
        }
    }
    else {
        // ==========================================
        // CASE B: THREAT DOES NOT HAVE THE BALL
        // ==========================================
        // Just stay goal-side to block the passing lane.
        float offBallGap = std::max(250.f - (aggressionNorm * 200.f), 40.f);

        // Off the ball, we still want to predict their runs slightly using awareness
        sf::Vector2f lookAhead = threatVel * (awarenessNorm * 0.5f);
        return threatPos + lookAhead + (toGoal * offBallGap);
    }
}

Player* NPCController::findBestThreat(const sf::Vector2f& npcPos, const std::vector<Player*>& opponents, const TacticalZone& zone, MatchState matchstate) {
    Player* bestThreat = nullptr;
    float minDist = zone.markingRange;

    for (auto& opp : opponents) {
        float d = dist(npcPos, opp->getPosition());
        if (opp->getPositionRole() == PositionRole::Goalkeeper) continue;

        // Priority 1: The guy with the ball (if he's within reasonable range)
        if (opp->getBallPossession() && d < zone.markingRange * 1.5f && matchstate == MatchState::InPlay && opp->getPositionRole() != PositionRole::Goalkeeper) {
            return opp;
        }

        // Priority 2: The closest guy in my zone
        if (d < minDist && opp->getPositionRole() != PositionRole::Goalkeeper) {
            minDist = d;
            bestThreat = opp;
        }
    }
    return bestThreat;
}

Player* NPCController::findNearestOpponent(const sf::Vector2f& npcPos, const std::vector<Player*>& opponents) {
    Player* nearest = nullptr;
    float minDistanceSq = std::numeric_limits<float>::max();

    for (auto& opp : opponents) {
        sf::Vector2f oppPos = opp->getPosition();
        if (opp->getPositionRole() == PositionRole::Goalkeeper) continue;
        sf::Vector2f diff = npcPos - oppPos;

        // Using squared distance is faster (avoids the expensive sqrt() call)
        float distSq = (diff.x * diff.x) + (diff.y * diff.y);

        if (distSq < minDistanceSq && opp->getPositionRole() != PositionRole::Goalkeeper) {
            minDistanceSq = distSq;
            nearest = opp;
        }
    }

    return nearest;
}

void NPCController::handleGoalkeeping(NPCPlayer& npc, Ball& ball, const Pitch& pitch,
    const std::vector<Player*>& team, const std::vector<Player*>& opposition, float dt)
{
    if (npc.getState() == PlayerState::Diving)
    {
        // KEEPER DIVE FRICTION FIX
        sf::Vector2f vel = npc.getVelocity();
        vel -= vel * 4.0f * dt; // Apply drag so they slide to a stop
        npc.setVelocity(vel);

        float speedSq = (vel.x * vel.x) + (vel.y * vel.y);
        if (speedSq < 150.0f) {
            npc.setState(PlayerState::Normal);
        }
        else {
            return; // Still sliding! Skip AI logic.
        }
    }

    // --- POSSESSION ---
    if (npc.getBallPossession())
    {
        npc.m_possessionTimer += dt;

        // Scan for pressing attackers
        float closestOpp = 9999.f;
        for (auto* opp : opposition) {
            float d = dist(npc.getPosition(), opp->getPosition());
            if (d < closestOpp) closestOpp = d;
        }

        // PANIC CLEAR: If an attacker gets within 3.5m, OR we held it too long, kick it!
        if (closestOpp < 350.f || npc.m_possessionTimer > 3.5f)
        {
            distributeBallAsGoalie(npc, ball, team);
            npc.m_possessionTimer = 0.0f;
        }
        return; // Don't do normal goalie logic while holding the ball
    }
    else npc.m_possessionTimer = 0.0f;

    // --- TACTICAL DECISION MAKING ---
    sf::Vector2f myGoalCenter = (npc.getTeam() == Team::Home) ? pitch.homeGoalCenter : pitch.awayGoalCenter;
    sf::Vector2f targetPos;
    bool sprint = false;

    sf::Vector2f ballPos = ball.getPosition();
    bool isHomeSide = (npc.getTeam() == Team::Home);
    bool ballInBoxX = isHomeSide ? (ballPos.x < 1650.f) : (ballPos.x > pitch.totalWidth - 1650.f);
    bool ballInBoxY = (ballPos.y > 3500.f - 2000.f && ballPos.y < 3500.f + 2000.f);

    if (!ball.hasOwner() && ballInBoxX && ballInBoxY && ball.z < 100.f) {
        targetPos = ballPos;
        sprint = true;
    }
    else if (shouldGoalieRush(npc, ball, opposition, pitch)) {
        targetPos = ballPos;
        sprint = true;
    }
    else {
        targetPos = calculateGoaliePositioning(npc, ballPos, myGoalCenter, pitch);
        sprint = false;
    }

    // 3. PURE GOALKEEPER MOTOR CONTROL
    float distToTarget = dist(npc.getPosition(), targetPos);
    if (distToTarget > 5.0f)
    {
        sf::Vector2f moveDir = normalize(targetPos - npc.getPosition());
        float footworkStat = std::max(npc.getAgility(), npc.getGkReactions());
        float shuffleSpeed = sprint ? (npc.getTopSpeed() * 10.0f) : 400.0f + ((footworkStat / 100.0f) * 200.0f);
        float maxSpeedToTarget = distToTarget / dt;
        float actualSpeed = std::min(shuffleSpeed, maxSpeedToTarget);

        npc.setVelocity(moveDir * actualSpeed);

        // Ensure they face the ball while shuffling!
        npc.setRotationToward(ballPos);
    }
    else {
        npc.setVelocity({ 0.f, 0.f });
        npc.setRotationToward(ballPos);
    }

    // 4. SHOT DETECTION & SAVING
    attemptSave(npc, ball, dt);
}

void NPCController::attemptSave(NPCPlayer& npc, Ball& ball, float dt)
{
    sf::Vector2f ballVel = ball.getVelocity();
    float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);

    // KEEPER FIX: Lowered the threshold so they dive for normal shots too!
    if (ballSpeed < 300.0f) return;

    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f keeperPos = npc.getPosition();

    bool isHomeSide = (npc.getTeam() == Team::Home);
    if (isHomeSide && ballVel.x >= -10.0f) return;
    if (!isHomeSide && ballVel.x <= 10.0f) return;

    float ballTTI = std::abs((keeperPos.x - ballPos.x) / ballVel.x);
    if (ballTTI < 0.0f || ballTTI > 1.5f) return;

    float interceptY = ballPos.y + (ballVel.y * ballTTI);
    sf::Vector2f interceptPoint(keeperPos.x, interceptY);
    float diveDistance = std::abs(keeperPos.y - interceptY);

    float distToBall = dist(keeperPos, ballPos);
    float activeStat = (distToBall < 600.0f) ? npc.getGkBlocking() : npc.getGkReactions();
    float maxDiveSpeed = 600.0f + ((activeStat / 100.0f) * 1000.0f);

    float keeperTTI = diveDistance / maxDiveSpeed;
    if (ballTTI > keeperTTI + 0.15f) return;

    float attemptDiveRadius = 500.0f; // Increased their reach slightly
    if (diveDistance <= attemptDiveRadius && ball.z <= (npc.height + 100.0f))
    {
        float optimalSpeed = maxDiveSpeed;
        if (ballTTI > 0.05f) optimalSpeed = diveDistance / ballTTI;
        float finalSpeed = std::clamp(optimalSpeed, 150.0f, maxDiveSpeed);

        triggerDive(npc, interceptPoint, finalSpeed);
    }
}

sf::Vector2f NPCController::calculateGoaliePositioning(NPCPlayer& npc, sf::Vector2f ballPos, sf::Vector2f goalCenter, const Pitch& pitch)
{
    sf::Vector2f goalToBall = ballPos - goalCenter;
    float distToBall = dist(goalCenter, ballPos);

    if (distToBall < 1.0f) return goalCenter;

    sf::Vector2f directionToBall = normalize(goalToBall);

    // --- SMOOTH STEP OUT MATH ---
    // Instead of a hard threshold, step out by 15% of the ball's distance, 
    // capped at a maximum of 250px (2.5m). 
    // This creates a perfect, natural retreat as the ball approaches the box.
    float maxStep = 250.0f * (npc.getGkCoverage() / 100.0f);
    float actualStepOutDistance = std::min(maxStep, distToBall * 0.15f);

    sf::Vector2f targetPos = goalCenter + (directionToBall * actualStepOutDistance);

    // --- STRICT CLAMPING ---
    float goalHalfWidth = 366.0f;
    float postBuffer = 40.0f;

    // Y-Clamp (Left/Right)
    float minY = goalCenter.y - goalHalfWidth + postBuffer;
    float maxY = goalCenter.y + goalHalfWidth - postBuffer;
    if (targetPos.y < minY) targetPos.y = minY;
    if (targetPos.y > maxY) targetPos.y = maxY;

    // X-Clamp (Forward/Backward)
    bool isHomeSide = (npc.getTeam() == Team::Home);
    if (isHomeSide)
    {
        float minX = goalCenter.x;
        float maxX = goalCenter.x + maxStep;
        if (targetPos.x < minX) targetPos.x = minX;
        if (targetPos.x > maxX) targetPos.x = maxX;
    }
    else
    {
        float maxX = goalCenter.x;
        float minX = goalCenter.x - maxStep;
        if (targetPos.x > maxX) targetPos.x = maxX;
        if (targetPos.x < minX) targetPos.x = minX;
    }

    return targetPos;
}

bool NPCController::shouldGoalieRush(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& opposition, const Pitch& pitch)
{
    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f keeperPos = npc.getPosition();
    bool isHomeSide = (npc.getTeam() == Team::Home);

    // 1. Basic Sanity Check: Only rush if the ball is in our defensive third
    // (Assuming you have a way to check which side of the field we are on)
    sf::Vector2f myGoalCenter = isHomeSide ? pitch.homeGoalCenter : pitch.awayGoalCenter;
    if (dist(ballPos, myGoalCenter) > getZoneForRole(PositionRole::Goalkeeper).forwardLeash)
    {
        return false;
    }

    // 2. Find the most dangerous opponent (the one closest to the ball)
    Player* nearestAttacker = nullptr;
    float closestAttackerDist = 99999.0f;

    for (Player* opp : opposition)
    {
        float d = dist(opp->getPosition(), ballPos);
        if (d < closestAttackerDist)
        {
            closestAttackerDist = d;
            nearestAttacker = opp;
        }
    }

    if (!nearestAttacker) return false;

    // 3. Calculate Time-To-Intercept (TTI)
    // Time = Distance / Speed. (Using a hardcoded sprint speed for estimation, 
    // or you can pull the actual Pace stat from the players if accessible).
    float keeperDistToBall = dist(keeperPos, ballPos);

    float keeperSpeed = npc.getTopSpeed() * 10.0f; // Assuming you have a general Pace stat
    float attackerSpeed = nearestAttacker->getTopSpeed() * 10.0f;

    float keeperTTI = keeperDistToBall / (keeperSpeed > 0 ? keeperSpeed : 1.0f);
    float attackerTTI = closestAttackerDist / (attackerSpeed > 0 ? attackerSpeed : 1.0f);

    // 4. Integrate the GkAwareness Stat (1-100)
    // High awareness (near 100) means the keeper trusts the raw math.
    // Low awareness (near 0) means the keeper is hesitant and needs a massive head start to feel comfortable rushing.

    // We create a "hesitation penalty" in seconds/frames.
    // If awareness is 100, penalty is 0. If awareness is 20, penalty is significant.
    float hesitationPenalty = (100.0f - npc.getGkAwareness() * 0.05f);

    // Add the hesitation penalty to the keeper's calculated time
    float perceivedKeeperTTI = keeperTTI + hesitationPenalty;

    // 5. The Decision
    // If the keeper *thinks* they can get there before the attacker, they rush.
    // We add a small buffer (e.g., 0.2f) so they don't rush for exact 50/50 ties unless they are very aggressive.
    return (perceivedKeeperTTI < (attackerTTI - 0.2f));
}

void NPCController::resolveSaveOutcome(NPCPlayer& npc, Ball& ball)
{
    sf::Vector2f incomingVel = ball.getVelocity();

    // Calculate total 3D speed including the vertical velocity (vz)
    float ballSpeed = std::sqrt(incomingVel.x * incomingVel.x +
        incomingVel.y * incomingVel.y +
        ball.vz * ball.vz);

    // 1. Calculate Catch Probability
    float catchingStat = npc.getGkCatching();

    // Speed Penalty: A 30 m/s shot (108 km/h) is exactly 3000 pixels/sec.
    // If a shot is 3000px/s, it reduces the keeper's catch chance by a massive 50 points.
    float speedPenalty = (ballSpeed / 3000.0f) * 50.0f;

    float catchChance = catchingStat - speedPenalty;
    catchChance = std::max(5.0f, std::min(95.0f, catchChance)); // Keep it between 5% and 95%

    // 2. Roll the Dice
    float roll = (rand() % 100);

    if (roll <= catchChance)
    {
        // --- OUTCOME: CLEAN CATCH ---

        // Stop the keeper's diving momentum instantly and reset state
        npc.setVelocity({ 0.f, 0.f });
        npc.setState(PlayerState::Normal); // Or PlayerState::Idle, depending on your enum

        // Use your Ball's native possess method
        ball.possess(&npc);
    }
    else
    {
        // --- OUTCOME: PARRY / REBOUND ---
        // The keeper stops the goal but spills the ball.

        // A keeper with a high catching stat dampens the ball more, dropping it closer.
        // A 100-stat keeper leaves 20% of the speed; a 0-stat keeper leaves 50%.
        float dampenFactor = 0.5f - ((catchingStat / 100.0f) * 0.3f);

        sf::Vector2f parryVel;
        parryVel.x = -incomingVel.x * dampenFactor;
        parryVel.y = -incomingVel.y * dampenFactor;

        // Deflection Angle: Randomize the rebound trajectory (-45 to +45 degrees)
        float randomDeviation = ((rand() % 100) - 50) / 100.0f; // -0.5 to 0.5
        float maxAngleError = 45.0f;
        float angleOffset = randomDeviation * maxAngleError;

        // Rotate the 2D vector
        float radOffset = angleOffset * 3.14159f / 180.0f;
        float cosA = std::cos(radOffset);
        float sinA = std::sin(radOffset);

        sf::Vector2f finalParry;
        finalParry.x = parryVel.x * cosA - parryVel.y * sinA;
        finalParry.y = parryVel.x * sinA + parryVel.y * cosA;

        // Apply the new X/Y velocity
        ball.setVelocity(finalParry);

// --- VERTICAL BOUNCE (Z-Axis) ---
        // FIX 2: Positive vz pops the ball UP into the air! 
        // Negative vz was slamming it instantly into the dirt, causing collision glitches.
        ball.vz = 300.0f + (std::abs(randomDeviation) * 200.0f);

        // Stop the keeper's dive and let them try to scramble for the loose ball
        npc.setVelocity({ 0.f, 0.f });
        npc.setState(PlayerState::Normal);
    }
}

void NPCController::distributeBallAsGoalie(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates)
{
    // --- 1. FIND THE BEST COUNTER-ATTACK TARGET ---
    Player* bestTarget = nullptr;
    float bestScore = -1e9f;
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);

    for (Player* mate : teammates)
    {
        if (mate == &npc) continue;

        sf::Vector2f matePos = mate->getPosition();
        float d = dist(npcPos, matePos);

        // Calculate a "Distribution Score"
        // We prefer teammates further up the pitch (progress) but within range (max 50m / 5000px)
        float progress = isHome ? (matePos.x - npcPos.x) : (npcPos.x - matePos.x);
        float score = (progress * 0.8f);

        // Penalty for defenders being too close to the target
        // (We'll reuse findNearestOpponent or a simple distance check here if possible)

        if (d > 500.0f && d < 5000.0f) // Between 5m and 50m
        {
            if (score > bestScore)
            {
                bestScore = score;
                bestTarget = mate;
            }
        }
    }

    // Failsafe: Short roll to the nearest defender
    if (!bestTarget && !teammates.empty()) {
        bestTarget = teammates[0];
    }

    if (!bestTarget) return;

    // --- 2. PHYSICS & TRAJECTORY DECISION ---
    sf::Vector2f targetPos = bestTarget->getPosition();
    float finalDist = dist(npcPos, targetPos);
    sf::Vector2f throwDir = normalize(targetPos - npcPos);

    float throwingStat = npc.getGkThrowing();
    float longPassStat = npc.getLongPassing();

    float vzPower = 0.f;
    float finalBackspin = 0.f;
    float finalStatUsed = 0.f;

    // --- CASE A: THE LONG LAUNCH (Goal Kick / Long Throw) ---
    if (finalDist > 1500.f)
    {
        // Use a blend of throwing power and long passing technique
        finalStatUsed = std::min(throwingStat, (finalDist / 5000.f) * 100.f);
        finalStatUsed = std::max(finalStatUsed, 40.f);

        // High arc to clear the midfield press
        vzPower = 400.f + (finalStatUsed * 2.5f);

        // Backspin helps the ball "die" when it hits the target (Long Passing stat)
        finalBackspin = longPassStat * 0.8f;
    }
    // --- CASE B: THE SHORT DISTRIBUTION (Hand Roll / Short Toss) ---
    else
    {
        finalStatUsed = std::max(25.f, (finalDist / 1500.f) * 60.f);

        // Flatter trajectory for a quick roll to a defender's feet
        vzPower = 50.f + (finalStatUsed * 1.5f);
        finalBackspin = 10.f; // Tiny bit of bite
    }

    // --- 3. ACCURACY & ERROR ---
    // Higher GkThrowing reduces the random spray
    float errorMagnitude = (1.0f - (throwingStat / 100.f));
    float maxAngleError = 12.0f; // 12 degrees max error
    float randomDeviation = ((rand() % 200) - 100) / 100.0f;
    float radOffset = (errorMagnitude * maxAngleError * randomDeviation) * 3.14159f / 180.0f;

    sf::Vector2f finalDir(
        throwDir.x * std::cos(radOffset) - throwDir.y * std::sin(radOffset),
        throwDir.x * std::sin(radOffset) + throwDir.y * std::cos(radOffset)
    );

    // --- 4. EXECUTE ---
    // direction, power, side-spin (0 for hands), vertical-speed, backspin
    ball.shoot(finalDir, finalStatUsed, 0.0f, vzPower, finalBackspin);

    npc.resetKickCooldown();
}

void NPCController::triggerDive(NPCPlayer& npc, sf::Vector2f diveTarget, float jumpSpeed)
{
    // Find the direction (This will effectively be purely on the Y-axis now)
    sf::Vector2f diveDir = normalize(diveTarget - npc.getPosition());

    // Apply the perfectly calculated velocity
    npc.setVelocity(diveDir * jumpSpeed);
    npc.vz = 200.f;

    // Lock AI state
    npc.setState(PlayerState::Diving);
}

sf::Vector2f NPCController::calculateInterceptionPoint(NPCPlayer& npc, Ball& ball) {
    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f ballVel = ball.getVelocity();
    float ballZ = ball.z;
    float ballVz = ball.vz;
    sf::Vector2f npcPos = npc.getPosition();

    float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);
    if (ballSpeed < 50.f) return ballPos;

    sf::Vector2f interceptPoint = ballPos;
    sf::Vector2f ballDir = ballVel / ballSpeed;

// --- 1. AWARENESS ERROR (The "Blur" Effect) ---
    float awareness = npc.getAwareness();
    float errorSeverity = std::clamp((100.f - awareness) / 100.f, 0.f, 1.f);

    // ==========================================
    // THE RECEIVER BUFF: No error for intentional passes!
    // ==========================================
    Player* lastOwner = ball.getLastOwner();
    if (lastOwner != nullptr && lastOwner->getTeam() == npc.getTeam()) {
        errorSeverity *= 0.3f; // 70% reduction in blur. They know it's coming.
    }

    float distToBall = dist(npcPos, ballPos);

    // --- 2. THE AIR BALL (Landing Spot Prediction) ---
    if (ballZ > 40.f) {
        // Low awareness players misjudge gravity (thinking it hangs longer or drops faster)
        // We use ballVel.x to make the misjudgment stable (pseudo-random but constant during flight)
        float misjudgment = 1.0f + (errorSeverity * 0.35f * ((ballVel.x > 0) ? 1.f : -1.f));
        float perceivedGravity = 980.f * misjudgment;

        float a = 0.5f * perceivedGravity;
        float b = ballVz;
        float c = -ballZ;

        float discriminant = (b * b) - (4.f * a * c);
        if (discriminant > 0.f) {
            float t = (-b + std::sqrt(discriminant)) / (2.f * a);
            interceptPoint = ballPos + (ballVel * t);
        }
    }
    // --- 3. THE GROUND PASS (Attacking the Line) ---
    else {
        sf::Vector2f toNPC = npcPos - ballPos;
        float projection = (toNPC.x * ballDir.x + toNPC.y * ballDir.y);
        float distToLine = std::abs(toNPC.x * ballDir.y - toNPC.y * ballDir.x);

        float npcSpeed = npc.getTopSpeed() * 10.f;
        float timeToLine = distToLine / (npcSpeed + 1.f);

        // Low awareness players react late, meaning they target a spot further down the line
        // essentially letting the ball run past them before they try to trap it.
        float reactionDelay = errorSeverity * ballSpeed * 0.4f;

        float interceptDist = projection + (ballSpeed * timeToLine * 0.7f) + reactionDelay;
        interceptDist = std::max(20.f, interceptDist);

        interceptPoint = ballPos + ballDir * interceptDist;

        // Lateral Drift: Misjudging the left/right line of the pass
        // We use a dot product to determine which side of the line the NPC is on
        float side = (toNPC.x * ballDir.y - toNPC.y * ballDir.x > 0) ? 1.f : -1.f;
        sf::Vector2f lateralDrift = sf::Vector2f(-ballDir.y, ballDir.x) * side;

        // Up to 180px (1.8m) off-target for terrible awareness, scaling down as ball arrives
        interceptPoint += lateralDrift * (errorSeverity * 180.f);
    }

    // Clamp to the 10000x7000 pitch limits so they don't sprint into the void
    interceptPoint.x = std::clamp(interceptPoint.x, 0.f, 10000.f);
    interceptPoint.y = std::clamp(interceptPoint.y, 0.f, 7000.f);

    return interceptPoint;
}

void NPCController::executeThrowIn(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates) {
    Player* bestTarget = nullptr;
    float bestScore = -9999.f;

    // 1. Find a teammate within throwing range (approx 25 meters / 2500px)
    for (Player* mate : teammates) {
        if (mate == &npc) continue; // Don't throw to yourself

        float d = dist(npc.getPosition(), mate->getPosition());
        if (d < 2500.f) {
            float score = 2500.f - d; // Simple scoring: closer is better
            if (score > bestScore) {
                bestScore = score;
                bestTarget = mate;
            }
        }
    }

    // 2. Fallback: If no one is close, throw it toward the center of the pitch
    sf::Vector2f targetPos = bestTarget ? bestTarget->getPosition() : sf::Vector2f(5000.f, 3500.f);
    sf::Vector2f throwDir = normalize(targetPos - npc.getPosition());

    // 3. Throw-In Physics
    float throwPower = 35.0f;   // Much lower than a foot kick
    float vzPower = 450.0f;     // High vertical arc to simulate hands
    float backspin = 5.0f;      // Minimal spin

    // Execute the throw!
    ball.shoot(throwDir, throwPower, 0.0f, vzPower, backspin);
    npc.resetKickCooldown();
}

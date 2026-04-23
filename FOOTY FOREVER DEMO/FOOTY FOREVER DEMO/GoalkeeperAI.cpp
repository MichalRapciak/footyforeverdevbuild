#include "GoalkeeperAI.h"
#include "PlayerAI.h"
#include "AimAssist.h"
#include "PhysicsEngine.h"

void GoalkeeperAI::handleGoalkeeping(NPCPlayer& npc, Ball& ball, const Pitch& pitch, const std::vector<Player*>& team, const std::vector<Player*>& opposition, float dt, const TeamAI& teamAI, SoundManager& soundManager) {
    if (npc.getState() == PlayerState::Diving) {
        PhysicsEngine::applyKeeperDiveFriction(npc, dt);
        if (npc.getState() == PlayerState::Diving) return;
    }

    if (npc.getBallPossession()) {
        npc.m_possessionTimer += dt;
        bool isHome = teamAI.isHome();

        // ==========================================
        // THE FIX 1: THE CONTROL TOUCH (Step in front of the ball!)
        // ==========================================
        // Instead of letting the ball sit between their legs, the keeper will actively 
        // step goal-side of the ball to shield it from their own net.
        float myGoalX = isHome ? pitch.margin : pitch.totalWidth - pitch.margin;
        float distToGoalLine = std::abs(npc.getPosition().x - myGoalX);

        if (distToGoalLine < 150.f) {
            // Push the ball slightly forward so it doesn't accidentally cross the line!
            sf::Vector2f safeSpot = isHome ? sf::Vector2f(pitch.margin + 160.f, npc.getPosition().y)
                : sf::Vector2f(pitch.totalWidth - pitch.margin - 160.f, npc.getPosition().y);

            sf::Vector2f safeDir = PlayerAI::normalize(safeSpot - npc.getPosition());
            npc.setVelocity(safeDir * (npc.getTopSpeed() * 4.0f));

            // Force facing forward so the ball stays in front of them
            sf::Vector2f oppGoal = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);
            npc.setRotationToward(oppGoal);
            return; // Don't try to pass while securing the ball
        }

        sf::Vector2f oppGoal = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);
        npc.setRotationToward(oppGoal);

        Player* opp = PlayerAI::findNearestOpponent(npc.getPosition(), opposition);
        float oppDist = opp ? PlayerAI::dist(npc.getPosition(), opp->getPosition()) : 9999.f;
        PlaystyleType type = npc.getPlaystyle().type;

        float gkDribbleSkill = (npc.getBallControl() * 0.7f + npc.getAgility() * 0.3f) / 100.f;

        float penaltyBoxX = isHome ? pitch.margin + 1650.f : pitch.totalWidth - pitch.margin - 1650.f;
        float distToBoxEdge = std::abs(penaltyBoxX - npc.getPosition().x);

        // ==========================================
        // 1. HIGH PRESSURE (The Panic Zone)
        // ==========================================
        if (oppDist < 450.f) {
            if (gkDribbleSkill > 0.75f && oppDist > 150.f && distToBoxEdge > 300.f) {
                sf::Vector2f toOpp = opp->getPosition() - npc.getPosition();
                sf::Vector2f awayDir = PlayerAI::normalize(-toOpp);

                sf::Vector2f sideStep(-awayDir.y, awayDir.x);
                if ((rand() % 100) > 50) sideStep = -sideStep;

                sf::Vector2f dribbleDir = PlayerAI::normalize(awayDir + sideStep * 0.5f);
                npc.setVelocity(dribbleDir * (npc.getTopSpeed() * 8.0f));
                return;
            }

            float clearPower = npc.getKickPower() * 0.60f;
            float vzPower = 800.f;
            sf::Vector2f clearDir = isHome ? sf::Vector2f(1.f, 0.f) : sf::Vector2f(-1.f, 0.f);

            float maxError = 15.0f - (gkDribbleSkill * 10.0f);
            float randError = ((rand() % 200) - 100) / 100.f * maxError;
            float rad = randError * 3.14159f / 180.f;

            sf::Vector2f finalDir(
                clearDir.x * std::cos(rad) - clearDir.y * std::sin(rad),
                clearDir.x * std::sin(rad) + clearDir.y * std::cos(rad)
            );

            float kickVol = std::clamp(0.f + (clearPower / npc.getKickPower()) * 40.0f, 10.f, 100.f);
            soundManager.playRandomSound("kick", 3, kickVol, 0.15f);
            ball.shoot(finalDir, clearPower, 0.f, vzPower, 50.f);

            npc.m_possessionTimer = 0.0f;
            npc.resetKickCooldown();
            npc.setVelocity({ 0.f, 0.f });
            return;
        }

        // ==========================================
        // THE FIX 2: INSTANT DISTRIBUTION
        // ==========================================
        // Sweepers play it instantly (0.2s). Standard keepers wait a tiny bit longer (0.6s) to let players fan out.
        float patience = (type == PlaystyleType::SweeperKeeper) ? 0.2f : 0.6f;

        if (npc.m_possessionTimer > patience) {
            distributeBallAsGoalie(npc, ball, team, opposition, pitch, teamAI, soundManager);
            npc.m_possessionTimer = 0.0f;
            npc.setVelocity({ 0.f, 0.f });
        }
        else {
            // Carry the ball out while waiting for the short timer
            if (type == PlaystyleType::SweeperKeeper && gkDribbleSkill > 0.6f && distToBoxEdge > 400.f) {
                sf::Vector2f forwardDir = isHome ? sf::Vector2f(1.f, 0.f) : sf::Vector2f(-1.f, 0.f);
                float carrySpeed = npc.getTopSpeed() * 3.0f * gkDribbleSkill;
                npc.setVelocity(forwardDir * carrySpeed);
            }
            else {
                npc.setVelocity({ 0.f, 0.f });
            }
        }
        return;
    }

    // ==========================================
        // --- THE FIX: GK AUTO-PICKUP (HANDS) ---
        // ==========================================
    float dx = npc.getPosition().x - ball.getPosition().x;
    float dy = npc.getPosition().y - ball.getPosition().y;
    float distToBall = std::sqrt(dx * dx + dy * dy);

    // Ensure they are actually inside their penalty box to use their hands!
    float penaltyBoxEdgeX = teamAI.isHome() ? pitch.margin + 1650.f : pitch.totalWidth - pitch.margin - 1650.f;
    bool inOwnBoxX = teamAI.isHome() ? (npc.getPosition().x < penaltyBoxEdgeX) : (npc.getPosition().x > penaltyBoxEdgeX);
    bool inOwnBoxY = (npc.getPosition().y > 3500.f - 2040.f && npc.getPosition().y < 3500.f + 2040.f);

    // THE FIX: Added npc.getKickCooldown() <= 0.0f !!!
    // This stops the keeper from instantly re-catching the ball the exact millisecond they distribute it!
    if (!ball.hasOwner() && distToBall < 90.f && inOwnBoxX && inOwnBoxY && ball.z < 200.f && npc.getKickCooldown() <= 0.0f) {
        if (npc.getState() != PlayerState::Stunned && npc.getState() != PlayerState::FallOver) {
            ball.possess(&npc);
            npc.setVelocity({ 0.f, 0.f }); // Slam the brakes so they don't slide into the net with the ball!
            return;
        }
    }

    sf::Vector2f myGoalCenter = teamAI.isHome() ? pitch.homeGoalCenter : pitch.awayGoalCenter;
    sf::Vector2f targetPos;
    bool sprint = false;

    // THE FIX: Added pitch.margin to the box check so they accurately scan their area!
    bool ballInBoxX = teamAI.isHome() ? (ball.getPosition().x < pitch.margin + 1650.f) : (ball.getPosition().x > pitch.totalWidth - pitch.margin - 1650.f);
    bool ballInBoxY = (ball.getPosition().y > 3500.f - 2000.f && ball.getPosition().y < 3500.f + 2000.f);

    Player* nearestAttacker = PlayerAI::findNearestOpponent(ball.getPosition(), opposition);
    float attackerDist = nearestAttacker ? PlayerAI::dist(nearestAttacker->getPosition(), ball.getPosition()) : 9999.f;
    float keeperTTI = PlayerAI::dist(npc.getPosition(), ball.getPosition()) / (npc.getTopSpeed() * 10.0f > 0 ? npc.getTopSpeed() * 10.0f : 1.0f);
    float attackerTTI = attackerDist / (nearestAttacker ? nearestAttacker->getTopSpeed() * 10.0f : 1.0f);
    bool shouldRush = PlayerAI::dist(ball.getPosition(), myGoalCenter) < 800.f && (keeperTTI + (100.0f - npc.getGkAwareness() * 0.05f) < attackerTTI - 0.2f);

    if (!ball.hasOwner() && ballInBoxX && ballInBoxY && ball.z < 100.f) { targetPos = ball.getPosition(); sprint = true; }
    else if (shouldRush) { targetPos = ball.getPosition(); sprint = true; }
    else {
        // Calculate goal positioning
        sf::Vector2f directionToBall = PlayerAI::normalize(ball.getPosition() - myGoalCenter);
        float actualStepOutDistance = std::min(250.0f * (npc.getGkCoverage() / 100.0f), PlayerAI::dist(myGoalCenter, ball.getPosition()) * 0.15f);
        targetPos = myGoalCenter + (directionToBall * actualStepOutDistance);
        targetPos.y = std::clamp(targetPos.y, myGoalCenter.y - 366.0f + 40.0f, myGoalCenter.y + 366.0f - 40.0f);
        if (teamAI.isHome()) targetPos.x = std::clamp(targetPos.x, myGoalCenter.x, myGoalCenter.x + 250.0f);
        else targetPos.x = std::clamp(targetPos.x, myGoalCenter.x - 250.0f, myGoalCenter.x);
        sprint = false;
    }

    // ==========================================
    // --- THE SMOOTH GK MOVEMENT FIX ---
    // ==========================================
    float distToTarget = PlayerAI::dist(npc.getPosition(), targetPos);

    // 1. Calculate the ideal max speed for this movement
    float maxSpeed = sprint ? (npc.getTopSpeed() * 10.0f) : 400.0f + ((std::max(npc.getAgility(), npc.getGkReactions()) / 100.0f) * 200.0f);

    sf::Vector2f desiredVelocity(0.f, 0.f);

    // 2. Expanded Deadzone & Arrival Easing
    if (distToTarget > 15.0f) {
        float slowingRadius = 150.0f; // Start hitting the brakes 1.5 meters out
        float actualSpeed = maxSpeed;

        if (distToTarget < slowingRadius) {
            // Smoothly ramp down speed as they approach the exact spot
            float ramp = distToTarget / slowingRadius;
            actualSpeed = maxSpeed * ramp;
        }

        // Prevent mathematical overshooting in a single frame
        actualSpeed = std::min(actualSpeed, distToTarget / dt);

        desiredVelocity = PlayerAI::normalize(targetPos - npc.getPosition()) * actualSpeed;
    }

    // 3. Velocity Interpolation (Lerping)
    // Instead of instantly snapping to the new velocity, blend it smoothly.
    // This gives the goalkeeper physical "weight" and inertia on the line.
    sf::Vector2f currentVel = npc.getVelocity();
    float responsiveness = 12.0f; // Higher = snappier, Lower = smoother/heavier

    sf::Vector2f smoothedVel = currentVel + (desiredVelocity - currentVel) * (responsiveness * dt);

    // 4. Kill remaining micro-vibrations
    if (std::abs(smoothedVel.x) < 2.0f && std::abs(smoothedVel.y) < 2.0f) {
        smoothedVel = { 0.f, 0.f };
    }

    npc.setVelocity(smoothedVel);
    npc.setRotationToward(ball.getPosition());

    // Dive Math
    float ballSpeed = std::sqrt(ball.getVelocity().x * ball.getVelocity().x + ball.getVelocity().y * ball.getVelocity().y);
    if (ballSpeed > 300.0f && ((teamAI.isHome() && ball.getVelocity().x < -10.0f) || (!teamAI.isHome() && ball.getVelocity().x > 10.0f))) {
        float ballTTI = std::abs((npc.getPosition().x - ball.getPosition().x) / ball.getVelocity().x);
        if (ballTTI >= 0.0f && ballTTI <= 1.5f) {
            float interceptZ = std::max(0.f, ball.z + (ball.vz * ballTTI) - (0.5f * 980.f * ballTTI * ballTTI));
            float interceptY = ball.getPosition().y + (ball.getVelocity().y * ballTTI);
            float diveDistance = std::abs(npc.getPosition().y - interceptY);
            float maxDiveSpeed = 600.0f + (((PlayerAI::dist(npc.getPosition(), ball.getPosition()) < 600.f) ? npc.getGkBlocking() : npc.getGkReactions()) / 100.0f * 1000.0f);

            if (ballTTI <= (diveDistance / maxDiveSpeed) + 0.15f && diveDistance <= 800.0f && interceptZ <= npc.height + 120.0f) {
                float finalSpeed = std::clamp((ballTTI > 0.05f) ? diveDistance / ballTTI : maxDiveSpeed, 150.0f, maxDiveSpeed);
                npc.setVelocity(PlayerAI::normalize(sf::Vector2f(npc.getPosition().x, interceptY) - npc.getPosition()) * finalSpeed);
                npc.vz = (interceptZ < 40.f) ? 0.f : ((interceptZ < 120.f) ? 140.f : 200.f + (interceptZ * 0.2f));
                npc.setState(PlayerState::Diving);
            }
        }
    }
}

sf::Vector2f GoalkeeperAI::calculateGoaliePositioning(NPCPlayer& npc, sf::Vector2f ballPos, sf::Vector2f goalCenter, const Pitch& pitch)
{
    sf::Vector2f goalToBall = ballPos - goalCenter;
    float distToBall = PlayerAI::dist(goalCenter, ballPos);
    if (distToBall < 1.0f) return goalCenter;

    PlaystyleType type = npc.getPlaystyle().type;

    // --- DNA INJECTION: BASE POSITIONING ---
    float baseMaxStep = 250.0f; // Standard
    if (type == PlaystyleType::SweeperKeeper) baseMaxStep = 700.0f; // Pushes way up!
    else if (type == PlaystyleType::OnTheLine) baseMaxStep = 50.0f;  // Glued to the line

    sf::Vector2f directionToBall = PlayerAI::normalize(goalToBall);
    float maxStep = baseMaxStep * (npc.getGkCoverage() / 100.0f);

    // Sweepers react to the ball further up the pitch (30% scale vs 15% scale)
    float stepScale = (type == PlaystyleType::SweeperKeeper) ? 0.3f : 0.15f;
    float actualStepOutDistance = std::min(maxStep, distToBall * stepScale);

    sf::Vector2f targetPos = goalCenter + (directionToBall * actualStepOutDistance);

    float goalHalfWidth = 366.0f;
    float postBuffer = 40.0f;
    float minY = goalCenter.y - goalHalfWidth + postBuffer;
    float maxY = goalCenter.y + goalHalfWidth - postBuffer;
    targetPos.y = std::clamp(targetPos.y, minY, maxY);

    bool isHomeSide = (npc.getTeam() == Team::Home);
    if (isHomeSide) {
        targetPos.x = std::clamp(targetPos.x, goalCenter.x, goalCenter.x + maxStep);
    }
    else {
        targetPos.x = std::clamp(targetPos.x, goalCenter.x - maxStep, goalCenter.x);
    }
    return targetPos;
}

bool GoalkeeperAI::shouldGoalieRush(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI)
{
    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f keeperPos = npc.getPosition();
    bool isHomeSide = teamAI.isHome();
    sf::Vector2f myGoalCenter = isHomeSide ? pitch.homeGoalCenter : pitch.awayGoalCenter;

    Player* nearestAttacker = PlayerAI::findNearestOpponent(ballPos, opposition);
    if (!nearestAttacker) return false;

    float closestAttackerDist = PlayerAI::dist(nearestAttacker->getPosition(), ballPos);
    float keeperDistToBall = PlayerAI::dist(keeperPos, ballPos);

    float keeperSpeed = npc.getTopSpeed() * 10.0f;
    float attackerSpeed = nearestAttacker->getTopSpeed() * 10.0f;

    float keeperTTI = keeperDistToBall / (keeperSpeed > 0 ? keeperSpeed : 1.0f);
    float attackerTTI = closestAttackerDist / (attackerSpeed > 0 ? attackerSpeed : 1.0f);

    // --- DNA INJECTION: RUSH DECISION ---
    float hesitationPenalty = (100.0f - npc.getGkAwareness()) * 0.05f;
    PlaystyleType type = npc.getPlaystyle().type;

    if (type == PlaystyleType::SweeperKeeper) hesitationPenalty *= 0.1f; // Instant reaction, no fear
    else if (type == PlaystyleType::OnTheLine) hesitationPenalty *= 3.0f; // Terrified to leave the box

    float perceivedKeeperTTI = keeperTTI + hesitationPenalty;

    return (perceivedKeeperTTI < (attackerTTI - 0.2f));
}

void GoalkeeperAI::attemptSave(NPCPlayer& npc, Ball& ball, float dt, const TeamAI& teamAI)
{
    sf::Vector2f ballVel = ball.getVelocity();
    float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);
    if (ballSpeed < 300.0f) return;

    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f keeperPos = npc.getPosition();

    bool isHomeSide = teamAI.isHome();
    if (isHomeSide && ballVel.x >= -10.0f) return;
    if (!isHomeSide && ballVel.x <= 10.0f) return;

    float ballTTI = std::abs((keeperPos.x - ballPos.x) / ballVel.x);
    if (ballTTI < 0.0f || ballTTI > 1.5f) return;

    float gravity = 980.f;
    float interceptZ = ball.z + (ball.vz * ballTTI) - (0.5f * gravity * ballTTI * ballTTI);
    interceptZ = std::max(0.f, interceptZ);

    float interceptY = ballPos.y + (ballVel.y * ballTTI);
    sf::Vector2f interceptPoint(keeperPos.x, interceptY);
    float diveDistance = std::abs(keeperPos.y - interceptY);

    float distToBall = PlayerAI::dist(keeperPos, ballPos);
    float activeStat = (distToBall < 600.0f) ? npc.getGkBlocking() : npc.getGkReactions();
    float maxDiveSpeed = 800.0f + ((activeStat / 100.0f) * 1200.0f);

    // ==========================================
    // --- THE LOW SHOT FIX: COLLAPSE DIVES ---
    // ==========================================
    // Dropping to the floor is physically faster than leaping into the air.
    // If the projected shot is low (< 60px high), we give them a 35% speed boost 
    // to snap down and cover the bottom corners instantly!
    if (interceptZ < 60.0f) {
        maxDiveSpeed *= 1.35f;
    }

    float keeperTTI = diveDistance / maxDiveSpeed;
    if (ballTTI > keeperTTI + 0.25f) return; // Cannot reach it in time

    // BUFF 3: Increased dive radius from 800px (8m) to 1000px (10m) to cover the whole net.
    float attemptDiveRadius = 1000.0f;

    if (diveDistance <= attemptDiveRadius && interceptZ <= (npc.height + 150.0f)) {
        float optimalSpeed = maxDiveSpeed;
        if (ballTTI > 0.05f) optimalSpeed = diveDistance / ballTTI;

        // Let them launch at higher minimum speeds
        float finalSpeed = std::clamp(optimalSpeed, 350.0f, maxDiveSpeed);

        triggerDive(npc, interceptPoint, finalSpeed, interceptZ);
    }
}

void GoalkeeperAI::triggerDive(NPCPlayer& npc, sf::Vector2f diveTarget, float jumpSpeed, float targetZ)
{
    sf::Vector2f diveDir = PlayerAI::normalize(diveTarget - npc.getPosition());
    npc.setVelocity(diveDir * jumpSpeed);

    if (targetZ < 40.f) npc.vz = 0.f;
    else if (targetZ < 120.f) npc.vz = 140.f;
    else npc.vz = 200.f + (targetZ * 0.2f);

    npc.setState(PlayerState::Diving);
}

void GoalkeeperAI::distributeBallAsGoalie(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI, SoundManager& soundManager)
{
    Player* bestTarget = nullptr;
    float bestScore = -1e9f;
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);

    PlayerBehavior behavior = npc.getPlaystyle().behavior;
    float passRisk = behavior.passRiskBias;
    float tikiTakaPref = 1.0 - teamAI.getPassingLengthPref();

    for (Player* mate : teammates) {
        if (mate == &npc || mate->getPositionRole() == PositionRole::Goalkeeper) continue; // NEVER pass to yourself!

        sf::Vector2f matePos = mate->getPosition();
        float d = PlayerAI::dist(npcPos, matePos);
        float progress = isHome ? (matePos.x - npcPos.x) : (npcPos.x - matePos.x);

        float score = (progress * 0.5f);

        // --- DNA & TACTICAL OVERRIDES ---
        if (d < 1500.f) {
            score += (tikiTakaPref * 1500.f);
            score += (1.0f - passRisk) * 800.f;
        }
        else if (d > 3000.f) {
            score += ((teamAI.getPassingLengthPref()) * 1500.f);
            score += (passRisk * 1200.f) * (npc.getLongPassing() / 100.f);
        }

        // --- PASSING LANE SAFETY CHECK ---
        sf::Vector2f passDir = PlayerAI::normalize(matePos - npcPos);
        bool laneBlocked = false;

        for (Player* opp : opposition) {
            float dOpp = PlayerAI::dist(npcPos, opp->getPosition());
            if (dOpp < d && dOpp > 100.f) {
                sf::Vector2f toOpp = opp->getPosition() - npcPos;
                if ((passDir.x * (toOpp.x / dOpp) + passDir.y * (toOpp.y / dOpp)) > 0.95f) {
                    laneBlocked = true;
                    break;
                }
            }
        }

        if (laneBlocked) score -= 5000.f;

        Player* nearestToTarget = PlayerAI::findNearestOpponent(matePos, opposition);
        if (nearestToTarget) {
            float distToTarget = PlayerAI::dist(matePos, nearestToTarget->getPosition());
            if (distToTarget < 400.f) score -= 1000.f;
        }

        // Removed the upper bounds cap so they can target far wingers!
        if (d > 500.0f) {
            if (score > bestScore) {
                bestScore = score;
                bestTarget = mate;
            }
        }
    }

    // THE FIX: Find any valid outfield teammate if the loop failed
    if (!bestTarget) {
        for (Player* tm : teammates) {
            if (tm != &npc && tm->getPositionRole() != PositionRole::Goalkeeper) {
                bestTarget = tm;
                break;
            }
        }
    }
    if (!bestTarget) return;

    // THE FIX: If the highest scoring pass was highly negative, every lane is blocked. BOOT IT!
    bool forceClearance = (bestScore < -1000.f);

    // --- EXECUTION ---
    sf::Vector2f targetPos = bestTarget->getPosition();
    float rawDist = PlayerAI::dist(npcPos, targetPos);

    bool goHigh = (rawDist > 2800.f) || forceClearance; // Lowered high-pass threshold, forced high if panicked
    sf::Vector2f directDir = PlayerAI::normalize(targetPos - npcPos);
    bool useThrow = (!forceClearance && rawDist < 2500.f && npc.getGkThrowing() > npc.getLongPassing());
    float statToUse = useThrow ? npc.getGkThrowing() : npc.getLongPassing();

    float finalPower = npc.getKickPower(); // Provide maximum base power to avoid 0.0f tap passes
    AimAssist::applyPassAssist(npc, bestTarget, directDir, finalPower, goHigh, true, pitch);

    float vzPower = 0.f;
    float finalBackspin = 0.f;

    if (goHigh) {
        // If we are forced to clear, launch it to maximum height!
        float maxLoft = forceClearance ? 1100.f : std::clamp(500.f + (rawDist * 0.25f), 700.f, 1150.f);
        vzPower = std::max(maxLoft - ((statToUse / 100.f) * 80.f), 300.f);
        finalBackspin = 60.f + (statToUse * 0.5f);

        if (forceClearance) finalPower = npc.getKickPower() * 0.40f; // Ensure maximum distance
    }
    else {
        vzPower = 50.f + (finalPower * 0.5f);
        finalBackspin = 10.f;
    }

    float errorMagnitude = (1.0f - (statToUse / 100.f));
    float randError = ((rand() % 200) - 100) / 100.f * errorMagnitude * 7.5f;
    float rad = randError * 3.14159f / 180.f;

    sf::Vector2f finalDir(
        directDir.x * std::cos(rad) - directDir.y * std::sin(rad),
        directDir.x * std::sin(rad) + directDir.y * std::cos(rad)
    );

    ball.shoot(finalDir, finalPower, 0.0f, vzPower, finalBackspin);
    if (!useThrow) {
        float kickVol = std::clamp(0.0f + (finalPower / npc.getKickPower()) * 40.0f, 10.f, 100.f);
        soundManager.playRandomSound("kick", 3, kickVol, 0.15f);
    }

    npc.resetKickCooldown();
}
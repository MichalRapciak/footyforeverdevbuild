#include "GoalkeeperAI.h"
#include "PlayerAI.h"
#include "AimAssist.h"
#include "PhysicsEngine.h"
#include "SoundManager.h"
#include "Ball.h"
#include "Pitch.h"
#include <cmath>
#include <algorithm>

void GoalkeeperAI::handleGoalkeeping(NPCPlayer& npc, float dt, const TeamAI& teamAI, MatchEnvironment& env) {
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
        float myGoalX = isHome ? env.pitch->margin : env.pitch->totalWidth - env.pitch->margin;
        float distToGoalLine = std::abs(npc.getPosition().x - myGoalX);

        if (distToGoalLine < 150.f) {
            sf::Vector2f safeSpot = isHome ? sf::Vector2f(env.pitch->margin + 160.f, npc.getPosition().y)
                : sf::Vector2f(env.pitch->totalWidth - env.pitch->margin - 160.f, npc.getPosition().y);

            sf::Vector2f safeDir = PlayerAI::normalize(safeSpot - npc.getPosition());
            npc.setVelocity(safeDir * (npc.getTopSpeed() * 4.0f));

            sf::Vector2f oppGoal = isHome ? sf::Vector2f(env.pitch->totalWidth - env.pitch->margin, 3500.f) : sf::Vector2f(env.pitch->margin, 3500.f);
            npc.setRotationToward(oppGoal);
            return;
        }

        sf::Vector2f oppGoal = isHome ? sf::Vector2f(env.pitch->totalWidth - env.pitch->margin, 3500.f) : sf::Vector2f(env.pitch->margin, 3500.f);
        npc.setRotationToward(oppGoal);

        Player* opp = PlayerAI::findNearestOpponent(npc.getPosition(), *(env.opposition));
        float oppDist = opp ? PlayerAI::dist(npc.getPosition(), opp->getPosition()) : 9999.f;
        PlaystyleType type = npc.getPlaystyle().type;

        float gkDribbleSkill = (npc.getBallControl() * 0.7f + npc.getAgility() * 0.3f) / 100.f;
        float penaltyBoxX = isHome ? env.pitch->margin + 1650.f : env.pitch->totalWidth - env.pitch->margin - 1650.f;
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
            env.sound->playRandomSound("kick", 3, kickVol, 0.15f);
            env.ball->shoot(finalDir, clearPower, 0.f, vzPower, 50.f);

            npc.m_possessionTimer = 0.0f;
            npc.resetKickCooldown();
            npc.setVelocity({ 0.f, 0.f });
            return;
        }

        // ==========================================
        // THE FIX 2: INSTANT DISTRIBUTION
        // ==========================================
        float patience = (type == PlaystyleType::SweeperKeeper) ? 0.2f : 0.6f;

        if (npc.m_possessionTimer > patience) {
            distributeBallAsGoalie(npc, teamAI, env);
            npc.m_possessionTimer = 0.0f;
            npc.setVelocity({ 0.f, 0.f });
        }
        else {
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
    float dx = npc.getPosition().x - env.ball->getPosition().x;
    float dy = npc.getPosition().y - env.ball->getPosition().y;
    float distToBall = std::sqrt(dx * dx + dy * dy);

    float penaltyBoxEdgeX = teamAI.isHome() ? env.pitch->margin + 1650.f : env.pitch->totalWidth - env.pitch->margin - 1650.f;
    bool inOwnBoxX = teamAI.isHome() ? (npc.getPosition().x < penaltyBoxEdgeX) : (npc.getPosition().x > penaltyBoxEdgeX);
    bool inOwnBoxY = (npc.getPosition().y > 3500.f - 2040.f && npc.getPosition().y < 3500.f + 2040.f);

    if (!env.ball->hasOwner() && distToBall < 90.f && inOwnBoxX && inOwnBoxY && env.ball->z < 200.f && npc.getKickCooldown() <= 0.0f) {
        if (npc.getState() != PlayerState::Stunned && npc.getState() != PlayerState::FallOver) {
            env.ball->possess(&npc);
            npc.setVelocity({ 0.f, 0.f });
            npc.setRotation(90.0f);
            return;
        }
    }

    sf::Vector2f myGoalCenter = teamAI.isHome() ? env.pitch->homeGoalCenter : env.pitch->awayGoalCenter;
    sf::Vector2f targetPos;
    bool sprint = false;

    bool ballInBoxX = teamAI.isHome() ? (env.ball->getPosition().x < env.pitch->margin + 1650.f) : (env.ball->getPosition().x > env.pitch->totalWidth - env.pitch->margin - 1650.f);
    bool ballInBoxY = (env.ball->getPosition().y > 3500.f - 2000.f && env.ball->getPosition().y < 3500.f + 2000.f);

    bool shouldRush = GoalkeeperAI::shouldGoalieRush(npc, teamAI, env);

    // ==========================================
    // --- NEW: THE SET PIECE LINE LOCK ---
    // ==========================================
    sf::Vector2f ballVel = env.ball->getVelocity();
    float ballSpeedSq = ballVel.x * ballVel.x + ballVel.y * ballVel.y;

    if (ballSpeedSq < 10.f && !env.ball->hasOwner()) {
        shouldRush = false;
        float goalLineX = teamAI.isHome() ? env.pitch->homeGoalCenter.x : env.pitch->awayGoalCenter.x;
        float yOffset = (env.ball->getPosition().y - myGoalCenter.y) * 0.15f;

        targetPos = sf::Vector2f(goalLineX, myGoalCenter.y + yOffset);
        sprint = false;
    }
    else if (!env.ball->hasOwner() && ballInBoxX && ballInBoxY && env.ball->z < 100.f) {
        targetPos = env.ball->getPosition();
        sprint = true;
    }
    else if (shouldRush) {
        targetPos = env.ball->getPosition();
        sprint = true;
    }
    else {
        targetPos = GoalkeeperAI::calculateGoaliePositioning(npc, myGoalCenter, env);
        sprint = false;
    }

    // ==========================================
    // --- THE SMOOTH GK MOVEMENT FIX ---
    // ==========================================
    float distToTarget = PlayerAI::dist(npc.getPosition(), targetPos);
    float maxSpeed = sprint ? (npc.getTopSpeed() * 10.0f) : 400.0f + ((std::max(npc.getAgility(), npc.getGkReactions()) / 100.0f) * 200.0f);

    sf::Vector2f desiredVelocity(0.f, 0.f);

    if (distToTarget > 15.0f) {
        float slowingRadius = 150.0f;
        float actualSpeed = maxSpeed;

        if (distToTarget < slowingRadius) {
            float ramp = distToTarget / slowingRadius;
            actualSpeed = maxSpeed * ramp;
        }

        actualSpeed = std::min(actualSpeed, distToTarget / dt);
        desiredVelocity = PlayerAI::normalize(targetPos - npc.getPosition()) * actualSpeed;
    }

    sf::Vector2f currentVel = npc.getVelocity();
    float responsiveness = 12.0f;
    sf::Vector2f smoothedVel = currentVel + (desiredVelocity - currentVel) * (responsiveness * dt);

    if (std::abs(smoothedVel.x) < 2.0f && std::abs(smoothedVel.y) < 2.0f) {
        smoothedVel = { 0.f, 0.f };
    }

    npc.setVelocity(smoothedVel);
    npc.setRotationToward(env.ball->getPosition());

    GoalkeeperAI::attemptSave(npc, dt, teamAI, env);
}

sf::Vector2f GoalkeeperAI::calculateGoaliePositioning(NPCPlayer& npc, sf::Vector2f goalCenter, MatchEnvironment& env)
{
    sf::Vector2f ballPos = env.ball->getPosition();
    sf::Vector2f goalToBall = ballPos - goalCenter;
    float distToBall = PlayerAI::dist(goalCenter, ballPos);
    if (distToBall < 1.0f) return goalCenter;

    PlaystyleType type = npc.getPlaystyle().type;

    float baseMaxStep = 250.0f;
    if (type == PlaystyleType::SweeperKeeper) baseMaxStep = 700.0f;
    else if (type == PlaystyleType::OnTheLine) baseMaxStep = 50.0f;

    sf::Vector2f directionToBall = PlayerAI::normalize(goalToBall);
    float maxStep = baseMaxStep * (npc.getGkCoverage() / 100.0f);

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

bool GoalkeeperAI::shouldGoalieRush(NPCPlayer& npc, const TeamAI& teamAI, MatchEnvironment& env)
{
    sf::Vector2f ballPos = env.ball->getPosition();
    sf::Vector2f keeperPos = npc.getPosition();
    bool isHomeSide = teamAI.isHome();
    sf::Vector2f myGoalCenter = isHomeSide ? env.pitch->homeGoalCenter : env.pitch->awayGoalCenter;

    float distToGoal = PlayerAI::dist(ballPos, myGoalCenter);
    PlaystyleType type = npc.getPlaystyle().type;

    float maxRushDist = 1050.f;
    if (type == PlaystyleType::SweeperKeeper) maxRushDist = 1800.f;
    else if (type == PlaystyleType::OnTheLine) maxRushDist = 600.f;

    if (distToGoal > maxRushDist) return false;

    Player* nearestAttacker = PlayerAI::findNearestOpponent(ballPos, *(env.opposition));
    if (!nearestAttacker) return false;

    float closestAttackerDist = PlayerAI::dist(nearestAttacker->getPosition(), ballPos);
    float keeperDistToBall = PlayerAI::dist(keeperPos, ballPos);

    float keeperSpeed = npc.getTopSpeed() * 10.0f;
    float attackerSpeed = nearestAttacker->getTopSpeed() * 10.0f;

    float keeperTTI = keeperDistToBall / (keeperSpeed > 0 ? keeperSpeed : 1.0f);
    float attackerTTI = closestAttackerDist / (attackerSpeed > 0 ? attackerSpeed : 1.0f);

    float hesitationPenalty = (100.0f - npc.getGkAwareness()) * 0.10f;
    if (type == PlaystyleType::SweeperKeeper) hesitationPenalty *= 0.1f;
    else if (type == PlaystyleType::OnTheLine) hesitationPenalty *= 3.0f;

    float perceivedKeeperTTI = keeperTTI + hesitationPenalty;

    return (perceivedKeeperTTI < (attackerTTI - 0.2f));
}

void GoalkeeperAI::attemptSave(NPCPlayer& npc, float dt, const TeamAI& teamAI, MatchEnvironment& env)
{
    sf::Vector2f ballVel = env.ball->getVelocity();
    float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);
    if (ballSpeed < 300.0f) return;

    sf::Vector2f ballPos = env.ball->getPosition();
    sf::Vector2f keeperPos = npc.getPosition();

    bool isHomeSide = teamAI.isHome();
    if (isHomeSide && ballVel.x >= -10.0f) return;
    if (!isHomeSide && ballVel.x <= 10.0f) return;

    float ballTTI = std::abs((keeperPos.x - ballPos.x) / ballVel.x);

    if (ballSpeed < 800.f) ballTTI *= 1.15f;
    else if (ballSpeed < 1400.f) ballTTI *= 1.08f;

    if (ballTTI < 0.0f || ballTTI > 1.5f) return;

    float gravity = 980.f;
    float interceptZ = env.ball->z + (env.ball->vz * ballTTI) - (0.5f * gravity * ballTTI * ballTTI);
    interceptZ = std::max(0.f, interceptZ);

    float interceptY = ballPos.y + (ballVel.y * ballTTI);
    float diveDistance = std::abs(keeperPos.y - interceptY);

    float lateralOffset = interceptY - keeperPos.y;
    float keepersRightOffset = isHomeSide ? lateralOffset : -lateralOffset;

    int dirX = 0;
    if (keepersRightOffset < -80.f) dirX = -1;
    else if (keepersRightOffset > 80.f) dirX = 1;

    int dirZ = 0;
    if (interceptZ < 60.f) dirZ = -1;
    else if (interceptZ > 140.f) dirZ = 1;

    std::string diveAnim = "Center";
    if (dirX == -1 && dirZ == 1) diveAnim = "UpLeft";
    else if (dirX == -1 && dirZ == 0) diveAnim = "Left";
    else if (dirX == -1 && dirZ == -1) diveAnim = "DownLeft";
    else if (dirX == 1 && dirZ == 1) diveAnim = "UpRight";
    else if (dirX == 1 && dirZ == 0) diveAnim = "Right";
    else if (dirX == 1 && dirZ == -1) diveAnim = "DownRight";
    else if (dirX == 0 && dirZ == 1) diveAnim = "Up";
    else if (dirX == 0 && dirZ == -1) diveAnim = "Down";

    float distToBall = PlayerAI::dist(keeperPos, ballPos);
    float activeStat = (distToBall < 600.0f) ? npc.getGkBlocking() : npc.getGkReactions();
    float maxDiveSpeed = 250.0f + ((activeStat / 100.0f) * 500.0f);

    sf::Vector2f committedTarget = keeperPos;
    float committedVz = 0.f;
    float speedMultiplier = 1.0f;

    float lateralPushY = 0.f;
    if (dirX == -1) lateralPushY = isHomeSide ? -diveDistance : diveDistance;
    else if (dirX == 1) lateralPushY = isHomeSide ? diveDistance : -diveDistance;

    if (diveAnim == "UpLeft" || diveAnim == "UpRight") {
        committedTarget.y += lateralPushY;
        committedVz = 240.f + (interceptZ * 0.15f);
        speedMultiplier = 0.9f;
    }
    else if (diveAnim == "Left" || diveAnim == "Right") {
        committedTarget.y += lateralPushY;
        committedVz = 120.f;
    }
    else if (diveAnim == "DownLeft" || diveAnim == "DownRight") {
        committedTarget.y += lateralPushY;
        committedVz = 0.f;
        speedMultiplier = 1.35f;
    }
    else if (diveAnim == "Up") {
        float backpedalX = isHomeSide ? -150.f : 150.f;
        committedTarget.x += backpedalX;
        committedVz = 280.f;
        speedMultiplier = 0.5f;
    }
    else if (diveAnim == "Down") {
        committedVz = 0.f;
        speedMultiplier = 0.2f;
    }
    else if (diveAnim == "Center") {
        committedVz = 80.f;
        speedMultiplier = 0.3f;
    }

    maxDiveSpeed *= speedMultiplier;

    float gkAwareness = npc.getGkAwareness();
    float errorMargin = std::max(0.0f, (90.0f - gkAwareness)) * 0.6f;
    float randomError = ((rand() % 200) - 100) / 100.0f * errorMargin;

    if (dirX != 0) {
        committedTarget.y += randomError;
    }

    float walkingTTI = diveDistance / (npc.getTopSpeed() * 10.0f);
    if (walkingTTI < ballTTI - 0.2f && interceptZ < 100.f) {
        return;
    }

    float lateralTTI = diveDistance / maxDiveSpeed;
    float verticalTTI = (committedVz > 0.f) ? (committedVz / gravity) : 0.1f;
    float physicalKeeperTTI = std::max(lateralTTI, verticalTTI);

    float mentalStat = (npc.getGkReactions() * 0.6f) + (gkAwareness * 0.4f);
    float hesitationDelay = std::max(0.0f, (90.0f - mentalStat) / 100.0f * 0.4f);
    float perceivedKeeperTTI = physicalKeeperTTI - hesitationDelay;

    float patienceBuffer = std::clamp((ballSpeed - 500.f) / 2000.f * 0.15f, 0.02f, 0.15f);

    if (env.ball->vz > 150.f && ballTTI > 0.5f) {
        return;
    }

    if (ballTTI > perceivedKeeperTTI + patienceBuffer) return;

    float attemptDiveRadius = 1000.0f;

    if (diveDistance <= attemptDiveRadius && interceptZ <= (npc.height + 150.0f)) {
        float optimalSpeed = maxDiveSpeed;
        if (ballTTI > 0.05f && dirX != 0) optimalSpeed = diveDistance / ballTTI;

        float finalSpeed = std::clamp(optimalSpeed, 350.0f, maxDiveSpeed);
        triggerDive(npc, committedTarget, finalSpeed, committedVz, diveAnim);
    }
}

void GoalkeeperAI::triggerDive(NPCPlayer& npc, sf::Vector2f diveTarget, float jumpSpeed, float targetZ, const std::string& diveAnim)
{
    sf::Vector2f diveDir = PlayerAI::normalize(diveTarget - npc.getPosition());

    float dirLen = std::sqrt(diveDir.x * diveDir.x + diveDir.y * diveDir.y);
    if (dirLen < 0.1f) {
        diveDir = (npc.getTeam() == Team::Home) ? sf::Vector2f(-1.f, 0.f) : sf::Vector2f(1.f, 0.f);
        jumpSpeed = 50.f;
    }

    npc.setVelocity(diveDir * jumpSpeed);
    npc.vz = targetZ;

    npc.setState(PlayerState::Diving);
    npc.setLastDiveDirection(diveAnim);

    if (diveAnim != "Center" && diveAnim != "Up" && diveAnim != "Down") {
        float angle = std::atan2(diveDir.y, diveDir.x) * 180.f / 3.14159f;
        npc.setRotation(angle + 90.f);
    }
}

void GoalkeeperAI::distributeBallAsGoalie(NPCPlayer& npc, const TeamAI& teamAI, MatchEnvironment& env)
{
    Player* bestTarget = nullptr;
    float bestScore = -1e9f;
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);

    PlayerBehavior behavior = npc.getPlaystyle().behavior;
    float passRisk = behavior.passRiskBias;
    float tikiTakaPref = 1.0 - teamAI.getPassingLengthPref();

    for (Player* mate : *(env.teammates)) {
        if (mate == &npc || mate->getPositionRole() == PositionRole::Goalkeeper) continue;

        sf::Vector2f matePos = mate->getPosition();
        float d = PlayerAI::dist(npcPos, matePos);
        float progress = isHome ? (matePos.x - npcPos.x) : (npcPos.x - matePos.x);

        float score = (progress * 0.5f);

        if (d < 1500.f) {
            score += (tikiTakaPref * 1500.f);
            score += (1.0f - passRisk) * 800.f;
        }
        else if (d > 3000.f) {
            score += ((teamAI.getPassingLengthPref()) * 1500.f);
            score += (passRisk * 1200.f) * (npc.getLongPassing() / 100.f);
        }

        sf::Vector2f passDir = PlayerAI::normalize(matePos - npcPos);
        bool laneBlocked = false;

        for (Player* opp : *(env.opposition)) {
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

        Player* nearestToTarget = PlayerAI::findNearestOpponent(matePos, *(env.opposition));
        if (nearestToTarget) {
            float distToTarget = PlayerAI::dist(matePos, nearestToTarget->getPosition());
            if (distToTarget < 400.f) score -= 1000.f;
        }

        if (d > 500.0f) {
            if (score > bestScore) {
                bestScore = score;
                bestTarget = mate;
            }
        }
    }

    if (!bestTarget) {
        for (Player* tm : *(env.teammates)) {
            if (tm != &npc && tm->getPositionRole() != PositionRole::Goalkeeper) {
                bestTarget = tm;
                break;
            }
        }
    }
    if (!bestTarget) return;

    bool forceClearance = (bestScore < -1000.f);

    sf::Vector2f targetPos = bestTarget->getPosition();
    float rawDist = PlayerAI::dist(npcPos, targetPos);

    bool goHigh = (rawDist > 2800.f) || forceClearance;
    sf::Vector2f directDir = PlayerAI::normalize(targetPos - npcPos);
    bool useThrow = (!forceClearance && rawDist < 2500.f && npc.getGkThrowing() > npc.getLongPassing());
    float statToUse = useThrow ? npc.getGkThrowing() : npc.getLongPassing();

    float finalPower = npc.getKickPower();
    AimAssist::applyPassAssist(npc, bestTarget, directDir, finalPower, goHigh, true, *(env.pitch));

    float vzPower = 0.f;
    float finalBackspin = 0.f;

    if (goHigh) {
        float maxLoft = forceClearance ? 1100.f : std::clamp(500.f + (rawDist * 0.25f), 700.f, 1150.f);
        vzPower = std::max(maxLoft - ((statToUse / 100.f) * 80.f), 300.f);
        finalBackspin = 60.f + (statToUse * 0.5f);

        if (forceClearance) finalPower = npc.getKickPower() * 0.40f;
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

    env.ball->shoot(finalDir, finalPower, 0.0f, vzPower, finalBackspin);
    if (!useThrow) {
        float kickVol = std::clamp(0.0f + (finalPower / npc.getKickPower()) * 40.0f, 10.f, 100.f);
        env.sound->playRandomSound("kick", 3, kickVol, 0.15f);
    }

    npc.resetKickCooldown();
}
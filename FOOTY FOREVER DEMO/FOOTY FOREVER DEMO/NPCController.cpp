#include "NPCController.h"
#include "Ball.h"
#include "UserPlayer.h"
#include "Pitch.h"
#include "MatchEnvironment.h" // THE FIX: Now using MatchEnvironment
#include "MatchReferee.h"
#include "PhysicsEngine.h"
#include "AimAssist.h"
#include "PlayerAI.h"
#include "TeamAI.h"
#include "PossessionAI.h"
#include "PositioningAI.h"
#include "GoalkeeperAI.h"
#include "SpatialGrid.h"
#include "MatchInfo.h"
#include "MatchStatistics.h"
#include "SoundManager.h" // Need this to call playRandomSound
#include <cmath>

NPCController::NPCController() {}
NPCController::~NPCController() {}

void NPCController::update(NPCPlayer& npc, UserPlayer* userPlayer, float dt, Player* firstResponder, const TeamAI& teamAI, MatchEnvironment& env)
{
    npc.updateCooldown(dt);
    PhysicsEngine::updatePlayerAirPhysics(npc, dt);

    if (npc.getState() == PlayerState::Injured) {
        PhysicsEngine::applyPlayerIdleFriction(npc, dt);
        return;
    }

    // ==========================================
    // --- NEW: THE NPC STRIDE WATCHER ---
    // ==========================================
    if (npc.m_pendingKick.isActive) {

        int currentFrame = npc.getAnimator().getCurrentFrameIndex();
        npc.m_pendingKick.failsafeTimer += dt;

        bool frameHit = false;
        int target = npc.m_pendingKick.targetFrame;

        if (currentFrame == target || currentFrame == target + 1) frameHit = true;
        if (target == 11 && currentFrame == 0) frameHit = true;

        if (frameHit || npc.m_pendingKick.failsafeTimer > 0.4f) {

            // THE FIX: Use env.ball
            float distToBall = PlayerAI::dist(npc.getPosition(), env.ball->getPosition());

            if (distToBall < 100.f) {

                // 1. EXECUTE DEFERRED STAT LOGGING
                if (npc.m_pendingKick.isPassIntent) {
                    env.stats->recordPassAttempt(npc.getTeam());
                    env.ball->isPassIntent = true;
                }
                else if (npc.m_pendingKick.isShotIntent) {
                    env.ball->lastShooter = &npc;
                    env.ball->lastShotWasOnTarget = npc.m_pendingKick.isShotOnTarget;
                    env.ball->lastShooterAssister = npc.m_pendingKick.assistCandidate;
                    env.stats->recordShot(npc.getTeam(), npc.m_pendingKick.isShotOnTarget);
                    if (env.ball->assistCandidate != nullptr && env.ball->assistCandidate->getTeam() == npc.getTeam()) {

                        // We log a pass for the assister with the `isKeyPass` flag set to true!
                        // recordPass(playerId, isCompleted, isKeyPass)
                        env.info->recordPass(env.ball->assistCandidate->getId(), true, true);

                        // We don't wipe the assistCandidate yet, because if this shot goes in, 
                        // the Referee needs to know who gets the actual Assist!
                    }
                    env.info->recordShot(npc.getId(), npc.m_pendingKick.isShotOnTarget);
                    env.ball->isPassIntent = false;
                }

                // 2. Audio
                float kickVol = std::clamp(0.f + ((npc.m_pendingKick.power / npc.getKickPower()) * 40.0f), 10.f, 100.f);
                env.sound->playRandomSound("kick", 3, kickVol, 0.15f);

                // 3. The Physical Strike
                env.ball->shoot(
                    npc.m_pendingKick.aimDir,
                    npc.m_pendingKick.power,
                    npc.m_pendingKick.spin,
                    npc.m_pendingKick.vz,
                    npc.m_pendingKick.backspin
                );
            }

            // 4. Clean up the stride
            npc.m_pendingKick.isActive = false;
            npc.setState(PlayerState::Normal);
            npc.resetKickCooldown();
        }

        return;
    }

    bool isTaker = (&npc == env.referee->getSetPieceTaker());
    TacticalContext ctx = env.referee->getTacticalContext(npc.getTeam(), isTaker);
    PositioningMask mask = env.referee->getPositioningMask(&npc, *(env.pitch));

    // 1. DEAD BALL STATES
    if (ctx.state == MatchState::HalfTime || ctx.state == MatchState::FullTime || ctx.state == MatchState::GoalScored) {
        npc.updateStamina(dt, false);
        return;
    }
    else if (ctx.state != MatchState::InPlay) {
        npc.updateStamina(dt, false);
        if (isTaker) {
            handleSetPiece(npc, userPlayer, dt, teamAI, ctx, env);
        }
        return;
    }

    // 2. AERIAL LOGIC (Headers and Volleys)
    if (env.ball->z > 80.f) {
        if (handleAerialLogic(npc, userPlayer, teamAI, env)) {
            return;
        }
    }

    // 3. ROLE DISTRIBUTION
    if (npc.getPositionRole() == PositionRole::Goalkeeper) {
        GoalkeeperAI::handleGoalkeeping(npc, dt, teamAI, env);
    }
    else {
        handleOutfieldActions(npc, userPlayer, dt, firstResponder, teamAI, ctx, mask, env);
    }

    // 4. IDLE FACING
    if (ctx.state == MatchState::InPlay) {
        if (std::sqrt(npc.getVelocity().x * npc.getVelocity().x + npc.getVelocity().y * npc.getVelocity().y) < 2.f) {
            if (env.ball->getOwner() != &npc) npc.setRotationToward(env.ball->getPosition());
        }
    }
}

// ==========================================
// --- NEW HELPER FUNCTIONS ---
// ==========================================

void NPCController::handleSetPiece(NPCPlayer& npc, UserPlayer* user, float dt, const TeamAI& teamAI, const TacticalContext& ctx, MatchEnvironment& env)
{
    if (!env.referee->isWhistleBlown()) {
        npc.setVelocity({ 0.f, 0.f });
        npc.setRotationToward(env.ball->getPosition());
        npc.m_passTimer = 0.0f;
        return;
    }

    if (npc.getKickCooldown() > 0.0f) return;

    bool isHome = (npc.getTeam() == Team::Home);
    sf::Vector2f goalPos = isHome ? sf::Vector2f(env.pitch->totalWidth - env.pitch->margin, 3500.f) : sf::Vector2f(env.pitch->margin, 3500.f);

    if (ctx.state == MatchState::ThrowIn) {
        PossessionAI::executeThrowIn(npc, env);
        return;
    }

    // Patience Timer
    npc.m_passTimer += dt;
    float waitTime = 1.0f + ((100.f - npc.getAwareness()) / 100.f * 1.5f);
    if (ctx.state == MatchState::KickOff) waitTime *= 0.5f;

    if (npc.m_passTimer < waitTime && ctx.state != MatchState::Penalty) {
        npc.setVelocity({ 0.f, 0.f });
        sf::Vector2f surveyTarget = isHome ? sf::Vector2f(env.pitch->totalWidth, env.pitch->totalHeight / 2.f) : sf::Vector2f(0.f, env.pitch->totalHeight / 2.f);
        npc.setRotationToward(surveyTarget);
        return;
    }

    // Run-up
    sf::Vector2f ballPos = env.ball->getPosition();
    sf::Vector2f npcPos = npc.getPosition();
    float distToBall = PlayerAI::dist(npcPos, ballPos);
    sf::Vector2f moveDir = PlayerAI::normalize(ballPos - npcPos);

    if (moveDir.x == 0.f && moveDir.y == 0.f) moveDir = sf::Vector2f(1.f, 0.f);
    float sprintSpeed = npc.getTopSpeed() * 8.f;
    PhysicsEngine::applyPlayerLocomotion(npc, moveDir, sprintSpeed, dt);
    npc.setRotationToward(ballPos);

    if (distToBall < 60.f) {
        bool isShooting = false;
        float distToGoal = PlayerAI::dist(ballPos, goalPos);

        if (ctx.state == MatchState::Penalty) isShooting = true;
        else if (ctx.state == MatchState::FreeKick && distToGoal < 2800.f && npc.getPlaystyle().behavior.shootBias >= 0.3f) isShooting = true;

        if (ctx.state == MatchState::GoalKick) {
            GoalkeeperAI::distributeBallAsGoalie(npc, teamAI, env);
        }
        else if (isShooting) {
            PossessionAI::executeShot(npc, goalPos, dt, teamAI, env);
        }
        else if (ctx.state == MatchState::Corner || ctx.state == MatchState::FreeKick) {
            PossessionAI::executeSetPiece(npc, ctx.state, teamAI, env);
        }
        else {
            Player* passTarget = PossessionAI::findBestPassOption(npc, user, teamAI, env);
            if (!passTarget) {
                float maxScore = -99999.f;
                for (Player* tm : *(env.teammates)) {
                    if (tm == &npc || tm->isSentOff() || tm->getState() == PlayerState::Injured) continue;
                    float score = -PlayerAI::dist(npcPos, tm->getPosition());
                    if (ctx.state == MatchState::KickOff) {
                        bool isBackward = isHome ? (tm->getPosition().x < npcPos.x) : (tm->getPosition().x > npcPos.x);
                        if (isBackward) score += 5000.f;
                    }
                    if (score > maxScore) { maxScore = score; passTarget = tm; }
                }
            }
            if (passTarget) PossessionAI::executePass(npc, passTarget, teamAI, env);
            else PossessionAI::executeShot(npc, goalPos, dt, teamAI, env);
        }

        npc.setVelocity({ 0.f, 0.f });
        npc.resetKickCooldown();
        npc.m_passTimer = 0.0f;
    }
}

bool NPCController::handleAerialLogic(NPCPlayer& npc, UserPlayer* user, const TeamAI& teamAI, MatchEnvironment& env)
{
    PossessionAI::handleNPCJumpLogic(npc, *(env.ball));

    bool isHome = (npc.getTeam() == Team::Home);
    sf::Vector2f oppGoalPos = isHome ? sf::Vector2f(env.pitch->totalWidth - env.pitch->margin, 3500.f) : sf::Vector2f(env.pitch->margin, 3500.f);
    float distToGoal = PlayerAI::dist(npc.getPosition(), oppGoalPos);

    bool isShot = false;
    sf::Vector2f aimDir;

    if (distToGoal < 2000.f) {
        isShot = true;
        aimDir = PlayerAI::normalize(oppGoalPos - npc.getPosition());
    }
    else {
        Player* aerialPassTarget = PossessionAI::findBestPassOption(npc, user, teamAI, env);
        if (aerialPassTarget) {
            isShot = false;
            aimDir = PlayerAI::normalize(aerialPassTarget->getPosition() - npc.getPosition());
        }
        else {
            isShot = false;
            aimDir = isHome ? sf::Vector2f(1.f, 0.f) : sf::Vector2f(-1.f, 0.f);
        }
    }

    if (PossessionAI::tryNPCAerialStrike(npc, aimDir, isShot, env)) {
        npc.deductStaminaAction(1.5f);
        return true;
    }
    return false;
}

void NPCController::handleOutfieldActions(NPCPlayer& npc, UserPlayer* user, float dt, Player* firstResponder, const TeamAI& teamAI, const TacticalContext& ctx, const PositioningMask& mask, MatchEnvironment& env)
{
    sf::Vector2f npcPos = npc.getPosition();
    sf::Vector2f finalDirection(0.f, 0.f);
    bool isSprinting = false;
    float distToTarget = 0.f;
    Player* effectiveResponder = firstResponder;

    // A. OFFENSIVE POSSESSION
    if (env.ball->getOwner() == &npc) {
        finalDirection = PossessionAI::handlePossession(npc, user, dt, ctx.state, teamAI, env);
        distToTarget = 500.f;
        isSprinting = true;
    }
    // B. OFF-BALL MOVEMENT
    else {
        bool isTeammatePass = (!env.ball->hasOwner() && env.ball->getLastOwner() && env.ball->getLastOwner()->getTeam() == npc.getTeam());
        bool amITargetReceiver = false;

        if (isTeammatePass) {
            Player* targetReceiver = PositioningAI::identifyTargetReceiver(env);
            if (targetReceiver) {
                effectiveResponder = targetReceiver;
                if (targetReceiver == &npc) amITargetReceiver = true;
            }
        }

        sf::Vector2f targetPos = PositioningAI::decideTargetPosition(npc, effectiveResponder, mask, teamAI, env);
        sf::Vector2f toTarget = targetPos - npcPos;
        distToTarget = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);

        sf::Vector2f separation(0.f, 0.f);
        if (!amITargetReceiver) {
            separation = PositioningAI::calculateSeparation(npc, teamAI, env);
        }

        sf::Vector2f normTarget = (distToTarget > 0.1f) ? (toTarget / distToTarget) : sf::Vector2f(0.f, 0.f);
        finalDirection = amITargetReceiver ? normTarget : PlayerAI::normalize(normTarget + (separation * 0.8f));

        AIUrgency urgency = (teamAI.getCurrentState().phase == MatchPhase::Defending) ? AIUrgency::Recovery : AIUrgency::AttackingRun;
        if (effectiveResponder == &npc) urgency = AIUrgency::Critical;

        isSprinting = PositioningAI::evaluateSprintUrgency(npc, urgency, distToTarget, PlayerAI::dist(npcPos, env.ball->getPosition()), teamAI.getCurrentState());
        if (amITargetReceiver || npc.getBallPossession()) isSprinting = true;

        // C. PICK UP LOOSE BALL
        float distToBall = PlayerAI::dist(env.ball->getPosition(), npc.getPosition());
        if (!env.ball->hasOwner() && distToBall < 70.f && ctx.canPossess) {
            if (npc.getState() != PlayerState::Tackling && npc.getState() != PlayerState::Stunned &&
                npc.getState() != PlayerState::Stumbled && npc.getState() != PlayerState::FallOver &&
                env.ball->z < 40.f && npc.getKickCooldown() <= 0.0f)
            {
                env.ball->possess(&npc, env);
            }
        }

        // D. DEFENSIVE ACTIONS
        if (env.ball->hasOwner() && env.ball->getOwner()->getTeam() != npc.getTeam() && distToBall < 200.f && ctx.canTackle) {
            processDefensiveActions(npc, dt, ctx, teamAI, env);
        }
    }

    // E. ROTATION AND FACING LOGIC
    if (finalDirection.x != 0.f || finalDirection.y != 0.f) {
        float targetLen = std::sqrt(finalDirection.x * finalDirection.x + finalDirection.y * finalDirection.y);
        if (targetLen > 0.001f) finalDirection /= targetLen;

        sf::Vector2f currentFacing = npc.getAimDirection();
        float facingLen = std::sqrt(currentFacing.x * currentFacing.x + currentFacing.y * currentFacing.y);

        if (facingLen > 0.1f && ctx.state == MatchState::InPlay) {
            currentFacing /= facingLen;
            float turnRate = std::clamp((25.0f + ((npc.getAgility() / 100.f) * 45.0f)) * dt, 0.1f, 1.0f);
            finalDirection = PlayerAI::normalize(currentFacing + (finalDirection - currentFacing) * turnRate);
        }

        if (env.ball->getOwner() == &npc) {
            npc.setRotationToward(npcPos + finalDirection);
        }
        else if (teamAI.getCurrentState().phase == MatchPhase::Defending && distToTarget < 400.f) {
            sf::Vector2f vel = npc.getVelocity();
            float currentSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
            sf::Vector2f toBallDir = PlayerAI::normalize(env.ball->getPosition() - npcPos);
            bool isRunningAway = ((finalDirection.x * toBallDir.x + finalDirection.y * toBallDir.y) < -0.3f);

            if (currentSpeed > (npc.getTopSpeed() * 6.0f) && isRunningAway) {
                npc.setRotationToward(npcPos + finalDirection);
            }
            else {
                npc.setRotationToward(env.ball->getPosition());
            }
        }
        else {
            npc.setRotationToward(npcPos + finalDirection);
        }
    }

    // F. APPLY FINAL PHYSICS
    if (npc.getState() != PlayerState::Stunned && npc.getState() != PlayerState::Stumbled) {
        bool isKeeperBall = (env.ball->hasOwner() && env.ball->getOwner()->getPositionRole() == PositionRole::Goalkeeper);
        applyMovementPhysics(npc, finalDirection, isSprinting, dt, distToTarget, effectiveResponder, isKeeperBall, ctx, env);
    }
}

void NPCController::processDefensiveActions(NPCPlayer& npc, float dt, const TacticalContext& ctx, const TeamAI& teamAI, MatchEnvironment& env)
{
    Player* attacker = env.ball->getOwner();
    if (attacker->getPositionRole() == PositionRole::Goalkeeper) return;

    sf::Vector2f npcPos = npc.getPosition();
    float timeScaleNorm = std::clamp(npc.getMatchTimeScale() / 22.5f, 0.4f, 2.5f);
    float effectiveAggression = std::clamp(npc.getAggression() * timeScaleNorm, 1.0f, 99.0f);

    sf::Vector2f toAttacker = attacker->getPosition() - npcPos;
    sf::Vector2f attackerVel = attacker->getVelocity();
    float attackerSpeed = std::sqrt(attackerVel.x * attackerVel.x + attackerVel.y * attackerVel.y);
    sf::Vector2f attackerDir = (attackerSpeed > 10.f) ? (attackerVel / attackerSpeed) : sf::Vector2f(0.f, 0.f);

    sf::Vector2f toNpc = PlayerAI::normalize(npcPos - attacker->getPosition());
    float refDot = (attackerDir.x * toNpc.x) + (attackerDir.y * toNpc.y);

    bool isTackleFromBehind = (refDot < -0.85f);
    bool isTackleFromFront = (refDot > 0.4f);
    bool isHomeSide = (npc.getTeam() == Team::Home);

    sf::Vector2f myGoalPos = isHomeSide ? sf::Vector2f(env.pitch->margin, 3500.f) : sf::Vector2f(env.pitch->totalWidth - env.pitch->margin, 3500.f);
    bool inOwnBox = (std::abs(npcPos.x - myGoalPos.x) < 1650.f && std::abs(npcPos.y - myGoalPos.y) < 2050.f);
    bool inOpponentHalf = isHomeSide ? (npcPos.x > env.pitch->totalWidth / 2.f) : (npcPos.x < env.pitch->totalWidth / 2.f);

    float ballExposedDist = PlayerAI::dist(attacker->getPosition(), env.ball->getPosition());
    float requiredExposureOppHalf = std::clamp(40.f / timeScaleNorm, 15.f, 80.f);
    float requiredExposureOpen = std::clamp(25.f / timeScaleNorm, 10.f, 50.f);
    float requiredExposureOwnBox = std::clamp(15.f / timeScaleNorm, 5.f, 40.f);

    if (isTackleFromFront) {
        requiredExposureOppHalf *= 2.0f;
        requiredExposureOpen *= 2.5f;
        requiredExposureOwnBox *= 2.5f;
    }
    else if (isTackleFromBehind) {
        requiredExposureOppHalf *= 0.4f;
        requiredExposureOpen *= 0.4f;
        requiredExposureOwnBox *= 0.4f;
    }

    bool safeToTackle = true;
    if (inOpponentHalf) {
        if (isTackleFromBehind) safeToTackle = false;
        else if (ballExposedDist < requiredExposureOppHalf) {
            float aggThreshold = isTackleFromFront ? 10.f : 45.f;
            if (effectiveAggression < aggThreshold) safeToTackle = false;
        }
        else safeToTackle = false;
    }
    else if (inOwnBox) {
        if (isTackleFromBehind) {
            if ((rand() % 100) > (effectiveAggression * 0.2f)) safeToTackle = false;
        }
        else if (ballExposedDist < requiredExposureOwnBox) {
            float aggThreshold = isTackleFromFront ? 2.5f : 10.f;
            if (effectiveAggression < aggThreshold) safeToTackle = false;
        }
        else safeToTackle = false;
    }
    else {
        if (isTackleFromBehind) {
            if ((rand() % 100) > (effectiveAggression * 0.4f)) safeToTackle = false;
        }
        else if (ballExposedDist < requiredExposureOpen) {
            float aggThreshold = isTackleFromFront ? 5.f : 25.f;
            if (effectiveAggression < aggThreshold) safeToTackle = false;
        }
        else safeToTackle = false;
    }

    if (npc.getYellowCards() > 0) {
        float yellowFear = std::clamp(85.f / timeScaleNorm, 40.f, 95.f);
        if (isTackleFromBehind || (rand() % 100) < (yellowFear - (effectiveAggression * 0.2f))) safeToTackle = false;
    }

    if (npc.getBargeCooldown() <= 0.0f) {
        float distToAttacker = std::sqrt(toAttacker.x * toAttacker.x + toAttacker.y * toAttacker.y);
        if (distToAttacker < 150.f) {
            sf::Vector2f npcVel = npc.getVelocity();
            float npcSpeed = std::sqrt(npcVel.x * npcVel.x + npcVel.y * npcVel.y);
            bool shoulderToShoulder = false;

            if (npcSpeed > 100.f && attackerSpeed > 100.f) {
                sf::Vector2f nDir = npcVel / npcSpeed;
                if ((nDir.x * attackerDir.x + nDir.y * attackerDir.y) > 0.5f) shoulderToShoulder = true;
            }

            if (shoulderToShoulder || distToAttacker < 80.f || (attackerSpeed > 250.f && !isTackleFromBehind)) {
                float bargeChance = effectiveAggression;
                PositionRole role = npc.getPositionRole();
                bool isDefenderOrMid = (role == PositionRole::CenterBack || role == PositionRole::LeftBack ||
                    role == PositionRole::RightBack || role == PositionRole::LeftWingBack ||
                    role == PositionRole::RightWingBack || role == PositionRole::DefensiveMid ||
                    role == PositionRole::CenterMid);

                if (isDefenderOrMid) bargeChance += 40.0f;
                if (shoulderToShoulder) bargeChance += 30.0f;

                if ((rand() % 100) < bargeChance) {
                    npc.executeShoulderBarge(attacker);
                }
            }
        }
    }

    float triggerDist = isTackleFromFront ? 180.f : 140.f;
    if (safeToTackle && PlayerAI::dist(npcPos, env.ball->getPosition()) < triggerDist) {
        sf::Vector2f futureBallPos = env.ball->getPosition() + (env.ball->getVelocity() * 0.24f);
        npc.startTackle(PlayerAI::normalize(futureBallPos - npcPos));
    }
}

void NPCController::applyMovementPhysics(NPCPlayer& npc, sf::Vector2f directionInput, bool isSprinting,
    float dt, float distToTarget, Player* firstResponder,
    bool keeperBall, TacticalContext ctx, MatchEnvironment& env)
{
    if (npc.getCurrentStamina() < 2.0f) {
        isSprinting = false;
    }
    npc.updateStamina(dt, isSprinting);

    if (npc.getState() == PlayerState::Tackling) {
        PhysicsEngine::applySlideTackleFriction(npc, dt);
        return;
    }

    sf::Vector2f vel = npc.getVelocity();
    sf::Vector2f npcPos = npc.getPosition();
    float sprintSpeed = npc.getTopSpeed() * 10.f;
    float maxSpeed = isSprinting ? sprintSpeed : sprintSpeed * 0.5f;

    maxSpeed = std::min(maxSpeed, ctx.maxSpeedLimit);

    float slowingRadius = 150.f;
    float stopRadius = 30.f;

    float dx = npcPos.x - env.ball->getPosition().x;
    float dy = npcPos.y - env.ball->getPosition().y;
    float distToBall = std::sqrt(dx * dx + dy * dy);

    bool hasBall = (env.ball->getOwner() == &npc);
    bool isChasingBall = !hasBall && !keeperBall && ctx.ballInfluence > 0.0f &&
        (!env.ball->hasOwner() || env.ball->getOwner()->getTeam() != npc.getTeam()) &&
        (&npc == firstResponder || (distToTarget < 450.f && distToBall < 300.f && npc.getState() != PlayerState::Tackling));

    if (hasBall) {
        maxSpeed = std::min(sprintSpeed, ctx.maxSpeedLimit);
        slowingRadius = 0.f;
        stopRadius = 10.f;

        if (distToTarget < slowingRadius && (directionInput.x != 0.f || directionInput.y != 0.f)) {
            float ramp = (distToTarget - stopRadius) / (slowingRadius - stopRadius);
            ramp = std::max(0.4f, std::min(ramp, 1.f));
            maxSpeed *= ramp;
        }
    }
    else if (isChasingBall) {
        maxSpeed = std::min(sprintSpeed, ctx.maxSpeedLimit);

        if (distToBall > 10.f) {
            sf::Vector2f ballDir = { dx / distToBall, dy / distToBall };
            ballDir = -ballDir;

            PhysicsEngine::applyTangentialVelocityDamping(npc, ballDir, 8.0f, dt);

            if (distToBall < 120.f) {
                directionInput = ballDir;
            }
            else {
                bool isTeammatePass = (!env.ball->hasOwner() && env.ball->getLastOwner() && env.ball->getLastOwner()->getTeam() == npc.getTeam());
                bool isTargetReceiver = (isTeammatePass && &npc == firstResponder);

                if (!isTargetReceiver) {
                    float pullStrength = 0.75f * ctx.ballInfluence;
                    directionInput = PlayerAI::normalize((directionInput * (1.0f - pullStrength)) + (ballDir * pullStrength));
                }
            }
        }
    }
    else {
        if (!isSprinting) {
            float aggressionFactor = npc.getAggression() / 100.0f;
            float jockeyClamp = 0.60f + (aggressionFactor * 0.30f);
            maxSpeed *= jockeyClamp;
        }

        if (ctx.state == MatchState::InPlay && !ctx.canPossess) {
            slowingRadius = 50.f;
            stopRadius = 10.f;
        }

        if (distToTarget < slowingRadius && (directionInput.x != 0.f || directionInput.y != 0.f)) {
            float ramp = (distToTarget - stopRadius) / (slowingRadius - stopRadius);
            ramp = std::max(0.f, std::min(ramp, 1.f));
            maxSpeed *= ramp;
        }
    }

    if (directionInput.x != 0.f || directionInput.y != 0.f) {
        if (isChasingBall) maxSpeed *= 1.1f;
        PhysicsEngine::applyPlayerLocomotion(npc, directionInput, maxSpeed, dt);
    }
    else {
        PhysicsEngine::applyPlayerIdleFriction(npc, dt);
    }

    PhysicsEngine::resolvePlayerPitchBoundaries(npc, *(env.pitch));

    vel = npc.getVelocity();
    float currentSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
    if (currentSpeed > maxSpeed && currentSpeed > 0.1f) {
        npc.setVelocity((vel / currentSpeed) * maxSpeed);
    }
}
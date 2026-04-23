#include "NPCController.h"
#include "Ball.h"
#include "UserPlayer.h"
#include "Pitch.h"
#include "MatchContext.h"
#include "MatchReferee.h"
#include "PhysicsEngine.h"
#include "AimAssist.h"
#include "PlayerAI.h"
#include "TeamAI.h"
#include "PossessionAI.h"
#include "PositioningAI.h"
#include "GoalkeeperAI.h"
#include "SpatialGrid.h"
#include "MatchStatistics.h"
#include <cmath>

NPCController::NPCController() {}
NPCController::~NPCController() {}

void NPCController::update(NPCPlayer& npc, UserPlayer* user, Ball& ball,
    const std::vector<Player*>& team, const std::vector<Player*>& opposition,
    const Pitch& pitch, TeamState teamState, float dt, Player* firstResponder,
    const MatchReferee& referee, const TeamAI& teamAI, SoundManager& soundManager, const SpatialGrid& spatialGrid, MatchStatistics& stats)
{
    npc.updateCooldown(dt);
    PhysicsEngine::updatePlayerAirPhysics(npc, dt);

    if (npc.getState() == PlayerState::Injured) {
        PhysicsEngine::applyPlayerIdleFriction(npc, dt);
        return;
    }

    bool isTaker = (&npc == referee.getSetPieceTaker());
    TacticalContext ctx = referee.getTacticalContext(npc.getTeam(), isTaker);
    PositioningMask mask = referee.getPositioningMask(&npc, pitch);

    if (ctx.state == MatchState::HalfTime || ctx.state == MatchState::FullTime || ctx.state == MatchState::GoalScored)
    {
        npc.updateStamina(dt, false);
        return;
    }
    else if (ctx.state != MatchState::InPlay)
    {
        npc.updateStamina(dt, false);

        if (isTaker) {
            if (!referee.isWhistleBlown()) {
                npc.setVelocity({ 0.f, 0.f });
                npc.setRotationToward(ball.getPosition());

                // Reset their pass timer while waiting for the whistle
                npc.m_passTimer = 0.0f;
            }
            else if (npc.getKickCooldown() <= 0.0f) {
                bool isHome = (npc.getTeam() == Team::Home);
                sf::Vector2f goalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);

                if (ctx.state == MatchState::ThrowIn) {
                    PossessionAI::executeThrowIn(npc, ball, team);
                    return;
                }

                // ==========================================
                // --- THE FIX 1: SET PIECE PATIENCE ---
                // ==========================================
                // Don't instantly run and boot the ball. Wait 1 to 2 seconds for players to establish their tactical positions!
                npc.m_passTimer += dt;

                // High awareness players wait slightly less. Max wait is ~2.5s.
                float waitTime = 1.0f + ((100.f - npc.getAwareness()) / 100.f * 1.5f);

                // Kick-offs can be slightly faster since players are already mostly set
                if (ctx.state == MatchState::KickOff) waitTime *= 0.5f;

                if (npc.m_passTimer < waitTime && ctx.state != MatchState::Penalty) {
                    npc.setVelocity({ 0.f, 0.f });

                    // Look towards the attacking half while waiting
                    sf::Vector2f surveyTarget = isHome ? sf::Vector2f(pitch.totalWidth, pitch.totalHeight / 2.f) : sf::Vector2f(0.f, pitch.totalHeight / 2.f);
                    npc.setRotationToward(surveyTarget);
                    return;
                }

                // ==========================================
                // --- THE RUN-UP ---
                // ==========================================
                sf::Vector2f ballPos = ball.getPosition();
                sf::Vector2f npcPos = npc.getPosition();
                float distToBall = PlayerAI::dist(npcPos, ballPos);

                sf::Vector2f moveDir = PlayerAI::normalize(ballPos - npcPos);
                if (moveDir.x == 0.f && moveDir.y == 0.f) moveDir = sf::Vector2f(1.f, 0.f);

                float sprintSpeed = npc.getTopSpeed() * 8.f; // Slightly controlled run-up, not a dead sprint
                PhysicsEngine::applyPlayerLocomotion(npc, moveDir, sprintSpeed, dt);
                npc.setRotationToward(ballPos);

                // EXECUTE KICK
                if (distToBall < 60.f) {
                    bool isShooting = false;
                    float distToGoal = PlayerAI::dist(ballPos, goalPos);

                    if (ctx.state == MatchState::Penalty) {
                        isShooting = true;
                    }
                    // Only attempt a shot from a free kick if we are actually close to the goal! (< 28 meters)
                    else if (ctx.state == MatchState::FreeKick && distToGoal < 2800.f && npc.getPlaystyle().behavior.shootBias >= 0.3f) {
                        isShooting = true;
                    }

                    if (ctx.state == MatchState::GoalKick) {
                        GoalkeeperAI::distributeBallAsGoalie(npc, ball, team, opposition, pitch, teamAI, soundManager);
                    }
                    else if (isShooting) {
                        PossessionAI::executeShot(npc, ball, goalPos, opposition, pitch, dt, soundManager, stats);
                    }
                    else {
                        Player* passTarget = PossessionAI::findBestPassOption(npc, team, opposition, user, teamAI, pitch);

                        // ==========================================
                        // --- THE FIX 2: NO MOON-BALL FALLBACK ---
                        // ==========================================
                        if (!passTarget) {
                            float maxScore = -99999.f;
                            for (Player* tm : team) {
                                if (tm == &npc || tm->isSentOff() || tm->getState() == PlayerState::Injured) continue;

                                float d = PlayerAI::dist(npcPos, tm->getPosition());
                                float score = -d; // Base: prefer closer players for a safe dump-off

                                // If it's a corner, massively reward aiming into the penalty box mixer
                                if (ctx.state == MatchState::Corner) {
                                    bool inBox = isHome ? (tm->getPosition().x > pitch.totalWidth - 1650.f) : (tm->getPosition().x < 1650.f);
                                    if (inBox) score += 10000.f;
                                }
                                // If it's a KickOff, prioritize knocking it backward to the midfield
                                else if (ctx.state == MatchState::KickOff) {
                                    bool isBackward = isHome ? (tm->getPosition().x < npcPos.x) : (tm->getPosition().x > npcPos.x);
                                    if (isBackward) score += 5000.f;
                                }

                                if (score > maxScore) {
                                    maxScore = score;
                                    passTarget = tm;
                                }
                            }
                        }

                        if (passTarget) {
                            PossessionAI::executePass(npc, ball, passTarget, opposition, pitch, soundManager, stats);
                        }
                        else {
                            // Absolute last resort (e.g. your entire team is magically injured)
                            PossessionAI::executeShot(npc, ball, goalPos, opposition, pitch, dt, soundManager, stats);
                        }
                    }

                    npc.setVelocity({ 0.f, 0.f });
                    npc.resetKickCooldown();
                    npc.m_passTimer = 0.0f;
                }
            }
            return;
        }
    }

    if (ball.z > 80.f)
    {
        PossessionAI::handleNPCJumpLogic(npc, ball);

        bool isHome = (npc.getTeam() == Team::Home);
        sf::Vector2f oppGoalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);
        float distToGoal = PlayerAI::dist(npc.getPosition(), oppGoalPos);

        bool isShot = false;
        sf::Vector2f aimDir;

        if (distToGoal < 2000.f) {
            isShot = true;
            aimDir = PlayerAI::normalize(oppGoalPos - npc.getPosition());
        }
        else {
            Player* aerialPassTarget = PossessionAI::findBestPassOption(npc, team, opposition, user, teamAI, pitch);

            if (aerialPassTarget) {
                isShot = false;
                aimDir = PlayerAI::normalize(aerialPassTarget->getPosition() - npc.getPosition());
            }
            else {
                isShot = false;
                aimDir = isHome ? sf::Vector2f(1.f, 0.f) : sf::Vector2f(-1.f, 0.f);
            }
        }

        if (PossessionAI::tryNPCAerialStrike(npc, ball, aimDir, isShot, soundManager)) {
            npc.deductStaminaAction(1.5f);
            return;
        }
    }

    if (npc.getPositionRole() == PositionRole::Goalkeeper)
    {
        GoalkeeperAI::handleGoalkeeping(npc, ball, pitch, team, opposition, dt, teamAI, soundManager);
    }
    else
    {
        sf::Vector2f npcPos = npc.getPosition();
        sf::Vector2f finalDirection(0.f, 0.f);
        bool isSprinting = false;
        float distToTarget = 0.f;

        if (ball.getOwner() == &npc) {
            finalDirection = PossessionAI::handlePossession(npc, ball, team, opposition, user, pitch, dt, ctx.state, teamAI, soundManager, stats);
            distToTarget = 500.f;
            isSprinting = true;
        }
        else {
            bool isTeammatePass = (!ball.hasOwner() && ball.getLastOwner() && ball.getLastOwner()->getTeam() == npc.getTeam());
            Player* effectiveResponder = firstResponder;
            bool amITargetReceiver = false;

            if (isTeammatePass) {
                Player* targetReceiver = PositioningAI::identifyTargetReceiver(ball, team, pitch);
                if (targetReceiver) {
                    effectiveResponder = targetReceiver;
                    if (targetReceiver == &npc) amITargetReceiver = true;
                }
            }

            sf::Vector2f targetPos = PositioningAI::decideTargetPosition(npc, ball, pitch, teamState, team, opposition, effectiveResponder, mask, teamAI, spatialGrid);

            sf::Vector2f toTarget = targetPos - npcPos;
            distToTarget = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
            sf::Vector2f separation = PositioningAI::calculateSeparation(npc, team, opposition, ball.getPosition(), teamAI);

            sf::Vector2f normTarget = (distToTarget > 0.1f) ? (toTarget / distToTarget) : sf::Vector2f(0.f, 0.f);
            finalDirection = PlayerAI::normalize(normTarget + (separation * 0.8f));

            AIUrgency urgency = (teamState == TeamState::Defending) ? AIUrgency::Recovery : AIUrgency::AttackingRun;
            if (effectiveResponder == &npc) urgency = AIUrgency::Critical;

            isSprinting = PositioningAI::evaluateSprintUrgency(npc, urgency, distToTarget, PlayerAI::dist(npcPos, ball.getPosition()));

            if (amITargetReceiver) isSprinting = true;
            if (npc.getBallPossession()) isSprinting = true;

            float distToBall = PlayerAI::dist(ball.getPosition(), npc.getPosition());

            if (!ball.hasOwner() && distToBall < 70.f && ctx.canPossess) {
                if (npc.getState() != PlayerState::Tackling &&
                    npc.getState() != PlayerState::Stunned &&
                    npc.getState() != PlayerState::Stumbled &&
                    npc.getState() != PlayerState::FallOver &&
                    ball.z < 40.f &&
                    npc.getKickCooldown() <= 0.0f)
                {
                    ball.possess(&npc);
                }
            }

            if (ball.hasOwner() && ball.getOwner()->getTeam() != npc.getTeam() && distToBall < 200.f && ctx.canTackle) {
                Player* attacker = ball.getOwner();

                if (attacker->getPositionRole() != PositionRole::Goalkeeper) {
                    float awareness = npc.getAwareness();
                    float baseAggression = npc.getAggression();
                    bool safeToTackle = true;

                    float timeScaleNorm = std::clamp(npc.getMatchTimeScale() / 22.5f, 0.4f, 2.5f);
                    float effectiveAggression = std::clamp(baseAggression * timeScaleNorm, 1.0f, 99.0f);

                    sf::Vector2f toAttacker = attacker->getPosition() - npcPos;
                    sf::Vector2f attackerVel = attacker->getVelocity();
                    float attackerSpeed = std::sqrt(attackerVel.x * attackerVel.x + attackerVel.y * attackerVel.y);

                    sf::Vector2f attackerDir = (attackerSpeed > 10.f) ? (attackerVel / attackerSpeed) : sf::Vector2f(0.f, 0.f);
                    sf::Vector2f npcDir = PlayerAI::normalize(toAttacker);

                    float approachAngle = (npcDir.x * attackerDir.x) + (npcDir.y * attackerDir.y);
                    bool trailingBehind = (approachAngle > 0.4f);
                    bool headOn = (approachAngle < -0.3f);

                    bool isHomeSide = (npc.getTeam() == Team::Home);
                    sf::Vector2f myGoalPos = isHomeSide ? sf::Vector2f(pitch.margin, 3500.f) : sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f);
                    float dxToGoal = std::abs(npcPos.x - myGoalPos.x);
                    float dyToGoal = std::abs(npcPos.y - myGoalPos.y);

                    bool inOwnBox = (dxToGoal < 1650.f && dyToGoal < 2050.f);

                    float pitchHalfX = pitch.totalWidth / 2.f;
                    bool inOpponentHalf = isHomeSide ? (npcPos.x > pitchHalfX) : (npcPos.x < pitchHalfX);

                    bool onYellow = (npc.getYellowCards() > 0);
                    float ballExposedDist = PlayerAI::dist(attacker->getPosition(), ball.getPosition());

                    float requiredExposureOppHalf = std::clamp(60.f / timeScaleNorm, 20.f, 100.f);
                    float requiredExposureOwnBox = std::clamp(70.f / timeScaleNorm, 30.f, 110.f);
                    float requiredExposureOpen = std::clamp(45.f / timeScaleNorm, 15.f, 80.f);

                    if (inOpponentHalf) {
                        if (!headOn || ballExposedDist < requiredExposureOppHalf || effectiveAggression < 55.f) {
                            safeToTackle = false;
                        }
                        else if ((rand() % 100) > (effectiveAggression * 0.7f)) {
                            safeToTackle = false;
                        }
                    }
                    else if (inOwnBox) {
                        if (trailingBehind || ballExposedDist < requiredExposureOwnBox || effectiveAggression < 70.f) {
                            safeToTackle = false;
                        }
                    }
                    else {
                        if (trailingBehind) {
                            float effectiveAwareness = std::clamp(awareness / timeScaleNorm, 1.0f, 99.0f);
                            if ((rand() % 100) < effectiveAwareness) safeToTackle = false;
                        }
                        else if (ballExposedDist < requiredExposureOpen) {
                            if ((rand() % 100) > effectiveAggression) {
                                safeToTackle = false;
                            }
                        }
                    }

                    if (onYellow) {
                        float yellowFear = std::clamp(75.f / timeScaleNorm, 30.f, 95.f);
                        if (trailingBehind || (rand() % 100) < yellowFear) safeToTackle = false;
                    }

                    bool barged = false;
                    if (npc.getBargeCooldown() <= 0.0f) {
                        float distToAttacker = std::sqrt(toAttacker.x * toAttacker.x + toAttacker.y * toAttacker.y);

                        if (distToAttacker < 130.f) {
                            sf::Vector2f npcVel = npc.getVelocity();
                            float npcSpeed = std::sqrt(npcVel.x * npcVel.x + npcVel.y * npcVel.y);

                            bool shoulderToShoulder = false;
                            if (npcSpeed > 100.f && attackerSpeed > 100.f) {
                                sf::Vector2f nDir = npcVel / npcSpeed;
                                float parallelDot = (nDir.x * attackerDir.x) + (nDir.y * attackerDir.y);
                                if (parallelDot > 0.5f) shoulderToShoulder = true;
                            }

                            if (shoulderToShoulder || distToAttacker < 70.f || (attackerSpeed > 250.f && !trailingBehind)) {
                                if ((rand() % 100) < effectiveAggression) {
                                    npc.executeShoulderBarge(attacker);
                                    barged = true;
                                }
                            }
                        }
                    }

                    if (safeToTackle && distToBall < 140.f) {
                        sf::Vector2f futureBallPos = ball.getPosition() + (ball.getVelocity() * 0.24f);
                        npc.startTackle(PlayerAI::normalize(futureBallPos - npcPos));
                    }
                }
            }
        }

        if (finalDirection.x != 0.f || finalDirection.y != 0.f) {

            float targetLen = std::sqrt(finalDirection.x * finalDirection.x + finalDirection.y * finalDirection.y);
            if (targetLen > 0.001f) finalDirection /= targetLen;

            sf::Vector2f currentFacing = npc.getAimDirection();
            float facingLen = std::sqrt(currentFacing.x * currentFacing.x + currentFacing.y * currentFacing.y);

            if (facingLen > 0.1f && ctx.state == MatchState::InPlay) {
                currentFacing /= facingLen;
                float agilityNorm = npc.getAgility() / 100.f;
                float turnRate = (25.0f + (agilityNorm * 45.0f)) * dt;
                turnRate = std::clamp(turnRate, 0.1f, 1.0f);

                finalDirection = currentFacing + (finalDirection - currentFacing) * turnRate;

                float blendLen = std::sqrt(finalDirection.x * finalDirection.x + finalDirection.y * finalDirection.y);
                if (blendLen > 0.001f) finalDirection /= blendLen;
            }

            if (ball.getOwner() == &npc) {
                npc.setRotationToward(npcPos + finalDirection);
            }
            else if (teamState == TeamState::Defending && distToTarget < 400.f) {
                sf::Vector2f vel = npc.getVelocity();
                float currentSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y);

                sf::Vector2f toBall = ball.getPosition() - npcPos;
                sf::Vector2f toBallDir = PlayerAI::normalize(toBall);

                float runAngle = (finalDirection.x * toBallDir.x) + (finalDirection.y * toBallDir.y);
                bool isRunningAway = (runAngle < -0.3f);

                if (currentSpeed > (npc.getTopSpeed() * 6.0f) && isRunningAway) {
                    npc.setRotationToward(npcPos + finalDirection);
                }
                else {
                    npc.setRotationToward(ball.getPosition());
                }
            }
            else {
                npc.setRotationToward(npcPos + finalDirection);
            }
        }

        if (npc.getState() != PlayerState::Stunned && npc.getState() != PlayerState::Stumbled) {
            bool isKeeperBall = (ball.hasOwner() && ball.getOwner()->getPositionRole() == PositionRole::Goalkeeper);
            applyMovementPhysics(npc, finalDirection, isSprinting, dt, distToTarget, ball, firstResponder, pitch, isKeeperBall, ctx);
        }
    }

    if (ctx.state == MatchState::InPlay) {
        if (std::sqrt(npc.getVelocity().x * npc.getVelocity().x + npc.getVelocity().y * npc.getVelocity().y) < 2.f)
        {
            if (ball.getOwner() != &npc) npc.setRotationToward(ball.getPosition());
        }
    }
}

void NPCController::applyMovementPhysics(NPCPlayer& npc, sf::Vector2f directionInput, bool isSprinting,
    float dt, float distToTarget, Ball& ball, Player* firstResponder,
    const Pitch& pitch, bool keeperBall, TacticalContext ctx)
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

    float dx = npcPos.x - ball.getPosition().x;
    float dy = npcPos.y - ball.getPosition().y;
    float distToBall = std::sqrt(dx * dx + dy * dy);

    bool hasBall = (ball.getOwner() == &npc);
    bool isChasingBall = !hasBall && !keeperBall && ctx.ballInfluence > 0.0f &&
        (!ball.hasOwner() || ball.getOwner()->getTeam() != npc.getTeam()) &&
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

            // Stiffer damping so they don't over-run the trajectory
            PhysicsEngine::applyTangentialVelocityDamping(npc, ballDir, 8.0f, dt);

            // ==========================================
            // --- THE FIX 5: THE FINAL STEP ---
            // ==========================================
            if (distToBall < 120.f) {
                // If they are within 1.2m, completely abandon the tactical curve and STEP ON IT
                directionInput = ballDir;
            }
            else {
                float pullStrength = 0.75f * ctx.ballInfluence;
                directionInput = PlayerAI::normalize((directionInput * (1.0f - pullStrength)) + (ballDir * pullStrength));
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

    PhysicsEngine::resolvePlayerPitchBoundaries(npc, pitch);

    vel = npc.getVelocity();
    float currentSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
    if (currentSpeed > maxSpeed && currentSpeed > 0.1f) {
        npc.setVelocity((vel / currentSpeed) * maxSpeed);
    }
}
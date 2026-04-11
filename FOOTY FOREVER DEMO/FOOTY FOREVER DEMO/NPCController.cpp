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
#include <cmath>

NPCController::NPCController() {}
NPCController::~NPCController() {}

void NPCController::update(NPCPlayer& npc, UserPlayer& user, Ball& ball,
    const std::vector<Player*> team, const std::vector<Player*> opposition,
    const Pitch& pitch, TeamState teamState, float dt, Player* firstResponder,
    const MatchReferee& referee, const TeamAI& teamAI)
{
    npc.updateCooldown(dt);
    PhysicsEngine::updatePlayerAirPhysics(npc, dt);

    bool isTaker = (&npc == referee.getSetPieceTaker());
    TacticalContext ctx = referee.getTacticalContext(npc.getTeam(), isTaker);
    PositioningMask mask = referee.getPositioningMask(&npc, pitch);

    // ==========================================
    // 1. MATCH PAUSED / CELEBRATION
    // ==========================================
    if (ctx.state == MatchState::HalfTime || ctx.state == MatchState::FullTime || ctx.state == MatchState::GoalScored)
    {
        npc.updateStamina(dt, false);
        return;
    }

    // ==========================================
     // 2. DEAD BALL STATE (Set Pieces)
     // ==========================================
    else if (ctx.state != MatchState::InPlay)
    {
        npc.updateStamina(dt, false);

        if (isTaker) {
            npc.setVelocity({ 0.f, 0.f });

            if (referee.isWhistleBlown() && npc.getKickCooldown() <= 0.0f) {

                bool isHome = (npc.getTeam() == Team::Home);
                sf::Vector2f goalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);

                if (ctx.state == MatchState::ThrowIn) {
                    PlayerAI::executeThrowIn(npc, ball, team);
                }
                else if (ctx.state == MatchState::GoalKick) {
                    PlayerAI::distributeBallAsGoalie(npc, ball, team, opposition, pitch, teamAI);
                }
                else if (ctx.state == MatchState::Penalty) {
                    // FIX: Actually shoot at the goal, not (0,0)!
                    PlayerAI::executeShot(npc, ball, goalPos, opposition, pitch, dt);
                }
                else if (ctx.state == MatchState::KickOff || ctx.state == MatchState::Corner) {
                    // --- STRICT PASS ENFORCEMENT ---
                    Player* bestTarget = PlayerAI::findBestPassOption(npc, team, opposition, user, teamAI, pitch);

                    // Fallback: If the strict scoring system rejected everyone (e.g. because 
                    // the taker is facing the wrong way at kickoff), force a pass to the nearest teammate.
                    if (!bestTarget) {
                        float minDist = 99999.f;
                        std::vector<Player*> receivers;
                        for (auto& t : team) if (t != &npc) receivers.push_back(t);
                        if (npc.getTeam() == user.getTeam()) receivers.push_back(&user);

                        for (Player* tm : receivers) {
                            float d = PlayerAI::dist(npc.getPosition(), tm->getPosition());
                            if (d < minDist) {
                                minDist = d;
                                bestTarget = tm;
                            }
                        }
                    }

                    if (bestTarget) PlayerAI::executePass(npc, ball, bestTarget, opposition);
                }
                else if (ctx.state == MatchState::FreeKick) {
                    // Free kicks can be shots or passes depending on distance
                    float distToGoal = PlayerAI::dist(npc.getPosition(), goalPos);

                    // If we are within 25 meters, there is a 50% chance to just rip a shot!
                    if (distToGoal < 2500.f && (rand() % 100) < 50) {
                        PlayerAI::executeShot(npc, ball, goalPos, opposition, pitch, dt);
                    }
                    else {
                        Player* bestTarget = PlayerAI::findBestPassOption(npc, team, opposition, user, teamAI, pitch);
                        if (bestTarget) PlayerAI::executePass(npc, ball, bestTarget, opposition);
                        else PlayerAI::executeShot(npc, ball, goalPos, opposition, pitch, dt);
                    }
                }
            }

            // Set piece taker rotation fixes
            if (ctx.state == MatchState::Corner) {
                // Face the penalty box
                sf::Vector2f center(pitch.totalWidth / 2.f, pitch.totalHeight / 2.f);
                npc.setRotationToward(center);
            }
            else {
                // Face the opponent's goal
                sf::Vector2f oppGoal = (npc.getTeam() == Team::Home) ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);
                npc.setRotationToward(oppGoal);
            }
            return;
        }
    }

    // ==========================================
    // 3. AERIAL LOGIC (Headers / Volleys)
    // ==========================================
    if (ball.z > 40.f) {
        PlayerAI::handleNPCJumpLogic(npc, ball);

        bool isHome = (npc.getTeam() == Team::Home);
        sf::Vector2f oppGoalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);

        if (PlayerAI::tryNPCAerialStrike(npc, ball, PlayerAI::normalize(oppGoalPos - npc.getPosition()), true)) {
            npc.deductStaminaAction(1.5f);
            return;
        }
    }

    // ==========================================
    // 4. ROLE ROUTING (GK vs Outfield)
    // ==========================================
    if (npc.getPositionRole() == PositionRole::Goalkeeper) {
        PlayerAI::handleGoalkeeping(npc, ball, pitch, team, opposition, dt, teamAI);
    }
    else {
        sf::Vector2f npcPos = npc.getPosition();
        sf::Vector2f finalDirection(0.f, 0.f);
        bool isSprinting = false;
        float distToTarget = 0.f;

        if (ball.getOwner() == &npc) {
            // DECISION: Handle Possession logic and return dribble direction
            finalDirection = PlayerAI::handlePossession(npc, ball, team, opposition, user, pitch, dt, ctx.state,teamAI);
            distToTarget = 500.f;
            isSprinting = true; // AI usually sprints on ball unless specified otherwise
        }
        else {
            // --- OFF THE BALL ---
            // FIX: We now pass the 'team' vector into decideTargetPosition so the Spatial Engine can use it!
            sf::Vector2f targetPos = PlayerAI::decideTargetPosition(npc, ball, pitch, teamState, team, opposition, firstResponder, mask, teamAI);

            sf::Vector2f toTarget = targetPos - npcPos;
            distToTarget = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
            sf::Vector2f separation = PlayerAI::calculateSeparation(npc, team, opposition, ball.getPosition(), teamAI);

            sf::Vector2f normTarget = (distToTarget > 0.1f) ? (toTarget / distToTarget) : sf::Vector2f(0.f, 0.f);
            finalDirection = PlayerAI::normalize(normTarget + (separation * 0.8f));

            // SPRINT EVALUATION
            // Dynamically assign urgency based on the current phase of play
            AIUrgency urgency = (teamState == TeamState::Defending) ? AIUrgency::Recovery : AIUrgency::AttackingRun;
            if (firstResponder == &npc) urgency = AIUrgency::Critical;

            isSprinting = PlayerAI::evaluateSprintUrgency(npc, urgency, distToTarget, PlayerAI::dist(npcPos, ball.getPosition()));

            float distToBall = PlayerAI::dist(ball.getPosition(), npc.getPosition());

            // ==========================================
            // --- RESTORED: AUTO-POSSESS THE BALL ---
            // ==========================================
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

            // ==========================================
                        // --- RESTORED: TACKLE TRIGGER ---
                        // ==========================================
            if (ball.hasOwner() && ball.getOwner()->getTeam() != npc.getTeam() && distToBall < 250.f && ctx.canTackle) {
                Player* attacker = ball.getOwner();

                if (attacker->getPositionRole() != PositionRole::Goalkeeper) {
                    float awareness = npc.getAwareness();
                    bool safeToTackle = true;

                    // 1. CALCULATE ANGLES & POSITION
                    sf::Vector2f toAttacker = attacker->getPosition() - npcPos;
                    sf::Vector2f attackerVel = attacker->getVelocity();
                    float attackerSpeed = std::sqrt(attackerVel.x * attackerVel.x + attackerVel.y * attackerVel.y);

                    sf::Vector2f attackerDir = (attackerSpeed > 10.f) ? (attackerVel / attackerSpeed) : sf::Vector2f(0.f, 0.f);
                    sf::Vector2f npcDir = PlayerAI::normalize(toAttacker);

                    // Dot product: > 0.4f means we are chasing them from behind.
                    float approachAngle = (npcDir.x * attackerDir.x) + (npcDir.y * attackerDir.y);
                    bool trailingBehind = (approachAngle > 0.4f);

                    // 2. ARE WE IN THE PENALTY BOX?
                    sf::Vector2f myGoalPos = (npc.getTeam() == Team::Home) ? sf::Vector2f(pitch.margin, 3500.f) : sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f);
                    float dxToGoal = std::abs(npcPos.x - myGoalPos.x);
                    float dyToGoal = std::abs(npcPos.y - myGoalPos.y);
                    bool inOwnBox = (dxToGoal < 1650.f && dyToGoal < 2050.f);

                    bool onYellow = (npc.getYellowCards() > 0);

                    // ==========================================
                    // --- SMART FOUL AVOIDANCE ---
                    // ==========================================
                    if (inOwnBox) {
                        // THE "100% CERTAIN" RULE:
                        // To tackle in the box, ALL of these must be true:
                        // 1. Not chasing from behind.
                        // 2. The ball must be VERY exposed (striker took a heavy touch > 70px away from their feet).
                        // 3. Defender must have high tackling stats (> 70) to ensure a clean win.
                        float ballExposedDist = PlayerAI::dist(attacker->getPosition(), ball.getPosition());

                        if (trailingBehind || ballExposedDist < 70.f || (npc.getAggression() < 70.f)) {
                            safeToTackle = false;
                        }
                    }
                    else {
                        // OUTSIDE THE BOX: Standard foul avoidance
                        if (trailingBehind) {
                            // Dumb players might still dive in from behind. Smart players hold off.
                            if ((rand() % 100) < awareness) safeToTackle = false;
                        }

                        if (onYellow) {
                            // Players on a yellow are terrified of getting a red.
                            // They will NEVER tackle from behind, and hesitate 60% of the time head-on.
                            if (trailingBehind || (rand() % 100) < 60) safeToTackle = false;
                        }
                    }

                    // ==========================================
                    // --- EXECUTE TACKLE ---
                    // ==========================================
                    // Outside the box, they can dive in from 120px away. 
                    // Inside the box, the safeToTackle logic above already requires the ball to be loose!
                    if (safeToTackle && distToBall < 120.f) {
                        sf::Vector2f futureBallPos = ball.getPosition() + (ball.getVelocity() * 0.24f);
                        npc.startTackle(PlayerAI::normalize(futureBallPos - npcPos));
                    }
                }
            }
        }

        // PHYSICAL EXECUTION
        if (npc.getState() != PlayerState::Stunned && npc.getState() != PlayerState::Stumbled) {
            bool isKeeperBall = (ball.hasOwner() && ball.getOwner()->getPositionRole() == PositionRole::Goalkeeper);
            applyMovementPhysics(npc, finalDirection, isSprinting, dt, distToTarget, ball, firstResponder, pitch, isKeeperBall, ctx);
        }
    }

    // Final rotation facing
    if (std::sqrt(npc.getVelocity().x * npc.getVelocity().x + npc.getVelocity().y * npc.getVelocity().y) < 2.f) {
        if (ball.getOwner() != &npc) npc.setRotationToward(ball.getPosition());
    }
}

void NPCController::applyMovementPhysics(NPCPlayer& npc, sf::Vector2f directionInput, bool isSprinting,
    float dt, float distToTarget, Ball& ball, Player* firstResponder,
    const Pitch& pitch, bool keeperBall, TacticalContext ctx)
{
    // ==========================================
    // 1. STAMINA EXHAUSTION
    // ==========================================
    if (npc.getCurrentStamina() < 2.0f) {
        isSprinting = false; // Dead on their feet, cancel AI sprint!
    }
    npc.updateStamina(dt, isSprinting);

    // Slide tackles take over the physics completely
    if (npc.getState() == PlayerState::Tackling) {
        PhysicsEngine::applySlideTackleFriction(npc, dt);
        return;
    }

    sf::Vector2f vel = npc.getVelocity();
    sf::Vector2f npcPos = npc.getPosition();
    float sprintSpeed = npc.getTopSpeed() * 10.f;
    float maxSpeed = isSprinting ? sprintSpeed : sprintSpeed * 0.5f;

    // Context Speed Limit (e.g., walking during half-time)
    maxSpeed = std::min(maxSpeed, ctx.maxSpeedLimit);

    // Dribbling Speed Penalty
    if (npc.getBallPossession()) {
        maxSpeed *= 0.7f + ((npc.getBallControl() / 100.f) * 0.25f);
    }

    // ==========================================
    // 2. STEERING & ARRIVAL LOGIC
    // ==========================================
    float slowingRadius = 300.f;
    float stopRadius = 30.f; // <--- CRITICAL FIX: Lets them actually arrive at their target!

    float dx = npcPos.x - ball.getPosition().x;
    float dy = npcPos.y - ball.getPosition().y;
    float distToBall = std::sqrt(dx * dx + dy * dy);

    bool isChasingBall = !keeperBall && ctx.ballInfluence > 0.0f &&
        (!ball.hasOwner() || ball.getOwner()->getTeam() != npc.getTeam()) &&
        (&npc == firstResponder || (distToTarget < 450.f && distToBall < 300.f && npc.getState() != PlayerState::Tackling));

    if (isChasingBall) {
        maxSpeed = std::min(sprintSpeed, ctx.maxSpeedLimit);

        // --- THE "HONE IN" LOGIC (Ball Magnetism) ---
        if (distToBall > 10.f) {
            sf::Vector2f ballDir = { dx / distToBall, dy / distToBall }; // Direction TO player
            ballDir = -ballDir; // Direction TO ball

            // Ask the Physics Engine to kill orbital drift so they run straight at it!
            PhysicsEngine::applyTangentialVelocityDamping(npc, ballDir, 6.0f, dt);

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

        // Tactical slowdown bypass (Ignore braking during live play if we can't possess)
        if (ctx.state == MatchState::InPlay && !ctx.canPossess) {
            slowingRadius = 50.f;
            stopRadius = 10.f;
        }

        // Arrival Easing: Smoothly hit the brakes as they approach the target!
        if (distToTarget < slowingRadius && (directionInput.x != 0.f || directionInput.y != 0.f)) {
            float ramp = (distToTarget - stopRadius) / (slowingRadius - stopRadius);
            ramp = std::max(0.f, std::min(ramp, 1.f));
            maxSpeed *= ramp;
        }
    }

    // ==========================================
    // 3. PHYSICAL EXECUTION
    // ==========================================
    if (directionInput.x != 0.f || directionInput.y != 0.f) {
        // If chasing the ball, give them a slight artificial speed boost as an AI assist
        if (isChasingBall) maxSpeed *= 1.1f;
        PhysicsEngine::applyPlayerLocomotion(npc, directionInput, maxSpeed, dt);
    }
    else {
        PhysicsEngine::applyPlayerIdleFriction(npc, dt);
    }

    // Pitch Boundaries & Final Speed Cap
    PhysicsEngine::resolvePlayerPitchBoundaries(npc, pitch);

    vel = npc.getVelocity();
    float currentSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
    if (currentSpeed > maxSpeed && currentSpeed > 0.1f) {
        npc.setVelocity((vel / currentSpeed) * maxSpeed);
    }
}
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

    // ==========================================
    // --- THE HARD LOCK: INJURED PLAYERS ---
    // ==========================================
    if (npc.getState() == PlayerState::Injured) {
        // Bleed off any remaining momentum from the tackle
        PhysicsEngine::applyPlayerIdleFriction(npc, dt);

        // Return immediately! They do not process match states, 
        // set pieces, or tactical movement ever again.
        return;
    }

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
            if (!referee.isWhistleBlown()) {
                npc.setVelocity({ 0.f, 0.f });

                // Pre-whistle facing
                if (ctx.state == MatchState::Corner) {
                    sf::Vector2f center(pitch.totalWidth / 2.f, pitch.totalHeight / 2.f);
                    npc.setRotationToward(center);
                }
                else if (ctx.state == MatchState::KickOff) {
                    // THE FIX: Face our OWN goal to pass backwards!
                    sf::Vector2f myGoal = (npc.getTeam() == Team::Home) ? sf::Vector2f(pitch.margin, 3500.f) : sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f);
                    npc.setRotationToward(myGoal);
                }
                else {
                    sf::Vector2f oppGoal = (npc.getTeam() == Team::Home) ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);
                    npc.setRotationToward(oppGoal);
                }
            }
            else if (npc.getKickCooldown() <= 0.0f) {

                bool isHome = (npc.getTeam() == Team::Home);
                sf::Vector2f goalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);

                if (ctx.state == MatchState::ThrowIn) {
                    PossessionAI::executeThrowIn(npc, ball, team);
                    return;
                }

                // ==========================================
                // --- THE AI SET PIECE RUN-UP ---
                // ==========================================
                sf::Vector2f ballPos = ball.getPosition();
                sf::Vector2f npcPos = npc.getPosition();

                // 1. STABLE DECISION MAKING
                sf::Vector2f kickDir;
                sf::Vector2f approachDir;
                bool isShooting = false;
                Player* passTarget = nullptr;

                if (ctx.state == MatchState::Penalty) {
                    kickDir = PlayerAI::normalize(goalPos - ballPos);
                    approachDir = kickDir;
                    isShooting = true;
                }
                else if (ctx.state == MatchState::FreeKick && PlayerAI::dist(ballPos, goalPos) < 2500.f && npc.getPlaystyle().behavior.shootBias >= 0.4f) {
                    kickDir = PlayerAI::normalize(goalPos - ballPos);
                    approachDir = kickDir;
                    isShooting = true;
                }
                else if (ctx.state == MatchState::GoalKick) {
                    kickDir = isHome ? sf::Vector2f(1.f, 0.f) : sf::Vector2f(-1.f, 0.f);
                    approachDir = kickDir;
                }
                else if (ctx.state == MatchState::KickOff) {
                    passTarget = PossessionAI::findBestPassOption(npc, team, opposition, user, teamAI, pitch);
                    if (!passTarget) {
                        float minDist = 99999.f;
                        std::vector<Player*> receivers;
                        for (auto& t : team) if (t != &npc) receivers.push_back(t);
                        if (user && npc.getTeam() == user->getTeam()) receivers.push_back(user);

                        for (Player* tm : receivers) {
                            float d = PlayerAI::dist(npcPos, tm->getPosition());
                            if (d < minDist) { minDist = d; passTarget = tm; }
                        }
                    }

                    // THE FIX: If they pass backwards, kickDir points backward. 
                    // By setting approachDir = kickDir, they will ALWAYS stand in the opponent's 
                    // half facing their own team!
                    if (passTarget) kickDir = PlayerAI::normalize(passTarget->getPosition() - ballPos);
                    else kickDir = isHome ? sf::Vector2f(-1.f, 0.f) : sf::Vector2f(1.f, 0.f);

                    approachDir = kickDir;
                }
                else {
                    passTarget = PossessionAI::findBestPassOption(npc, team, opposition, user, teamAI, pitch);
                    if (passTarget) kickDir = PlayerAI::normalize(passTarget->getPosition() - ballPos);
                    else kickDir = PlayerAI::normalize(goalPos - ballPos);
                    approachDir = kickDir;
                }

                // 2. DYNAMIC RUN-UP DISTANCES
                float runUpLength = 250.f;
                if (ctx.state == MatchState::KickOff) runUpLength = 80.f;
                else if (ctx.state == MatchState::GoalKick) runUpLength = 150.f;

                sf::Vector2f runUpSpot = ballPos - (approachDir * runUpLength);

                runUpSpot.x = std::clamp(runUpSpot.x, pitch.margin + 30.f, pitch.totalWidth - pitch.margin - 30.f);
                runUpSpot.y = std::clamp(runUpSpot.y, pitch.margin + 30.f, pitch.totalHeight - pitch.margin - 30.f);

                float distToRunUp = PlayerAI::dist(npcPos, runUpSpot);
                float distToBall = PlayerAI::dist(npcPos, ballPos);

                // ==========================================
                // 3. EXECUTE THE MOVEMENT PHASES (THE HYSTERESIS LOCK)
                // ==========================================
                bool isWalkingBack = false;
                float dotVel = (npc.getVelocity().x * approachDir.x) + (npc.getVelocity().y * approachDir.y);
                float speed = std::sqrt(npc.getVelocity().x * npc.getVelocity().x + npc.getVelocity().y * npc.getVelocity().y);

                // 1. Are we essentially at the run-up spot? Lock into SPRINT!
                if (distToRunUp < 35.f) {
                    isWalkingBack = false;
                }
                // 2. Are we too close to the ball (just tackled/fumbled it)? Lock into WALK BACK!
                else if (distToBall < 35.f) {
                    isWalkingBack = true;
                }
                // 3. Are we actively moving? TRUST OUR MOMENTUM!
                else if (speed > 20.f) {
                    // If our velocity against the approach direction is heavily negative, we are retreating.
                    // If it is positive, we are already sprinting to strike it!
                    if (dotVel < -10.f) isWalkingBack = true;
                    else isWalkingBack = false;
                }
                // 4. We are standing completely still (Just spawned in by the Referee)
                else {
                    // Look at where we are. If we haven't reached the run-up spot yet, go to it!
                    if (distToRunUp > 40.f) isWalkingBack = true;
                    else isWalkingBack = false;
                }

                if (isWalkingBack) {
                    // PHASE 1: Walk backward to the Run-Up spot
                    sf::Vector2f moveDir = PlayerAI::normalize(runUpSpot - npcPos);
                    float walkSpeed = npc.getTopSpeed() * 5.0f;

                    PhysicsEngine::applyPlayerLocomotion(npc, moveDir, walkSpeed, dt);
                    npc.setRotationToward(runUpSpot);
                }
                else {
                    // PHASE 2: Sprint at the ball!
                    sf::Vector2f moveDir = PlayerAI::normalize(ballPos - npcPos);
                    float sprintSpeed = npc.getTopSpeed() * 10.f;

                    PhysicsEngine::applyPlayerLocomotion(npc, moveDir, sprintSpeed, dt);

                    if (ctx.state == MatchState::KickOff) {
                        // Keep eyes locked forward on Kickoff
                        sf::Vector2f myGoal = isHome ? sf::Vector2f(0.f, pitch.totalHeight / 2.f) : sf::Vector2f(pitch.totalWidth, pitch.totalHeight / 2.f);
                        npc.setRotationToward(myGoal);
                    }
                    else {
                        npc.setRotationToward(ballPos);
                    }

                    // PHASE 3: The Strike! (Within 1 meter)
                    if (distToBall < 100.f) {

                        if (ctx.state == MatchState::GoalKick) {
                            GoalkeeperAI::distributeBallAsGoalie(npc, ball, team, opposition, pitch, teamAI, soundManager);
                        }
                        else if (isShooting) {
                            PossessionAI::executeShot(npc, ball, goalPos, opposition, pitch, dt, soundManager, stats);
                        }
                        else if (passTarget) {
                            PossessionAI::executePass(npc, ball, passTarget, opposition, soundManager, stats);
                        }
                        else {
                            PossessionAI::executeShot(npc, ball, goalPos, opposition, pitch, dt, soundManager, stats); // Fallback
                        }

                        npc.setVelocity({ 0.f, 0.f });
                        npc.resetKickCooldown();
                    }
                }
            }
            return; // Taker is finished processing
        }
    }

    // ==========================================
    // 3. AERIAL LOGIC (Headers / Volleys)
    // ==========================================
    if (ball.z > 80.f)
    {
        PossessionAI::handleNPCJumpLogic(npc, ball);

        bool isHome = (npc.getTeam() == Team::Home);
        sf::Vector2f oppGoalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);
        float distToGoal = PlayerAI::dist(npc.getPosition(), oppGoalPos);

        // THE FIX: Decide if this aerial strike should be a SHOT or a PASS
        bool isShot = false;
        sf::Vector2f aimDir;

        // If we are within 20 meters of the goal, attack the net!
        if (distToGoal < 2000.f) {
            isShot = true;
            aimDir = PlayerAI::normalize(oppGoalPos - npc.getPosition());
        }
        else {
            // We are outside the box. Look for a teammate to pass to!
            Player* aerialPassTarget = PossessionAI::findBestPassOption(npc, team, opposition, user, teamAI, pitch);

            if (aerialPassTarget) {
                isShot = false; // It's a pass!
                aimDir = PlayerAI::normalize(aerialPassTarget->getPosition() - npc.getPosition());
            }
            else {
                // No pass target, just head it forward to clear it
                isShot = false;
                aimDir = isHome ? sf::Vector2f(1.f, 0.f) : sf::Vector2f(-1.f, 0.f);
            }
        }

        if (PossessionAI::tryNPCAerialStrike(npc, ball, aimDir, isShot, soundManager)) {
            npc.deductStaminaAction(1.5f);
            return;
        }
    }

    // ==========================================
    // 4. ROLE ROUTING (GK vs Outfield)
    // ==========================================
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
            // DECISION: Handle Possession logic and return dribble direction
            finalDirection = PossessionAI::handlePossession(npc, ball, team, opposition, user, pitch, dt, ctx.state, teamAI, soundManager, stats);
            distToTarget = 500.f;
            isSprinting = true; // AI usually sprints on ball unless specified otherwise
        }
        else {
            // --- OFF THE BALL ---

            // ==========================================
            // --- THE TELEPATHIC RECEIVER OVERRIDE ---
            // ==========================================
            bool isTeammatePass = (!ball.hasOwner() && ball.getLastOwner() && ball.getLastOwner()->getTeam() == npc.getTeam());
            Player* effectiveResponder = firstResponder;
            bool amITargetReceiver = false;

            if (isTeammatePass) {
                Player* targetReceiver = PositioningAI::identifyTargetReceiver(ball, team);
                if (targetReceiver) {
                    effectiveResponder = targetReceiver; // Override the game's default first responder!
                    if (targetReceiver == &npc) amITargetReceiver = true;
                }
            }

            // Pass the explicitly corrected 'effectiveResponder' into the movement decisions
            sf::Vector2f targetPos = PositioningAI::decideTargetPosition(npc, ball, pitch, teamState, team, opposition, effectiveResponder, mask, teamAI, spatialGrid);

            sf::Vector2f toTarget = targetPos - npcPos;
            distToTarget = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
            sf::Vector2f separation = PositioningAI::calculateSeparation(npc, team, opposition, ball.getPosition(), teamAI);

            sf::Vector2f normTarget = (distToTarget > 0.1f) ? (toTarget / distToTarget) : sf::Vector2f(0.f, 0.f);
            finalDirection = PlayerAI::normalize(normTarget + (separation * 0.8f));

            // SPRINT EVALUATION
            AIUrgency urgency = (teamState == TeamState::Defending) ? AIUrgency::Recovery : AIUrgency::AttackingRun;
            if (effectiveResponder == &npc) urgency = AIUrgency::Critical;

            isSprinting = PositioningAI::evaluateSprintUrgency(npc, urgency, distToTarget, PlayerAI::dist(npcPos, ball.getPosition()));

            // THE LOCK: If I am the target receiver, I MUST sprint to the ball!
            if (amITargetReceiver) isSprinting = true;
            if (npc.getBallPossession()) isSprinting = true;

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
            // --- RESTORED & NERFED: TACKLE TRIGGER ---
            // ==========================================
            // Reduced scan radius so they don't lock onto tackles too early
            if (ball.hasOwner() && ball.getOwner()->getTeam() != npc.getTeam() && distToBall < 200.f && ctx.canTackle) {
                Player* attacker = ball.getOwner();

                if (attacker->getPositionRole() != PositionRole::Goalkeeper) {
                    float awareness = npc.getAwareness();
                    float baseAggression = npc.getAggression();
                    bool safeToTackle = true;

                    // ==========================================
                    // --- NEW: TIME-SCALED AGGRESSION ---
                    // ==========================================
                    // Shorter matches = Higher scale = More frantic tackling
                    // Longer matches = Lower scale = Patient, tactical jockeying
                    float timeScaleNorm = std::clamp(npc.getMatchTimeScale() / 22.5f, 0.4f, 2.5f);

                    // Boost or nerf the player's core aggression stat based on the match length
                    float effectiveAggression = std::clamp(baseAggression * timeScaleNorm, 1.0f, 99.0f);

                    // 1. CALCULATE ANGLES & POSITION
                    sf::Vector2f toAttacker = attacker->getPosition() - npcPos;
                    sf::Vector2f attackerVel = attacker->getVelocity();
                    float attackerSpeed = std::sqrt(attackerVel.x * attackerVel.x + attackerVel.y * attackerVel.y);

                    sf::Vector2f attackerDir = (attackerSpeed > 10.f) ? (attackerVel / attackerSpeed) : sf::Vector2f(0.f, 0.f);
                    sf::Vector2f npcDir = PlayerAI::normalize(toAttacker);

                    // Dot product: > 0.4f means we are chasing them from behind. < -0.3f means Head-On!
                    float approachAngle = (npcDir.x * attackerDir.x) + (npcDir.y * attackerDir.y);
                    bool trailingBehind = (approachAngle > 0.4f);
                    bool headOn = (approachAngle < -0.3f);

                    // 2. DETERMINE PITCH ZONES
                    bool isHomeSide = (npc.getTeam() == Team::Home);
                    sf::Vector2f myGoalPos = isHomeSide ? sf::Vector2f(pitch.margin, 3500.f) : sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f);
                    float dxToGoal = std::abs(npcPos.x - myGoalPos.x);
                    float dyToGoal = std::abs(npcPos.y - myGoalPos.y);

                    bool inOwnBox = (dxToGoal < 1650.f && dyToGoal < 2050.f);

                    // Are we in the opponent's half?
                    float pitchHalfX = pitch.totalWidth / 2.f;
                    bool inOpponentHalf = isHomeSide ? (npcPos.x > pitchHalfX) : (npcPos.x < pitchHalfX);

                    bool onYellow = (npc.getYellowCards() > 0);
                    float ballExposedDist = PlayerAI::dist(attacker->getPosition(), ball.getPosition());

                    // ==========================================
                    // --- DYNAMIC EXPOSURE REQUIREMENTS ---
                    // ==========================================
                    // Fast games: They will dive in if the ball is barely off the foot.
                    // Slow games: They demand a massive, clumsy touch to risk a tackle.
                    float requiredExposureOppHalf = std::clamp(60.f / timeScaleNorm, 20.f, 100.f);
                    float requiredExposureOwnBox = std::clamp(70.f / timeScaleNorm, 30.f, 110.f);
                    float requiredExposureOpen = std::clamp(45.f / timeScaleNorm, 15.f, 80.f);

                    // ==========================================
                    // --- SMART FOUL AVOIDANCE & PATIENCE ---
                    // ==========================================
                    if (inOpponentHalf) {
                        if (!headOn || ballExposedDist < requiredExposureOppHalf || effectiveAggression < 55.f) {
                            safeToTackle = false;
                        }
                        else if ((rand() % 100) > (effectiveAggression * 0.7f)) {
                            safeToTackle = false;
                        }
                    }
                    else if (inOwnBox) {
                        // PENALTY BOX: Extreme caution. No tackling from behind, need an exposed ball.
                        if (trailingBehind || ballExposedDist < requiredExposureOwnBox || effectiveAggression < 70.f) {
                            safeToTackle = false;
                        }
                    }
                    else {
                        // OWN HALF (Open Play): Tactical Defending
                        if (trailingBehind) {
                            // Smart players know trailing tackles are useless fouls.
                            // In fast games, we lower their effective awareness so they risk it more often!
                            float effectiveAwareness = std::clamp(awareness / timeScaleNorm, 1.0f, 99.0f);
                            if ((rand() % 100) < effectiveAwareness) safeToTackle = false;
                        }
                        else if (ballExposedDist < requiredExposureOpen) {
                            // THE PATIENCE FIX: Glued to feet.
                            if ((rand() % 100) > effectiveAggression) {
                                safeToTackle = false;
                            }
                        }
                    }

                    // Yellow card overrides everything with pure terror
                    if (onYellow) {
                        // Even in frantic games, they don't want a red card, but fear is reduced
                        float yellowFear = std::clamp(75.f / timeScaleNorm, 30.f, 95.f);
                        if (trailingBehind || (rand() % 100) < yellowFear) safeToTackle = false;
                    }

                    // ==========================================
                    // --- NEW: EXECUTE SHOULDER BARGE ---
                    // ==========================================
                    bool barged = false;
                    if (npc.getBargeCooldown() <= 0.0f) {
                        float distToAttacker = std::sqrt(toAttacker.x * toAttacker.x + toAttacker.y * toAttacker.y);

                        // Must be within physical contact distance
                        if (distToAttacker < 130.f) {
                            sf::Vector2f npcVel = npc.getVelocity();
                            float npcSpeed = std::sqrt(npcVel.x * npcVel.x + npcVel.y * npcVel.y);

                            bool shoulderToShoulder = false;
                            if (npcSpeed > 100.f && attackerSpeed > 100.f) {
                                sf::Vector2f nDir = npcVel / npcSpeed;
                                float parallelDot = (nDir.x * attackerDir.x) + (nDir.y * attackerDir.y);
                                if (parallelDot > 0.5f) shoulderToShoulder = true; // Running the same direction
                            }

                            // "Slam the door": If running shoulder-to-shoulder, OR grinding up close, 
                            // OR a fast attacker is trying to squeeze past us head-on!
                            if (shoulderToShoulder || distToAttacker < 70.f || (attackerSpeed > 250.f && !trailingBehind)) {

                                // Aggression determines if they risk the physical contact
                                if ((rand() % 100) < effectiveAggression) {
                                    npc.executeShoulderBarge(attacker);
                                    barged = true;
                                }
                            }
                        }
                    }

                    // ==========================================
                    // --- EXECUTE TACKLE ---
                    // ==========================================
                    if (safeToTackle && distToBall < 140.f) {
                        sf::Vector2f futureBallPos = ball.getPosition() + (ball.getVelocity() * 0.24f);
                        npc.startTackle(PlayerAI::normalize(futureBallPos - npcPos));
                    }
                }
            }
        }

        // ==========================================
        // --- THE FIX: AI BODY ROTATION SYNC ---
        // ==========================================
        if (finalDirection.x != 0.f || finalDirection.y != 0.f) {
            if (ball.getOwner() == &npc) {
                // Dribblers ALWAYS face their movement direction
                npc.setRotationToward(npcPos + finalDirection);
            }
            else if (teamState == TeamState::Defending && distToTarget < 400.f) {
                // THE FIX 1: DYNAMIC JOCKEYING vs SPRINTING
                // If the defender is moving slowly, they face the ball (Jockeying).
                // If they are sprinting backwards to catch a runner, they MUST turn their body to run!
                sf::Vector2f vel = npc.getVelocity();
                float currentSpeed = std::sqrt(vel.x * vel.x + vel.y * vel.y);

                sf::Vector2f toBall = ball.getPosition() - npcPos;
                sf::Vector2f toBallDir = PlayerAI::normalize(toBall);

                // Are they running away from the ball?
                float runAngle = (finalDirection.x * toBallDir.x) + (finalDirection.y * toBallDir.y);
                bool isRunningAway = (runAngle < -0.3f);

                // If sprinting away from the ball, face the run direction! Otherwise, jockey!
                if (currentSpeed > (npc.getTopSpeed() * 6.0f) && isRunningAway) {
                    npc.setRotationToward(npcPos + finalDirection);
                }
                else {
                    npc.setRotationToward(ball.getPosition());
                }
            }
            else {
                // Off-ball runners face their run direction
                npc.setRotationToward(npcPos + finalDirection);
            }
        }

        // PHYSICAL EXECUTION
        if (npc.getState() != PlayerState::Stunned && npc.getState() != PlayerState::Stumbled) {
            bool isKeeperBall = (ball.hasOwner() && ball.getOwner()->getPositionRole() == PositionRole::Goalkeeper);
            applyMovementPhysics(npc, finalDirection, isSprinting, dt, distToTarget, ball, firstResponder, pitch, isKeeperBall, ctx);
        }
    }

    // Final rotation facing (Standing still only, and ONLY during open play!)
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

    // ==========================================
    // 2. STEERING & ARRIVAL LOGIC
    // ==========================================
    float slowingRadius = 150.f;
    float stopRadius = 30.f;

    float dx = npcPos.x - ball.getPosition().x;
    float dy = npcPos.y - ball.getPosition().y;
    float distToBall = std::sqrt(dx * dx + dy * dy);

    // --- THE FIX: Isolate the Dribbler! ---
    bool hasBall = (ball.getOwner() == &npc);
    bool isChasingBall = !hasBall && !keeperBall && ctx.ballInfluence > 0.0f &&
        (!ball.hasOwner() || ball.getOwner()->getTeam() != npc.getTeam()) &&
        (&npc == firstResponder || (distToTarget < 450.f && distToBall < 300.f && npc.getState() != PlayerState::Tackling));

    if (hasBall) {
        // --- ON THE BALL (The Open Highway) ---
        maxSpeed = std::min(sprintSpeed, ctx.maxSpeedLimit);

        // Drastically reduce braking so they blast through open space
        slowingRadius = 0.f;
        stopRadius = 10.f;

        // Arrival Easing (Nerfed for dribblers so they don't stutter)
        if (distToTarget < slowingRadius && (directionInput.x != 0.f || directionInput.y != 0.f)) {
            float ramp = (distToTarget - stopRadius) / (slowingRadius - stopRadius);
            ramp = std::max(0.4f, std::min(ramp, 1.f)); // Never drop below 70% speed while dribbling
            maxSpeed *= ramp;
        }
    }
    else if (isChasingBall) {
        // --- CHASING LOOSE BALL / INTERCEPTING ---
        maxSpeed = std::min(sprintSpeed, ctx.maxSpeedLimit);

        // The "Hone In" Logic (Ball Magnetism)
        if (distToBall > 10.f) {
            sf::Vector2f ballDir = { dx / distToBall, dy / distToBall };
            ballDir = -ballDir;

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
        // --- OFF THE BALL: PRESSING / JOCKEYING / TACTICAL ---

        // THE FIX 1: Don't nerf the speed of players making attacking runs!
        if (!isSprinting) {
            float aggressionFactor = npc.getAggression() / 100.0f;
            // The Jockey Clamp: Only apply this to defenders/off-ball players who are jogging
            float jockeyClamp = 0.60f + (aggressionFactor * 0.30f);
            maxSpeed *= jockeyClamp;
        }

        // Tactical slowdown bypass (Ignore braking during live play if we can't possess)
        if (ctx.state == MatchState::InPlay && !ctx.canPossess) {
            slowingRadius = 50.f;
            stopRadius = 10.f;
        }

        // Standard Arrival Easing
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
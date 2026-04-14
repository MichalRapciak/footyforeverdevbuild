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

void NPCController::update(NPCPlayer& npc, UserPlayer* user, Ball& ball,
    const std::vector<Player*>& team, const std::vector<Player*>& opposition,
    const Pitch& pitch, TeamState teamState, float dt, Player* firstResponder,
    const MatchReferee& referee, const TeamAI& teamAI, SoundManager& soundManager)
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
                    PlayerAI::executeThrowIn(npc, ball, team);
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
                    passTarget = PlayerAI::findBestPassOption(npc, team, opposition, user, teamAI, pitch);
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
                    passTarget = PlayerAI::findBestPassOption(npc, team, opposition, user, teamAI, pitch);
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

                // 1. Are we extremely close to the run-up spot? Lock into Sprint!
                if (distToRunUp < 30.f) {
                    isWalkingBack = false;
                }
                // 2. Are we extremely close to the ball? Lock into Walk!
                else if (distToBall < 30.f) {
                    isWalkingBack = true;
                }
                // 3. We are in the middle of the run-up. Use physical momentum to decide!
                else {
                    // If we are actively moving backwards (or hit a boundary wall and stopped), keep walking back.
                    // If we have forward momentum, keep sprinting!
                    if (dotVel < -5.f) isWalkingBack = true;
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
                            PlayerAI::distributeBallAsGoalie(npc, ball, team, opposition, pitch, teamAI, soundManager);
                        }
                        else if (isShooting) {
                            PlayerAI::executeShot(npc, ball, goalPos, opposition, pitch, dt, soundManager);
                        }
                        else if (passTarget) {
                            PlayerAI::executePass(npc, ball, passTarget, opposition, soundManager);
                        }
                        else {
                            PlayerAI::executeShot(npc, ball, goalPos, opposition, pitch, dt, soundManager); // Fallback
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
        PlayerAI::handleNPCJumpLogic(npc, ball);

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
            Player* aerialPassTarget = PlayerAI::findBestPassOption(npc, team, opposition, user, teamAI, pitch);

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

        if (PlayerAI::tryNPCAerialStrike(npc, ball, aimDir, isShot, soundManager)) {
            npc.deductStaminaAction(1.5f);
            return;
        }
    }

    // ==========================================
    // 4. ROLE ROUTING (GK vs Outfield)
    // ==========================================
    if (npc.getPositionRole() == PositionRole::Goalkeeper)
    {
        PlayerAI::handleGoalkeeping(npc, ball, pitch, team, opposition, dt, teamAI, soundManager);
    }
    else
    {
        sf::Vector2f npcPos = npc.getPosition();
        sf::Vector2f finalDirection(0.f, 0.f);
        bool isSprinting = false;
        float distToTarget = 0.f;

        if (ball.getOwner() == &npc) {
            // DECISION: Handle Possession logic and return dribble direction
            finalDirection = PlayerAI::handlePossession(npc, ball, team, opposition, user, pitch, dt, ctx.state, teamAI, soundManager);
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
                Player* targetReceiver = PlayerAI::identifyTargetReceiver(ball, team);
                if (targetReceiver) {
                    effectiveResponder = targetReceiver; // Override the game's default first responder!
                    if (targetReceiver == &npc) amITargetReceiver = true;
                }
            }

            // Pass the explicitly corrected 'effectiveResponder' into the movement decisions
            sf::Vector2f targetPos = PlayerAI::decideTargetPosition(npc, ball, pitch, teamState, team, opposition, effectiveResponder, mask, teamAI);

            sf::Vector2f toTarget = targetPos - npcPos;
            distToTarget = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
            sf::Vector2f separation = PlayerAI::calculateSeparation(npc, team, opposition, ball.getPosition(), teamAI);

            sf::Vector2f normTarget = (distToTarget > 0.1f) ? (toTarget / distToTarget) : sf::Vector2f(0.f, 0.f);
            finalDirection = PlayerAI::normalize(normTarget + (separation * 0.8f));

            // SPRINT EVALUATION
            AIUrgency urgency = (teamState == TeamState::Defending) ? AIUrgency::Recovery : AIUrgency::AttackingRun;
            if (effectiveResponder == &npc) urgency = AIUrgency::Critical;

            isSprinting = PlayerAI::evaluateSprintUrgency(npc, urgency, distToTarget, PlayerAI::dist(npcPos, ball.getPosition()));

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
                    float aggression = npc.getAggression();
                    bool safeToTackle = true;

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
                    // --- SMART FOUL AVOIDANCE & PATIENCE ---
                    // ==========================================
                    if (inOpponentHalf) {
                        // SEVERE NERF: In the opponent's half, they just jockey and hold the team shape.
                        // To tackle here, the situation must be an absolute gift.
                        // 1. Must be face-to-face (no side/behind tackles)
                        // 2. Attacker must have taken a terrible heavy touch (> 80px exposed)
                        // 3. AI must have high aggression to even care.
                        if (!headOn || ballExposedDist < 60.f || aggression < 55.f) {
                            safeToTackle = false;
                        }
                        // Even if it's perfect, add a hesitation roll so they don't constantly bite
                        else if ((rand() % 100) > (aggression * 0.7f)) {
                            safeToTackle = false;
                        }
                    }
                    else if (inOwnBox) {
                        // PENALTY BOX: Extreme caution. No tackling from behind, need an exposed ball.
                        if (trailingBehind || ballExposedDist < 70.f || aggression < 70.f) {
                            safeToTackle = false;
                        }
                    }
                    else {
                        // OWN HALF (Open Play): Tactical Defending
                        if (trailingBehind) {
                            // Smart players know trailing tackles are useless fouls
                            if ((rand() % 100) < awareness) safeToTackle = false;
                        }
                        else if (ballExposedDist < 45.f) {
                            // THE PATIENCE FIX: If the ball is glued to the attacker's feet, 
                            // they wait for a touch before diving in, unless they are extremely aggressive!
                            if ((rand() % 100) > aggression) {
                                safeToTackle = false;
                            }
                        }
                    }

                    // Yellow card overrides everything with pure terror
                    if (onYellow) {
                        if (trailingBehind || (rand() % 100) < 75) safeToTackle = false;
                    }

                    // ==========================================
                    // --- EXECUTE TACKLE ---
                    // ==========================================
                    // Tightened the physical lunge range from 120px to 90px. 
                    // This forces them to get closer and jockey rather than lunging from miles away!
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
        // Before applying physics, force the AI to look exactly where they want to run.
        // This prevents the PhysicsEngine from thinking they are trying to strafe or backpedal!
        if (finalDirection.x != 0.f || finalDirection.y != 0.f) {
            if (ball.getOwner() == &npc) {
                // Dribblers ALWAYS face their movement direction
                npc.setRotationToward(npcPos + finalDirection);
            }
            else if (teamState == TeamState::Defending && distToTarget < 400.f) {
                // Defenders closing in on a target will jockey (face the ball)
                npc.setRotationToward(ball.getPosition());
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

        // Artificial arcade boost: Make them 15% faster while carrying the ball!
        maxSpeed *= 1.15f;

        // Drastically reduce braking so they blast through open space
        slowingRadius = 0.f;
        stopRadius = 10.f;

        // Arrival Easing (Nerfed for dribblers so they don't stutter)
        if (distToTarget < slowingRadius && (directionInput.x != 0.f || directionInput.y != 0.f)) {
            float ramp = (distToTarget - stopRadius) / (slowingRadius - stopRadius);
            ramp = std::max(0.6f, std::min(ramp, 1.f)); // Never drop below 70% speed while dribbling
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
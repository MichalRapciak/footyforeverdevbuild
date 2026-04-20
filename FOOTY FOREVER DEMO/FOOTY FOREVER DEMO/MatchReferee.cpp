#include "MatchReferee.h"
#include "Player.h"
#include "Ball.h"
#include "Pitch.h"
#include <iostream>
#include "SoundManager.h"
#include "GlobalSettings.h"

void MatchReferee::update(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, float dt, const Goal& homeGoal, const Goal& awayGoal, SoundManager& soundManager, MatchStatistics& stats)
{
    // ==========================================
    // 1. MATCH CLOCK LOGIC
    // ==========================================
    if (m_matchState == MatchState::InPlay || m_matchState == MatchState::GoalScored) {
        m_matchMinute += (dt * m_timeScale) / 60.0f;

        if (m_matchMinute >= 45.0f && m_half == 1 && m_matchState == MatchState::InPlay) {
            m_matchState = MatchState::HalfTime;
            m_whistleTimer = 5.0f;
            updateMatchContexts();
            soundManager.playSound("ref_fulltime", 100.f); // <-- HALF TIME SOUND
            std::cout << "HALF TIME!\n";
        }
        else if (m_matchMinute >= 90.0f && m_half == 2 && m_matchState == MatchState::InPlay) {
            m_matchState = MatchState::FullTime;
            updateMatchContexts();
            soundManager.playSound("ref_fulltime", 100.f); // <-- FULL TIME SOUND
            std::cout << "FULL TIME!\n";
        }
    }

    // ==========================================
    // 2. STATE HANDLING
    // ==========================================
    if (m_matchState == MatchState::InPlay) {
        checkBoundaries(ball, pitch, homeGoal, awayGoal, soundManager, stats); // <-- PASS TO BOUNDARIES
        if (m_matchState != MatchState::InPlay) {
            prepareRestart(m_matchState, ball, pitch, players, soundManager); // <-- PASS TO RESTART
        }
    }
    else if (m_matchState == MatchState::FoulDelay) {
        m_foulDelayTimer -= dt;
        if (m_foulDelayTimer <= 0.f) m_matchState = MatchState::RequestReplay;
    }
    else if (m_matchState == MatchState::GoalScored) {
        m_whistleTimer -= dt;
        if (m_whistleTimer <= 0.f) {
            m_restartPos = sf::Vector2f(pitch.totalWidth / 2.f, 3500.f);
            m_matchState = MatchState::RequestReplay;
        }
    }
    else if (m_matchState == MatchState::OutOfBoundsDelay) {
        m_whistleTimer -= dt;
        if (m_whistleTimer <= 0.f) {
            if ((rand() % 100) < 30) m_matchState = MatchState::RequestReplay;
            else prepareRestart(m_pendingState, ball, pitch, players, soundManager);
        }
    }
    else if (m_matchState == MatchState::HalfTime) {
        m_whistleTimer -= dt;
        if (m_whistleTimer <= 0.f) {
            m_half = 2;
            m_matchMinute = 45.0f;
            m_awardedTo = Team::Away;
            m_restartPos = sf::Vector2f(pitch.totalWidth / 2.f, 3500.f);
            prepareRestart(MatchState::KickOff, ball, pitch, players, soundManager);
        }
    }
    else if (m_matchState == MatchState::FullTime) {
        // Handled by GamePlay
    }
    else {
        // --- ALL DEAD BALL STATES ---
        bool justBlew = (m_whistleTimer > 0.f && m_whistleTimer - dt <= 0.f);
        m_whistleTimer -= dt;

        // Play the whistle sound EXACTLY once when the timer hits zero!
        if (justBlew) {
            soundManager.playSound("ref_whistle", 100.f);
        }

        // 1. Release condition: The run-up finished and the ball was struck!
        if (m_whistleTimer <= 0.f) {
            float speed = std::sqrt(ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y + ball.vz * ball.vz);

            if (ball.getOwner() == nullptr && speed > 15.0f) {
                m_matchState = MatchState::InPlay;
                m_setPieceTaker = nullptr;
                updateMatchContexts();
                return;
            }
        }

        // ==========================================
        // --- THE FIX 1: UNFREEZE THE BALL ---
        // ==========================================
        if (m_whistleTimer > 0.f) {
            // PRE-WHISTLE: Nobody is allowed to move. Ball is locked.
            if (m_setPieceTaker) m_setPieceTaker->setVelocity({ 0.f, 0.f });
            ball.setPosition(m_restartPos);
            ball.setVelocity({ 0.f, 0.f });
            ball.vz = 0.f;
        }
        else {
            // POST-WHISTLE: The TAKER is allowed to do their run-up.
            // ONLY lock the ball to the grass if it hasn't been kicked yet!
            float speed = std::sqrt(ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y + ball.vz * ball.vz);
            if (speed < 15.0f) {
                ball.setPosition(m_restartPos);
                ball.setVelocity({ 0.f, 0.f });
                ball.vz = 0.f;
            }
        }

        // ==========================================
        // 3. ENCROACHMENT FORCEFIELD & WALL
        // ==========================================
        if (m_matchState == MatchState::FreeKick || m_matchState == MatchState::Corner ||
            m_matchState == MatchState::Penalty || m_matchState == MatchState::KickOff ||
            m_matchState == MatchState::ThrowIn)
        {
            sf::Vector2f targetGoal = (m_awardedTo == Team::Home) ? pitch.awayGoalCenter : pitch.homeGoalCenter;
            float distToTargetGoal = std::sqrt(std::pow(m_restartPos.x - targetGoal.x, 2) + std::pow(m_restartPos.y - targetGoal.y, 2));
            bool needsWall = (m_matchState == MatchState::FreeKick && distToTargetGoal < 2500.f);

            int wallCount = 0;
            int maxWallPlayers = (distToTargetGoal < 1800.f) ? 4 : 3;

            sf::Vector2f toGoal = targetGoal - m_restartPos;
            float lenToGoal = std::sqrt(toGoal.x * toGoal.x + toGoal.y * toGoal.y);
            if (lenToGoal > 0.1f) toGoal /= lenToGoal;
            sf::Vector2f wallCenter = m_restartPos + (toGoal * 915.f);
            sf::Vector2f perp(-toGoal.y, toGoal.x);

            for (Player* p : players)
            {
                if (p == m_setPieceTaker || p->getTeam() == m_awardedTo || p->isSentOff() || p->getState() == PlayerState::Injured) continue;

                bool isDefenderOrMid = (p->getPositionRole() != PositionRole::Goalkeeper &&
                    p->getPositionRole() != PositionRole::Striker &&
                    p->getPositionRole() != PositionRole::CenterForward &&
                    p->getPositionRole() != PositionRole::LeftWing &&
                    p->getPositionRole() != PositionRole::RightWing);

                // 1. HARD-LOCK THE WALL
                if (needsWall && wallCount < maxWallPlayers && isDefenderOrMid)
                {
                    float offset = (wallCount - (maxWallPlayers / 2.0f) + 0.5f) * 60.f;
                    sf::Vector2f targetPos = wallCenter + (perp * offset);

                    targetPos.x = std::clamp(targetPos.x, pitch.margin, pitch.totalWidth - pitch.margin);
                    targetPos.y = std::clamp(targetPos.y, pitch.margin, pitch.totalHeight - pitch.margin);

                    p->setPosition(targetPos);
                    p->setVelocity({ 0.f, 0.f });
                    wallCount++;
                    continue;
                }

                // 2. INVISIBLE FORCEFIELD
                if (m_matchState != MatchState::Penalty) {
                    sf::Vector2f pPos = p->getPosition();
                    float dx = pPos.x - m_restartPos.x;
                    float dy = pPos.y - m_restartPos.y;
                    float distSq = dx * dx + dy * dy;

                    // THE FIX 4: Dynamically assign the legal minimum distance!
                    // Free kicks and corners require 9.15 meters (915px). Throw-ins only require 2 meters (200px).
                    float reqDist = (m_matchState == MatchState::ThrowIn) ? 200.f : 915.f;

                    if (distSq < (reqDist * reqDist)) {
                        float dist = std::sqrt(distSq);
                        if (dist > 0.1f) {
                            sf::Vector2f pushDir = { dx / dist, dy / dist };
                            sf::Vector2f targetPos = m_restartPos + (pushDir * reqDist);

                            if (targetPos.x < pitch.margin || targetPos.x > pitch.totalWidth - pitch.margin ||
                                targetPos.y < pitch.margin || targetPos.y > pitch.totalHeight - pitch.margin)
                            {
                                sf::Vector2f center(pitch.totalWidth / 2.f, pitch.totalHeight / 2.f);
                                sf::Vector2f toCenter = center - pPos;
                                float centerLen = std::sqrt(toCenter.x * toCenter.x + toCenter.y * toCenter.y);

                                if (centerLen > 0.1f) {
                                    pushDir += (toCenter / centerLen) * 0.8f;
                                    float newLen = std::sqrt(pushDir.x * pushDir.x + pushDir.y * pushDir.y);
                                    pushDir /= newLen;
                                    targetPos = m_restartPos + (pushDir * reqDist);
                                }
                            }

                            targetPos.x = std::clamp(targetPos.x, pitch.margin, pitch.totalWidth - pitch.margin);
                            targetPos.y = std::clamp(targetPos.y, pitch.margin, pitch.totalHeight - pitch.margin);

                            p->setPosition(targetPos);
                            p->setVelocity({ 0.f, 0.f });
                        }
                    }
                }
            }
        }
    }
}

void MatchReferee::checkBoundaries(Ball& ball, const Pitch& pitch, const Goal& homeGoal, const Goal& awayGoal, SoundManager& soundManager, MatchStatistics& stats)
{
    sf::Vector2f bPos = ball.getPosition();
    sf::Vector2f bVel = ball.getVelocity();

    // THE FIX 3: Use the new lastTouch pointer instead of lastOwner!
    Player* lastTouch = ball.lastTouch;

    // ==========================================
    // 1. SIDELINE CHECK (With Velocity Grace Period)
    // ==========================================
    bool isOutY = (bPos.y < pitch.margin || bPos.y > pitch.totalHeight - pitch.margin);

    // If the ball is over the line, but the velocity is pushing it back INTO the pitch, let it fly!
    bool movingInwardsY = (bPos.y < pitch.margin && bVel.y > 0.f) ||
        (bPos.y > pitch.totalHeight - pitch.margin && bVel.y < 0.f);

    if (isOutY && !movingInwardsY) {
        m_matchState = MatchState::ThrowIn;
        float safeY = (bPos.y < pitch.margin) ? pitch.margin + 25.f : pitch.totalHeight - pitch.margin - 25.f;
        float safeX = std::clamp(bPos.x, pitch.margin + 25.f, pitch.totalWidth - pitch.margin - 25.f);
        m_restartPos = sf::Vector2f(safeX, safeY);

        if (lastTouch != nullptr) m_awardedTo = (lastTouch->getTeam() == Team::Home) ? Team::Away : Team::Home;
        else m_awardedTo = (bPos.x > pitch.halfwayLineX) ? Team::Home : Team::Away;
        return;
    }

    // ==========================================
    // 2. ENDLINE CHECK (With Velocity Grace Period)
    // ==========================================
    bool isOutX = (bPos.x < pitch.margin || bPos.x > pitch.totalWidth - pitch.margin);

    if (isOutX) {
        // ALWAYS check for a goal first!
        if (checkGoalScored(ball, pitch, homeGoal, awayGoal, soundManager, stats)) {
            m_matchState = MatchState::GoalScored;
            m_pendingState = MatchState::KickOff;
            m_whistleTimer = 2.5f;
            return;
        }

        bool movingInwardsX = (bPos.x < pitch.margin && bVel.x > 0.f) ||
            (bPos.x > pitch.totalWidth - pitch.margin && bVel.x < 0.f);

        if (!movingInwardsX) {
            bool isHomeEnd = (bPos.x < pitch.margin);
            const Goal& activeGoal = isHomeEnd ? homeGoal : awayGoal;

            float goalTopY = activeGoal.center.y - 366.f;
            float goalBottomY = activeGoal.center.y + 366.f;

            // Wait for the ball to settle if it's currently rattling around inside the net
            if (bPos.y >= goalTopY - 2.f && bPos.y <= goalBottomY + 2.f && ball.z <= 249.f) {
                return;
            }

            if (lastTouch == nullptr) {
                m_pendingState = MatchState::GoalKick;
                m_awardedTo = isHomeEnd ? Team::Home : Team::Away;
                m_restartPos = isHomeEnd ? sf::Vector2f(pitch.margin + 600.f, 3500.f)
                    : sf::Vector2f(pitch.totalWidth - pitch.margin - 600.f, 3500.f);
            }
            else {
                bool lastTouchedByHome = (lastTouch->getTeam() == Team::Home);

                if (isHomeEnd) {
                    if (lastTouchedByHome) {
                        m_pendingState = MatchState::Corner;
                        m_awardedTo = Team::Away;
                        m_restartPos = (bPos.y < 3500.f) ? sf::Vector2f(pitch.margin + 50, pitch.margin + 50)
                            : sf::Vector2f(pitch.margin + 50, pitch.totalHeight - pitch.margin - 50);
                    }
                    else {
                        m_pendingState = MatchState::GoalKick;
                        m_awardedTo = Team::Home;
                        m_restartPos = sf::Vector2f(pitch.margin + 600.f, 3500.f);
                    }
                }
                else {
                    if (lastTouchedByHome) {
                        m_pendingState = MatchState::GoalKick;
                        m_awardedTo = Team::Away;
                        m_restartPos = sf::Vector2f(pitch.totalWidth - pitch.margin - 600.f, 3500.f);
                    }
                    else {
                        m_pendingState = MatchState::Corner;
                        m_awardedTo = Team::Home;
                        m_restartPos = (bPos.y < 3500.f) ? sf::Vector2f(pitch.totalWidth - pitch.margin - 50, pitch.margin + 50)
                            : sf::Vector2f(pitch.totalWidth - pitch.margin - 50, pitch.totalHeight - pitch.margin - 50);
                    }
                }
            }

            if (m_pendingState == MatchState::GoalKick) {
                if (m_matchState != MatchState::GoalScored) {
                    soundManager.playSound("crowd_miss", 80.f);
                }
            }

            m_matchState = MatchState::OutOfBoundsDelay;
            m_whistleTimer = 1.5f;
        }
    }
}

bool MatchReferee::checkGoalScored(Ball& ball, const Pitch& pitch, const Goal& homeGoal, const Goal& awayGoal, SoundManager& soundManager, MatchStatistics& stats) {
    sf::Vector2f bPos = ball.getPosition();
    if (ball.z > 244.f + 5.f) return false;

    float goalLineBuffer = 24.f;

    // --- CHECK HOME GOAL (Away Team Scoring) ---
    if (bPos.x < pitch.margin - goalLineBuffer) {
        // Remove the bRadius contraction so the side-netting is legally inside the goal!
        float goalTopY = homeGoal.center.y - 366.f;
        float goalBottomY = homeGoal.center.y + 366.f;

        // Use inclusive >= and <= with a tiny 2px floating-point margin
        if (bPos.y >= goalTopY - 2.f && bPos.y <= goalBottomY + 2.f) {
            m_awayScore++;
            std::string scorer = ball.lastTouch ? ball.lastTouch->getName() : "Unknown";
            stats.recordGoal(Team::Away, scorer, static_cast<int>(m_matchMinute));
            soundManager.playSound("ref_whistle", 100.f);
            soundManager.playSound("crowd_goal", 100.f);
            m_awardedTo = Team::Home;
            return true;
        }
    }

    // --- CHECK AWAY GOAL (Home Team Scoring) ---
    if (bPos.x > pitch.totalWidth - pitch.margin + goalLineBuffer) {
        float goalTopY = awayGoal.center.y - 366.f;
        float goalBottomY = awayGoal.center.y + 366.f;

        if (bPos.y >= goalTopY - 2.f && bPos.y <= goalBottomY + 2.f) {
            m_homeScore++;
            std::string scorer = ball.lastTouch ? ball.lastTouch->getName() : "Unknown";
            stats.recordGoal(Team::Home, scorer, static_cast<int>(m_matchMinute));
            soundManager.playSound("ref_whistle", 100.f);
            soundManager.playSound("crowd_goal", 100.f);
            m_awardedTo = Team::Away;
            return true;
        }
    }

    return false;
}

void MatchReferee::updateMatchContexts() {
    m_attackCtx = TacticalContext();
    m_defendCtx = TacticalContext();
    m_attackMask = PositioningMask();
    m_defendMask = PositioningMask();

    // REMOVED all the massive `homeOffset` hacks that were clashing with TeamAI.
    // The TeamAI now handles the macro pitch compression naturally.

    switch (m_matchState) {
    case MatchState::InPlay:
        break;
        // ... we don't need all the individual cases here anymore because we handle 
        // the constraints flawlessly inside getTacticalContext now!
    }
}

TacticalContext MatchReferee::getTacticalContext(Team team, bool isTaker) const {
    TacticalContext ctx = (team == m_awardedTo) ? m_attackCtx : m_defendCtx;
    ctx.state = m_matchState;

    if (m_matchState != MatchState::InPlay) {
        if (m_matchState == MatchState::RequestReplay || m_matchState == MatchState::ReplayPlaying) {
            ctx.isTaker = false;
            ctx.canPossess = false;
            ctx.ballInfluence = 0.0f;
            ctx.canTackle = false;
            ctx.maxSpeedLimit = 0.f; // Freeze
        }
        else if (isTaker) {
            ctx.isTaker = true;
            ctx.canPossess = true;
            ctx.ballInfluence = 1.0f;
            ctx.maxSpeedLimit = 300.f; // Let the taker walk to the ball
            ctx.canTackle = false;
        }
        else {
            ctx.isTaker = false;
            ctx.canPossess = false;
            ctx.canTackle = false;

            // ==========================================
            // --- THE "TURN YOUR BACK" FIX ---
            // ==========================================
            // Explicitly set ball influence to zero. This strips the ball's gravity 
            // from the spatial engine, forcing players to walk purely to their tactical/manual targets.
            ctx.ballInfluence = 0.0f;

            // Notice we check m_matchState here, not effectiveState!
            // This means during OutOfBoundsDelay and FoulDelay, they jog (350.f) towards their new masks.
            // Once the state actually ticks over to KickOff/Penalty/FreeKick, they freeze (0.f) on their spots.
            if (m_matchState == MatchState::KickOff || m_matchState == MatchState::Penalty || m_matchState == MatchState::FreeKick) {
                ctx.maxSpeedLimit = 0.f;
            }
            else {
                ctx.maxSpeedLimit = 350.f;
            }
        }
    }

    return ctx;
}

PositioningMask MatchReferee::getPositioningMask(const Player* p, const Pitch& pitch) const {
    PositioningMask mask;
    Team team = p->getTeam();
    PositionRole role = p->getPositionRole();
    sf::Vector2f homePos = p->getHomePosition(); // <--- The magic key!

    bool isAttacking = (team == m_awardedTo);
    bool isHome = (team == Team::Home);
    bool attackingHomeEnd = (m_awardedTo == Team::Away);

    float attackingGoalX = attackingHomeEnd ? pitch.margin : pitch.totalWidth - pitch.margin;
    float defendingGoalX = attackingHomeEnd ? pitch.totalWidth - pitch.margin : pitch.margin;
    float boxEdgeX = attackingHomeEnd ? pitch.margin + 1650.f : pitch.totalWidth - pitch.margin - 1650.f;
    float halfwayX = pitch.totalWidth / 2.f;
    float centerY = pitch.totalHeight / 2.f;

    // --- THE DELAY PREVIEW FIX ---
        // If we are waiting for a restart, pretend we are already in that restart state!
    MatchState effectiveState = (m_matchState == MatchState::ReplayPlaying ||
        m_matchState == MatchState::OutOfBoundsDelay ||
        m_matchState == MatchState::FoulDelay)
        ? m_pendingState : m_matchState;

    // --- TAKER LOGIC ---
    if (m_setPieceTaker && isAttacking && p == m_setPieceTaker && effectiveState != MatchState::InPlay) {
        mask.useManualTarget = true;
        bool isRightFooted = (m_setPieceTaker->getPreferredFoot() == "Right");
        bool attackingAway = (m_awardedTo == Team::Home);

        float xOff = attackingAway ? -45.f : 45.f;
        float yOff = (attackingAway) ? (isRightFooted ? -15.f : 15.f) : (isRightFooted ? 15.f : -15.f);

        // THE FIX: Reverse the positioning for Kick-Offs!
        if (effectiveState == MatchState::KickOff) {
            xOff = -xOff; // Stand on the opponent's side of the ball
            yOff = -yOff; // Swap feet offset since we turned 180 degrees
        }

        mask.manualTarget = m_restartPos + sf::Vector2f(xOff, yOff);
        return mask;
    }

    // ==========================================
       // --- FORMATION-RELATIVE CORNERS ---
       // ==========================================
    if (effectiveState == MatchState::Corner) {
        mask.useManualTarget = true;

        bool isFullback = (role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);
        bool isStriker = (role == PositionRole::Striker || role == PositionRole::CenterForward);
        bool isCB = (role == PositionRole::CenterBack);

        // Compress their Y distance to keep the formation shape inside the penalty box
        float relativeY = (homePos.y - centerY) * 0.6f;

        // Directional multipliers to handle both sides of the pitch
        // 1.0 pushes right (into pitch from Home goal), -1.0 pushes left (into pitch from Away goal)
        float sign = attackingHomeEnd ? 1.0f : -1.0f;
        // -1.0 pushes left (into Home half), 1.0 pushes right (into Away half)
        float ownHalfDir = isHome ? -1.0f : 1.0f;

        // Create a realistic "Cluster" scatter so they don't form straight lines
        int seed = static_cast<int>(role) * 7 + (isHome ? 13 : 31);
        float scatterX = static_cast<float>((seed % 400) - 200);
        float scatterY = static_cast<float>((seed % 300) - 150);

        if (isAttacking) {
            if (role == PositionRole::Goalkeeper) {
                // Attacking GK stays all the way back in their own net
                mask.manualTarget.x = defendingGoalX;
                mask.manualTarget.y = centerY;
            }
            else if (isFullback) {
                // Fullbacks stay back to defend counters, safely inside their OWN half
                mask.manualTarget.x = halfwayX + (ownHalfDir * 400.f);
                mask.manualTarget.y = homePos.y;
            }
            else if (isCB) {
                // Tall Center Backs crash the 6-yard box and penalty spot
                mask.manualTarget.x = attackingGoalX + (sign * 800.f) + scatterX;
                mask.manualTarget.y = centerY + (relativeY * 0.5f) + scatterY;
            }
            else {
                // Midfielders and Strikers loiter around the penalty spot to the edge of the box
                mask.manualTarget.x = attackingGoalX + (sign * 1300.f) + scatterX;
                mask.manualTarget.y = centerY + relativeY + scatterY;
            }
        }
        else { // Defending
            if (role == PositionRole::Goalkeeper) {
                // Defending GK stands right in the middle of the goal being attacked
                mask.manualTarget.x = attackingGoalX;
                mask.manualTarget.y = centerY;
            }
            else if (isStriker) {
                // Strikers wait near the halfway line, inside their OWN half so they aren't offside!
                mask.manualTarget.x = halfwayX + (ownHalfDir * 400.f);
                mask.manualTarget.y = homePos.y;
            }
            else if (isCB) {
                // Defending Center Backs heavily pack the 6-yard box
                mask.manualTarget.x = attackingGoalX + (sign * 400.f) + (scatterX * 0.5f);
                mask.manualTarget.y = centerY + (relativeY * 0.5f) + scatterY;
            }
            else {
                // Defending Midfielders and Fullbacks mark the 12-yard penalty spot area
                mask.manualTarget.x = attackingGoalX + (sign * 1000.f) + scatterX;
                mask.manualTarget.y = centerY + relativeY + scatterY;
            }
        }
    }
    // ==========================================
    // --- STRICT PENALTY LINEUP ---
    // ==========================================
    else if (effectiveState == MatchState::Penalty) {
        if (role != PositionRole::Goalkeeper && p != m_setPieceTaker) {
            mask.useManualTarget = true;

            // Compress their formation Y position to fit perfectly along the D-Arc!
            float relativeY = (homePos.y - centerY) * 0.8f;

            float dArcEdgeX = attackingHomeEnd ? pitch.margin + 2200.f : pitch.totalWidth - pitch.margin - 2200.f;
            mask.manualTarget = sf::Vector2f(dArcEdgeX + (attackingHomeEnd ? 100.f : -100.f), centerY + relativeY);
        }
    }
    else if (effectiveState == MatchState::KickOff) {
        mask.forwardLeashMod = 0.25f;
    }
    // ==========================================
    // --- THE FIX: GOAL KICK STRUCTURE ---
    // ==========================================
    else if (effectiveState == MatchState::GoalKick) {
        mask.useManualTarget = true;

        float relativeY = (homePos.y - centerY) * 0.8f;
        float scatterY = static_cast<float>(((static_cast<int>(role) * 7) % 300) - 150);

        if (isAttacking) {
            // TEAM TAKING THE KICK (Home team if awardedTo == Home)
            if (role == PositionRole::Goalkeeper) {
                // GK takes the kick from the 6-yard box (handled by prepareRestart mostly, but good fallback)
                mask.manualTarget.x = defendingGoalX + (attackingHomeEnd ? -100.f : 100.f);
                mask.manualTarget.y = centerY;
            }
            else if (role == PositionRole::CenterBack) {
                // Centerbacks split wide to the edges of the 18-yard box to offer safe short passes
                float pushY = (homePos.y < centerY) ? -800.f : 800.f;
                mask.manualTarget.x = defendingGoalX + (attackingHomeEnd ? -800.f : 800.f);
                mask.manualTarget.y = centerY + pushY;
            }
            else {
                // Midfielders and Strikers push up towards the halfway line to win the long ball
                float pushX = (attackingHomeEnd) ? -2500.f : 2500.f;
                mask.manualTarget.x = halfwayX + pushX;
                mask.manualTarget.y = centerY + relativeY + scatterY;
            }
        }
        else {
            // TEAM DEFENDING THE KICK (Opponent)
            if (role == PositionRole::Goalkeeper) {
                mask.manualTarget.x = attackingGoalX;
                mask.manualTarget.y = centerY;
            }
            else {
                // THE FIX: The entire opponent team MUST evacuate the penalty box!
                // Push them all the way back to the halfway line (or slightly in their own half)
                // to wait for the goalkeeper to boot it long.
                float pushX = (attackingHomeEnd) ? 800.f : -800.f; // Stay on their side of halfway
                mask.manualTarget.x = halfwayX + pushX;
                mask.manualTarget.y = centerY + relativeY + scatterY;
            }
        }
    }

    return mask;
}

void MatchReferee::notifyPlayerSwap(Player* p1, Player* p2) {
    // Repair the Set Piece Taker
    if (m_setPieceTaker == p1) m_setPieceTaker = p2;
    else if (m_setPieceTaker == p2) m_setPieceTaker = p1;

    // Repair the Fouled Player
    if (m_fouledPlayer == p1) m_fouledPlayer = p2;
    else if (m_fouledPlayer == p2) m_fouledPlayer = p1;

    // Repair the Offside Trap Tracker
    if (m_prevBallOwner == p1) m_prevBallOwner = p2;
    else if (m_prevBallOwner == p2) m_prevBallOwner = p1;

    for (size_t i = 0; i < m_offsideSnapshot.flaggedPlayers.size(); ++i) {
        if (m_offsideSnapshot.flaggedPlayers[i] == p1) m_offsideSnapshot.flaggedPlayers[i] = p2;
        else if (m_offsideSnapshot.flaggedPlayers[i] == p2) m_offsideSnapshot.flaggedPlayers[i] = p1;
    }
}

void MatchReferee::prepareRestart(MatchState state, Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager) {
    updateMatchContexts();

    m_matchState = state;
    m_whistleTimer = 2.0f;

    if (state == MatchState::KickOff || state == MatchState::Penalty || state == MatchState::FreeKick) {
    }

    if (state == MatchState::GoalScored || state == MatchState::HalfTime ||
        state == MatchState::FullTime || state == MatchState::OutOfBoundsDelay) {
        return;
    }

    ball.release();
    ball.setSetPiece(true);
    ball.setPosition(m_restartPos);
    ball.setVelocity({ 0.f, 0.f });
    ball.z = 0.f;
    ball.vz = 0.f;

    float closestDist = 99999.f;
    m_setPieceTaker = nullptr;

    for (Player* p : players) {
        // THE FIX 1: Do not interact with Sent Off or Injured players! Let them lie.
        if (p->isSentOff() || p->getState() == PlayerState::Injured) continue;

        p->setVelocity({ 0.f, 0.f });
        if (p->getState() == PlayerState::Tackling || p->getState() == PlayerState::Diving) {
            p->setState(PlayerState::Normal);
        }

        if (p->getTeam() == m_awardedTo) {
            if (state == MatchState::KickOff) {
                if (p->getPositionRole() == PositionRole::Striker || p->getPositionRole() == PositionRole::CenterForward) {
                    m_setPieceTaker = p;
                }
            }
            else if (state == MatchState::GoalKick) {
                if (p->getPositionRole() == PositionRole::Goalkeeper) {
                    m_setPieceTaker = p;
                }
            }
            else if (state == MatchState::Penalty || state == MatchState::FreeKick) {
                // Only let the fouled player take it if they are healthy!
                if (m_fouledPlayer && m_fouledPlayer == p) {
                    m_setPieceTaker = m_fouledPlayer;
                }
            }
            else if (state == MatchState::ThrowIn || state == MatchState::Corner) {
                if (p->getPositionRole() != PositionRole::Goalkeeper) {
                    float d = std::sqrt(std::pow(p->getPosition().x - m_restartPos.x, 2) + std::pow(p->getPosition().y - m_restartPos.y, 2));
                    if (d < closestDist) {
                        closestDist = d;
                        m_setPieceTaker = p;
                    }
                }
            }
        }
    }

    // ==========================================
    // --- THE FIX 2: DYNAMIC TAKER FALLBACK ---
    // ==========================================
    if (m_setPieceTaker == nullptr) {
        // Find the most "Forward" healthy player on the pitch
        float mostForwardX = (m_awardedTo == Team::Home) ? -9999.f : 99999.f;

        for (Player* p : players) {
            if (p->getTeam() == m_awardedTo && p->getPositionRole() != PositionRole::Goalkeeper && !p->isSentOff() && p->getState() != PlayerState::Injured) {

                float xPos = p->getHomePosition().x;
                bool isMoreForward = (m_awardedTo == Team::Home) ? (xPos > mostForwardX) : (xPos < mostForwardX);

                if (isMoreForward) {
                    mostForwardX = xPos;
                    m_setPieceTaker = p;
                }
            }
        }
        if (m_setPieceTaker) ball.possess(m_setPieceTaker);
    }

    if (m_setPieceTaker) {
        bool isRightFooted = (m_setPieceTaker->getPreferredFoot() == "Right");
        bool attackingAway = (m_awardedTo == Team::Home);

        // ==========================================
        // --- THE FIX 2: RUN-UP SPACING ---
        // ==========================================
        // Spawn them far enough away to not overlap the ball's collision radius!
        float xOffset = attackingAway ? -150.f : 150.f;
        float yOffset = isRightFooted ? -30.f : 30.f;

        if (state == MatchState::KickOff) {
            xOffset = attackingAway ? -60.f : 60.f;
            yOffset = -yOffset;
        }

        m_setPieceTaker->setPosition(m_restartPos + sf::Vector2f(xOffset, yOffset));
        ball.setPosition(m_restartPos);
    }

    // ==========================================
    // --- TACTICAL TELEPORT FIXES ---
    // ==========================================
    for (Player* p : players) {
        // THE FIX 3: Do not teleport Sent Off or Injured players!
        if (p == m_setPieceTaker || p->isSentOff() || p->getState() == PlayerState::Injured) continue;

        if (state == MatchState::KickOff) {
            p->setPosition(p->getHomePosition());
            p->setVelocity({ 0.f, 0.f });
        }
        else if (state == MatchState::Corner) {
            PositioningMask mask = getPositioningMask(p, pitch);
            if (mask.useManualTarget) {
                p->setPosition(mask.manualTarget);
                p->setVelocity({ 0.f, 0.f });
            }
        }

        if (state == MatchState::Penalty) {
            if (p->getPositionRole() == PositionRole::Goalkeeper && p->getTeam() != m_awardedTo) {
                p->setPosition(m_awardedTo == Team::Home ? pitch.awayGoalCenter : pitch.homeGoalCenter);
            }
            else {
                PositioningMask mask = getPositioningMask(p, pitch);
                if (mask.useManualTarget) {
                    p->setPosition(mask.manualTarget);
                    p->setVelocity({ 0.f, 0.f });
                }
            }
        }
    }
}

void MatchReferee::awardFoul(FoulEvent foul, const Pitch& pitch, Ball& ball, const std::vector<Player*>& players, Player* victim, SoundManager& soundManager, MatchStatistics& stats) {
    if (m_matchState != MatchState::InPlay) return;

    bool isHomeDefending = (foul.offender->getTeam() == Team::Home);
    m_awardedTo = isHomeDefending ? Team::Away : Team::Home;
    m_fouledPlayer = victim;

    // 1. Check Offside First!
    if (foul.type == FoulType::Offside) {
        awardOffside(foul.offender, ball, pitch, soundManager);
        return;
    }

    // ==========================================
    // --- THE FIX 1: ACTUAL FOULS ONLY ---
    // ==========================================
    stats.recordFoul(foul.offender->getTeam());
    soundManager.playSound("ref_foul", 100.f); // <-- FOUL WHISTLE

    if (foul.type == FoulType::Sliding) {
        foul.offender->giveYellowCard();
        std::cout << "YELLOW CARD: " << foul.offender->getName() << "\n";
    }
    else if (foul.type == FoulType::Violent) {
        foul.offender->giveRedCard();
        std::cout << "STRAIGHT RED CARD: " << foul.offender->getName() << "\n";
    }

    bool inBox = false;
    if (isHomeDefending && pitch.homePenaltyBox.contains(foul.location)) inBox = true;
    if (!isHomeDefending && pitch.awayPenaltyBox.contains(foul.location)) inBox = true;

    if (inBox) {
        m_pendingState = MatchState::Penalty;
        m_restartPos = isHomeDefending ? pitch.homePenaltySpot : pitch.awayPenaltySpot;
    }
    else {
        m_pendingState = MatchState::FreeKick;
        m_restartPos = foul.location;
    }

    m_foulDelayTimer = 2.0f;
    m_matchState = MatchState::FoulDelay;

    ball.setVelocity({ 0.f, 0.f });
    updateMatchContexts();
}

void MatchReferee::applyForfeitScore(bool homeForfeited) {
    if (homeForfeited) {
        m_homeScore = 0;
        m_awayScore = 3;
    }
    else {
        m_homeScore = 3;
        m_awayScore = 0;
    }
    m_matchState = MatchState::FullTime;
}

void MatchReferee::setupReplayTeleports(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager)
{
    for (Player* p : players) {
        if (p->isSentOff() && p->getPosition().x > 0.f) {
            p->setPosition({ -5000.f, -5000.f });
            p->setVelocity({ 0.f, 0.f });
        }
    }

    // THE FIX 2a: Do NOT call prepareRestart here! The Replay Engine will just overwrite it!
    m_matchState = MatchState::ReplayPlaying;
    m_whistleTimer = 5.0f;
}

// THE FIX 2b: Setup the set-piece AFTER the replay finishes!
void MatchReferee::resumeFromReplay(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager)
{
    // The ReplayEngine has finished hijacking the physical objects. 
    // We now overwrite the garbage replay data and perfectly arrange them for the set piece!
    prepareRestart(m_pendingState, ball, pitch, players, soundManager);
    
    // m_matchState is now safely set to FreeKick, Penalty, etc. by prepareRestart
}

void MatchReferee::startMatch(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager)
{
    m_timeScale = 90.0f / static_cast<float>(GlobalSettings::matchLengthMinutes);
    m_half = 1;
    m_matchMinute = 0.0f;
    m_awardedTo = Team::Home;
    m_restartPos = sf::Vector2f(pitch.totalWidth / 2.f, pitch.totalHeight / 2.f);
    prepareRestart(MatchState::KickOff, ball, pitch, players, soundManager);
}

void MatchReferee::checkOffsideLogic(Ball& ball, const std::vector<Player*>& players, float homeOffsideLine, float awayOffsideLine, const Pitch& pitch, SoundManager& soundManager) {
    Player* currentOwner = ball.getOwner();

    // ==========================================
    // 0. EXEMPTION CHECK
    // ==========================================
    // You cannot be offside directly from these restarts.
    // If the ball is kicked during these states, we do NOT take a snapshot.
    bool isExemptRestart = (m_matchState == MatchState::Corner ||
        m_matchState == MatchState::GoalKick ||
        m_matchState == MatchState::ThrowIn ||
        m_matchState == MatchState::Penalty ||
        m_matchState == MatchState::KickOff);

    // ==========================================
    // 1. THE SNAPSHOT (Detecting the Pass)
    // ==========================================
    if (m_prevBallOwner != nullptr && currentOwner == nullptr) {

        // Only flag players if it's a standard pass in open play (not an exempt restart)
        if (!isExemptRestart) {
            Team attackingTeam = m_prevBallOwner->getTeam();
            float currentLine = (attackingTeam == Team::Home) ? homeOffsideLine : awayOffsideLine;

            m_offsideSnapshot.flaggedPlayers.clear();
            m_offsideSnapshot.attackingTeam = attackingTeam;
            m_offsideSnapshot.isActive = true;

            for (Player* p : players) {
                if (p->getTeam() != attackingTeam || p == m_prevBallOwner) continue;

                // Rule: You cannot be offside in your own half
                float halfwayX = pitch.totalWidth / 2.f;
                bool inOpponentHalf = (attackingTeam == Team::Home) ? (p->getPosition().x > halfwayX) : (p->getPosition().x < halfwayX);

                if (inOpponentHalf) {
                    bool isOffsidePos = (attackingTeam == Team::Home) ? (p->getPosition().x > currentLine) : (p->getPosition().x < currentLine);

                    if (isOffsidePos) {
                        m_offsideSnapshot.flaggedPlayers.push_back(p);
                    }
                }
            }
        }
        else {
            // It was a Corner/Throw-in/etc. Reset the snapshot so the next touch is clean.
            m_offsideSnapshot.isActive = false;
            m_offsideSnapshot.flaggedPlayers.clear();
        }
    }

    // ==========================================
    // 2. THE TRAP (Detecting the Reception)
    // ==========================================
    if (m_prevBallOwner == nullptr && currentOwner != nullptr) {
        if (m_offsideSnapshot.isActive && currentOwner->getTeam() == m_offsideSnapshot.attackingTeam) {
            for (Player* flagged : m_offsideSnapshot.flaggedPlayers) {
                if (currentOwner == flagged) {
                    awardOffside(currentOwner, ball, pitch, soundManager);
                    break;
                }
            }
        }
        // Possession has changed or ball was received; snapshot is now spent.
        m_offsideSnapshot.isActive = false;
        m_offsideSnapshot.flaggedPlayers.clear();
    }

    m_prevBallOwner = currentOwner;
}

void MatchReferee::awardOffside(Player* offender, Ball& ball, const Pitch& pitch, SoundManager& soundManager) {
    // Reset ball physics
    ball.setVelocity({ 0.f, 0.f });
    ball.z = 0.f;

    soundManager.playSound("ref_foul", 100.f); // <-- OFFSIDE WHISTLE

    // Restart position is where the offside player was when they touched it
    m_restartPos = ball.getPosition();
    m_awardedTo = (offender->getTeam() == Team::Home) ? Team::Away : Team::Home;

    // Offside is ALWAYS a free kick, never a penalty, even if in the box!
    m_pendingState = MatchState::FreeKick;
    m_matchState = MatchState::FoulDelay;
    m_foulDelayTimer = 2.0f;
}
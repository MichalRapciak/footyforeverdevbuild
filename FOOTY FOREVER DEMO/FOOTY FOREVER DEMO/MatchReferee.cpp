#include "MatchReferee.h"
#include "Player.h"
#include "Ball.h"
#include "Pitch.h"
#include <iostream>


void MatchReferee::update(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, float dt)
{
    // ==========================================
    // 1. MATCH CLOCK LOGIC
    // ==========================================
    if (m_matchState == MatchState::InPlay || m_matchState == MatchState::GoalScored) {
        // Advance the clock (dt is in seconds. Divide by 60 to get minutes)
        m_matchMinute += (dt * m_timeScale) / 60.0f;

        // Check for Half Time (45 mins)
        if (m_matchMinute >= 45.0f && m_half == 1 && m_matchState == MatchState::InPlay) {
            m_matchState = MatchState::HalfTime;
            m_whistleTimer = 5.0f; // Wait 5 seconds before auto-starting second half (or wait for UI button!)
            updateMatchContexts();
            std::cout << "HALF TIME!\n";
        }
        // Check for Full Time (90 mins)
        else if (m_matchMinute >= 90.0f && m_half == 2 && m_matchState == MatchState::InPlay) {
            m_matchState = MatchState::FullTime;
            updateMatchContexts();
            std::cout << "FULL TIME!\n";
        }
    }

    // ==========================================
        // 2. STATE HANDLING
        // ==========================================
    if (m_matchState == MatchState::InPlay) {
        checkBoundaries(ball, pitch);
        if (m_matchState != MatchState::InPlay) {
            prepareRestart(m_matchState, ball, pitch, players);
        }
    }
    else if (m_matchState == MatchState::FoulDelay) {
        m_foulDelayTimer -= dt;

        if (m_foulDelayTimer <= 0.f) {
            // The 2 seconds of sliding on the grass are over. 
            // Ask the engine to roll the tape!
            m_matchState = MatchState::RequestReplay;
        }
    }
    // ==========================================
    // --- UPDATED: GOAL REPLAY TRANSITION ---
    // ==========================================
    else if (m_matchState == MatchState::GoalScored) {
        m_whistleTimer -= dt;

        if (m_whistleTimer <= 0.f) {
            // The 2.5 seconds of live celebration are over. 
            // 1. Set the exact coordinates for the Kick-Off teleport
            m_restartPos = sf::Vector2f(pitch.totalWidth / 2.f, 3500.f);

            // 2. Roll the tape!
            m_matchState = MatchState::RequestReplay;
        }
    }
    // ==========================================
    // --- NEW: OUT OF BOUNDS HIGHLIGHT CHECK ---
    // ==========================================
    else if (m_matchState == MatchState::OutOfBoundsDelay) {
        m_whistleTimer -= dt;

        if (m_whistleTimer <= 0.f) {

            // 30% chance to show a replay. 
            // (You can also change this to check if ball.getVelocity() > 600 to only replay hard shots!)
            if ((rand() % 100) < 30) {
                // Roll the tape! The DVR will handle the teleports behind the scenes.
                m_matchState = MatchState::RequestReplay;
            }
            else {
                // Skip the replay entirely so the game doesn't get boring.
                // Instantly set up the set piece and teleport everyone so play continues!
                prepareRestart(m_pendingState, ball, pitch, players);
            }
        }
    }
    else if (m_matchState == MatchState::HalfTime) {
        // Wait for the half time break to finish
        m_whistleTimer -= dt;
        if (m_whistleTimer <= 0.f) {
            // Start the Second Half!
            m_half = 2;
            m_matchMinute = 45.0f; // Ensure it starts exactly at 45:00
            m_awardedTo = Team::Away; // Away team usually kicks off second half
            m_restartPos = sf::Vector2f(pitch.totalWidth / 2.f, 3500.f);

            // Swap sides logic can go here if you implement side-swapping!

            prepareRestart(MatchState::KickOff, ball, pitch, players);
        }
    }
    else if (m_matchState == MatchState::FullTime) {
        // Game Over! Do nothing, wait for GamePlay to detect this state and pull up the End Screen UI.
    }
    else {
        // --- ALL DEAD BALL STATES (Free Kick, Corner, Penalty, etc) ---
        m_whistleTimer -= dt;

        // 1. CHECK FOR KICK-OFF / PLAY RESUMPTION
        if (m_whistleTimer <= 0.f) {
            sf::Vector2f bVel = ball.getVelocity();
            float speed = std::sqrt(bVel.x * bVel.x + bVel.y * bVel.y);

            // Play ONLY resumes if the ball leaves the taker's feet!
            if (ball.getOwner() == nullptr && speed > 15.0f) {
                m_matchState = MatchState::InPlay;
                m_setPieceTaker = nullptr;
                updateMatchContexts();
                return; // <--- ADD THIS RETURN: Skip the forcefield this frame!
            }
        }

        // 2. LOCK THE BALL AND TAKER
        // (Notice we removed the 'else' that used to be here!)
        if (m_setPieceTaker) {
            m_setPieceTaker->setVelocity({ 0.f, 0.f });
            ball.setVelocity({ 0.f, 0.f });
            ball.possess(m_setPieceTaker);
        }
        else {
            ball.setPosition(m_restartPos);
            ball.setVelocity({ 0.f, 0.f });
        }

        // ==========================================
        // 3. CONTINUOUS ENCROACHMENT FORCEFIELD & WALL
        // ==========================================
        // Because this is no longer trapped in an 'else' block, it will hold the 
        // AI back even after the whistle blows, right up until you strike the ball!
        if (m_matchState == MatchState::FreeKick || m_matchState == MatchState::Corner ||
            m_matchState == MatchState::Penalty || m_matchState == MatchState::KickOff)
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
                    if (p == m_setPieceTaker || p->getTeam() == m_awardedTo) continue;

                    // 1. HARD-LOCK THE WALL
                    if (needsWall && wallCount < maxWallPlayers &&
                        p->getPositionRole() != PositionRole::Goalkeeper &&
                        p->getPositionRole() != PositionRole::LCenterBack &&
                        p->getPositionRole() != PositionRole::RCenterBack)
                    {
                        float offset = (wallCount - (maxWallPlayers / 2.0f) + 0.5f) * 60.f;
                        sf::Vector2f targetPos = wallCenter + (perp * offset);

                        // Failsafe: Wall players cannot stand in the stands
                        targetPos.x = std::clamp(targetPos.x, pitch.margin, pitch.totalWidth - pitch.margin);
                        targetPos.y = std::clamp(targetPos.y, pitch.margin, pitch.totalHeight - pitch.margin);

                        p->setPosition(targetPos);
                        p->setVelocity({ 0.f, 0.f }); // Kill momentum
                        wallCount++;
                        continue;
                    }

                    // 2. INVISIBLE 9.15m FORCEFIELD
                    if (m_matchState != MatchState::Penalty) {
                        sf::Vector2f pPos = p->getPosition();
                        float dx = pPos.x - m_restartPos.x;
                        float dy = pPos.y - m_restartPos.y;
                        float distSq = dx * dx + dy * dy;

                        // If they encroach closer than 9.15m...
                        if (distSq < (915.f * 915.f)) {
                            float dist = std::sqrt(distSq);
                            if (dist > 0.1f) {
                                sf::Vector2f pushDir = { dx / dist, dy / dist };
                                sf::Vector2f targetPos = m_restartPos + (pushDir * 915.f);

                                // FAILSAFE: Bending the Vector
                                // If the push sends them off the grass, blend the vector towards the pitch center!
                                if (targetPos.x < pitch.margin || targetPos.x > pitch.totalWidth - pitch.margin ||
                                    targetPos.y < pitch.margin || targetPos.y > pitch.totalHeight - pitch.margin)
                                {
                                    sf::Vector2f center(pitch.totalWidth / 2.f, pitch.totalHeight / 2.f);
                                    sf::Vector2f toCenter = center - pPos;
                                    float centerLen = std::sqrt(toCenter.x * toCenter.x + toCenter.y * toCenter.y);

                                    if (centerLen > 0.1f) {
                                        pushDir += (toCenter / centerLen) * 0.8f; // Strong blend to slide them inward
                                        float newLen = std::sqrt(pushDir.x * pushDir.x + pushDir.y * pushDir.y);
                                        pushDir /= newLen;
                                        targetPos = m_restartPos + (pushDir * 915.f);
                                    }
                                }

                                // Final Safety Clamp (Guarantees they physically cannot render out of bounds)
                                targetPos.x = std::clamp(targetPos.x, pitch.margin, pitch.totalWidth - pitch.margin);
                                targetPos.y = std::clamp(targetPos.y, pitch.margin, pitch.totalHeight - pitch.margin);

                                p->setPosition(targetPos);
                                p->setVelocity({ 0.f, 0.f }); // Kill momentum
                            }
                        }
                    }
                }
        }
    }
}

void MatchReferee::checkBoundaries(Ball& ball, const Pitch& pitch) {
    sf::Vector2f bPos = ball.getPosition();
    Player* lastOwner = ball.getLastOwner();

    // --- 1. SIDELINE CHECK ---
    if (bPos.y < pitch.margin || bPos.y > pitch.totalHeight - pitch.margin) {
        m_matchState = MatchState::ThrowIn;

        // Push the ball 25 pixels INSIDE the line so it doesn't trigger again instantly
        float safeY = (bPos.y < pitch.margin) ? pitch.margin + 25.f : pitch.totalHeight - pitch.margin - 25.f;

        // Clamp X so it doesn't accidentally spawn in the corner flags out of bounds
        float safeX = std::clamp(bPos.x, pitch.margin + 25.f, pitch.totalWidth - pitch.margin - 25.f);

        m_restartPos = sf::Vector2f(safeX, safeY);

        if (lastOwner != nullptr) {
            m_awardedTo = (lastOwner->getTeam() == Team::Home) ? Team::Away : Team::Home;
        }
        else {
            m_awardedTo = (bPos.x > pitch.halfwayLineX) ? Team::Home : Team::Away;
        }
        return;
    }

    // --- 2. ENDLINE CHECK ---
    if (bPos.x < pitch.margin || bPos.x > pitch.totalWidth - pitch.margin) {

        // A. Did it fully cross the goal line?
        if (checkGoalScored(ball, pitch)) {
            m_matchState = MatchState::GoalScored;

            // --- NEW: REPLAY SYNC ---
            m_pendingState = MatchState::KickOff; // Tell the DVR what state to teleport to!
            m_whistleTimer = 2.5f;                // Let them celebrate for 2.5 seconds before triggering the replay

            return;
        }

        // B. Is it inside the posts but hasn't fully crossed the 24px buffer yet?
        float goalTopY = 3500.f - 366.f;
        float goalBottomY = 3500.f + 366.f;
        if (bPos.y > goalTopY && bPos.y < goalBottomY && ball.z <= 249.f) {
            return; // DO NOTHING! Let the physics engine roll it into the net!
        }

        // C. If we reach here, it went wide of the posts. Set up Goal Kick or Corner.
        bool isHomeEnd = (bPos.x < pitch.margin);

        // FALLBACK: If lastOwner is null, assume Goal Kick
        if (lastOwner == nullptr) {
            m_pendingState = MatchState::GoalKick;     // <--- CHANGED
            m_awardedTo = isHomeEnd ? Team::Home : Team::Away;
            m_restartPos = isHomeEnd ? sf::Vector2f(pitch.margin + 600.f, 3500.f)
                : sf::Vector2f(pitch.totalWidth - pitch.margin - 600.f, 3500.f);
        }
        else {
            // STANDARD LOGIC
            bool lastTouchedByHome = (lastOwner->getTeam() == Team::Home);

            if (isHomeEnd) {
                if (lastTouchedByHome) {
                    m_pendingState = MatchState::Corner;   // <--- CHANGED
                    m_awardedTo = Team::Away;
                    m_restartPos = (bPos.y < 3500.f) ? sf::Vector2f(pitch.margin + 50, pitch.margin + 50)
                        : sf::Vector2f(pitch.margin + 50, pitch.totalHeight - pitch.margin - 50);
                }
                else {
                    m_pendingState = MatchState::GoalKick; // <--- CHANGED
                    m_awardedTo = Team::Home;
                    m_restartPos = sf::Vector2f(pitch.margin + 600.f, 3500.f);
                }
            }
            else { // Away End
                if (lastTouchedByHome) {
                    m_pendingState = MatchState::GoalKick; // <--- CHANGED
                    m_awardedTo = Team::Away;
                    m_restartPos = sf::Vector2f(pitch.totalWidth - pitch.margin - 600.f, 3500.f);
                }
                else {
                    m_pendingState = MatchState::Corner;   // <--- CHANGED
                    m_awardedTo = Team::Home;
                    m_restartPos = (bPos.y < 3500.f) ? sf::Vector2f(pitch.totalWidth - pitch.margin - 50, pitch.margin + 50)
                        : sf::Vector2f(pitch.totalWidth - pitch.margin - 50, pitch.totalHeight - pitch.margin - 50);
                }
            }
        }

        // --- NEW: TRIGGER THE DELAY ---
        // We have set up the pending state above. Now we freeze open play!
        m_matchState = MatchState::OutOfBoundsDelay;
        m_whistleTimer = 1.5f; // Wait 1.5 seconds before deciding to replay or teleport
    }
}

bool MatchReferee::checkGoalScored(Ball& ball, const Pitch& pitch) {
    sf::Vector2f bPos = ball.getPosition();
    float bZ = ball.z;

    // 1. Vertical Check: Over the crossbar
    if (bZ > 244.f + 5.f) return false;

    // 2. Horizontal Check: Between the posts
    float goalTopY = 3500.f - 366.f;
    float goalBottomY = 3500.f + 366.f;
    bool withinPosts = (bPos.y > goalTopY && bPos.y < goalBottomY);
    if (!withinPosts) return false;

    // 3. Depth Check: Requires the ball to fully cross
    float goalLineBuffer = 24.f;

    // HOME GOAL (Left side of screen)
    if (bPos.x < pitch.margin - goalLineBuffer) {
        m_awayScore++;            // Away team scored!
        m_awardedTo = Team::Home; // Home team gets the Kick-Off
        return true;
    }

    // AWAY GOAL (Right side of screen)
    if (bPos.x > pitch.totalWidth - pitch.margin + goalLineBuffer) {
        m_homeScore++;            // Home team scored!
        m_awardedTo = Team::Away; // Away team gets the Kick-Off
        return true;
    }

    return false;
}

void MatchReferee::prepareRestart(MatchState state, Ball& ball, const Pitch& pitch, const std::vector<Player*>& players) {
    updateMatchContexts();

    m_matchState = state;
    m_whistleTimer = 5.0f;

// ==========================================
	// EARLY EXITS (No ball placement needed)
	// ==========================================
	if (state == MatchState::GoalScored || state == MatchState::HalfTime || 
        state == MatchState::FullTime || state == MatchState::OutOfBoundsDelay) {
		return;
	}

    ball.release();
    ball.setSetPiece(true);
    // 1. Reset the ball
    ball.setPosition(m_restartPos);
    ball.setVelocity({ 0.f, 0.f });
    ball.z = 0.f;
    ball.vz = 0.f;



    // 2. Find the Taker
    float closestDist = 99999.f;
    m_setPieceTaker = nullptr;

    for (Player* p : players) {
        p->setVelocity({ 0.f, 0.f });
        p->setState(PlayerState::Normal);

        if (p->getTeam() == m_awardedTo) {
            // --- 0. KICK OFF LOGIC ---
            if (state == MatchState::KickOff) {
                if (p->getPositionRole() == PositionRole::Striker) {
                    m_setPieceTaker = p;
                    ball.possess(m_setPieceTaker);
                    break;
                }
            }
            // --- 1. GOAL KICK LOGIC ---
            else if (state == MatchState::GoalKick) {
                if (p->getPositionRole() == PositionRole::Goalkeeper) {
                    m_setPieceTaker = p;
                    ball.possess(m_setPieceTaker);
                    break;
                }
            }
            // --- 2. PENALTY LOGIC ---
            else if (state == MatchState::Penalty) { // FIX: Was checking if p == nullptr
                    m_setPieceTaker = m_fouledPlayer;
                    ball.possess(m_setPieceTaker);
                    break;
            }
            // --- 3. THROW-IN LOGIC ---
            else if (state == MatchState::ThrowIn) {
                if (p->getPositionRole() != PositionRole::Goalkeeper) {
                    float d = std::sqrt(std::pow(p->getPosition().x - m_restartPos.x, 2) + std::pow(p->getPosition().y - m_restartPos.y, 2));
                    if (d < closestDist) {
                        closestDist = d;
                        m_setPieceTaker = p;
                        ball.possess(m_setPieceTaker);
                    }
                }
            }
            // --- 4. CORNER & FREE KICK LOGIC ---
            else if (state == MatchState::Corner) {
                if (p->getPositionRole() != PositionRole::Goalkeeper &&
                    p->getPositionRole() != PositionRole::LCenterBack &&
                    p->getPositionRole() != PositionRole::RCenterBack) {
                    float d = std::sqrt(std::pow(p->getPosition().x - m_restartPos.x, 2) + std::pow(p->getPosition().y - m_restartPos.y, 2));
                    if (d < closestDist) {
                        closestDist = d;
                        m_setPieceTaker = p;
                        ball.possess(m_setPieceTaker);
                    }
                }
            }
            else if (state == MatchState::FreeKick) {
                if (p->getPositionRole() != PositionRole::Goalkeeper &&
                    p->getPositionRole() != PositionRole::LCenterBack &&
                    p->getPositionRole() != PositionRole::RCenterBack) {
                    float d = std::sqrt(std::pow(p->getPosition().x - m_restartPos.x, 2) + std::pow(p->getPosition().y - m_restartPos.y, 2));
                    if (d < closestDist) {
                        closestDist = d;
                        m_setPieceTaker = m_fouledPlayer;
                        ball.possess(m_setPieceTaker);
                    }
                }
            }
        }
    }

    // 3. Position the Taker 
    if (m_setPieceTaker) {
        bool isRightFooted = (m_setPieceTaker->getPreferredFoot() == "Right"); // Adjust string/enum to match your data
        bool attackingAway = (m_awardedTo == Team::Home);

        // --- X OFFSET (Stand behind the ball) ---
        // If attacking Away (X+), stand further Down (X-).
        float xOffset = attackingAway ? -45.f : 45.f;

        // --- Y OFFSET (Align favored foot with ball) ---
        float yOffset = 0.f;

        if (attackingAway) {
            // Facing X+: Right foot is Y+, Left foot is Y-
            // To put ball at Right foot, player must shift Left (Y-)
            yOffset = isRightFooted ? -15.f : 15.f;
        }
        else {
            // Facing X-: Right foot is Y-, Left foot is Y+
            // To put ball at Right foot, player must shift "Right" relative to pitch (Y+)
            yOffset = isRightFooted ? 15.f : -15.f;
        }

        // Apply position
        m_setPieceTaker->setPosition(m_restartPos + sf::Vector2f(xOffset, yOffset));

        // Lock the ball to the taker
        ball.setPosition(m_restartPos);
        ball.possess(m_setPieceTaker);
    }

    // ==========================================
    // 4. PITCH RULES & DEFENSIVE WALL
    // ==========================================
    sf::Vector2f targetGoal = (m_awardedTo == Team::Home) ? pitch.awayGoalCenter : pitch.homeGoalCenter;
    float distToTargetGoal = std::sqrt(std::pow(m_restartPos.x - targetGoal.x, 2) + std::pow(m_restartPos.y - targetGoal.y, 2));

    bool needsWall = (state == MatchState::FreeKick && distToTargetGoal < 2500.f); // 25 meters
    int wallCount = 0;
    int maxWallPlayers = (distToTargetGoal < 1800.f) ? 4 : 3; // Thicker wall if closer

    // Wall Math
    sf::Vector2f toGoal = targetGoal - m_restartPos;
    float lenToGoal = std::sqrt(toGoal.x * toGoal.x + toGoal.y * toGoal.y);
    if (lenToGoal > 0.1f) toGoal /= lenToGoal;
    sf::Vector2f wallCenter = m_restartPos + (toGoal * 915.f);
    sf::Vector2f perp(-toGoal.y, toGoal.x); // Perpendicular axis to line them up

    for (Player* p : players) {
        if (p == m_setPieceTaker) continue;

        // Do not put sent-off players in the wall or push them around!
        if (p->isSentOff()) continue;

        // ==========================================
        // --- NEW: TACTICAL SNAP (Teleport to Box) ---
        // ==========================================
        // For major set pieces where play has completely stopped, instantly teleport 
        // everyone to their tactical zones rather than making them run there.
        if (state == MatchState::Corner || state == MatchState::GoalKick) {
            PositioningMask mask = getPositioningMask(p->getTeam(), p->getPositionRole(), pitch);

            if (mask.useManualTarget) {
                // Add a tiny random scatter (±50px) so players don't perfectly stack on each other
                float offsetX = (rand() % 100 - 50) * 1.0f;
                float offsetY = (rand() % 100 - 50) * 1.0f;

                sf::Vector2f snapPos = mask.manualTarget + sf::Vector2f(offsetX, offsetY);

                // Failsafe clamp to keep them on the grass
                snapPos.x = std::clamp(snapPos.x, pitch.margin, pitch.totalWidth - pitch.margin);
                snapPos.y = std::clamp(snapPos.y, pitch.margin, pitch.totalHeight - pitch.margin);

                p->setPosition(snapPos);
                p->setVelocity({ 0.f, 0.f });
            }
        }

        // Grab the position AFTER the snap so the Encroachment logic below pushes them away 
        // correctly if the mask accidentally spawned them too close to the corner flag!
        sf::Vector2f pPos = p->getPosition();

        if (state == MatchState::Penalty) {
            if (p->getPositionRole() == PositionRole::Goalkeeper && p->getTeam() != m_awardedTo) {
                p->setPosition(m_awardedTo == Team::Home ? pitch.awayGoalCenter : pitch.homeGoalCenter);
            }
            else {
                bool homeDefending = (m_awardedTo == Team::Away);
                bool inBox = homeDefending ? pitch.homePenaltyBox.contains(pPos) : pitch.awayPenaltyBox.contains(pPos);
                bool inArc = pitch.isInPenaltyArc(pPos, homeDefending);

                if (inBox || inArc) {
                    p->setPosition({ pPos.x + (homeDefending ? 600.f : -600.f), pPos.y });
                }
            }
        }
        else if (p->getTeam() != m_awardedTo && state != MatchState::ThrowIn && state != MatchState::GoalKick) {

            // A. Build the Wall
            if (needsWall && wallCount < maxWallPlayers &&
                p->getPositionRole() != PositionRole::Goalkeeper &&
                p->getPositionRole() != PositionRole::LCenterBack &&
                p->getPositionRole() != PositionRole::RCenterBack)
            {
                float offset = (wallCount - (maxWallPlayers / 2.0f) + 0.5f) * 60.f;
                sf::Vector2f targetPos = wallCenter + (perp * offset);

                // Failsafe clamp
                targetPos.x = std::clamp(targetPos.x, pitch.margin, pitch.totalWidth - pitch.margin);
                targetPos.y = std::clamp(targetPos.y, pitch.margin, pitch.totalHeight - pitch.margin);

                p->setPosition(targetPos);
                p->setVelocity({ 0.f, 0.f }); // Kill momentum
                wallCount++;
                continue;
            }

            // B. Generic 9.15m push for everyone else
            float distToBall = std::sqrt(std::pow(pPos.x - m_restartPos.x, 2) + std::pow(pPos.y - m_restartPos.y, 2));
            if (distToBall < 915.f) {
                sf::Vector2f pushDir = pPos - m_restartPos;
                float len = std::sqrt(pushDir.x * pushDir.x + pushDir.y * pushDir.y);
                if (len > 0.1f) {
                    pushDir /= len;
                    sf::Vector2f targetPos = m_restartPos + (pushDir * 920.f); // Give them a tiny 5px buffer

                    // FAILSAFE: Bend the Vector
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
                            targetPos = m_restartPos + (pushDir * 920.f);
                        }
                    }

                    // Final Safety Clamp
                    targetPos.x = std::clamp(targetPos.x, pitch.margin, pitch.totalWidth - pitch.margin);
                    targetPos.y = std::clamp(targetPos.y, pitch.margin, pitch.totalHeight - pitch.margin);

                    p->setPosition(targetPos);
                }
            }
        }
    }
}

void MatchReferee::updateMatchContexts() {
    // RESET TO DEFAULTS
    m_attackCtx = TacticalContext();
    m_defendCtx = TacticalContext();
    m_attackMask = PositioningMask();
    m_defendMask = PositioningMask();

    switch (m_matchState) {
    case MatchState::InPlay:
        break;

    case MatchState::GoalKick:

        m_attackCtx.maxSpeedLimit = 300.f;
        m_attackCtx.ballInfluence = 0.0f;
        m_attackCtx.canTackle = false;

        m_defendCtx.maxSpeedLimit = 450.f;
        m_defendCtx.ballInfluence = 0.2f;
        m_defendMask.homeOffset = (m_awardedTo == Team::Home) ? sf::Vector2f(-2000.f, 0.f) : sf::Vector2f(2000.f, 0.f);
        break;

    case MatchState::ThrowIn:
        m_attackCtx.maxSpeedLimit = 400.f;
        m_attackCtx.ballInfluence = 0.5f;
        m_attackCtx.canTackle = false;

        m_defendCtx.maxSpeedLimit = 400.f;
        m_defendCtx.awarenessMod = 1.5f;
        m_defendCtx.ballInfluence = 0.0f;
        m_defendCtx.canTackle = false;
        break;

    case MatchState::FreeKick:
    case MatchState::Corner:
        m_attackCtx.maxSpeedLimit = 350.f;
        m_attackCtx.ballInfluence = 0.0f;
        m_attackCtx.canTackle = false;

        m_defendCtx.maxSpeedLimit = 350.f;
        m_defendCtx.ballInfluence = 0.0f;   // CRITICAL FIX: Stop them from hunting the taker!
        m_defendCtx.canTackle = false;
        break;

    case MatchState::Penalty:
        m_attackCtx.maxSpeedLimit = 400.f;
        m_attackCtx.ballInfluence = 0.0f;
        m_attackCtx.canTackle = false;

        m_defendCtx.maxSpeedLimit = 400.f;
        m_defendCtx.ballInfluence = 0.0f;
        m_defendCtx.canTackle = false;
        break;

    case MatchState::GoalScored:
        m_attackCtx.maxSpeedLimit = 500.f;
        m_attackCtx.ballInfluence = 0.0f;   // Ignore the ball during celebration
        m_attackCtx.canTackle = false;

        m_defendCtx.maxSpeedLimit = 800.f;  // <--- INCREASED: Briskly jog back to center for KickOff!
        m_defendCtx.ballInfluence = 0.0f;
        m_defendCtx.canTackle = false;
        break;

    case MatchState::HalfTime:
    case MatchState::FullTime:
        // EVERYONE STOPS PLAYING
        m_attackCtx.maxSpeedLimit = 250.f;  // Slow walk
        m_attackCtx.ballInfluence = 0.0f;   // Completely ignore the ball
        m_attackCtx.canTackle = false;

        m_defendCtx.maxSpeedLimit = 250.f;  // Slow walk
        m_defendCtx.ballInfluence = 0.0f;   // Completely ignore the ball
        m_defendCtx.canTackle = false;
        break;
    case MatchState::KickOff:               // <--- NEW: Stop players from swarming the center circle
        m_attackCtx.maxSpeedLimit = 800.f;  // <--- INCREASED: Let them jog to their starting formations!
        m_attackCtx.ballInfluence = 0.0f;
        m_attackCtx.canTackle = false;

        m_defendCtx.maxSpeedLimit = 800.f;  // <--- INCREASED
        m_defendCtx.ballInfluence = 0.0f;
        m_defendCtx.canTackle = false;
        break;
    case MatchState::FoulDelay:
    case MatchState::OutOfBoundsDelay: // <--- ADD HERE
        // Stop sprinting and ignore the ball while we wait for the set piece.
        m_attackCtx.maxSpeedLimit = 250.f;
        m_attackCtx.ballInfluence = 0.0f;
        m_attackCtx.canTackle = false;

        m_defendCtx.maxSpeedLimit = 250.f;
        m_defendCtx.ballInfluence = 0.0f;
        m_defendCtx.canTackle = false;
        break;
    case MatchState::RequestReplay:
    case MatchState::ReplayPlaying:
        // Completely freeze the AI behind the scenes. No moving, no touching the ball.
        m_attackCtx.maxSpeedLimit = 0.f;
        m_attackCtx.ballInfluence = 0.0f;
        m_attackCtx.canTackle = false;

        m_defendCtx.maxSpeedLimit = 0.f;
        m_defendCtx.ballInfluence = 0.0f;
        m_defendCtx.canTackle = false;
        break;
    }
}

TacticalContext MatchReferee::getTacticalContext(Team team, bool isTaker) const {
    // 1. Grab the base context for this team (Attacking or Defending)
    TacticalContext ctx = (team == m_awardedTo) ? m_attackCtx : m_defendCtx;

    // CRITICAL FIX: Give the context the actual state!
    ctx.state = m_matchState;

    // 2. Apply strict Overrides
    if (m_matchState != MatchState::InPlay) {

        // --- NEW: REPLAY SAFETY LOCK ---
        // If we are transitioning or holding for a replay, deny possession to EVERYONE.
        if (m_matchState == MatchState::RequestReplay || m_matchState == MatchState::ReplayPlaying) {
            ctx.isTaker = false;
            ctx.canPossess = false;
            ctx.ballInfluence = 0.0f;
            ctx.canTackle = false;
        }
        else if (isTaker) {
            ctx.isTaker = true;
            ctx.canPossess = true;
            ctx.ballInfluence = 1.0f;
            ctx.maxSpeedLimit = 500.f;
            ctx.canTackle = false;
        }
        else {
            ctx.isTaker = false;
            ctx.canPossess = false;
            ctx.canTackle = false;
        }
    }

    return ctx;
}

PositioningMask MatchReferee::getPositioningMask(Team team, PositionRole role, const Pitch& pitch) const {
    PositioningMask mask; // Defaults
    bool isAttacking = (team == m_awardedTo);
    bool isHome = (team == Team::Home);
    bool attackingHomeEnd = (m_awardedTo == Team::Away); 

    float attackingGoalX = attackingHomeEnd ? pitch.margin : pitch.totalWidth - pitch.margin;
    float boxEdgeX = attackingHomeEnd ? pitch.margin + 1650.f : pitch.totalWidth - pitch.margin - 1650.f;
    float halfwayX = pitch.totalWidth / 2.f;
    float centerY = pitch.totalHeight / 2.f;

    // ==========================================
    // --- NEW: EFFECTIVE STATE ---
    // If we are holding for a replay, players need to line up for the UPCOMING set piece!
    // ==========================================
    MatchState effectiveState = (m_matchState == MatchState::ReplayPlaying) ? m_pendingState : m_matchState;

    // 1. UNIVERSAL TAKER POSITIONING (Preferred Foot)
    if (m_setPieceTaker && isAttacking && role == m_setPieceTaker->getPositionRole() && effectiveState != MatchState::InPlay) {
        mask.useManualTarget = true;

        bool isRightFooted = (m_setPieceTaker->getPreferredFoot() == "Right");
        bool attackingAway = (m_awardedTo == Team::Home); 

        float xOff = attackingAway ? -45.f : 45.f;
        float yOff = 0.f;
        
        if (attackingAway) {
            yOff = isRightFooted ? -15.f : 15.f;
        } else {
            yOff = isRightFooted ? 15.f : -15.f;
        }

        mask.manualTarget = m_restartPos + sf::Vector2f(xOff, yOff);
        return mask;
    }

    // 2. STATE-SPECIFIC POSITIONING (Non-Takers)
    // NOTE: We now check 'effectiveState' instead of 'm_matchState'
    if (effectiveState == MatchState::Corner) {
        mask.useManualTarget = true;

        if (isAttacking) {
            if (role == PositionRole::LCenterBack || role == PositionRole::RCenterBack || role == PositionRole::Striker) {
                mask.manualTarget.x = attackingGoalX + (attackingHomeEnd ? 800.f : -800.f);
                mask.manualTarget.y = centerY + ((role == PositionRole::LCenterBack) ? -350.f : ((role == PositionRole::RCenterBack) ? 350.f : 0.f));
            }
            else if (role == PositionRole::LeftBack || role == PositionRole::RightBack) {
                mask.manualTarget.x = halfwayX + (isHome ? -400.f : 400.f);
                mask.manualTarget.y = (role == PositionRole::LeftBack) ? centerY - 1500.f : centerY + 1500.f;
            }
            else if (role != PositionRole::Goalkeeper) {
                mask.manualTarget.x = boxEdgeX + (attackingHomeEnd ? 200.f : -200.f);
                float yOffset = (role == PositionRole::LeftWing) ? -800.f : ((role == PositionRole::RightWing) ? 800.f : 0.f);
                mask.manualTarget.y = centerY + yOffset;
            }
        }
        else {
            if (role == PositionRole::Striker) {
                mask.manualTarget.x = halfwayX + (isHome ? 500.f : -500.f);
                mask.manualTarget.y = centerY;
            }
            else if (role == PositionRole::LeftWing || role == PositionRole::RightWing) {
                mask.manualTarget.x = boxEdgeX + (attackingHomeEnd ? -300.f : 300.f);
                mask.manualTarget.y = centerY + ((role == PositionRole::LeftWing) ? -1000.f : 1000.f);
            }
            else if (role != PositionRole::Goalkeeper) {
                mask.manualTarget.x = attackingGoalX + (attackingHomeEnd ? 500.f : -500.f);
                float yOffset = 0.f;
                if (role == PositionRole::LCenterBack) yOffset = -350.f;
                if (role == PositionRole::RCenterBack) yOffset = 350.f;
                if (role == PositionRole::LeftBack) yOffset = -750.f;
                if (role == PositionRole::RightBack) yOffset = 750.f;
                mask.manualTarget.y = centerY + yOffset;
            }
        }
    }
    else if (effectiveState == MatchState::Penalty) {
        if (role != PositionRole::Goalkeeper && role != PositionRole::Striker) {
            mask.homeOffset.x = isHome ? (attackingHomeEnd ? 2200.f : -2200.f) : (attackingHomeEnd ? 2200.f : -2200.f);
        }
    }
    else if (effectiveState == MatchState::KickOff) {
        mask.forwardLeashMod = 0.25f;
    }

    return mask;
}

void MatchReferee::awardFoul(FoulEvent foul, const Pitch& pitch, Ball& ball, const std::vector<Player*>& players, Player* victim) {
    if (m_matchState != MatchState::InPlay) return;

    bool isHomeDefending = (foul.offender->getTeam() == Team::Home);
    m_awardedTo = isHomeDefending ? Team::Away : Team::Home;
    m_fouledPlayer = victim;

    // ==========================================
    // --- DISCIPLINARY ACTIONS (CARDS) ---
    // ==========================================
    if (foul.type == FoulType::Sliding) {
        foul.offender->giveYellowCard();
        std::cout << "YELLOW CARD: " << foul.offender->getName() << "\n";
    }
    else if (foul.type == FoulType::Violent) {
        foul.offender->giveRedCard();
        std::cout << "STRAIGHT RED CARD: " << foul.offender->getName() << "\n";
    }

    // --- DELAY THE TELEPORT ---
    // We don't teleport the sent-off player here anymore. We wait until the delay finishes.

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

    // --- TRIGGER THE DELAY ---
    // 2.0 seconds gives us enough time to see the player fall over and slide on the grass
    m_foulDelayTimer = 2.0f;
    m_matchState = MatchState::FoulDelay;

    // Stop the ball dead so it doesn't roll into the net and trigger a goal during the delay!
    ball.setVelocity({ 0.f, 0.f });

    updateMatchContexts();
}

void MatchReferee::applyForfeitScore(bool homeForfeited) {
    // Standard rule: A forfeited match is recorded as a 3-0 loss
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

void MatchReferee::setupReplayTeleports(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players)
{
    // 1. Hide sent-off players
    for (Player* p : players) {
        if (p->isSentOff() && p->getPosition().x > 0.f) {
            p->setPosition({ -5000.f, -5000.f });
            p->setVelocity({ 0.f, 0.f });
        }
    }

    // 2. Setup the Set Piece (Teleports the taker, ball, and defensive wall)
    prepareRestart(m_pendingState, ball, pitch, players);

    // 3. OVERRIDE THE STATE! 
    // prepareRestart just set the state to FreeKick/Penalty. We force it into the Holding State.
    m_matchState = MatchState::ReplayPlaying;
    m_whistleTimer = 5.0f; // Freeze the timer just to be safe
}

void MatchReferee::resumeFromReplay()
{
    // The replay is officially over. NOW we start the actual set piece!
    m_matchState = m_pendingState;
    m_whistleTimer = 5.0f; // Start the 5-second countdown!

    // (Optional: You can trigger your Referee Whistle sound effect right here!)
}

void MatchReferee::startMatch(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players)
{
    m_half = 1;
    m_matchMinute = 0.0f;
    m_awardedTo = Team::Home; // Home team traditionally kicks off first

    // Set the target to the exact dead-center of the pitch
    m_restartPos = sf::Vector2f(pitch.totalWidth / 2.f, pitch.totalHeight / 2.f);

    // Call the restart logic to assign the taker and build the formations!
    prepareRestart(MatchState::KickOff, ball, pitch, players);
}
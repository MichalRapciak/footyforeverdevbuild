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
    else if (m_matchState == MatchState::GoalScored) {
        m_whistleTimer -= dt;
        if (m_whistleTimer <= 0.f) {
            m_restartPos = sf::Vector2f(pitch.totalWidth / 2.f, 3500.f);
            prepareRestart(MatchState::KickOff, ball, pitch, players);
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

        if (m_whistleTimer <= 0.f) {
            sf::Vector2f bVel = ball.getVelocity();
            float speed = std::sqrt(bVel.x * bVel.x + bVel.y * bVel.y);

            if (ball.getOwner() == nullptr && speed > 15.0f) {
                m_matchState = MatchState::InPlay;
                m_setPieceTaker = nullptr;
                updateMatchContexts();
            }
        }
        else {
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
            // CONTINUOUS ENCROACHMENT FORCEFIELD & WALL
            // ==========================================
            if (m_matchState == MatchState::FreeKick || m_matchState == MatchState::Corner || m_matchState == MatchState::Penalty) {

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

                for (Player* p : players) {
                    if (p == m_setPieceTaker || p->getTeam() == m_awardedTo) continue;

                    // 1. HARD-LOCK THE WALL
                    // Every frame, we physically root these players to the ground so the NPCController can't walk them away!
                    if (needsWall && wallCount < maxWallPlayers &&
                        p->getPositionRole() != PositionRole::Goalkeeper &&
                        p->getPositionRole() != PositionRole::LCenterBack &&
                        p->getPositionRole() != PositionRole::RCenterBack)
                    {
                        float offset = (wallCount - (maxWallPlayers / 2.0f) + 0.5f) * 60.f;
                        p->setPosition(wallCenter + (perp * offset));
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
                                p->setPosition(m_restartPos + (pushDir * 915.f)); // Snap to exactly 9.15m!
                                p->setVelocity({ 0.f, 0.f }); // Kill momentum
                            }
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
            m_matchState = MatchState::GoalKick;
            m_awardedTo = isHomeEnd ? Team::Home : Team::Away;
            m_restartPos = isHomeEnd ? sf::Vector2f(pitch.margin + 600.f, 3500.f)
                : sf::Vector2f(pitch.totalWidth - pitch.margin - 600.f, 3500.f);
            return;
        }

        // STANDARD LOGIC
        bool lastTouchedByHome = (lastOwner->getTeam() == Team::Home);

        if (isHomeEnd) {
            if (lastTouchedByHome) {
                m_matchState = MatchState::Corner;
                m_awardedTo = Team::Away;
                m_restartPos = (bPos.y < 3500.f) ? sf::Vector2f(pitch.margin, pitch.margin)
                    : sf::Vector2f(pitch.margin, pitch.totalHeight - pitch.margin);
            }
            else {
                m_matchState = MatchState::GoalKick;
                m_awardedTo = Team::Home;
                m_restartPos = sf::Vector2f(pitch.margin + 600.f, 3500.f);
            }
        }
        else { // Away End
            if (lastTouchedByHome) {
                m_matchState = MatchState::GoalKick;
                m_awardedTo = Team::Away;
                m_restartPos = sf::Vector2f(pitch.totalWidth - pitch.margin - 600.f, 3500.f);
            }
            else {
                m_matchState = MatchState::Corner;
                m_awardedTo = Team::Home;
                m_restartPos = (bPos.y < 3500.f) ? sf::Vector2f(pitch.totalWidth - pitch.margin, pitch.margin)
                    : sf::Vector2f(pitch.totalWidth - pitch.margin, pitch.totalHeight - pitch.margin);
            }
        }
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
    if (state == MatchState::GoalScored || state == MatchState::HalfTime || state == MatchState::FullTime) {
        return;
    }

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
            else if (state == MatchState::Corner || state == MatchState::FreeKick) {
                if (p->getPositionRole() != PositionRole::Goalkeeper &&
                    p->getPositionRole() != PositionRole::LCenterBack &&
                    p->getPositionRole() != PositionRole::RCenterBack) {
                    float d = std::sqrt(std::pow(p->getPosition().x - m_restartPos.x, 2) + std::pow(p->getPosition().y - m_restartPos.y, 2));
                    if (d < closestDist) {
                        closestDist = d;
                        m_setPieceTaker = m_fouledPlayer;;
                    }
                }
            }
        }
    }

    // 3. Position the Taker 
    if (m_setPieceTaker) {
        m_setPieceTaker->setPosition(m_restartPos + sf::Vector2f(-20.f, 0.f));
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
                // Space them out 60px apart along the perpendicular line
                float offset = (wallCount - (maxWallPlayers / 2.0f) + 0.5f) * 60.f;
                p->setPosition(wallCenter + (perp * offset));
                wallCount++;
                continue; // Skip the generic 9.15m push
            }

            // B. Generic 9.15m push for everyone else
            float distToBall = std::sqrt(std::pow(pPos.x - m_restartPos.x, 2) + std::pow(pPos.y - m_restartPos.y, 2));
            if (distToBall < 915.f) {
                sf::Vector2f pushDir = pPos - m_restartPos;
                float len = std::sqrt(pushDir.x * pushDir.x + pushDir.y * pushDir.y);
                if (len > 0.1f) {
                    pushDir /= len;
                    p->setPosition(m_restartPos + (pushDir * 920.f));
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

        m_defendCtx.maxSpeedLimit = 300.f; // Walk dejectedly back to center
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
    }
}

TacticalContext MatchReferee::getTacticalContext(Team team, bool isTaker) const {
    // 1. Grab the base context for this team (Attacking or Defending)
    TacticalContext ctx = (team == m_awardedTo) ? m_attackCtx : m_defendCtx;

    // ==========================================
    // CRITICAL FIX: Give the context the actual state!
    // ==========================================
    ctx.state = m_matchState;

    // 2. Apply strict Overrides
    if (m_matchState != MatchState::InPlay) {
        if (isTaker) {
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
    PositioningMask mask = (team == m_awardedTo) ? m_attackMask : m_defendMask;
    bool isAttacking = (team == m_awardedTo);
    bool isHome = (team == Team::Home);
    bool attackingHomeEnd = (m_awardedTo == Team::Away); // If Away is attacking, they attack the Home end

    if (m_matchState == MatchState::Corner) {
        float boxEdgeX = attackingHomeEnd ? pitch.margin + 1000.f : pitch.totalWidth - pitch.margin - 1000.f;
        
        if (role == PositionRole::LCenterBack || role == PositionRole::RCenterBack || role == PositionRole::Striker) {
            // Big guys crash the box!
            mask.homeOffset.x = isHome ? (boxEdgeX - pitch.halfwayLineX) : (boxEdgeX - pitch.halfwayLineX);
            mask.homeOffset.y = (role == PositionRole::Striker) ? 0.f : ((role == PositionRole::LCenterBack) ? -300.f : 300.f);
        } else if (role != PositionRole::Goalkeeper) {
            // Everyone else lingers outside the box
            mask.homeOffset.x = isHome ? (boxEdgeX - pitch.halfwayLineX) * 0.8f : (boxEdgeX - pitch.halfwayLineX) * 0.8f;
        }
    }
    else if (m_matchState == MatchState::Penalty) {
        // Everyone lines up on the edge of the D
        if (role != PositionRole::Goalkeeper && role != PositionRole::Striker) {
             mask.homeOffset.x = isHome ? (attackingHomeEnd ? 2200.f : -2200.f) : (attackingHomeEnd ? 2200.f : -2200.f);
        }
    }

    return mask;
}

void MatchReferee::awardFoul(FoulEvent foul, const Pitch& pitch, Ball& ball, const std::vector<Player*>& players, Player* victim) {
    if (m_matchState != MatchState::InPlay) return; // Can't foul while play is dead

    bool isHomeDefending = (foul.offender->getTeam() == Team::Home);
    m_awardedTo = isHomeDefending ? Team::Away : Team::Home;
    m_fouledPlayer = victim;

    // Check if foul happened inside the offender's penalty box
    bool inBox = false;
    if (isHomeDefending && pitch.homePenaltyBox.contains(foul.location)) inBox = true;
    if (!isHomeDefending && pitch.awayPenaltyBox.contains(foul.location)) inBox = true;

    if (inBox) {
        m_matchState = MatchState::Penalty;
        m_restartPos = isHomeDefending ? pitch.homePenaltySpot : pitch.awayPenaltySpot;
    }
    else {
        m_matchState = MatchState::FreeKick;
        m_restartPos = foul.location;
    }

    prepareRestart(m_matchState, ball, pitch, players);
}
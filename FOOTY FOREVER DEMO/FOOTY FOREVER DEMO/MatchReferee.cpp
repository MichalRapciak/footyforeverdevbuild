#include "MatchReferee.h"
#include "Player.h"
#include "Ball.h"
#include "Pitch.h"
#include <iostream>


void MatchReferee::update(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, float dt)
{
    if (m_matchState == MatchState::InPlay)
    {
        // Only check boundaries while the ball is live
        checkBoundaries(ball, pitch);

        // If checkBoundaries changed the state, instantly prepare the restart.
        if (m_matchState != MatchState::InPlay) {
            prepareRestart(m_matchState, ball, pitch, players);
        }
    }
    else if (m_matchState == MatchState::GoalScored)
    {
        // Wait for celebrations, then set up KickOff
        m_whistleTimer -= dt;
        if (m_whistleTimer <= 0.f) {
            m_restartPos = sf::Vector2f(pitch.totalWidth / 2.f, 3500.f);
            prepareRestart(MatchState::KickOff, ball, pitch, players);
        }
    }
    else
    {
        // --- ALL DEAD BALL STATES (ThrowIn, GoalKick, Corner, etc.) ---
        m_whistleTimer -= dt;

        if (m_whistleTimer <= 0.f) {
            sf::Vector2f bVel = ball.getVelocity();
            float speed = std::sqrt(bVel.x * bVel.x + bVel.y * bVel.y);

            // If the ball suddenly gets velocity, the set piece was taken!
            if (ball.getOwner() == nullptr && speed > 15.0f) {
                m_matchState = MatchState::InPlay;
                m_setPieceTaker = nullptr;

                // CRITICAL: Reset the contexts back to open-play defaults!
                updateMatchContexts();
            }
        }
        else {
            // Keep the ball glued to the spot while waiting for the whistle
            if (m_setPieceTaker) {
                m_setPieceTaker->setVelocity({ 0.f, 0.f });
                ball.setVelocity({ 0.f, 0.f });
                ball.possess(m_setPieceTaker);
            }
            else {
                ball.setPosition(m_restartPos);
                ball.setVelocity({ 0.f, 0.f });
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

        if (checkGoalScored(ball, pitch)) {
            //m_matchState = MatchState::GoalScored;
            return;
        }

        bool isHomeEnd = (bPos.x < pitch.margin);

        // FALLBACK: If lastOwner is null, we assume it's a Goal Kick for the keeper
        if (lastOwner == nullptr) {
            m_matchState = MatchState::GoalKick;
            m_awardedTo = isHomeEnd ? Team::Home : Team::Away;
            m_restartPos = isHomeEnd ? sf::Vector2f(pitch.margin + 600.f, 3500.f)
                : sf::Vector2f(pitch.totalWidth - pitch.margin - 600.f, 3500.f);
            return;
        }

        // --- STANDARD LOGIC (Safe to use lastOwner now) ---
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

    // 1. Vertical Check: If the ball is flying over the crossbar, it's not a goal
    // We add a tiny buffer (5px) for safety
    if (bZ > 244.f + 5.f) return false;

    // 2. Horizontal Check: Between the posts (7.32m = 732px wide)
    // Centered at Y = 3500.f (the pitch center)
    float goalTopY = 3500.f - 366.f;
    float goalBottomY = 3500.f + 366.f;

    bool withinPosts = (bPos.y > goalTopY && bPos.y < goalBottomY);
    if (!withinPosts) return false;

    // 3. Depth Check: Did it cross the Home line or Away line?
    // We require the ball to be FULLY over the line (bRadius approx 12px)
    float goalLineBuffer = 24.f;

    // HOME GOAL (Left side of screen)
    if (bPos.x < pitch.margin - goalLineBuffer) {
        m_awardedTo = Team::Away; // Away team scores in Home goal
        m_homeScore++; // If you have score variables in Referee
       // std::cout << "GOAL FOR AWAY TEAM!\n";
        return true;
    }

    // AWAY GOAL (Right side of screen)
    if (bPos.x > pitch.totalWidth - pitch.margin + goalLineBuffer) {
        m_awardedTo = Team::Home; // Home team scores in Away goal
        m_awayScore++;
       // std::cout << "GOAL FOR HOME TEAM!\n";
        return true;
    }

    return false;
}

void MatchReferee::prepareRestart(MatchState state, Ball& ball, const Pitch& pitch, const std::vector<Player*>& players) {
    // CRITICAL: Generate the new Tactical Contexts for this specific set piece!
    updateMatchContexts();

    m_matchState = state;
    m_whistleTimer = 1.5f; // Wait 1.5 seconds before allowing the kick

    // 1. Reset the ball
    ball.setPosition(m_restartPos);
    ball.setVelocity({ 0.f, 0.f });
    ball.z = 0.f;
    ball.vz = 0.f;

    // 2. Find the Taker
    float closestDist = 99999.f;
    m_setPieceTaker = nullptr;

    for (Player* p : players) {
        p->setVelocity({ 0.f, 0.f }); // Stop momentum

        if (p->getTeam() == m_awardedTo) {
            // --- 1. GOAL KICK LOGIC ---
            if (state == MatchState::GoalKick) {
                if (p->getPositionRole() == PositionRole::Goalkeeper) {
                    m_setPieceTaker = p;
                    ball.possess(m_setPieceTaker);
                    break; // Found the keeper, stop looking
                }
            }
            // --- 2. PENALTY LOGIC ---
            else if (state == MatchState::Penalty) {
                if (p->getPositionRole() == PositionRole::Striker) {
                    m_setPieceTaker = p;
                    ball.possess(m_setPieceTaker);
                    break; // Found the Striker, stop looking
                }
            }
            // --- 3. THROW-IN LOGIC ---
            else if (state == MatchState::ThrowIn) {
                if (p->getPositionRole() != PositionRole::Goalkeeper) {
                    float d = std::sqrt(std::pow(p->getPosition().x - m_restartPos.x, 2) +
                        std::pow(p->getPosition().y - m_restartPos.y, 2));
                    if (d < closestDist) {
                        closestDist = d;
                        m_setPieceTaker = p;
                        ball.possess(m_setPieceTaker);
                    }
                }
            }
            // --- 4. CORNER & FREE KICK LOGIC ---
            else {
                // Find closest player to the ball to take the kick
                // Exclude GK and Center Backs (we want CBs in the box for headers!)
                if (p->getPositionRole() != PositionRole::Goalkeeper &&
                    p->getPositionRole() != PositionRole::LCenterBack &&
                    p->getPositionRole() != PositionRole::RCenterBack) {

                    float d = std::sqrt(std::pow(p->getPosition().x - m_restartPos.x, 2) +
                        std::pow(p->getPosition().y - m_restartPos.y, 2));

                    if (d < closestDist) {
                        closestDist = d;
                        m_setPieceTaker = p;
                    }
                }
            }
        }
    }

    // 3. Position the Taker 
    if (m_setPieceTaker) {
        // Put the taker right next to the ball
        m_setPieceTaker->setPosition(m_restartPos + sf::Vector2f(-20.f, 0.f));
    }

    // 4. Enforce Pitch Rules for all other players
    for (Player* p : players) {
        if (p == m_setPieceTaker) continue; // Don't move the taker!

        sf::Vector2f pPos = p->getPosition();

        // --- PENALTY ENFORCEMENT ---
        if (state == MatchState::Penalty) {
            if (p->getPositionRole() == PositionRole::Goalkeeper && p->getTeam() != m_awardedTo) {
                // Defending GK goes strictly on the goal line center
                p->setPosition(m_awardedTo == Team::Home ? pitch.awayGoalCenter : pitch.homeGoalCenter);
            }
            else {
                // ALL other players must be outside the box AND the arc
                bool homeDefending = (m_awardedTo == Team::Away);
                bool inBox = homeDefending ? pitch.homePenaltyBox.contains(pPos) : pitch.awayPenaltyBox.contains(pPos);
                bool inArc = pitch.isInPenaltyArc(pPos, homeDefending);

                if (inBox || inArc) {
                    // Shove them outside the penalty box toward the midfield
                    p->setPosition({ pPos.x + (homeDefending ? 600.f : -600.f), pPos.y });
                }
            }
        }
        // --- STANDARD 9.15m RULE (Free Kicks & Corners) ---
        else if (p->getTeam() != m_awardedTo && state != MatchState::ThrowIn && state != MatchState::GoalKick) {
            float distToBall = std::sqrt(std::pow(pPos.x - m_restartPos.x, 2) + std::pow(pPos.y - m_restartPos.y, 2));

            // 9.15 meters = 915 pixels
            if (distToBall < 915.f) {
                // Push them away from the ball along the vector between them
                sf::Vector2f pushDir = pPos - m_restartPos;
                float len = std::sqrt(pushDir.x * pushDir.x + pushDir.y * pushDir.y);
                if (len > 0.1f) {
                    pushDir /= len; // Normalize
                    p->setPosition(m_restartPos + (pushDir * 920.f)); // Push just outside the radius
                }
            }
        }
    }
}

void MatchReferee::updateMatchContexts() {
    // RESET TO DEFAULTS
    m_attackCtx = TacticalContext(); // Full speed, can tackle, etc.
    m_defendCtx = TacticalContext();
    m_attackMask = PositioningMask();
    m_defendMask = PositioningMask();

    switch (m_matchState) {
    case MatchState::InPlay:
        // Standard settings (Defaults are fine)
        break;

    case MatchState::GoalKick:
        // --- ATTACKING TEAM (Taking the kick) ---
        m_attackCtx.maxSpeedLimit = 300.f;  // Walk to spots
        m_attackCtx.ballInfluence = 0.0f;   // Don't chase the keeper!
        m_attackCtx.canTackle = false;

        // --- DEFENDING TEAM (Pressing) ---
        m_defendCtx.maxSpeedLimit = 450.f;  // Brisk walk/jog
        m_defendCtx.ballInfluence = 0.2f;   // Squeeze toward the box
        m_defendMask.homeOffset = (m_awardedTo == Team::Home) ?
            sf::Vector2f(-2000.f, 0.f) : sf::Vector2f(2000.f, 0.f);
        break;

    case MatchState::ThrowIn:
        m_attackCtx.maxSpeedLimit = 400.f;
        m_attackCtx.ballInfluence = 0.5f;   // Jitter near the touchline
        m_attackCtx.canTackle = false;

        m_defendCtx.maxSpeedLimit = 400.f;
        m_defendCtx.awarenessMod = 1.5f;    // Defenders become "Hyper Aware" for marking
        m_defendCtx.canTackle = false;
        break;

    case MatchState::GoalScored:
        m_attackCtx.maxSpeedLimit = 500.f;
        m_attackCtx.ballInfluence = 0.0f;   // Ignore the ball during celebration
        m_attackCtx.canTackle = false;
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

void MatchReferee::awardFoul(FoulEvent foul, const Pitch& pitch, Ball& ball, const std::vector<Player*>& players) {
    if (m_matchState != MatchState::InPlay) return; // Can't foul while play is dead

    bool isHomeDefending = (foul.offender->getTeam() == Team::Home);
    m_awardedTo = isHomeDefending ? Team::Away : Team::Home;

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
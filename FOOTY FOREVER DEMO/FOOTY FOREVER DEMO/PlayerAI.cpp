#include "PlayerAI.h"
#include "NPCPlayer.h"
#include "UserPlayer.h"
#include "Ball.h"
#include "Pitch.h"
#include "TeamAI.h"
#include "AimAssist.h"
#include "PhysicsEngine.h"
#include <cmath>
#include <algorithm>

// ==========================================
// --- HELPER MATH ---
// ==========================================

float PlayerAI::dist(sf::Vector2f p1, sf::Vector2f p2) {
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    return std::sqrt(dx * dx + dy * dy);
}

sf::Vector2f PlayerAI::normalize(sf::Vector2f source) {
    float length = std::sqrt(source.x * source.x + source.y * source.y);
    if (length != 0) return source / length;
    return source;
}

Player* PlayerAI::findNearestOpponent(const sf::Vector2f& npcPos, const std::vector<Player*>& opponents) {
    Player* nearest = nullptr;
    float minDistanceSq = std::numeric_limits<float>::max();

    for (auto& opp : opponents) {
        if (opp->getPositionRole() == PositionRole::Goalkeeper) continue;

        sf::Vector2f diff = npcPos - opp->getPosition();
        float distSq = (diff.x * diff.x) + (diff.y * diff.y);

        if (distSq < minDistanceSq) {
            minDistanceSq = distSq;
            nearest = opp;
        }
    }
    return nearest;
}

Player* PlayerAI::findBestThreat(const sf::Vector2f& npcPos, const std::vector<Player*>& opponents, const TacticalZone& zone, MatchState matchstate) {
    Player* bestThreat = nullptr;
    float minDist = zone.markingRange;

    for (auto& opp : opponents) {
        if (opp->getPositionRole() == PositionRole::Goalkeeper) continue;
        float d = dist(npcPos, opp->getPosition());

        // Priority 1: The guy with the ball
        if (opp->getBallPossession() && d < zone.markingRange * 1.5f && matchstate == MatchState::InPlay) {
            return opp;
        }
        // Priority 2: The closest guy in my zone
        if (d < minDist) {
            minDist = d;
            bestThreat = opp;
        }
    }
    return bestThreat;
}

// ==========================================
// --- OFF-BALL MOVEMENT ---
// ==========================================

sf::Vector2f PlayerAI::decideTargetPosition(NPCPlayer& npc, Ball& ball, const Pitch& pitch, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, Player* firstResponder, PositioningMask mask, const TeamAI& teamAI)
{
    if (mask.useManualTarget) return mask.manualTarget;

    bool isHomeSide = teamAI.isHome();
    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f npcPos = npc.getPosition();
    float distToBall = dist(npcPos, ballPos);
    sf::Vector2f goalPos = isHomeSide ? sf::Vector2f(pitch.margin, 3500.f) : sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f);

    // ==========================================
    // 1. CALCULATE DYNAMIC ANCHOR
    // ==========================================
    // We take the static Kickoff Pos and slide it based on the Manager's Team Shift!
    sf::Vector2f dynamicAnchor = npc.getHomePosition();

    PositionRole role = npc.getPositionRole();
    bool isDefender = (role == PositionRole::CenterBack || role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);
    dynamicAnchor.x += teamAI.getDefensiveLineOffset(isDefender);

    TacticalZone zone = teamAI.getEffectiveTacticalZone(npc.getPlaystyle());

    // ==========================================
    // 2. TACTICAL POSITIONING (Relative to Anchor)
    // ==========================================
    // Use the dynamicAnchor as the base for the tactical target
    sf::Vector2f target = applyTacticalPositioning(npc, ball, dynamicAnchor, ballPos, goalPos, state, zone, pitch, team, opponents, teamAI);

    // Apply set-piece masks
    target += mask.homeOffset;
    if (mask.lateralSqueeze > 0.0f) {
        target.y = target.y + ((ballPos.y - target.y) * mask.lateralSqueeze);
    }

    // ==========================================
    // 3. EMERGENCY CHASE (The Instinct)
    // ==========================================
    if (shouldEmergencyChase(npc, firstResponder, distToBall, pitch, ball, MatchState::InPlay, teamAI)) {
        if (ball.hasOwner() && ball.getOwner()->getTeam() != npc.getTeam()) {
            float pressDistance = std::max(70.f, 200.f - (npc.getAggression() * 1.5f));
            target = ballPos + (normalize(goalPos - ballPos) * pressDistance);
        }
        else {
            target = calculateInterceptionPoint(npc, ball);
        }
        return target; // Do not clamp while chasing!
    }

    // ==========================================
    // 4. CLAMP TO THE DYNAMIC ZONE
    // ==========================================
    // Crucial: Clamp relative to the MOVING anchor, not the static kickoff spot!
    return clampToTacticalZone(target, dynamicAnchor, zone, distToBall, isHomeSide, teamAI);
}

sf::Vector2f PlayerAI::evaluateShapeAndSpace(NPCPlayer& npc, sf::Vector2f currentTarget, sf::Vector2f ballPos, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, const TeamAI& teamAI, const TacticalZone& zone, Ball& ball, const Pitch& pitch)
{
    sf::Vector2f spatialCorrection(0.f, 0.f);
    sf::Vector2f npcPos = npc.getPosition();
    float awareness = npc.getAwareness() / 100.0f;

    bool isMid = (npc.getPositionRole() == PositionRole::CenterMid || npc.getPositionRole() == PositionRole::AttackingMid || npc.getPositionRole() == PositionRole::LeftMid || npc.getPositionRole() == PositionRole::RightMid ||
        npc.getPositionRole() == PositionRole::DefensiveMid);

    if (state == TeamState::Attacking) {
        // ---------------------------------------------------------
        // 1. PASSING TRIANGLES (DNA: Ball Influence & Roaming)
        // ---------------------------------------------------------
        // Only players who actually want the ball (Playmakers) will actively drop to form triangles.
        // Poachers (low influence) will ignore this and stay up top.
        if (zone.ballInfluence > 0.3f) {
            Player* nearestTeammate = nullptr;
            float minDist = 9999.f;

            for (Player* tm : team) {
                if (tm == &npc || tm->getBallPossession()) continue;
                float d = dist(npcPos, tm->getPosition());
                if (d < minDist) { minDist = d; nearestTeammate = tm; }
            }

            if (nearestTeammate && minDist < 3500.f) {
                sf::Vector2f tmPos = nearestTeammate->getPosition();
                sf::Vector2f midPoint = (ballPos + tmPos) * 0.5f;
                sf::Vector2f baseLine = tmPos - ballPos;

                sf::Vector2f perp(-baseLine.y, baseLine.x);
                float baseLen = std::sqrt(baseLine.x * baseLine.x + baseLine.y * baseLine.y);

                if (baseLen > 10.f) {
                    perp /= baseLen;

                    float longBallPref = teamAI.getPassingLengthPref();
                    float widthPref = teamAI.getAttackingWidthPref();
                    float minSpacing = 1000.f + (longBallPref * 1000.f) + (widthPref * 500.f);

                    float triangleHeight = std::max(baseLen * 0.866f, minSpacing);

                    sf::Vector2f idealPocket1 = midPoint + (perp * triangleHeight);
                    sf::Vector2f idealPocket2 = midPoint - (perp * triangleHeight);

                    float d1 = dist(currentTarget, idealPocket1);
                    float d2 = dist(currentTarget, idealPocket2);
                    sf::Vector2f bestPocket = (d1 < d2) ? idealPocket1 : idealPocket2;

                    sf::Vector2f steer = bestPocket - currentTarget;
                    float steerLen = std::sqrt(steer.x * steer.x + steer.y * steer.y);
                    if (steerLen > 0.1f) {
                        // INJECTION: Roaming Freedom determines how aggressively they stretch the triangle
                        float playstyleMultiplier = zone.roamingFreedom * zone.ballInfluence;
                        spatialCorrection += (steer / steerLen) * std::min(steerLen, 1000.f) * awareness * playstyleMultiplier;
                    }
                }
            }
        }

        // ---------------------------------------------------------
        // 2. AGGRESSIVE COVER SHADOW ESCAPE
        // ---------------------------------------------------------
        sf::Vector2f toBall = ballPos - npcPos;
        float distToBallExact = std::sqrt(toBall.x * toBall.x + toBall.y * toBall.y);

        if (distToBallExact > 10.f && isMid) {
            sf::Vector2f dirToBall = toBall / distToBallExact;
            bool isInShadow = false;

            for (Player* opp : opponents) {
                sf::Vector2f toOpp = opp->getPosition() - npcPos;
                float oppDist = std::sqrt(toOpp.x * toOpp.x + toOpp.y * toOpp.y);

                // Is the opponent standing between me and the ball?
                if (oppDist < distToBallExact) {
                    float dot = (toOpp.x * dirToBall.x) + (toOpp.y * dirToBall.y);
                    if (dot > 0.f) {
                        sf::Vector2f projection = dirToBall * dot;
                        sf::Vector2f rejection = toOpp - projection;
                        float distFromLane = std::sqrt(rejection.x * rejection.x + rejection.y * rejection.y);

                        // If the opponent is within 2 meters of the direct passing lane
                        if (distFromLane < 200.f) {
                            isInShadow = true;

                            // 1. Calculate the slide direction (perpendicular to the ball)
                            sf::Vector2f slideDir(-dirToBall.y, dirToBall.x);
                            float side = ((slideDir.x * rejection.x + slideDir.y * rejection.y) > 0) ? -1.f : 1.f;

                            // 2. Playmakers are desperate to be an option, they will sprint into the open lane
                            float evadeDesire = 600.f * awareness;

                            // Combine lateral sliding with a step TOWARD the ball to demand it
                            spatialCorrection += (slideDir * side * evadeDesire) + (dirToBall * evadeDesire * 0.5f);
                            break; // Reacting to the most immediate blocker is enough
                        }
                    }
                }
            }

            // If they are completely open, encourage them to sit in that pocket and hold still
            if (!isInShadow && distToBallExact < 2500.f) {
                spatialCorrection *= 0.5f; // Dampen other spatial noise to stay in the good spot
            }
        }
        // ---------------------------------------------------------
        // 3. TIKI-TAKA SUPPORT (Show to Feet)
        // ---------------------------------------------------------
        float tikiTakaPref = 1.0 - teamAI.getPassingLengthPref(); // 1.0 = Tiki-Taka, 0.0 = Long Ball
        distToBallExact = dist(npcPos, ballPos);

        // If the manager wants short passes, and we aren't already right next to the ball...
        if (tikiTakaPref > 0.3f && distToBallExact > 700.f && distToBallExact < 3500.f) {

            // Check if I am the absolute closest teammate to the ball carrier
            bool amIClosest = true;
            for (Player* tm : team) {
                if (tm == &npc || tm->getBallPossession()) continue;
                if (dist(tm->getPosition(), ballPos) < distToBallExact) {
                    amIClosest = false;
                    break;
                }
            }

            // If I am the closest, it is my job to rescue the ball carrier!
            if (amIClosest) {
                sf::Vector2f toBallVec = ballPos - npcPos;

                // You requested 40% of the way there:
                float shiftMagnitude = distToBallExact * 0.40f;

                // --- DNA INJECTION ---
                // A rigid Target Man (0.1 freedom) might only step slightly towards the ball.
                // A fluid False 9 or Roaming Playmaker (0.9 freedom) will sprint all the way to the winger's toes.
                float willingness = 0.4f + (zone.roamingFreedom * 0.6f);

                spatialCorrection += normalize(toBallVec) * shiftMagnitude * tikiTakaPref * willingness * awareness;
            }
        }
        // ---------------------------------------------------------
        // 4. MIDFIELD POCKET FINDING
        // ---------------------------------------------------------
        if (isMid) {
            Player* nearestOpp = findNearestOpponent(currentTarget, opponents);
            if (nearestOpp && dist(currentTarget, nearestOpp->getPosition()) < 700.f) {
                // If a defender is too close, shift laterally to open a clear passing lane
                sf::Vector2f toOpp = nearestOpp->getPosition() - currentTarget;
                float escapeY = (toOpp.y > 0) ? -1.0f : 1.0f;

                // Roaming Playmakers will aggressively seek these lateral gaps
                spatialCorrection.y += escapeY * 350.f * awareness * zone.roamingFreedom;
            }
        }
    }
    else {
        // ---------------------------------------------------------
        // 3. STRUCTURAL INTEGRITY & DEFENSIVE CHAINING
        // ---------------------------------------------------------
        Player* adjacentPartnerL = nullptr;
        Player* adjacentPartnerR = nullptr;
        float minDistL = 9999.f;
        float minDistR = 9999.f;

        bool isDefender = (npc.getPositionRole() == PositionRole::CenterBack ||
            npc.getPositionRole() == PositionRole::LeftBack ||
            npc.getPositionRole() == PositionRole::RightBack ||
            npc.getPositionRole() == PositionRole::LeftWingBack ||
            npc.getPositionRole() == PositionRole::RightWingBack);

        // Find my true positional partners (e.g., LB finds the LCB, CB finds the RCB)
        for (Player* tm : team) {
            if (tm == &npc || tm->getPositionRole() == PositionRole::Goalkeeper || tm->isSentOff()) continue;

            bool tmIsDef = (tm->getPositionRole() == PositionRole::CenterBack || tm->getPositionRole() == PositionRole::LeftBack || tm->getPositionRole() == PositionRole::RightBack || tm->getPositionRole() == PositionRole::LeftWingBack || tm->getPositionRole() == PositionRole::RightWingBack);
            bool tmIsMid = (tm->getPositionRole() == PositionRole::CenterMid || tm->getPositionRole() == PositionRole::DefensiveMid || tm->getPositionRole() == PositionRole::LeftMid || tm->getPositionRole() == PositionRole::RightMid || tm->getPositionRole() == PositionRole::AttackingMid);

            if ((isDefender && tmIsDef) || (isMid && tmIsMid)) {
                // Use HOME POSITION to determine adjecency, not current position!
                float homeDY = tm->getHomePosition().y - npc.getHomePosition().y;

                // Left partner (Negative Y)
                if (homeDY < -50.f && std::abs(homeDY) < minDistL) {
                    minDistL = std::abs(homeDY);
                    adjacentPartnerL = tm;
                }
                // Right partner (Positive Y)
                else if (homeDY > 50.f && std::abs(homeDY) < minDistR) {
                    minDistR = std::abs(homeDY);
                    adjacentPartnerR = tm;
                }
            }
        }

        // Lambda to execute Gap Filling and Chaining
        auto applyGapFill = [&](Player* partner, float dirY) {
            if (!partner) return;

            // A. STANDARD CHAINING: Don't let the physical gap get too big
            float currentLateralGap = std::abs(partner->getPosition().y - npcPos.y);
            float maxSafeGap = 800.f - (teamAI.getDefensiveDepthPref() * 200.f);

            if (currentLateralGap > maxSafeGap) {
                float pinchMagnitude = (currentLateralGap - maxSafeGap) * 0.7f * awareness;
                spatialCorrection.y += dirY * pinchMagnitude;
            }

            // B. STRUCTURAL INTEGRITY (Covering rushed-out teammates)
            float tmDisplacementX = std::abs(partner->getPosition().x - partner->getHomePosition().x);
            float tmDisplacementY = std::abs(partner->getPosition().y - partner->getHomePosition().y);

            // If the partner has rushed out to tackle, or been dragged wide (Vacated their zone)
            if (tmDisplacementX > 700.f || tmDisplacementY > 700.f) {
                // High awareness + High positional freedom = extremely willing to slide over and cover!
                float coverWillingness = awareness * (0.5f + (zone.roamingFreedom * 0.7f));

                // Shift towards the partner's VACATED home Y coordinate to plug the hole
                float gapToFill = std::abs(partner->getHomePosition().y - npcPos.y);
                float fillAmount = std::min(gapToFill * 0.6f, 600.f);

                spatialCorrection.y += dirY * fillAmount * coverWillingness;
            }
            };

        applyGapFill(adjacentPartnerL, -1.0f);
        applyGapFill(adjacentPartnerR, 1.0f);

        // ==========================================
        // 4. VERTICAL LINE SYNC (Holding the Offside Trap)
        // ==========================================

        if (isDefender) {
            float avgLineX = 0.f;
            int defCount = 0;

            // Calculate the actual, real-time center of mass for the backline
            for (Player* tm : team) {
                bool tmIsDef = (tm->getPositionRole() == PositionRole::CenterBack ||
                    tm->getPositionRole() == PositionRole::LeftBack ||
                    tm->getPositionRole() == PositionRole::RightBack ||
                    tm->getPositionRole() == PositionRole::LeftWingBack ||
                    tm->getPositionRole() == PositionRole::RightWingBack);
                if (tmIsDef && !tm->isSentOff()) {
                    avgLineX += tm->getPosition().x;
                    defCount++;
                }
            }

            if (defCount > 0) {
                avgLineX /= defCount;
                float xDiff = avgLineX - npcPos.x;

                float discipline = 1.0f - (zone.roamingFreedom * 0.6f);
                sf::Vector2f toBall = ballPos - npcPos;
                float distToBall = std::sqrt(toBall.x * toBall.x + toBall.y * toBall.y);
                // --- THE STOPPER EXEMPTION ---
                // Are we the closest defender to the ball?
                bool isStopper = false;
                if (distToBall < 1800.f) {
                    isStopper = true;
                    for (Player* tm : team) {
                        if (tm == &npc || tm->getPositionRole() == PositionRole::Goalkeeper || tm->isSentOff()) continue;

                        bool tmIsDef = (tm->getPositionRole() == PositionRole::CenterBack ||
                            tm->getPositionRole() == PositionRole::LeftBack ||
                            tm->getPositionRole() == PositionRole::RightBack ||
                            tm->getPositionRole() == PositionRole::LeftWingBack ||
                            tm->getPositionRole() == PositionRole::RightWingBack);

                        if (tmIsDef && dist(tm->getPosition(), ballPos) < distToBall) {
                            isStopper = false;
                            break;
                        }
                    }
                }

                // Only forcefully sync if the line is breaking AND we aren't actively stepping up to press!
                if (std::abs(xDiff) > 60.f && !isStopper) {
                    spatialCorrection.x += xDiff * 0.85f * discipline * awareness;
                }
            }
        }
        bool isHomeSide = npc.getTeam() == Team::Home;
        // ==========================================
        // 5. MIDFIELD SPACE COVERAGE & MARKING
        // ==========================================
        if (isMid) {
            Player* nearestOpp = findNearestOpponent(npcPos, opponents);
            // Ignore the ball carrier (the Stopper handles them) and the Goalkeeper
            if (nearestOpp && nearestOpp->getPositionRole() != PositionRole::Goalkeeper && nearestOpp != ball.getOwner()) {
                sf::Vector2f oppPos = nearestOpp->getPosition();

                // If an attacker drifts into the space between defense and midfield (within 1600px)
                if (dist(npcPos, oppPos) < 1600.f) {
                    sf::Vector2f goalPos = isHomeSide ? sf::Vector2f(pitch.margin, 3500.f) : sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f);
                    sf::Vector2f toGoal = normalize(goalPos - oppPos);

                    // Pull the midfielder to sit exactly 3 meters (300px) goal-side of the threat!
                    sf::Vector2f idealMarkingPos = oppPos + (toGoal * 300.f);
                    sf::Vector2f markCorrection = idealMarkingPos - npcPos;

                    // Players with higher awareness track their marks much tighter
                    float markPull = 0.5f * awareness;
                    spatialCorrection += markCorrection * markPull;
                }
            }
        }
    }

    return spatialCorrection;
}

sf::Vector2f PlayerAI::applyTacticalPositioning(NPCPlayer& npc, Ball& ball, sf::Vector2f homePos, sf::Vector2f ballPos, sf::Vector2f goalPos, TeamState state, TacticalZone zone, const Pitch& pitch, const std::vector<Player*>& team, const std::vector<Player*>& opposition, const TeamAI& teamAI)
{
    // ==========================================
    // --- 0. THE S-CURVE PROGRESSION ---
    // ==========================================
    float rawProgress = teamAI.getBallProgress();
    rawProgress = std::clamp(rawProgress, 0.0f, 1.0f);
    float ballProgress = rawProgress * rawProgress * rawProgress * (rawProgress * (rawProgress * 6.0f - 15.0f) + 10.0f);
    bool isHomeSide = teamAI.isHome();

    // ==========================================
    // --- 1. NATIVE DIRECTIONAL INTELLIGENCE ---
    // ==========================================
    float enemyOffsideLineX = pitch.totalWidth / 2.f;
    float threatX = isHomeSide ? pitch.totalWidth : 0.f;

    if (!opposition.empty()) {
        std::vector<float> oppX;
        for (Player* opp : opposition) {
            if (opp->getPositionRole() == PositionRole::Goalkeeper || opp->isSentOff()) continue;

            float px = opp->getPosition().x;
            oppX.push_back(px);

            // Threat X: The opponent closest to the goal we are defending
            if (isHomeSide) {
                if (px < threatX) threatX = px; // Home defends 0 (Left)
            }
            else {
                if (px > threatX) threatX = px; // Away defends 10000 (Right)
            }
        }

        // Enemy Offside Line: The deepest outfield defender
        if (!oppX.empty()) {
            if (isHomeSide) {
                std::sort(oppX.begin(), oppX.end(), std::greater<float>()); // Home attacks 10000
            }
            else {
                std::sort(oppX.begin(), oppX.end(), std::less<float>());    // Away attacks 0
            }
            enemyOffsideLineX = oppX[0];
        }
    }

    // Safety: You cannot be offside in your own half
    if (isHomeSide && enemyOffsideLineX < pitch.totalWidth / 2.f) enemyOffsideLineX = pitch.totalWidth / 2.f;
    if (!isHomeSide && enemyOffsideLineX > pitch.totalWidth / 2.f) enemyOffsideLineX = pitch.totalWidth / 2.f;

    // My Defensive Line (For Midfield Screen)
    float myDefLineX = 0.f;
    int defCount = 0;
    for (Player* tm : team) {
        bool isDef = (tm->getPositionRole() == PositionRole::CenterBack || tm->getPositionRole() == PositionRole::LeftBack || tm->getPositionRole() == PositionRole::RightBack || tm->getPositionRole() == PositionRole::LeftWingBack || tm->getPositionRole() == PositionRole::RightWingBack);
        if (isDef && !tm->isSentOff()) {
            myDefLineX += tm->getPosition().x;
            defCount++;
        }
    }
    if (defCount > 0) myDefLineX /= defCount;
    else myDefLineX = isHomeSide ? (pitch.margin + 1500.f) : (pitch.totalWidth - pitch.margin - 1500.f);

    // ==========================================
    // --- 2. ROLE CLASSIFICATION ---
    // ==========================================
    bool isForward = (npc.getPositionRole() == PositionRole::Striker || npc.getPositionRole() == PositionRole::CenterForward || npc.getPositionRole() == PositionRole::LeftWing || npc.getPositionRole() == PositionRole::RightWing);
    bool isMid = (npc.getPositionRole() == PositionRole::CenterMid || npc.getPositionRole() == PositionRole::AttackingMid || npc.getPositionRole() == PositionRole::LeftMid || npc.getPositionRole() == PositionRole::RightMid || npc.getPositionRole() == PositionRole::DefensiveMid);
    bool isDefender = (!isForward && !isMid);

    sf::Vector2f tacticalTarget = homePos;
    float pitchCenterY = pitch.totalHeight / 2.0f;

    // --- FETCH MANAGER'S SLIDERS ---
    float passLengthPref = teamAI.getPassingLengthPref();
    float counterSpeed = teamAI.getPassingSpeedPref();
    float depthPref = teamAI.getDefensiveDepthPref();
    float freedomPref = teamAI.getPositionalFreedomPref();

    PlayerBehavior behavior = npc.getPlaystyle().behavior;

    if (state == TeamState::Attacking) {
        float depthPush = isHomeSide ? 1.0f : -1.0f;

        // 1. PROACTIVE BALL SENSITIVITY
        float targetBallX = ballPos.x;

        // THE FIX 2b: If the GK has the ball, push the team up the pitch so they can receive a pass!
        if (ball.hasOwner() && ball.getOwner()->getPositionRole() == PositionRole::Goalkeeper) {
            float pushAmount = 2500.f + (teamAI.getPassingLengthPref() * 2000.f);
            targetBallX += (isHomeSide ? pushAmount : -pushAmount);
        }

        if ((isForward || npc.getPositionRole() == PositionRole::AttackingMid) && zone.supportDepth > -0.3f) {
            float leadDist = 400.f + (passLengthPref * 1000.f) + (zone.supportDepth * 600.f);
            targetBallX += depthPush * leadDist;
        }
        else if (isMid && zone.supportDepth > -0.3f) {
            float leadDist = 100.f + (passLengthPref * 500.f) + (zone.supportDepth * 300.f);
            targetBallX += depthPush * leadDist;
        }

        float xDiff = targetBallX - tacticalTarget.x;
        tacticalTarget.x += (xDiff * zone.ballInfluence * 0.6f);

        // 2. TRANSITION SPEED & PUSH
        float unitExpansion = 0.0f;

        if (isForward) unitExpansion = std::clamp((ballProgress - 0.1f) * (1.8f + (counterSpeed * 0.5f)), 0.0f, 1.0f);
        else if (isMid) unitExpansion = std::clamp((ballProgress - 0.15f) * (1.2f + (counterSpeed * 1.2f)), 0.0f, 0.9f);
        else unitExpansion = std::clamp((ballProgress - 0.2f) * (1.0f + (counterSpeed * 0.4f)), 0.0f, 0.4f + (freedomPref * 0.3f));

        // ==========================================
        // --- TACTICAL INJECTION: RUN FREQUENCY ---
        // ==========================================
        // A Box-to-Box player (runFrequency 0.9) will push up an extra 20% of their leash instantly.
        float runModifier = (behavior.runFrequency - 0.5f) * 0.4f;
        unitExpansion = std::clamp(unitExpansion + runModifier, 0.0f, 1.0f);

        float pushFactor = (ballProgress - 0.5f) * 2.0f;
        pushFactor = std::max(0.0f, pushFactor);

        float leashMultiplier = isForward ? 0.5f : (isMid ? 0.3f : 0.15f);
        tacticalTarget.x += depthPush * (zone.forwardLeash * leashMultiplier * unitExpansion);
        tacticalTarget.x += depthPush * (zone.supportDepth * 600.f);

        // 3. ATTACKING SHAPE (Fanning Out)
        float yOffset = tacticalTarget.y - pitchCenterY;
        tacticalTarget.y += yOffset * (0.25f * pushFactor);

        if (std::abs(zone.widthPreference) > 0.1f) {
            float dirToTouchline = (tacticalTarget.y > pitchCenterY) ? 1.0f : -1.0f;
            tacticalTarget.y += (dirToTouchline * zone.widthPreference * 800.f);
        }

        // 4. DYNAMIC PASSING POCKETS
        float idealSupportDist = 600.f + (passLengthPref * 800.f);
        float distToBallTarget = dist(tacticalTarget, ballPos);

        if (distToBallTarget < idealSupportDist - 150.f) {
            sf::Vector2f repelDir = normalize(tacticalTarget - ballPos);
            repelDir.x *= 0.3f;
            repelDir.y *= 1.8f;
            repelDir = normalize(repelDir);
            tacticalTarget += repelDir * (idealSupportDist - distToBallTarget) * 0.7f;
        }
        else if (distToBallTarget > idealSupportDist + 300.f && !isForward) {
            sf::Vector2f attractDir = normalize(ballPos - tacticalTarget);
            tacticalTarget += attractDir * (distToBallTarget - idealSupportDist) * passLengthPref * 0.6f;
        }

        // 5. SPACE INVADER
        Player* nearestOpp = findNearestOpponent(tacticalTarget, opposition);
        if (nearestOpp && dist(tacticalTarget, nearestOpp->getPosition()) < 700.f) {
            sf::Vector2f escapeDir = normalize(tacticalTarget - nearestOpp->getPosition());
            escapeDir.x = -depthPush * std::abs(escapeDir.x) * 0.4f;
            escapeDir.y *= 1.6f;
            escapeDir = normalize(escapeDir);

            float spaceCreation = 150.f + (npc.getAwareness() / 100.0f * 400.f) + (freedomPref * 300.f);
            tacticalTarget += escapeDir * spaceCreation;
        }

        // 6. ATTACKING BOX LIMITS & OFFSIDE
        float oppGoalLineX = isHomeSide ? pitch.totalWidth - pitch.margin : pitch.margin;
        float attackingMaxPush = isHomeSide ? oppGoalLineX - 800.f : oppGoalLineX + 800.f;

        if (isHomeSide && tacticalTarget.x > attackingMaxPush) tacticalTarget.x = attackingMaxPush;
        else if (!isHomeSide && tacticalTarget.x < attackingMaxPush) tacticalTarget.x = attackingMaxPush;

        if (zone.supportDepth > -0.5f) {
            float awarenessError = (1.0f - (npc.getAwareness() / 100.0f)) * 300.f;
            if (isHomeSide && tacticalTarget.x > enemyOffsideLineX + awarenessError) {
                tacticalTarget.x = enemyOffsideLineX + awarenessError - 50.f;
            }
            else if (!isHomeSide && tacticalTarget.x < enemyOffsideLineX - awarenessError) {
                tacticalTarget.x = enemyOffsideLineX - awarenessError + 50.f;
            }
        }
    }
    else 
    {
        // ==========================================
                // --- 1. SLIDER-DRIVEN DEFENDING (Quicker) ---
                // ==========================================
        float dropIntensity = 1.0f - ballProgress;
        float dropMultiplier = 0.0f;

        if (isDefender) 
        {
            // High Line (0.0) = 0.30 drop. Low Block (1.0) = 0.60 drop.
            dropMultiplier = 0.30f + (depthPref * 0.30f);
        }
        else if (isMid) {
            dropMultiplier = 0.22f + (depthPref * 0.25f);
            if (npc.getPositionRole() == PositionRole::DefensiveMid) dropMultiplier += 0.08f;
        }
        else {
            // THE FIX: Attackers drop significantly more to stay connected to the midfield!
            dropMultiplier = 0.15f + (depthPref * 0.15f); // Was previously 0.05f
        }

        float dropDist = (zone.backwardLeash * dropMultiplier) * dropIntensity;
        sf::Vector2f dropDir = isHomeSide ? sf::Vector2f(-1.f, 0.f) : sf::Vector2f(1.f, 0.f);
        tacticalTarget += (dropDir * dropDist);

        // ==========================================
        // --- 2. PROACTIVE HIGH LINE & ATTACKER TETHER ---
        // ==========================================
        if (isDefender) {
            float buffer = 600.f + (depthPref * 800.f);
            if (isHomeSide && tacticalTarget.x < threatX - buffer) tacticalTarget.x = threatX - buffer;
            else if (!isHomeSide && tacticalTarget.x > threatX + buffer) tacticalTarget.x = threatX + buffer;
        }
        else if (isMid) {
            float midBuffer = 800.f;
            float referenceX = isHomeSide ? std::max(threatX, ballPos.x) : std::min(threatX, ballPos.x);

            if (isHomeSide && tacticalTarget.x < referenceX + midBuffer) tacticalTarget.x = referenceX + midBuffer;
            else if (!isHomeSide && tacticalTarget.x > referenceX - midBuffer) tacticalTarget.x = referenceX - midBuffer;
        }
        else if (isForward) {
            // THE FIX: The Attacker Tether! 
            // Instead of dropping to their static formation spot, they dynamically hover near the ball 
            // to act as a counter-attacking outlet and press the opponent's defense.
            float outletDepth = 400.f + (depthPref * 800.f); // Stay 4 to 12 meters behind the ball
            
            float targetX = isHomeSide ? (ballPos.x - outletDepth) : (ballPos.x + outletDepth);

            // 1. Stay Onside: Never push past the enemy offside line!
            if (isHomeSide) {
                targetX = std::min(targetX, enemyOffsideLineX - 150.f);
            } else {
                targetX = std::max(targetX, enemyOffsideLineX + 150.f);
            }

            // 2. Stay Connected: Don't drop deeper than the midfield line, even if the ball is in our box!
            float minStrikerDepth = isHomeSide ? (myDefLineX + 2500.f) : (myDefLineX - 2500.f);
            if (isHomeSide) {
                targetX = std::max(targetX, minStrikerDepth);
            } else {
                targetX = std::min(targetX, minStrikerDepth);
            }

            // Override the X target completely
            tacticalTarget.x = targetX;
        }

        // 3. DEFENSIVE COMPACTNESS (Slamming the Door)
        float yOffset = tacticalTarget.y - pitchCenterY;
        float squeezeFactor = 0.0f;

        // BUFF: Base compactness is significantly higher
        if (isDefender) squeezeFactor = (0.5f + (depthPref * 0.4f)) * dropIntensity;
        else if (isMid) squeezeFactor = (0.3f + (depthPref * 0.3f)) * dropIntensity;

        // THE DOOR SLAM: If the ball is central and approaching the penalty area, crush the space!
        float ballDistFromCenterY = std::abs(ballPos.y - pitchCenterY);
        float distToBallExact = dist(npc.getPosition(), ballPos);

        if (distToBallExact < 2000.f && ballDistFromCenterY < 1200.f) {
            squeezeFactor *= 1.8f; // Aggressively clamp down the central channel
        }
        tacticalTarget.y -= yOffset * squeezeFactor;

        // 4. THE SWARM
        float maxLateralShift = 800.f + (depthPref * 400.f); // BUFF: Increased max shift
        float desiredYShift = (ballPos.y - tacticalTarget.y) * (zone.ballInfluence * 0.85f);
        desiredYShift = std::clamp(desiredYShift, -maxLateralShift, maxLateralShift);
        tacticalTarget.y += desiredYShift;
        sf::Vector2f npcPos = npc.getPosition();

        // ==========================================
        // --- 4.5 THE STOPPER / SWEEPER SYSTEM ---
        // ==========================================
        if (isDefender && distToBallExact < 1800.f) {
            bool amIClosestDef = true;
            for (Player* tm : team) {
                if (tm == &npc || tm->getPositionRole() == PositionRole::Goalkeeper || tm->isSentOff()) continue;

                bool tmIsDef = (tm->getPositionRole() == PositionRole::CenterBack ||
                    tm->getPositionRole() == PositionRole::LeftBack ||
                    tm->getPositionRole() == PositionRole::RightBack ||
                    tm->getPositionRole() == PositionRole::LeftWingBack ||
                    tm->getPositionRole() == PositionRole::RightWingBack);

                if (tmIsDef && dist(tm->getPosition(), ballPos) < distToBallExact) {
                    amIClosestDef = false;
                    break;
                }
            }

            if (amIClosestDef) {
                // I AM THE STOPPER! Break the line to engage the ball carrier.
                sf::Vector2f toBall = ballPos - tacticalTarget;

                float defensiveUrgency = 1.0f;
                float penaltyBoxX = isHomeSide ? pitch.margin + 1650.f : pitch.totalWidth - pitch.margin - 1650.f;
                if (std::abs(ballPos.x - penaltyBoxX) < 1200.f) defensiveUrgency = 1.6f;

                float stopperBite = std::min(0.95f, (0.4f + (npc.getAggression() / 100.f * 0.4f)) * defensiveUrgency);
                tacticalTarget += toBall * stopperBite;
            }
            else {
                // I AM THE SWEEPER! Drop slightly to cover the space left by the Stopper.
                float coverDrop = 250.f + (depthPref * 150.f);
                tacticalTarget.x += isHomeSide ? -coverDrop : coverDrop;

                // BUFF: The Sweeper now aggressively pinches inward to sit directly behind the Stopper
                // This blocks the player from simply walking past the Stopper's tackle!
                float yPinch = (ballPos.y - tacticalTarget.y) * 0.65f;
                tacticalTarget.y += yPinch;
            }
        }

        // 5. STRICT BOX DISCIPLINE
        float penaltyBoxEdgeX = isHomeSide ? pitch.margin + 1650.f : pitch.totalWidth - pitch.margin - 1650.f;
        bool ballOutsideBox = isHomeSide ? (ballPos.x > penaltyBoxEdgeX) : (ballPos.x < penaltyBoxEdgeX);

        if (ballOutsideBox) {
            float lineError = (1.0f - (npc.getAwareness() / 100.0f)) * 150.f;

            if (isDefender) {
                if (isHomeSide && tacticalTarget.x < penaltyBoxEdgeX - lineError) tacticalTarget.x = penaltyBoxEdgeX - lineError;
                else if (!isHomeSide && tacticalTarget.x > penaltyBoxEdgeX + lineError) tacticalTarget.x = penaltyBoxEdgeX + lineError;
            }
            else if (isMid) {
                float midLine = isHomeSide ? penaltyBoxEdgeX + 600.f : penaltyBoxEdgeX - 600.f;
                if (isHomeSide && tacticalTarget.x < midLine) tacticalTarget.x = midLine;
                else if (!isHomeSide && tacticalTarget.x > midLine) tacticalTarget.x = midLine;
            }
        }
    }
    // ==========================================
        // --- 6. THE MIDFIELD GLUE (Dynamic Linking) ---
        // ==========================================
    if (isMid) {
        if (state == TeamState::Attacking) {
            // PIVOT: Sit perfectly between the ball and the rest of the team
            float idealDistanceBehindBall = 800.f + (passLengthPref * 600.f);
            if (npc.getPositionRole() == PositionRole::AttackingMid) idealDistanceBehindBall = 400.f;

            float pocketX = isHomeSide ? (ballPos.x - idealDistanceBehindBall) : (ballPos.x + idealDistanceBehindBall);

            float pullStrength = 0.4f + (zone.supportDepth * 0.2f);
            tacticalTarget.x = (tacticalTarget.x * (1.0f - pullStrength)) + (pocketX * pullStrength);
        }
        else {
            // SCREEN: Tether the midfield directly to the defensive line!
            float screenDist = 1000.f - (depthPref * 300.f);
            if (npc.getPositionRole() == PositionRole::DefensiveMid) screenDist = 500.f;
            else if (npc.getPositionRole() == PositionRole::AttackingMid) screenDist = 1600.f;

            float idealScreenX = isHomeSide ? (myDefLineX + screenDist) : (myDefLineX - screenDist);

            float discipline = 0.8f - (zone.roamingFreedom * 0.3f);
            tacticalTarget.x = (tacticalTarget.x * (1.0f - discipline)) + (idealScreenX * discipline);
        }
    }
    // ==========================================
    // --- FINAL LAYER: SPATIAL INTELLIGENCE ---
    // ==========================================
    // INJECTION: Pass the 'zone' explicitly into the engine!
    sf::Vector2f spatialAdjustments = evaluateShapeAndSpace(npc, tacticalTarget, ballPos, state, team, opposition, teamAI, zone, ball, pitch);

    tacticalTarget += spatialAdjustments;

    // TOUCHLINE SAFE ZONE
    float safeZone = 150.f;
    float minSafeX = pitch.margin + safeZone;
    float maxSafeX = pitch.totalWidth - pitch.margin - safeZone;
    float minSafeY = pitch.margin + safeZone;
    float maxSafeY = pitch.totalHeight - pitch.margin - safeZone;

    tacticalTarget.x = std::clamp(tacticalTarget.x, minSafeX, maxSafeX);
    tacticalTarget.y = std::clamp(tacticalTarget.y, minSafeY, maxSafeY);

    return tacticalTarget;
}

bool PlayerAI::evaluateSprintUrgency(NPCPlayer& npc, AIUrgency urgency, float distToTarget, float distToBall) {

    // THE FIX: If you have the ball, adrenaline takes over! Always sprint!
    if (npc.getBallPossession()) return true;

    float stamRatio = npc.getCurrentStamina() / npc.getMaxStamina();

    // If it's a Critical emergency (like receiving a pass), bypass the stamina lock!
    if (urgency != AIUrgency::Critical && npc.getCurrentStamina() < 2.0f) return false;

    PositionRole role = npc.getPositionRole();
    bool isAttacker = (role == PositionRole::Striker || role == PositionRole::CenterForward || role == PositionRole::LeftWing || role == PositionRole::RightWing);
    bool isDefender = (role == PositionRole::CenterBack || role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);
    bool isMid = (!isAttacker && !isDefender);

    switch (urgency) {
    case AIUrgency::Critical:
        return true;

    case AIUrgency::AttackingRun:
        if (isDefender) return (stamRatio > 0.6f && distToTarget > 800.f);
        return (stamRatio > 0.3f && distToTarget > 200.f);

    case AIUrgency::Pressing:
        if (isAttacker) return (stamRatio > 0.6f && distToBall < 500.f);
        else return (stamRatio > 0.35f && distToBall < 1000.f);

    case AIUrgency::Recovery:
    default:
        if (isDefender) {
            return (stamRatio > 0.2f && distToTarget > 300.f);
        }
        else if (isMid) {
            return (stamRatio > 0.25f && distToTarget > 400.f);
        }
        return (stamRatio > 0.4f && distToTarget > 700.f);
    }
}

sf::Vector2f PlayerAI::calculateInterceptionPoint(NPCPlayer& npc, Ball& ball) {
    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f ballVel = ball.getVelocity();
    float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);

    if (ballSpeed < 30.f && ball.z <= 40.f) return ballPos;

    sf::Vector2f ballDir = (ballSpeed > 0.f) ? ballVel / ballSpeed : sf::Vector2f(0.f, 0.f);

    float awareness = npc.getAwareness();
    float errorSeverity = std::clamp((100.f - awareness) / 100.f, 0.f, 1.f);

    // --- THE LASER FOCUS FIX ---
    bool isTeammatePass = (!ball.hasOwner() && ball.getLastOwner() && ball.getLastOwner()->getTeam() == npc.getTeam());
    if (isTeammatePass) {
        errorSeverity = 0.0f; // Perfect tracking for passes! No misjudging.
    }
    else {
        errorSeverity *= 0.5f; // Tone down general error so they aren't completely blind
    }

    if (ball.z > 40.f) {
        float misjudgment = 1.0f + (errorSeverity * 0.35f * ((ballVel.x > 0) ? 1.f : -1.f));
        float perceivedGravity = 980.f * misjudgment;

        float discriminant = (ball.vz * ball.vz) + (2.f * perceivedGravity * ball.z);
        if (discriminant > 0.f) {
            float t = (ball.vz + std::sqrt(discriminant)) / perceivedGravity;
            return ballPos + (ballVel * t);
        }
        return ballPos;
    }
    else {
        // ==========================================
        // --- PHYSICS-INFORMED GROUND TRACKING ---
        // ==========================================
        float ballFriction = ball.friction;

        // Calculate absolute physical stopping point: d = (v^2) / (2 * a)
        float maxStopDist = (ballSpeed * ballSpeed) / (2.f * ballFriction);

        sf::Vector2f toNPC = npc.getPosition() - ballPos;
        float projection = (toNPC.x * ballDir.x + toNPC.y * ballDir.y);
        float interceptDist = 0.f;

        if (isTeammatePass) {
            // Meet the ball on its path, but NEVER run past where it will physically stop rolling!
            interceptDist = std::max(20.f, projection - 150.f);
            interceptDist = std::min(interceptDist, maxStopDist);
        }
        else {
            // 1. Time to reach the line
            float distToLine = std::abs(toNPC.x * ballDir.y - toNPC.y * ballDir.x);
            float timeToLine = distToLine / ((npc.getTopSpeed() * 10.f) + 1.f);

            // 2. Kinematic distance the ball will travel in that time: d = vt - 0.5at^2
            float timeToStop = ballSpeed / ballFriction;
            float actualTime = std::min(timeToLine, timeToStop); // Ball can't travel after stopping
            float travelDist = (ballSpeed * actualTime) - (0.5f * ballFriction * actualTime * actualTime);

            interceptDist = travelDist + (errorSeverity * ballSpeed * 0.4f);
            interceptDist = std::min(interceptDist, maxStopDist); // Cap to physical limit
        }

        sf::Vector2f interceptPoint = ballPos + (ballDir * interceptDist);

        if (!isTeammatePass) {
            float side = (toNPC.x * ballDir.y - toNPC.y * ballDir.x > 0) ? 1.f : -1.f;
            interceptPoint += sf::Vector2f(-ballDir.y, ballDir.x) * side * (errorSeverity * 180.f);
        }

        interceptPoint.x = std::clamp(interceptPoint.x, 0.f, 10000.f);
        interceptPoint.y = std::clamp(interceptPoint.y, 0.f, 7000.f);
        return interceptPoint;
    }
}

sf::Vector2f PlayerAI::calculateSeparation(NPCPlayer& npc, const std::vector<Player*>& team, const std::vector<Player*>& opponents, sf::Vector2f ballPos, const TeamAI& teamAI) {
    sf::Vector2f force(0.f, 0.f);
    sf::Vector2f npcPos = npc.getPosition();
    Player* myTarget = findNearestOpponent(npcPos, opponents);

    // --- DNA INJECTION ---
    PlayerBehavior behavior = npc.getPlaystyle().behavior;

    float widthMultiplier = 0.5f + teamAI.getAttackingWidthPref();
    float freedomMultiplier = 0.8f + (teamAI.getPositionalFreedomPref() * 0.4f);

    // High run frequency players (Box-to-Box) are constantly looking for space, 
    // so they naturally repel from teammates a bit more to create passing angles.
    float movementActivity = 0.7f + (behavior.runFrequency * 0.6f);

    for (auto& teammate : team) {
        if (teammate == &npc) continue;
        sf::Vector2f teamPos = teammate->getPosition();
        sf::Vector2f diff = npcPos - teamPos;
        float distSq = diff.x * diff.x + diff.y * diff.y;

        Player* teammateTarget = findNearestOpponent(teamPos, opponents);
        bool markingSameGuy = (myTarget != nullptr && myTarget == teammateTarget);

        float baseBubble = markingSameGuy ? 300.f : 600.f;
        float currentBubble = baseBubble * widthMultiplier * freedomMultiplier * movementActivity;

        if (distSq < currentBubble * currentBubble && distSq > 0.1f) {
            float d = std::sqrt(distSq);
            float overlap = (currentBubble - d) / currentBubble;
            force += (diff / d) * (overlap * overlap);
        }
    }
    return force;
}

bool PlayerAI::shouldEmergencyChase(NPCPlayer& npc, Player* firstResponder, float distToBall, const Pitch& pitch, Ball& ball, MatchState matchstate, const TeamAI& teamAI) {
    if (&npc != firstResponder) return false;

    Player* owner = ball.getOwner();
    if ((owner != nullptr && owner->getTeam() != npc.getTeam() && owner->getPositionRole() == PositionRole::Goalkeeper) ||
        matchstate != MatchState::InPlay) {
        return false;
    }

    if (owner == nullptr) return true; // Loose ball override

    PositionRole role = npc.getPositionRole();
    bool isAttacker = (role == PositionRole::Striker || role == PositionRole::CenterForward || role == PositionRole::LeftWing || role == PositionRole::RightWing);
    bool isDefender = (role == PositionRole::CenterBack || role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);

    float depthPref = teamAI.getDefensiveDepthPref();
    float pressPref = teamAI.getPressingIntensityPref();

    // --- DNA INJECTION ---
    PlayerBehavior behavior = npc.getPlaystyle().behavior;

    float basePressRadius = 400.f;
    basePressRadius += (pressPref * 800.f);
    basePressRadius += (npc.getAggression() / 100.f * 300.f);

    // A Backline Brawler (1.0 tackleAggression) will hunt the ball much further out!
    basePressRadius += (behavior.tackleAggression * 400.f);
    bool isHomeTeam = npc.getTeam() == Team::Home;

    if (isAttacker) basePressRadius *= 0.7f;
    if (isDefender) {
        // --- THE HOME-HALF FIX ---
        bool ballInOwnHalf = isHomeTeam ? (ball.getPosition().x < 5000.f) : (ball.getPosition().x > 5000.f);

        if (!ballInOwnHalf) {
            // Reluctant to leave defensive shape in the opponent's half
            basePressRadius *= 0.4f;
            basePressRadius *= (1.0f - (depthPref * 0.8f));
        }
        else {
            // Much more willing to press and tackle in their own half!
            basePressRadius *= 0.8f;
            basePressRadius *= (1.0f - (depthPref * 0.3f));
        }
    }

    if (distToBall > basePressRadius) return false;

    if (distToBall > 500.f) {
        sf::Vector2f toBall = ball.getPosition() - npc.getPosition();
        sf::Vector2f ownerVel = owner->getVelocity();
        if ((toBall.x * ownerVel.x + toBall.y * ownerVel.y) > 0.f) return false;
    }

    sf::Vector2f home = npc.getHomePosition();
    TacticalZone zone = teamAI.getEffectiveTacticalZone(npc.getPlaystyle());

    // --- DNA INJECTION ---
    // A player's individual roaming freedom dictates how much they are willing to stretch their leash to press
    float freedomExpand = teamAI.getPositionalFreedomPref() * 600.f * (0.5f + zone.roamingFreedom);

    float xBuffer = 300.f + (pressPref * 500.f) + freedomExpand;
    float yBuffer = 300.f + (pressPref * 500.f) + freedomExpand;

    float dx = std::abs(home.x - ball.getPosition().x);
    float dy = std::abs(home.y - ball.getPosition().y);

    bool ballIsForward = isHomeTeam ? (ball.getPosition().x > home.x) : (ball.getPosition().x < home.x);
    float currentXLeash = ballIsForward ? zone.forwardLeash : zone.backwardLeash;

    if (dx > currentXLeash + xBuffer || dy > zone.lateralLeash + yBuffer) return false;

    return true;
}

sf::Vector2f PlayerAI::calculateMarkingPosition(NPCPlayer& npc, Player* threat, sf::Vector2f goalPos, const TacticalZone& zone, const TeamAI& teamAI) {
    sf::Vector2f threatPos = threat->getPosition();
    sf::Vector2f threatVel = threat->getVelocity();
    float threatSpeed = std::sqrt(threatVel.x * threatVel.x + threatVel.y * threatVel.y);

    float aggressionNorm = npc.getAggression() / 100.0f;
    float awarenessNorm = npc.getAwareness() / 100.0f;
    sf::Vector2f toGoal = normalize(goalPos - threatPos);
    float pressPref = teamAI.getPressingIntensityPref();

    // --- DNA INJECTION ---
    PlayerBehavior behavior = npc.getPlaystyle().behavior;

    if (threat->getBallPossession()) {
        // High tackle aggression = smaller jockey buffer (they want to get close enough to bite)
        float jockeyBuffer = 300.f - (aggressionNorm * 100.f) - (pressPref * 150.f) - (behavior.tackleAggression * 100.f);
        jockeyBuffer = std::max(40.f, jockeyBuffer);

        // ==========================================
        // --- 1. TACKLE RISK ASSESSMENT ---
        // ==========================================
        // A. Are we in our own penalty box?
        float dxToGoal = std::abs(npc.getPosition().x - goalPos.x);
        float dyToGoal = std::abs(npc.getPosition().y - goalPos.y);

        // The standard penalty box extends 1650px from the goal line, and is ~4032px wide (2016px up/down from center)
        bool inOwnBox = (dxToGoal < 1650.f && dyToGoal < 2050.f);

        // B. 100% Certainty Check (Are we chasing them from behind?)
        sf::Vector2f toThreat = threatPos - npc.getPosition();
        float distToThreat = std::sqrt(toThreat.x * toThreat.x + toThreat.y * toThreat.y);

        sf::Vector2f threatDir = (threatSpeed > 10.f) ? (threatVel / threatSpeed) : sf::Vector2f(0.f, 0.f);
        sf::Vector2f npcDir = (distToThreat > 0.1f) ? (toThreat / distToThreat) : sf::Vector2f(0.f, 0.f);

        // Dot product to check alignment
        float approachAngle = (npcDir.x * threatDir.x) + (npcDir.y * threatDir.y);

        // If the angle > 0.4f, the threat is running away from us in the same direction we are running.
        // Tackling now guarantees we clip their heels (Straight Red!).
        bool isTacklingFromBehind = (approachAngle > 0.4f);

        // You only have 100% certainty if you are facing them (or hitting them from the side) and have decent awareness
        bool isCertain = !isTacklingFromBehind && (awarenessNorm > 0.4f);

        // ==========================================
        // --- 2. EXECUTE OR JOCKEY ---
        // ==========================================
        if (inOwnBox || !isCertain) {
            // JOCKEY MODE: Never dive in! 
            // We increase the buffer so they stand the attacker up instead of crashing into their legs.
            float safeBuffer = inOwnBox ? std::max(120.f, jockeyBuffer) : std::max(100.f, jockeyBuffer);
            return threatPos + (toGoal * safeBuffer);
        }

        // AGGRESSIVE MODE: We passed the safety checks, we are clear to bite!
        sf::Vector2f baseDefendPos = threatPos + (toGoal * (jockeyBuffer + (threatSpeed * 0.8f)));
        sf::Vector2f tacticalTarget = baseDefendPos + (threatVel * 0.8f);

        if (dist(npc.getPosition(), threatPos) < 80.f + ((1.0f - awarenessNorm) * 60.f) && npc.canTackle()) {
            if (awarenessNorm > 0.6f && (npc.getBodyStrength() / 100.f) > 0.6f) {
                sf::Vector2f diveDir = (threatSpeed > 10.f) ? threatDir : toGoal;
                return threatPos + (diveDir * 80.f);
            }
            else {
                return threatPos + (toGoal * (80.f - (aggressionNorm * 20.f)));
            }
        }
        return tacticalTarget;
    }
    else {
        // Off-ball marking. Aggressive tacklers stick closer to their man to intercept.
        float offBallGap = std::max(350.f - (aggressionNorm * 150.f) - (pressPref * 200.f) - (behavior.tackleAggression * 100.f), 30.f);
        return threatPos + (threatVel * (awarenessNorm * 0.5f)) + (toGoal * offBallGap);
    }
}

sf::Vector2f PlayerAI::clampToTacticalZone(sf::Vector2f target, sf::Vector2f homePos, const TacticalZone& zone, float distToBall, bool isHomeSide, const TeamAI& teamAI) {
    // --- DNA INJECTION ---
    // The Manager sets the baseline freedom, but the Player's Playstyle scales it!
    // A rigid manager (0.2) + A rigid player (0.1) = 0 stretch.
    // A fluid manager (0.9) + A fluid player (0.9) = Massive stretch.
    float stretch = teamAI.getPositionalFreedomPref() * 1000.f * (0.3f + zone.roamingFreedom);

    float minX = (isHomeSide ? homePos.x - zone.backwardLeash : homePos.x - zone.forwardLeash) - stretch;
    float maxX = (isHomeSide ? homePos.x + zone.forwardLeash : homePos.x + zone.backwardLeash) + stretch;
    float minY = (homePos.y - zone.lateralLeash) - stretch;
    float maxY = (homePos.y + zone.lateralLeash) + stretch;

    if (distToBall < 800.f) {
        minX -= 400.f; maxX += 400.f;
        minY -= 300.f; maxY += 300.f;
    }

    target.x = std::clamp(target.x, minX, maxX);
    target.y = std::clamp(target.y, minY, maxY);
    return target;
}

bool PlayerAI::shouldRecoverPosition(const sf::Vector2f& npcPos, const sf::Vector2f& homePos, const TacticalZone& zone, float distToBall, bool isHomeSide, const TeamAI& teamAI) {
    float currentDX = std::abs(npcPos.x - homePos.x);
    float currentDY = std::abs(npcPos.y - homePos.y);

    bool isAheadOfHome = isHomeSide ? (npcPos.x > homePos.x) : (npcPos.x < homePos.x);
    float maxReachX = isAheadOfHome ? zone.forwardLeash : zone.backwardLeash;

    // --- DNA INJECTION ---
    // A fluid player (high roamingFreedom) won't instantly panic and recover if they step out of bounds.
    float freedomTolerance = teamAI.getPositionalFreedomPref() * 800.f * (0.3f + zone.roamingFreedom);

    return ((currentDX > maxReachX + 200.f + freedomTolerance || currentDY > zone.lateralLeash + 200.f + freedomTolerance) && distToBall > 400.f);
}

// ==========================================
// --- ON-BALL DECISIONS ---
// ==========================================

sf::Vector2f PlayerAI::handlePossession(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates, const std::vector<Player*>& opposition, UserPlayer* user, const Pitch& pitch, float dt, MatchState matchstate, const TeamAI& teamAI, SoundManager& soundManager)
{
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);
    sf::Vector2f goalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);

    float bc = npc.getBallControl() / 100.f;
    float awareness = npc.getAwareness() / 100.f;
    float passingSkill = (npc.getShortPassing() + npc.getLongPassing()) / 200.f;
    float dribbleSkill = (npc.getBallControl() * 0.6f + npc.getAgility() * 0.4f) / 100.f;

    sf::Vector2f facingVec = getFacingVec(npc.getDirection());

    Player* nearestOpp = findNearestOpponent(npcPos, opposition);
    float closestOppDist = nearestOpp ? dist(npcPos, nearestOpp->getPosition()) : 9999.f;
    bool isUnderPressure = (closestOppDist < 600.f);
    bool isCrammed = (closestOppDist < (250.f - (bc * 100.f)));

    PlayerBehavior behavior = npc.getPlaystyle().behavior;

    // --- 1. SHOOTING (Preserving xG & Shot Quality) ---
    float distToGoal = dist(npcPos, goalPos);
    float kickPowerNorm = npc.getKickPower() / 100.f;
    float finishingNorm = npc.getFinishing() / 100.f;

    float yDistFromCenter = std::abs(goalPos.y - npcPos.y);
    float xDistFromGoal = std::abs(goalPos.x - npcPos.x);

    // ==========================================
    // --- THE FIX: THE xG VETO ---
    // ==========================================
    // Check if the player is too far wide relative to how deep they are (The Byline/Dead Angle)
    bool isDeadAngle = (xDistFromGoal < 800.f && yDistFromCenter > 600.f);
    bool bypassShooting = false;

    // High awareness players realize this is a terrible shot and refuse to take it!
    if (isDeadAngle && awareness > 0.6f) {
        bypassShooting = true;
    }

    float maxShotRange = 800.f + (kickPowerNorm * 1400.f) + (behavior.shootBias * 500.f);

    // Only evaluate shooting if the Playmaker hasn't vetoed it
    if (!bypassShooting && distToGoal < maxShotRange && npc.getKickCooldown() <= 0.0f && matchstate == MatchState::InPlay) {
        bool isGoodAngle = (yDistFromCenter < xDistFromGoal * 1.2f) || (distToGoal < 1000.f);

        if (isGoodAngle) {
            bool isLongShot = (distToGoal > 2000.f);
            bool hasOpenSpaceToDrive = (isLongShot && closestOppDist > 700.f);
            bool takeLongShotAnyway = (isLongShot && behavior.shootBias > 0.7f && kickPowerNorm > 0.8f && (rand() % 100 < 20));

            if (!hasOpenSpaceToDrive || takeLongShotAnyway) {
                bool isFirstTime = (npc.m_possessionTimer < 0.4f);
                bool wantsToShoot = false; // THE FIX: Default to false!

                // THE FIX: Intelligent Shooting Decisions
                if (distToGoal < 1500.f) {
                    // Inside or near the box? Let it fly!
                    wantsToShoot = true;
                }
                else if (!isFirstTime && behavior.shootBias > 0.4f) {
                    // We aren't rushing, we have decent shooting bias, and a good angle. Take the shot!
                    wantsToShoot = true;
                }
                else if (isFirstTime && finishingNorm > 0.8f && behavior.shootBias > 0.7f) {
                    // First-time shot from outside the box? Only elite strikers attempt this!
                    if ((rand() % 100) < 50) wantsToShoot = true;
                }

                // If they are crammed by a defender and it's not a tap-in, panic ruins the shot chance
                if (isCrammed && distToGoal > 800.f) wantsToShoot = false;

                if (wantsToShoot) {
                    sf::Vector2f toGoal = normalize(goalPos - npcPos);
                    float alignment = (facingVec.x * toGoal.x + facingVec.y * toGoal.y);

                    if (alignment > 0.6f) {
                        executeShot(npc, ball, goalPos, opposition, pitch, dt, soundManager);
                        return { 0.f, 0.f };
                    }
                }
            }
        }
    }



    // --- 2. PASSING ---
    npc.m_passTimer += dt;
    float managerPassDelay = 0.6f - (teamAI.getPassingSpeedPref() * 0.5f);
    float playerHogDelay = (behavior.dribbleBias - 0.5f) * 1.5f;
    float actualPassDelay = std::max(0.05f, managerPassDelay + playerHogDelay);

    // ==========================================
    // --- THE METRONOME (Press Resistance) ---
    // ==========================================
    bool isMid = (npc.getPositionRole() == PositionRole::CenterMid ||
        npc.getPositionRole() == PositionRole::DefensiveMid ||
        npc.getPositionRole() == PositionRole::AttackingMid);

    // If a midfielder gets the ball and is instantly pressured...
    if (isMid && isUnderPressure) {
        // High awareness players have "eyes in the back of their head". 
        // They know they are about to get crunched, so they play one-touch!
        if (awareness > 0.7f || behavior.dribbleBias < 0.4f) {
            // Force an immediate pass to escape the press
            actualPassDelay = 0.0f;
        }
    }

    if (isUnderPressure) actualPassDelay *= 0.3f;

    // ==========================================
    // --- THE FIX: INSTANT CUTBACK SCANNER ---
    // ==========================================
    // If a Playmaker is stuck in a dead angle, they don't hold the ball. 
    // They instantly look to drill it across the face of the goal!
    if (isDeadAngle && awareness > 0.65f) actualPassDelay = 0.0f;

    if (npc.getKickCooldown() <= 0.0f && npc.m_passTimer > actualPassDelay) {
        Player* bestTarget = findBestPassOption(npc, teammates, opposition, user, teamAI, pitch);

        if (bestTarget) {
            sf::Vector2f toTarget = normalize(bestTarget->getPosition() - npcPos);
            float alignment = (facingVec.x * toTarget.x + facingVec.y * toTarget.y);

            if (alignment < 0.5f) {
                return toTarget * 0.5f;
            }

            Player* tOpp = findNearestOpponent(bestTarget->getPosition(), opposition);
            float targetOppDist = tOpp ? dist(bestTarget->getPosition(), tOpp->getPosition()) : 9999.f;

            if (isCrammed || (isUnderPressure && targetOppDist > closestOppDist + 200.f) ||
                (isUnderPressure && passingSkill > dribbleSkill * 1.2f) ||
                teamAI.getPassingSpeedPref() > 0.7f || behavior.dribbleBias < 0.3f ||
                isDeadAngle) // Playmakers will always pull the trigger if trapped deep!
            {
                executePass(npc, ball, bestTarget, opposition, soundManager);
                return { 0.f, 0.f };
            }
        }
    }

    // --- 3. DRIBBLE ---
    if (matchstate == MatchState::InPlay) {
        return calculateDribbleDirection(npc, goalPos, opposition, pitch, teamAI);
    }
    return { 0.f, 0.f };
}

Player* PlayerAI::findBestPassOption(NPCPlayer& npc, const std::vector<Player*>& team, const std::vector<Player*>& opposition, UserPlayer* user, const TeamAI& teamAI, const Pitch& pitch) {
    Player* bestOption = nullptr;
    float bestScore = -1e9f;
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);

    float shortPassNorm = npc.getShortPassing() / 100.f;
    float longPassNorm = npc.getLongPassing() / 100.f;
    float curlNorm = npc.getCurl() / 100.f;
    float visionNorm = npc.getAwareness() / 100.f; // The Playmaker Stat!

    float tikiTakaPref = 1.0f - teamAI.getPassingLengthPref();
    float routeOnePref = teamAI.getPassingLengthPref();
    float counterSpeed = teamAI.getPassingSpeedPref();

    // ==========================================
    // --- ON-THE-FLY OFFSIDE LINE SCANNER ---
    // ==========================================
    float deepestX = isHome ? 0.f : pitch.totalWidth;
    float secondDeepestX = deepestX;

    for (Player* opp : opposition) {
        float x = opp->getPosition().x;
        if (isHome) {
            if (x > deepestX) { secondDeepestX = deepestX; deepestX = x; }
            else if (x > secondDeepestX) { secondDeepestX = x; }
        }
        else {
            if (x < deepestX) { secondDeepestX = deepestX; deepestX = x; }
            else if (x < secondDeepestX) { secondDeepestX = x; }
        }
    }

    float halfwayX = pitch.totalWidth / 2.f;
    float offsideLineX = isHome ? std::max(halfwayX, std::max(secondDeepestX, npcPos.x))
        : std::min(halfwayX, std::min(secondDeepestX, npcPos.x));

    PlayerBehavior behavior = npc.getPlaystyle().behavior;
    float riskMultiplier = (behavior.passRiskBias - 0.5f) * 2.0f; // -1.0 (Safe) to +1.0 (Hollywood)

    bool inOwnDeepBox = isHome ? (npcPos.x < 2500.f) : (npcPos.x > 7500.f);
    Player* closestPresser = findNearestOpponent(npcPos, opposition);
    float presserDist = closestPresser ? dist(npcPos, closestPresser->getPosition()) : 9999.f;
    bool isCrammed = presserDist < 350.f;

    std::vector<Player*> receivers;
    for (auto& t : team) {
        // THE FIX: Don't pass to dead players!
        if (t != &npc && t->getState() != PlayerState::Injured && !t->isSentOff()) {
            receivers.push_back(t);
        }
    }
    if (user != nullptr && npc.getTeam() == user->getTeam() && user->getState() != PlayerState::Injured && !user->isSentOff()) {
        receivers.push_back(user);
    }

    for (Player* target : receivers) {
        float distToTarget = dist(npcPos, target->getPosition());

        float ballFriction = 800.f;
        float maxKickVel = npc.getKickPower() * 52.0f;
        float maxPhysicalRange = (maxKickVel * maxKickVel) / (2.f * ballFriction);

        if (distToTarget > maxPhysicalRange * 0.9f) continue;

        float arrivalSpeed = 400.f;
        float requiredV0 = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * ballFriction * distToTarget));
        if (requiredV0 / 52.0f > npc.getKickPower()) continue;

        float travelTime = (requiredV0 - arrivalSpeed) / ballFriction;
        sf::Vector2f predictedPos = target->getPosition() + (target->getVelocity() * travelTime);
        float predictedDist = dist(npcPos, predictedPos);
        sf::Vector2f passDir = normalize(predictedPos - npcPos);

        float forwardProgress = isHome ? (predictedPos.x - npcPos.x) : (npcPos.x - predictedPos.x);
        float score = 600.f;

        bool isOffside = isHome ? (target->getPosition().x > offsideLineX) : (target->getPosition().x < offsideLineX);
        if (isOffside) score -= 10000.f * visionNorm;

        bool isRightFooted = (npc.getPreferredFoot() == "Right");
        sf::Vector2f facing = getFacingVec(npc.getDirection());
        float cross = (facing.x * passDir.y - facing.y * passDir.x);
        bool favorsRight = (cross > 0);
        bool requiresWeakFoot = (isRightFooted && !favorsRight) || (!isRightFooted && favorsRight);

        if (requiresWeakFoot) score -= (5.0f - npc.getWeakFootAccuracy()) * 150.f;

        float bodyAlignment = PlayerAI::dot(facing, passDir);
        if (bodyAlignment < -0.2f) score -= 2000.f;

        if (riskMultiplier > 0.0f) {
            score += forwardProgress * (0.3f + (counterSpeed * 0.4f) + (riskMultiplier * 1.5f));
        }
        else {
            score += std::abs(forwardProgress) * (riskMultiplier * 0.5f);
            score += (2000.f - distToTarget) * std::abs(riskMultiplier * 0.8f);
        }

        if (distToTarget < 1200.f) score += (tikiTakaPref * 1500.f) * shortPassNorm;
        else if (distToTarget > 2500.f) score += (routeOnePref * 2500.f) * longPassNorm * visionNorm * (1.0f + std::max(0.0f, riskMultiplier));

        // --- CROSSING DNA ---
        bool inFinalThird = isHome ? (npcPos.x > 7000.f) : (npcPos.x < 3000.f);
        bool isWide = (npcPos.y < 1500.f || npcPos.y > pitch.totalHeight - 1500.f);
        bool targetInBox = isHome ? (predictedPos.x > 8350.f) : (predictedPos.x < 1650.f);

        if (inFinalThird && isWide && targetInBox) {
            score += behavior.crossBias * 3000.f;
        }

        // ==========================================
        // --- NEW: THE DEADLY CUTBACK ---
        // ==========================================
        // Are we extremely deep in the opponent's zone?
        bool amIDeep = isHome ? (npcPos.x > 8800.f) : (npcPos.x < 1200.f);

        if (amIDeep && targetInBox) {
            // Is the pass going backward away from the goal line?
            float cutbackAngle = isHome ? -forwardProgress : forwardProgress;

            // If it's a backward pass into the box, it's a highly dangerous cutback!
            if (cutbackAngle > 0.f) {
                // Playmakers heavily prioritize this high-xG pass over random chipped crosses
                score += 5000.f * visionNorm;

                Player* marker = findNearestOpponent(predictedPos, opposition);
                float distToMarker = marker ? dist(predictedPos, marker->getPosition()) : 9999.f;

                // If the trailing runner is unmarked near the penalty spot, this is a guaranteed goal!
                if (distToMarker > 350.f) {
                    score += 8000.f * visionNorm;
                }
            }
        }

        bool directLaneBlocked = false, canCurlAround = false;
        for (auto* opp : opposition) {
            float dOpp = dist(npcPos, opp->getPosition());
            if (dOpp < predictedDist && dOpp > 100.f) {
                sf::Vector2f toOpp = opp->getPosition() - npcPos;
                float alignment = (passDir.x * (toOpp.x / dOpp) + passDir.y * (toOpp.y / dOpp));

                if (alignment > 0.92f) {
                    directLaneBlocked = true;
                    if (alignment < 0.98f && curlNorm > 0.3f) canCurlAround = true;
                }
            }
        }

        bool wantHigh = (distToTarget > 2200.f || (directLaneBlocked && !canCurlAround));
        if (wantHigh && distToTarget < 1500.f) score -= 5000.f;

        float pressureFactor = std::clamp((600.f - presserDist) / 400.f, 0.f, 1.f);
        if (pressureFactor > 0.1f) {
            if (wantHigh && distToTarget < 2000.f) score -= 4000.f * pressureFactor;
            if (distToTarget < 1500.f) score += (3500.f * pressureFactor) * (directLaneBlocked ? 0.5f : 1.0f);
        }

        if (inOwnDeepBox && isCrammed) {
            if (target->getPositionRole() == PositionRole::Goalkeeper && !directLaneBlocked) score += 6000.f;
            else if (forwardProgress > 1500.f) score += 4000.f;
        }

        // ==========================================
        // --- NEW: THE "HERO BALL" & PATIENCE LOGIC ---
        // ==========================================
        Player* marker = findNearestOpponent(predictedPos, opposition);
        float distToMarker = marker ? dist(predictedPos, marker->getPosition()) : 9999.f;

        // 1. REWARD PATIENCE (The Safe Pass)
        if (distToMarker > 1000.f && !directLaneBlocked && !isOffside) {
            // High awareness players massively value an open man to safely reset the play!
            score += 2500.f * visionNorm;

            // If it's a backward or lateral pass, smart players see it as a great reset
            if (forwardProgress < 200.f) {
                score += 1500.f * visionNorm;
            }
        }

        bool isMid = (npc.getPositionRole() == PositionRole::CenterMid ||
            npc.getPositionRole() == PositionRole::DefensiveMid ||
            npc.getPositionRole() == PositionRole::AttackingMid);

        // ==========================================
        // --- THE PLAYMAKER'S SWITCH ---
        // ==========================================
        if (isMid && visionNorm > 0.7f && isCrammed) {
            // Which side of the pitch is the ball currently on?
            float pitchMidY = pitch.totalHeight / 2.f;
            bool ballOnRightSide = (npcPos.y > pitchMidY);
            bool targetOnRightSide = (predictedPos.y > pitchMidY);

            // If the target is on the completely opposite side of the pitch...
            if (ballOnRightSide != targetOnRightSide) {
                float lateralDistance = std::abs(npcPos.y - predictedPos.y);

                // If it's a true cross-field switch (> 25 meters lateral)
                if (lateralDistance > 2500.f) {
                    // Reward this massively to escape the swarm!
                    score += 4000.f * visionNorm * longPassNorm;
                }
            }
        }

        // 2. PUNISH THE HERO BALL
        if (distToMarker < 450.f) {
            // High awareness players realize the target is marked and will refuse the pass.
            // Low awareness players won't deduct enough points and will force it anyway!
            score -= 3500.f * visionNorm;

            // Extra massive penalty for forcing a forward pass into traffic
            if (forwardProgress > 500.f) {
                score -= 3000.f * visionNorm;
            }
        }

        // Trajectory specifics
        if (wantHigh) score -= (1.0f - longPassNorm) * 700.f;
        else if (directLaneBlocked && canCurlAround) score -= (1.0f - curlNorm) * 1000.f;
        else if (!directLaneBlocked) score += 400.f * shortPassNorm;

        if (score > bestScore) {
            bestScore = score;
            bestOption = target;
        }
    }

    // ==========================================
    // --- THE FIX: HIGHER STANDARDS ---
    // ==========================================
    // A player with 99 Vision will demand a score of > +400 to release a pass. 
    // If no pass is good enough, they hold the ball and dribble instead!
    // A player with 20 Vision has low standards (> -240) and will pass to almost anyone.
    float minimumAcceptableScore = isCrammed ? -2000.f : (-400.f + (visionNorm * 800.f));

    return (bestScore > minimumAcceptableScore) ? bestOption : nullptr;
}

sf::Vector2f PlayerAI::calculateDribbleDirection(NPCPlayer& npc, sf::Vector2f goalPos, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI) {
    sf::Vector2f npcPos = npc.getPosition();
    sf::Vector2f baseDir = normalize(goalPos - npcPos);

    float bcNorm = npc.getBallControl() / 100.f;
    float agilityNorm = npc.getAgility() / 100.f;
    float speedNorm = npc.getTopSpeed() / 100.f;
    float counterSpeed = teamAI.getPassingSpeedPref();
    float awarenessNorm = npc.getAwareness() / 100.f;

    // ==========================================
    // --- THE SPONGE BOUNDARY (Repulsive Force) ---
    // ==========================================
    float spongeDist = 800.f; // Activate the sponge 8 meters from the line
    sf::Vector2f boundaryPush(0.f, 0.f);

    float leftEdge = pitch.margin;
    float rightEdge = pitch.totalWidth - pitch.margin;
    float topEdge = pitch.margin;
    float bottomEdge = pitch.totalHeight - pitch.margin;

    // X-Axis (Goal lines)
    if (npcPos.x < leftEdge + spongeDist) {
        float factor = 1.0f - ((npcPos.x - leftEdge) / spongeDist);
        boundaryPush.x += factor * factor; // Quadratic curve for a soft-to-hard sponge feel
    }
    else if (npcPos.x > rightEdge - spongeDist) {
        float factor = 1.0f - ((rightEdge - npcPos.x) / spongeDist);
        boundaryPush.x -= factor * factor;
    }

    // Y-Axis (Touchlines)
    if (npcPos.y < topEdge + spongeDist) {
        float factor = 1.0f - ((npcPos.y - topEdge) / spongeDist);
        boundaryPush.y += factor * factor;
    }
    else if (npcPos.y > bottomEdge - spongeDist) {
        float factor = 1.0f - ((bottomEdge - npcPos.y) / spongeDist);
        boundaryPush.y -= factor * factor;
    }

    // If we are getting close to the edge, seamlessly bend our intended run direction inward!
    if (boundaryPush.x != 0.f || boundaryPush.y != 0.f) {
        // Players running at top speed have more momentum, so the sponge pushes them harder
        float bounceStrength = 1.5f + (speedNorm * 2.0f);
        baseDir = normalize(baseDir + (boundaryPush * bounceStrength));
    }

    sf::Vector2f bestDir = baseDir;
    float bestScore = -1e9f;

    PlayerBehavior behavior = npc.getPlaystyle().behavior;

    bool isSpeedster = (speedNorm > 0.85f && speedNorm > bcNorm);
    bool isTrickster = (bcNorm > 0.80f && agilityNorm > 0.80f);

    Player* closestOpp = findNearestOpponent(npcPos, opposition);
    float overallMinOppDist = closestOpp ? dist(npcPos, closestOpp->getPosition()) : 9999.f;
    sf::Vector2f toClosestOpp = closestOpp ? normalize(closestOpp->getPosition() - npcPos) : sf::Vector2f(0.f, 0.f);

    // ==========================================
    // --- DYNAMIC SHIELDING ---
    // ==========================================
    if (closestOpp) {
        sf::Vector2f currentMoveDir = npc.getVelocity();
        if (length(currentMoveDir) < 5.f) currentMoveDir = baseDir;

        float side = (currentMoveDir.x * toClosestOpp.y - currentMoveDir.y * toClosestOpp.x);

        if (side > 0 && npc.usingRightFoot()) npc.changeFoot();
        else if (side < 0 && !npc.usingRightFoot()) npc.changeFoot();

        if (overallMinOppDist < 300.f) {
            baseDir = normalize(baseDir + (-toClosestOpp * 0.4f * bcNorm));
        }
    }

    // Evaluate all 16 directional slices
    for (int i = 0; i < 16; ++i) {
        float angleDeg = -135.f + (i * (270.f / 15.f));
        float rad = angleDeg * 3.14159f / 180.f;
        sf::Vector2f testDir(baseDir.x * std::cos(rad) - baseDir.y * std::sin(rad), baseDir.x * std::sin(rad) + baseDir.y * std::cos(rad));

        // Base score: Forward momentum & Manager DNA
        float score = (200.f + (counterSpeed * 300.f) + (behavior.runFrequency * 200.f)) * (testDir.x * baseDir.x + testDir.y * baseDir.y);

        // DIRECTIONAL STICKINESS
        sf::Vector2f currentTargetDir = npc.getDribbleTargetDir();
        float stickiness = (testDir.x * currentTargetDir.x + testDir.y * currentTargetDir.y);
        if (stickiness > 0.95f) score += 400.f;

        // ==========================================
        // --- THE FIX: INTELLIGENT HOLD-UP PLAY ---
        // ==========================================
        if (overallMinOppDist < 250.f) {
            float escapeAlignment = (testDir.x * -toClosestOpp.x + testDir.y * -toClosestOpp.y);

            // 1. Scared/Defensive players turn their back naturally.
            // 2. BUT High-Awareness players (Target Men, Playmakers) will intentionally 
            // put their body between the ball and the defender to buy time for their team, 
            // even if they have high dribble confidence!
            float holdUpIntelligence = std::max(1.0f - behavior.dribbleBias, awarenessNorm);

            // If this direction slice points directly AWAY from the defender's chest:
            if (escapeAlignment > 0.5f) {
                // Heavily reward keeping your back to the man to shield the ball
                score += holdUpIntelligence * 1500.f * bcNorm;
            }
        }

        // ==========================================
        // --- THE "OPEN HIGHWAY" SCANNER ---
        // ==========================================
        float maxThreatInLane = 0.f;
        for (auto* opp : opposition) {
            float d = dist(npcPos, opp->getPosition());
            if (d < 800.f) {
                sf::Vector2f toOppLocal = normalize(opp->getPosition() - npcPos);
                float oppAlignment = (testDir.x * toOppLocal.x + testDir.y * toOppLocal.y);

                if (oppAlignment > 0.4f) {
                    float threat = (1.0f - (d / 800.f)) * oppAlignment;
                    if (threat > maxThreatInLane) maxThreatInLane = threat;
                }
            }
        }

        if (maxThreatInLane < 0.1f) {
            score += 2000.f * speedNorm * behavior.dribbleBias;
        }
        else {
            float fearFactor = 800.f - (behavior.dribbleBias * 500.f) - (bcNorm * 300.f);
            fearFactor = std::max(50.f, fearFactor);
            score -= maxThreatInLane * fearFactor;
        }

        // ==========================================
        // --- BEATING THE MAN vs ESCAPING ---
        // ==========================================
        if (closestOpp && overallMinOppDist < 400.f) {
            float testToOppDot = (testDir.x * toClosestOpp.x + testDir.y * toClosestOpp.y);

            // THE ESCAPE FIX
            if (testToOppDot < -0.2f) {
                score += 2500.f * speedNorm * behavior.dribbleBias * std::abs(testToOppDot);
            }
            // THE CONFRONTATION
            else if (testToOppDot > 0.0f) {
                if (isTrickster && overallMinOppDist < 250.f) {
                    float orthogonality = 1.0f - std::abs(testToOppDot);
                    if (orthogonality > 0.8f) score += 2500.f * bcNorm * agilityNorm * behavior.dribbleBias * orthogonality;
                    if (testToOppDot > 0.6f) score -= 4000.f;
                }
                else if (isSpeedster) {
                    if (testToOppDot > 0.4f && testToOppDot < 0.85f) score += 1800.f * speedNorm * behavior.dribbleBias;
                    if (testToOppDot > 0.85f) score -= 4000.f;
                }
                else {
                    if (testToOppDot > 0.5f) score -= 2500.f * (1.0f - agilityNorm);
                }
            }
        }

        // ==========================================
        // --- THE GRADIENT SPONGE PENALTY ---
        // ==========================================
        // Instead of a hard wall, we scale the projection by the player's speed.
        // Fast players look further ahead to brake earlier!
        float projectionDist = 200.f + (speedNorm * 250.f);
        sf::Vector2f projectedPos = npcPos + testDir * projectionDist;

        float distToLeft = projectedPos.x - leftEdge;
        float distToRight = rightEdge - projectedPos.x;
        float distToTop = projectedPos.y - topEdge;
        float distToBottom = bottomEdge - projectedPos.y;

        float minBoundDist = std::min({ distToLeft, distToRight, distToTop, distToBottom });

        if (minBoundDist < 0.f) {
            score -= 10000.f; // Definitely out of bounds, kill this slice
        }
        else if (minBoundDist < 300.f) {
            // The closer the projected position is to the line, the heavier the penalty.
            // This allows them to run safely *parallel* to the line without panicking, 
            // but heavily punishes turning their nose outward!
            score -= (300.f - minBoundDist) * 15.f;
        }

        if (score > bestScore) {
            bestScore = score;
            bestDir = testDir;
        }
    }

    npc.setDribbleTargetDir(bestDir);
    return bestDir;
}

void PlayerAI::executePass(NPCPlayer& npc, Ball& ball, Player* target, const std::vector<Player*>& opposition, SoundManager& soundManager) {
    sf::Vector2f npcPos = npc.getPosition();
    sf::Vector2f targetPos = target->getPosition();
    float distToTarget = dist(npcPos, targetPos);
    sf::Vector2f directDir = normalize(targetPos - npcPos);

    bool goHigh = (distToTarget > 2000.f);
    bool needsCurl = false;
    float curlSide = 0.f;

    // 1. SCAN FOR BLOCKERS: Decide if we need to go over or around
    for (auto* opp : opposition) {
        sf::Vector2f toOpp = opp->getPosition() - npcPos;
        float dOpp = dist(npcPos, opp->getPosition());

        if (dOpp < distToTarget && dOpp > 100.f) {
            float alignment = (directDir.x * (toOpp.x / dOpp) + directDir.y * (toOpp.y / dOpp));
            if (alignment > 0.90f) {
                if (distToTarget < 2500.f && (npc.getCurl() / 100.f) > 0.5f) {
                    needsCurl = true;
                    goHigh = false;
                    float cross = (directDir.x * toOpp.y - directDir.y * toOpp.x);
                    curlSide = (cross > 0) ? -1.0f : 1.0f;
                }
                else {
                    goHigh = true;
                }
            }
        }
    }

    // ==========================================
    // --- 2. APPLY RAW STAT ERROR FIRST ---
    // ==========================================
    float passingStat = goHigh ? npc.getLongPassing() : npc.getShortPassing();

    bool usingRight = npc.usingRightFoot();
    bool isRightFooted = (npc.getPreferredFoot() == "Right");
    bool isWeakFoot = (isRightFooted && !usingRight) || (!isRightFooted && usingRight);

    // Base error: up to 15 degrees of pure inaccuracy for a 0 passing stat player
    float errorAngle = (1.0f - (passingStat / 100.0f)) * 15.0f;
    float wfPowerMod = 1.0f;

    if (isWeakFoot) {
        float eMod;
        float shank = getWeakFootPenalty(npc.getWeakFootAccuracy(), wfPowerMod, eMod);
        errorAngle = (errorAngle * eMod) + shank;
    }

    // Mathematically shank the aim direction BEFORE it hits the assist
    float randError = ((rand() % 100) / 100.f - 0.5f) * errorAngle;
    float rad = randError * 3.14159f / 180.f;
    sf::Vector2f shankedAim(
        directDir.x * std::cos(rad) - directDir.y * std::sin(rad),
        directDir.x * std::sin(rad) + directDir.y * std::cos(rad)
    );

    // ==========================================
    // --- 3. APPLY AIM ASSIST (The Corrector) ---
    // ==========================================
    // High magnetism will correct the shankedAim perfectly. Low magnetism will leave it wild!
    float finalPower = 0.f;
    sf::Vector2f finalAim = shankedAim;
    AimAssist::applyPassAssist(npc, target, finalAim, finalPower, goHigh, true);

    // Friction Compensation and Weak Foot Power Drop
    finalPower *= wfPowerMod;
    if (!goHigh) finalPower *= 1.18f; // Punch through grass
    else finalPower *= 0.75f;

    finalPower = std::min(finalPower, npc.getKickPower());
    float kickStrength = std::clamp(finalPower / npc.getKickPower(), 0.0f, 1.0f);

    // ==========================================
    // --- 4. CURL LOGIC & MAGNUS EFFECT ---
    // ==========================================
    float finalSpin = 0.f;

    if (!needsCurl && (rand() % 100 < npc.getCurl())) {
        needsCurl = true;
        // 70% chance to use the natural Inside Foot, 30% chance for an Outside-of-Boot pass
        bool wantInsideFoot = (rand() % 100 < 70);
        curlSide = wantInsideFoot ? (usingRight ? -1.0f : 1.0f) : (usingRight ? 1.0f : -1.0f);
    }

    if (needsCurl) {
        float multiplier = (1.1f + kickStrength / 2.f);
        bool isLeftFoot = !usingRight;

        // Inside foot gives 100% spin, Outside of boot gives 50% spin
        if (isLeftFoot) {
            finalSpin = (curlSide > 0) ? (npc.getCurl() * multiplier) : (-(npc.getCurl() / 2.f) * multiplier);
        }
        else {
            finalSpin = (curlSide < 0) ? (-npc.getCurl() * multiplier) : ((npc.getCurl() / 2.f) * multiplier);
        }

        if (isWeakFoot) finalSpin *= (0.4f + (npc.getWeakFootAccuracy() / 5.0f) * 0.6f);

        // THE FIX 1: TIME-SCALED MAGNUS OFFSET
        // The Magnus effect displaces the ball continuously over time. 
        // A long pass requires a much wider starting angle to bend back to the target!
        float distanceScale = std::clamp(distToTarget / 1200.f, 0.4f, 3.5f); // Scales based on pass length
        float baseOffsetDegrees = 12.f;

        float offsetRad = -((finalSpin / 100.f) * baseOffsetDegrees * distanceScale) * (3.14159f / 180.f);

        finalAim = sf::Vector2f(
            finalAim.x * std::cos(offsetRad) - finalAim.y * std::sin(offsetRad),
            finalAim.x * std::sin(offsetRad) + finalAim.y * std::cos(offsetRad)
        );
    }

    // ==========================================
       // --- 5. TRAJECTORY (VZ) ---
       // ==========================================
    float vzPower = 10.f + (std::pow(kickStrength, 2.f) * 850.f);
    float finalBackspin = 0.f;

    if (goHigh) {
        float maxLoft = std::clamp(500.f + (distToTarget * 0.25f), 700.f, 1150.f);
        vzPower = std::max(maxLoft - ((passingStat / 100.f) * 80.f), 300.f);
        finalBackspin = 60.f + (passingStat * 0.5f);

        // THE FIX: DO NOT overwrite finalPower here anymore! 
        // AimAssist just calculated the exact Parabolic power needed.

        // Backspin causes the ball to "hang" in the air and travel less horizontally, so add a tiny bump
        finalPower *= (1.0f + (finalBackspin / 1500.f));

        // Cap to their physical limits so weak players can't magically ping 80m passes
        finalPower = std::min(finalPower, npc.getKickPower());
    }

    float kickVol = std::clamp(0.0f + (finalPower / npc.getKickPower()) * 40.0f, 10.f, 100.f);
    soundManager.playRandomSound("kick", 3, kickVol, 0.15f);

    ball.shoot(finalAim, finalPower, finalSpin, vzPower, finalBackspin);
    npc.resetKickCooldown();
}

void PlayerAI::executeShot(NPCPlayer& npc, Ball& ball, sf::Vector2f goalPos, const std::vector<Player*>& opposition, const Pitch& pitch, float dt, SoundManager& soundManager) {
    sf::Vector2f npcPos = npc.getPosition();

    // 1. TACTICAL SCAN: Spot if the Keeper is out of position for a chip
    bool tryChip = false;
    for (auto* opp : opposition) {
        if (opp->getPositionRole() == PositionRole::Goalkeeper) {
            float gkDistFromLine = std::abs(opp->getPosition().x - goalPos.x);
            if (gkDistFromLine > 400.f && (rand() % 100) < (npc.getFinishing() * 0.8f)) {
                tryChip = true;
            }
            break;
        }
    }

    // 2. INITIAL AIM: Aim for the corners!
    float topPostY = 3500.f - 366.f;
    float bottomPostY = 3500.f + 366.f;
    float rawTargetY;

    // THE FIX 3: 85% chance to aim at the corners, 15% chance to blast it down the middle
    int aimRoll = rand() % 100;
    if (aimRoll < 40) {
        // Aim near the top post (with slight variance inside the post)
        rawTargetY = topPostY + 40.f + (static_cast<float>(rand()) / RAND_MAX * 150.f);
    }
    else if (aimRoll < 80) {
        // Aim near the bottom post
        rawTargetY = bottomPostY - 40.f - (static_cast<float>(rand()) / RAND_MAX * 150.f);
    }
    else {
        // Aim generally at the center
        rawTargetY = 3500.f + (((rand() % 200) - 100) / 100.f * 150.f);
    }

    sf::Vector2f aimDir = normalize(sf::Vector2f(goalPos.x, rawTargetY) - npcPos);

    // ==========================================
     // --- 3. APPLY RAW STAT ERROR FIRST ---
     // ==========================================
    bool usingRight = npc.usingRightFoot();
    bool isRightFooted = (npc.getPreferredFoot() == "Right");
    bool isWeakFoot = (isRightFooted && !usingRight) || (!isRightFooted && usingRight);

    float simulatedCharge = 0.6f + ((rand() % 41) / 100.f);
    float basePower = npc.getKickPower() * simulatedCharge;
    float finalPower = basePower;

    float baseError = (1.0f - (npc.getFinishing() / 100.0f)) * 10.0f; // High base error for shots
    float wfPowerMod = 1.0f;

    // ==========================================
    // --- NEW: FIRST TIME SHOT PENALTY ---
    // ==========================================
    bool isFirstTimeShot = (npc.m_possessionTimer < 0.4f);
    if (isFirstTimeShot) {
        // First time shots are wild and lack power, especially for poor finishers!
        float rushPenalty = (1.0f - (npc.getFinishing() / 100.f)) * 15.0f;

        // Add at least 10 degrees of pure error, scaling up to 35 for bad shooters
        baseError += 10.0f + rushPenalty;

        // Slash power by 15% because they didn't plant their feet and set up the strike
        finalPower *= 0.85f;
    }

    if (isWeakFoot) {
        float eMod;
        float shank = getWeakFootPenalty(npc.getWeakFootAccuracy(), wfPowerMod, eMod);
        finalPower *= wfPowerMod;
        baseError = (baseError * eMod) + shank;
    }

    float randError = ((rand() % 100) / 100.f - 0.5f) * baseError;
    float rad = randError * 3.14159f / 180.f;

    // aimDir is now mathematically shanked based on their stats & rush penalty
    aimDir = sf::Vector2f(aimDir.x * std::cos(rad) - aimDir.y * std::sin(rad), aimDir.x * std::sin(rad) + aimDir.y * std::cos(rad));
    // ==========================================
    // --- 4. CORNER SNAPPING (AimAssist Corrector) ---
    // ==========================================
    float vzPower = 10.f + (std::pow(simulatedCharge, 2.f) * 850.f);
    AimAssist::applyShotAssist(npc, aimDir, vzPower, finalPower, pitch);

    float finalBackspin = 0.f;
    float finalSpin = 0.f;

    // 5. OVERRIDE TRAJECTORY: Handle Chip vs High vs Low Driven
    if (tryChip) {
        float distToGoal = dist(npcPos, goalPos);
        float floatMultiplier = 1.1f - (simulatedCharge * 0.4f);
        finalPower = std::min((distToGoal / 52.0f) * floatMultiplier, npc.getKickPower() * floatMultiplier);
        vzPower = 800.f + ((npc.getFinishing() / 100.f) * 80.f);
        finalBackspin = 90.f + ((npc.getFinishing() / 100.f) * 50.f);

        // Add specific chip error
        float errorRad = ((rand() % 100) / 100.f - 0.5f) * 10.0f * (3.14159f / 180.f);
        aimDir = sf::Vector2f(aimDir.x * std::cos(errorRad) - aimDir.y * std::sin(errorRad), aimDir.x * std::sin(errorRad) + aimDir.y * std::cos(errorRad));
    }
    else {
        bool isHighShot = (rand() % 100 > 10);
        if (isHighShot) {
            finalPower *= (1.1f - (simulatedCharge * 0.4f)) * 1.2f;
            finalBackspin = 50.f + (npc.getFinishing() * 0.8f);
        }
        else {
            finalPower *= 1.2f;
            vzPower *= 0.50f;   // Stay on the deck
        }

        // ==========================================
        // --- INTENTIONAL BEND (Shooting) ---
        // ==========================================
        // Almost all players attempt to curl shots into the corners, but the effectiveness
        // is governed entirely by their Curl stat!
        if ((rand() % 100) < (npc.getCurl() + 40.f)) { // High tendency to bend shots

            // If aiming at the bottom post (Y > 3500), curl it "Up" (Negative spin)
            // If aiming at the top post (Y < 3500), curl it "Down" (Positive spin)
            float curlDir = (aimDir.y < 3500.f) ? -1.0f : 1.0f;

            bool isLeftFoot = !usingRight;
            bool isInsideFoot = isLeftFoot ? (curlDir > 0) : (curlDir < 0);

            // Base spin comes from the Curl stat, slightly boosted by Finishing
            float rawSpin = npc.getCurl() * (0.8f + (npc.getFinishing() / 100.f) * 0.4f);

            // USER MATH: Outside of the boot generates half the spin of the inside foot
            if (!isInsideFoot) rawSpin /= 2.f;

            finalSpin = curlDir * rawSpin * (1.1f + simulatedCharge / 2.f);

            if (isWeakFoot) finalSpin *= (0.4f + (npc.getWeakFootAccuracy() / 5.0f) * 0.6f);

            // THE FIX: Distance-scaled Magnus offset for shots!
            float distToGoal = dist(npcPos, goalPos);
            float distanceScale = std::clamp(distToGoal / 1200.f, 0.4f, 2.5f);

            // Offset the aim so the shot bends INTO the corner, rather than missing wide!
            float offsetRad = -curlDir * (15.0f * (npc.getCurl() / 100.f) * distanceScale) * (3.14159f / 180.f);

            aimDir = sf::Vector2f(
                aimDir.x * std::cos(offsetRad) - aimDir.y * std::sin(offsetRad),
                aimDir.x * std::sin(offsetRad) + aimDir.y * std::cos(offsetRad)
            );
        }
        finalPower = std::min(finalPower, npc.getKickPower() * (vzPower > 100.f ? 1.0f : 1.1f));
    }

    float kickVol = std::clamp(0.0f + (finalPower / npc.getKickPower()) * 40.f, 10.f, 100.f);
    soundManager.playRandomSound("kick", 3, kickVol, 0.15f);

    ball.shoot(aimDir, finalPower, finalSpin, vzPower, finalBackspin);
    npc.resetKickCooldown();
}

Player* PlayerAI::identifyTargetReceiver(Ball& ball, const std::vector<Player*>& team) {
    if (ball.hasOwner() || ball.getLastOwner() == nullptr) return nullptr;

    sf::Vector2f ballVel = ball.getVelocity();
    float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);
    if (ballSpeed < 50.f) return nullptr; // Ball is dead/stopping

    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f ballDir = ballVel / ballSpeed;

    Player* bestReceiver = nullptr;
    float minTime = 9999.f;

    for (Player* p : team) {
        if (p == ball.getLastOwner() || p->getPositionRole() == PositionRole::Goalkeeper || p->isSentOff()) continue;

        sf::Vector2f pPos = p->getPosition();
        sf::Vector2f toPlayer = pPos - ballPos;

        // 1. Is the ball heading towards them?
        float projection = (toPlayer.x * ballDir.x + toPlayer.y * ballDir.y);
        if (projection < -50.f) continue;

        // 2. Are they close to the passing lane?
        float lateralDist = std::abs(toPlayer.x * ballDir.y - toPlayer.y * ballDir.x);
        if (lateralDist > 1500.f) continue;

        // ==========================================
        // --- EXACT PHYSICS TIMING ---
        // ==========================================
        float ballTime = 999.f;

        if (ball.z > 40.f) {
            // Air passes: Ball time is estimated via X/Y horizontal velocity
            ballTime = projection / ballSpeed;
        }
        else {
            // Ground passes: Solve quadratic equation for deceleration
            // 0.5 * a * t^2 - v * t + d = 0
            float a = 0.5f * ball.friction;
            float b = -ballSpeed;
            float c = projection;
            float discriminant = (b * b) - (4.f * a * c);

            if (discriminant >= 0.f) {
                // The smaller root tells us when the ball FIRST arrives at that distance
                ballTime = (-b - std::sqrt(discriminant)) / (2.f * a);
                if (ballTime < 0.f) ballTime = (-b + std::sqrt(discriminant)) / (2.f * a);
            }
            else {
                // The ball will physically stop due to friction before it reaches them!
                ballTime = ballSpeed / ball.friction;
            }
        }

        float playerTime = lateralDist / (p->getTopSpeed() * 10.f + 1.f);
        float totalScore = ballTime + (playerTime * 1.5f);

        if (totalScore < minTime) {
            minTime = totalScore;
            bestReceiver = p;
        }
    }
    return bestReceiver;
}

// ==========================================
// --- GOALKEEPER AI ---
// ==========================================

void PlayerAI::handleGoalkeeping(NPCPlayer& npc, Ball& ball, const Pitch& pitch, const std::vector<Player*>& team, const std::vector<Player*>& opposition, float dt, const TeamAI& teamAI, SoundManager& soundManager) {
    if (npc.getState() == PlayerState::Diving) {
        PhysicsEngine::applyKeeperDiveFriction(npc, dt);
        if (npc.getState() == PlayerState::Diving) return;
    }

    if (npc.getBallPossession()) {
        npc.m_possessionTimer += dt;

        sf::Vector2f oppGoal = teamAI.isHome() ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);
        npc.setRotationToward(oppGoal);

        Player* opp = findNearestOpponent(npc.getPosition(), opposition);
        float oppDist = opp ? dist(npc.getPosition(), opp->getPosition()) : 9999.f;
        PlaystyleType type = npc.getPlaystyle().type;

        // Calculate how comfortable the GK is with the ball at their feet (0.0 to 1.0)
        float gkDribbleSkill = (npc.getBallControl() * 0.7f + npc.getAgility() * 0.3f) / 100.f;

        // THE FIX 2a: GK Patience should be 1-3 seconds, not 6!
        float patience = (type == PlaystyleType::SweeperKeeper) ? 2.5f : 0.5f;
        patience += (gkDribbleSkill * 1.5f); // Max patience is now ~4.0 seconds

        // Calculate distance to the edge of the penalty box
        float penaltyBoxX = teamAI.isHome() ? pitch.margin + 1650.f : pitch.totalWidth - pitch.margin - 1650.f;
        float distToBoxEdge = teamAI.isHome() ? (penaltyBoxX - npc.getPosition().x) : (npc.getPosition().x - penaltyBoxX);

        // ==========================================
        // 1. HIGH PRESSURE (The Panic Zone vs The Turn)
        // ==========================================
        if (oppDist < 450.f) {

            // THE ALISSON TURN: 
            // Only do this if we are safe inside the box and have space to turn!
            if (gkDribbleSkill > 0.75f && oppDist > 150.f && distToBoxEdge > 300.f) {
                sf::Vector2f toOpp = opp->getPosition() - npc.getPosition();
                sf::Vector2f awayDir = normalize(-toOpp);

                sf::Vector2f sideStep(-awayDir.y, awayDir.x);
                if ((rand() % 100) > 50) sideStep = -sideStep;

                sf::Vector2f dribbleDir = normalize(awayDir + sideStep * 0.5f);
                npc.setVelocity(dribbleDir * (npc.getTopSpeed() * 8.0f));
                return;
            }

            // PANIC CLEARANCE
            // 100% Kick Power and massive VZ height so it actually clears the attacker!
            float clearPower = npc.getKickPower() * 0.60f;
            float vzPower = 1200.f;

            sf::Vector2f clearDir = teamAI.isHome() ? sf::Vector2f(1.f, 0.f) : sf::Vector2f(-1.f, 0.f);

            float maxError = 15.0f - (gkDribbleSkill * 10.0f);
            float randError = ((rand() % 200) - 100) / 100.f * maxError;
            float rad = randError * 3.14159f / 180.f;

            sf::Vector2f finalDir(
                clearDir.x * std::cos(rad) - clearDir.y * std::sin(rad),
                clearDir.x * std::sin(rad) + clearDir.y * std::cos(rad)
            );

            float kickVol = std::clamp(0.f + (clearPower / npc.getKickPower()) * 40.0f, 10.f, 100.f);
            soundManager.playRandomSound("kick", 3, kickVol, 0.15f);
            ball.shoot(finalDir, clearPower, 0.f, vzPower, 50.f);

            npc.m_possessionTimer = 0.0f;
            npc.resetKickCooldown();
            npc.setVelocity({ 0.f, 0.f });
        }
        // ==========================================
        // 2. DISTRIBUTION (Patience Expired)
        // ==========================================
        else if (npc.m_possessionTimer > patience) {
            distributeBallAsGoalie(npc, ball, team, opposition, pitch, teamAI, soundManager);
            npc.m_possessionTimer = 0.0f;
            npc.setVelocity({ 0.f, 0.f });
        }
        // ==========================================
        // 3. FREE SPACE (Carrying the Ball Out)
        // ==========================================
        else {
            // Only jog forward if we are safely deep inside the penalty box!
            if (type == PlaystyleType::SweeperKeeper && gkDribbleSkill > 0.6f && distToBoxEdge > 400.f) {
                sf::Vector2f forwardDir = teamAI.isHome() ? sf::Vector2f(1.f, 0.f) : sf::Vector2f(-1.f, 0.f);
                float carrySpeed = npc.getTopSpeed() * 3.0f * gkDribbleSkill;
                npc.setVelocity(forwardDir * carrySpeed);
            }
            else {
                npc.setVelocity({ 0.f, 0.f });
            }
        }
        return;
    }
    npc.m_possessionTimer = 0.0f;

    // ==========================================
        // --- THE FIX: GK AUTO-PICKUP (HANDS) ---
        // ==========================================
    float dx = npc.getPosition().x - ball.getPosition().x;
    float dy = npc.getPosition().y - ball.getPosition().y;
    float distToBall = std::sqrt(dx * dx + dy * dy);

    // Ensure they are actually inside their penalty box to use their hands!
    float penaltyBoxEdgeX = teamAI.isHome() ? pitch.margin + 1650.f : pitch.totalWidth - pitch.margin - 1650.f;
    bool inOwnBoxX = teamAI.isHome() ? (npc.getPosition().x < penaltyBoxEdgeX) : (npc.getPosition().x > penaltyBoxEdgeX);
    bool inOwnBoxY = (npc.getPosition().y > 3500.f - 2040.f && npc.getPosition().y < 3500.f + 2040.f);

    // THE FIX: Added npc.getKickCooldown() <= 0.0f !!!
    // This stops the keeper from instantly re-catching the ball the exact millisecond they distribute it!
    if (!ball.hasOwner() && distToBall < 90.f && inOwnBoxX && inOwnBoxY && ball.z < 200.f && npc.getKickCooldown() <= 0.0f) {
        if (npc.getState() != PlayerState::Stunned && npc.getState() != PlayerState::FallOver) {
            ball.possess(&npc);
            npc.setVelocity({ 0.f, 0.f }); // Slam the brakes so they don't slide into the net with the ball!
            return;
        }
    }

    sf::Vector2f myGoalCenter = teamAI.isHome() ? pitch.homeGoalCenter : pitch.awayGoalCenter;
    sf::Vector2f targetPos;
    bool sprint = false;

    // THE FIX: Added pitch.margin to the box check so they accurately scan their area!
    bool ballInBoxX = teamAI.isHome() ? (ball.getPosition().x < pitch.margin + 1650.f) : (ball.getPosition().x > pitch.totalWidth - pitch.margin - 1650.f);
    bool ballInBoxY = (ball.getPosition().y > 3500.f - 2000.f && ball.getPosition().y < 3500.f + 2000.f);

    Player* nearestAttacker = findNearestOpponent(ball.getPosition(), opposition);
    float attackerDist = nearestAttacker ? dist(nearestAttacker->getPosition(), ball.getPosition()) : 9999.f;
    float keeperTTI = dist(npc.getPosition(), ball.getPosition()) / (npc.getTopSpeed() * 10.0f > 0 ? npc.getTopSpeed() * 10.0f : 1.0f);
    float attackerTTI = attackerDist / (nearestAttacker ? nearestAttacker->getTopSpeed() * 10.0f : 1.0f);
    bool shouldRush = dist(ball.getPosition(), myGoalCenter) < 800.f && (keeperTTI + (100.0f - npc.getGkAwareness() * 0.05f) < attackerTTI - 0.2f);

    if (!ball.hasOwner() && ballInBoxX && ballInBoxY && ball.z < 100.f) { targetPos = ball.getPosition(); sprint = true; }
    else if (shouldRush) { targetPos = ball.getPosition(); sprint = true; }
    else {
        // Calculate goal positioning
        sf::Vector2f directionToBall = normalize(ball.getPosition() - myGoalCenter);
        float actualStepOutDistance = std::min(250.0f * (npc.getGkCoverage() / 100.0f), dist(myGoalCenter, ball.getPosition()) * 0.15f);
        targetPos = myGoalCenter + (directionToBall * actualStepOutDistance);
        targetPos.y = std::clamp(targetPos.y, myGoalCenter.y - 366.0f + 40.0f, myGoalCenter.y + 366.0f - 40.0f);
        if (teamAI.isHome()) targetPos.x = std::clamp(targetPos.x, myGoalCenter.x, myGoalCenter.x + 250.0f);
        else targetPos.x = std::clamp(targetPos.x, myGoalCenter.x - 250.0f, myGoalCenter.x);
        sprint = false;
    }

    // ==========================================
    // --- THE SMOOTH GK MOVEMENT FIX ---
    // ==========================================
    float distToTarget = dist(npc.getPosition(), targetPos);

    // 1. Calculate the ideal max speed for this movement
    float maxSpeed = sprint ? (npc.getTopSpeed() * 10.0f) : 400.0f + ((std::max(npc.getAgility(), npc.getGkReactions()) / 100.0f) * 200.0f);

    sf::Vector2f desiredVelocity(0.f, 0.f);

    // 2. Expanded Deadzone & Arrival Easing
    if (distToTarget > 15.0f) {
        float slowingRadius = 150.0f; // Start hitting the brakes 1.5 meters out
        float actualSpeed = maxSpeed;

        if (distToTarget < slowingRadius) {
            // Smoothly ramp down speed as they approach the exact spot
            float ramp = distToTarget / slowingRadius;
            actualSpeed = maxSpeed * ramp;
        }

        // Prevent mathematical overshooting in a single frame
        actualSpeed = std::min(actualSpeed, distToTarget / dt);

        desiredVelocity = normalize(targetPos - npc.getPosition()) * actualSpeed;
    }

    // 3. Velocity Interpolation (Lerping)
    // Instead of instantly snapping to the new velocity, blend it smoothly.
    // This gives the goalkeeper physical "weight" and inertia on the line.
    sf::Vector2f currentVel = npc.getVelocity();
    float responsiveness = 12.0f; // Higher = snappier, Lower = smoother/heavier

    sf::Vector2f smoothedVel = currentVel + (desiredVelocity - currentVel) * (responsiveness * dt);

    // 4. Kill remaining micro-vibrations
    if (std::abs(smoothedVel.x) < 2.0f && std::abs(smoothedVel.y) < 2.0f) {
        smoothedVel = { 0.f, 0.f };
    }

    npc.setVelocity(smoothedVel);
    npc.setRotationToward(ball.getPosition());

    // Dive Math
    float ballSpeed = std::sqrt(ball.getVelocity().x * ball.getVelocity().x + ball.getVelocity().y * ball.getVelocity().y);
    if (ballSpeed > 300.0f && ((teamAI.isHome() && ball.getVelocity().x < -10.0f) || (!teamAI.isHome() && ball.getVelocity().x > 10.0f))) {
        float ballTTI = std::abs((npc.getPosition().x - ball.getPosition().x) / ball.getVelocity().x);
        if (ballTTI >= 0.0f && ballTTI <= 1.5f) {
            float interceptZ = std::max(0.f, ball.z + (ball.vz * ballTTI) - (0.5f * 980.f * ballTTI * ballTTI));
            float interceptY = ball.getPosition().y + (ball.getVelocity().y * ballTTI);
            float diveDistance = std::abs(npc.getPosition().y - interceptY);
            float maxDiveSpeed = 600.0f + (((dist(npc.getPosition(), ball.getPosition()) < 600.f) ? npc.getGkBlocking() : npc.getGkReactions()) / 100.0f * 1000.0f);

            if (ballTTI <= (diveDistance / maxDiveSpeed) + 0.15f && diveDistance <= 800.0f && interceptZ <= npc.height + 120.0f) {
                float finalSpeed = std::clamp((ballTTI > 0.05f) ? diveDistance / ballTTI : maxDiveSpeed, 150.0f, maxDiveSpeed);
                npc.setVelocity(normalize(sf::Vector2f(npc.getPosition().x, interceptY) - npc.getPosition()) * finalSpeed);
                npc.vz = (interceptZ < 40.f) ? 0.f : ((interceptZ < 120.f) ? 140.f : 200.f + (interceptZ * 0.2f));
                npc.setState(PlayerState::Diving);
            }
        }
    }
}

sf::Vector2f PlayerAI::calculateGoaliePositioning(NPCPlayer& npc, sf::Vector2f ballPos, sf::Vector2f goalCenter, const Pitch& pitch)
{
    sf::Vector2f goalToBall = ballPos - goalCenter;
    float distToBall = dist(goalCenter, ballPos);
    if (distToBall < 1.0f) return goalCenter;

    PlaystyleType type = npc.getPlaystyle().type;

    // --- DNA INJECTION: BASE POSITIONING ---
    float baseMaxStep = 250.0f; // Standard
    if (type == PlaystyleType::SweeperKeeper) baseMaxStep = 700.0f; // Pushes way up!
    else if (type == PlaystyleType::OnTheLine) baseMaxStep = 50.0f;  // Glued to the line

    sf::Vector2f directionToBall = normalize(goalToBall);
    float maxStep = baseMaxStep * (npc.getGkCoverage() / 100.0f);

    // Sweepers react to the ball further up the pitch (30% scale vs 15% scale)
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

bool PlayerAI::shouldGoalieRush(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI)
{
    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f keeperPos = npc.getPosition();
    bool isHomeSide = teamAI.isHome();
    sf::Vector2f myGoalCenter = isHomeSide ? pitch.homeGoalCenter : pitch.awayGoalCenter;

    Player* nearestAttacker = findNearestOpponent(ballPos, opposition);
    if (!nearestAttacker) return false;

    float closestAttackerDist = dist(nearestAttacker->getPosition(), ballPos);
    float keeperDistToBall = dist(keeperPos, ballPos);

    float keeperSpeed = npc.getTopSpeed() * 10.0f;
    float attackerSpeed = nearestAttacker->getTopSpeed() * 10.0f;

    float keeperTTI = keeperDistToBall / (keeperSpeed > 0 ? keeperSpeed : 1.0f);
    float attackerTTI = closestAttackerDist / (attackerSpeed > 0 ? attackerSpeed : 1.0f);

    // --- DNA INJECTION: RUSH DECISION ---
    float hesitationPenalty = (100.0f - npc.getGkAwareness()) * 0.05f;
    PlaystyleType type = npc.getPlaystyle().type;

    if (type == PlaystyleType::SweeperKeeper) hesitationPenalty *= 0.1f; // Instant reaction, no fear
    else if (type == PlaystyleType::OnTheLine) hesitationPenalty *= 3.0f; // Terrified to leave the box

    float perceivedKeeperTTI = keeperTTI + hesitationPenalty;

    return (perceivedKeeperTTI < (attackerTTI - 0.2f));
}

void PlayerAI::attemptSave(NPCPlayer& npc, Ball& ball, float dt, const TeamAI& teamAI)
{
    sf::Vector2f ballVel = ball.getVelocity();
    float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);
    if (ballSpeed < 300.0f) return;

    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f keeperPos = npc.getPosition();

    bool isHomeSide = teamAI.isHome();
    if (isHomeSide && ballVel.x >= -10.0f) return;
    if (!isHomeSide && ballVel.x <= 10.0f) return;

    float ballTTI = std::abs((keeperPos.x - ballPos.x) / ballVel.x);
    if (ballTTI < 0.0f || ballTTI > 1.5f) return;

    float gravity = 980.f;
    float interceptZ = ball.z + (ball.vz * ballTTI) - (0.5f * gravity * ballTTI * ballTTI);
    interceptZ = std::max(0.f, interceptZ);

    float interceptY = ballPos.y + (ballVel.y * ballTTI);
    sf::Vector2f interceptPoint(keeperPos.x, interceptY);
    float diveDistance = std::abs(keeperPos.y - interceptY);

float distToBall = dist(keeperPos, ballPos);
    float activeStat = (distToBall < 600.0f) ? npc.getGkBlocking() : npc.getGkReactions();
    float maxDiveSpeed = 800.0f + ((activeStat / 100.0f) * 1200.0f);

    // ==========================================
    // --- THE LOW SHOT FIX: COLLAPSE DIVES ---
    // ==========================================
    // Dropping to the floor is physically faster than leaping into the air.
    // If the projected shot is low (< 60px high), we give them a 35% speed boost 
    // to snap down and cover the bottom corners instantly!
    if (interceptZ < 60.0f) {
        maxDiveSpeed *= 1.35f;
    }

    float keeperTTI = diveDistance / maxDiveSpeed;
    if (ballTTI > keeperTTI + 0.25f) return; // Cannot reach it in time

    // BUFF 3: Increased dive radius from 800px (8m) to 1000px (10m) to cover the whole net.
    float attemptDiveRadius = 1000.0f;

    if (diveDistance <= attemptDiveRadius && interceptZ <= (npc.height + 150.0f)) {
        float optimalSpeed = maxDiveSpeed;
        if (ballTTI > 0.05f) optimalSpeed = diveDistance / ballTTI;

        // Let them launch at higher minimum speeds
        float finalSpeed = std::clamp(optimalSpeed, 350.0f, maxDiveSpeed);

        triggerDive(npc, interceptPoint, finalSpeed, interceptZ);
    }
}

void PlayerAI::triggerDive(NPCPlayer& npc, sf::Vector2f diveTarget, float jumpSpeed, float targetZ)
{
    sf::Vector2f diveDir = normalize(diveTarget - npc.getPosition());
    npc.setVelocity(diveDir * jumpSpeed);

    if (targetZ < 40.f) npc.vz = 0.f;
    else if (targetZ < 120.f) npc.vz = 140.f;
    else npc.vz = 200.f + (targetZ * 0.2f);

    npc.setState(PlayerState::Diving);
}

void PlayerAI::distributeBallAsGoalie(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI, SoundManager& soundManager)
{
    Player* bestTarget = nullptr;
    float bestScore = -1e9f;
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);

    PlayerBehavior behavior = npc.getPlaystyle().behavior;
    float passRisk = behavior.passRiskBias;
    float tikiTakaPref = 1.0 - teamAI.getPassingLengthPref();

    for (Player* mate : teammates) {
        if (mate == &npc || mate->getPositionRole() == PositionRole::Goalkeeper) continue; // NEVER pass to yourself!

        sf::Vector2f matePos = mate->getPosition();
        float d = dist(npcPos, matePos);
        float progress = isHome ? (matePos.x - npcPos.x) : (npcPos.x - matePos.x);

        float score = (progress * 0.5f);

        // --- DNA & TACTICAL OVERRIDES ---
        if (d < 1500.f) {
            score += (tikiTakaPref * 1500.f);
            score += (1.0f - passRisk) * 800.f;
        }
        else if (d > 3000.f) {
            score += ((teamAI.getPassingLengthPref()) * 1500.f);
            score += (passRisk * 1200.f) * (npc.getLongPassing() / 100.f);
        }

        // --- PASSING LANE SAFETY CHECK ---
        sf::Vector2f passDir = normalize(matePos - npcPos);
        bool laneBlocked = false;

        for (Player* opp : opposition) {
            float dOpp = dist(npcPos, opp->getPosition());
            if (dOpp < d && dOpp > 100.f) {
                sf::Vector2f toOpp = opp->getPosition() - npcPos;
                if ((passDir.x * (toOpp.x / dOpp) + passDir.y * (toOpp.y / dOpp)) > 0.95f) {
                    laneBlocked = true;
                    break;
                }
            }
        }

        if (laneBlocked) score -= 5000.f;

        Player* nearestToTarget = findNearestOpponent(matePos, opposition);
        if (nearestToTarget) {
            float distToTarget = dist(matePos, nearestToTarget->getPosition());
            if (distToTarget < 400.f) score -= 1000.f;
        }

        // Removed the upper bounds cap so they can target far wingers!
        if (d > 500.0f) {
            if (score > bestScore) {
                bestScore = score;
                bestTarget = mate;
            }
        }
    }

    // THE FIX: Find any valid outfield teammate if the loop failed
    if (!bestTarget) {
        for (Player* tm : teammates) {
            if (tm != &npc && tm->getPositionRole() != PositionRole::Goalkeeper) {
                bestTarget = tm;
                break;
            }
        }
    }
    if (!bestTarget) return;

    // THE FIX: If the highest scoring pass was highly negative, every lane is blocked. BOOT IT!
    bool forceClearance = (bestScore < -1000.f);

    // --- EXECUTION ---
    sf::Vector2f targetPos = bestTarget->getPosition();
    float rawDist = dist(npcPos, targetPos);

    bool goHigh = (rawDist > 2800.f) || forceClearance; // Lowered high-pass threshold, forced high if panicked
    sf::Vector2f directDir = normalize(targetPos - npcPos);
    bool useThrow = (!forceClearance && rawDist < 2500.f && npc.getGkThrowing() > npc.getLongPassing());
    float statToUse = useThrow ? npc.getGkThrowing() : npc.getLongPassing();

    float finalPower = npc.getKickPower(); // Provide maximum base power to avoid 0.0f tap passes
    AimAssist::applyPassAssist(npc, bestTarget, directDir, finalPower, goHigh, true);

    float vzPower = 0.f;
    float finalBackspin = 0.f;

    if (goHigh) {
        // If we are forced to clear, launch it to maximum height!
        float maxLoft = forceClearance ? 1100.f : std::clamp(500.f + (rawDist * 0.25f), 700.f, 1150.f);
        vzPower = std::max(maxLoft - ((statToUse / 100.f) * 80.f), 300.f);
        finalBackspin = 60.f + (statToUse * 0.5f);

        if (forceClearance) finalPower = npc.getKickPower() * 0.60f; // Ensure maximum distance
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

    ball.shoot(finalDir, finalPower, 0.0f, vzPower, finalBackspin);
    if (!useThrow) {
        float kickVol = std::clamp(0.0f + (finalPower / npc.getKickPower()) * 40.0f, 10.f, 100.f);
        soundManager.playRandomSound("kick", 3, kickVol, 0.15f);
    }

    npc.resetKickCooldown();
}

// ==========================================
// --- AERIAL PHYSICS & STRIKES ---
// ==========================================

void PlayerAI::handleNPCJumpLogic(NPCPlayer& npc, Ball& ball) {
    if (npc.z > 0.0f || npc.getState() == PlayerState::Tackling) return;

    float d = dist(npc.getPosition(), ball.getPosition());

    // Ignore balls that are too far, already on the ground, or flying upwards fast
    if (d > 400.f || ball.z < 100.f || ball.vz > 50.f) return;

    float awarenessNorm = npc.getAwareness() / 100.f;
    float jumpingNorm = npc.getJumpingStrength() / 100.0f;
    float jumpVz = 240.f + (jumpingNorm * 160.f);

    // ==========================================
    // --- 1. KINEMATIC TIMING PREDICTION ---
    // ==========================================
    // Time to reach the apex of our jump: t = v / a
    float timeToApex = jumpVz / 980.f;

    // Predict the ball's exact Z height when we reach our jump apex
    float futureBallZ = ball.z + (ball.vz * timeToApex) - (0.5f * 980.f * timeToApex * timeToApex);
    float idealInterceptZ = 180.f; // Average Head height

    // High awareness players have a much tighter, more accurate timing window!
    float errorMargin = 20.f + ((1.0f - awarenessNorm) * 40.f);

    // ==========================================
    // --- 2. LAUNCH COMMAND (With X/Y Lock) ---
    // ==========================================
    if (std::abs(futureBallZ - idealInterceptZ) < errorMargin) {

        // THE FIX: Predict the exact X/Y position of the ball at that future time
        sf::Vector2f futureBallPos = ball.getPosition() + (ball.getVelocity() * timeToApex);

        // How far is the drop zone?
        float distToDropZone = dist(npc.getPosition(), futureBallPos);

        // Calculate our maximum physical reach during the jump time
        float maxJumpReach = (npc.getTopSpeed() * 10.f) * timeToApex;

        // ONLY jump if we can actually reach the ball in the air!
        if (distToDropZone < maxJumpReach + 30.f) { // Added a 30px head-width buffer
            sf::Vector2f jumpDir = normalize(futureBallPos - npc.getPosition());

            // Conserve momentum, or speed up slightly to reach the exact coordinate
            float speed = std::sqrt(npc.getVelocity().x * npc.getVelocity().x + npc.getVelocity().y * npc.getVelocity().y);
            float jumpSpeed = std::max(speed, distToDropZone / timeToApex);

            npc.setVelocity(jumpDir * jumpSpeed);
            npc.vz = jumpVz;
            npc.setState(PlayerState::Jumping);
        }
    }
}

bool PlayerAI::tryNPCAerialStrike(NPCPlayer& npc, Ball& ball, sf::Vector2f aimDir, bool isShot, SoundManager& soundManager) {
    if (npc.getKickCooldown() > 0.0f) return false;
    if (ball.hasOwner()) return false;

    Player* lastOwner = ball.getLastOwner();
    if (lastOwner != nullptr) {
        if (lastOwner == &npc) return false;
        if (!isShot && lastOwner->getTeam() == npc.getTeam()) {
            float distFromPasser = dist(npc.getPosition(), lastOwner->getPosition());
            if (distFromPasser < 1500.f) return false;
        }
    }

    float d = dist(npc.getPosition(), ball.getPosition());
    if (d > 120.f) return false;

    float relativeHeight = ball.z - npc.z;
    bool isHeader = (relativeHeight >= 140.f && relativeHeight <= 220.f);
    bool isVolley = (relativeHeight >= 40.f && relativeHeight < 140.f);

    if (!isHeader && !isVolley) return false;

    // ==========================================
    // --- NEW: THE "BRING IT DOWN" LOGIC (Taking a Touch) ---
    // ==========================================
    // If the ball is roughly chest/hip height (z < 110) and we aren't trying to score...
    if (isVolley && relativeHeight < 110.f && !isShot) {
        float bcNorm = npc.getBallControl() / 100.f;

        // The higher the ball control, the more likely they choose to trap it instead of volleying
        if (((rand() % 100) / 100.f) < bcNorm) {

            float touchError = 1.0f - bcNorm;

            // ELITE TOUCH: Instant possession, ball drops perfectly dead to their feet!
            if (bcNorm > 0.85f && (rand() % 100 < 80)) {
                ball.possess(&npc);
            }
            // STANDARD/POOR TOUCH: Kills the aerial momentum, but bounces off their chest
            else {
                // A poor touch knocks the ball further away and bounces higher
                float knockSpeed = 50.f + (touchError * 350.f);
                float bounceZ = 20.f + (touchError * 150.f);

                // The bounce direction follows their momentum, but is wildly inaccurate for bad players
                sf::Vector2f npcVel = npc.getVelocity();
                float currentSpeed = std::sqrt(npcVel.x * npcVel.x + npcVel.y * npcVel.y);
                sf::Vector2f baseDir = (currentSpeed > 10.f) ? normalize(npcVel) : aimDir;

                float randError = ((rand() % 200) - 100) / 100.f * (touchError * 90.f); // Up to 90 degrees of shank
                float rad = randError * 3.14159f / 180.f;
                sf::Vector2f knockDir(
                    baseDir.x * std::cos(rad) - baseDir.y * std::sin(rad),
                    baseDir.x * std::sin(rad) + baseDir.y * std::cos(rad)
                );

                // Actively shoot the ball away to simulate the heavy touch
                ball.shoot(knockDir, knockSpeed, 0.f, bounceZ, 0.f);
            }

            npc.resetKickCooldown();
            return true; // We successfully interacted with the ball!
        }
    }

    float headingStat = npc.getHeading();
    float finishingStat = npc.getFinishing();
    float activeStat = isHeader ? headingStat : finishingStat;
    float skillNorm = activeStat / 100.f;

    sf::Vector2f bVel = ball.getVelocity();
    float incomingSpeed = std::sqrt(bVel.x * bVel.x + bVel.y * bVel.y + ball.vz * ball.vz);
    float difficultyMultiplier = isVolley ? 1.85f : 1.0f;
    float speedPenalty = (incomingSpeed / 1500.f) * difficultyMultiplier;
    float whiffChance = (speedPenalty - skillNorm) * 35.f;

    if (whiffChance > 0.f && (rand() % 100) < whiffChance) {
        npc.resetKickCooldown();
        return false;
    }

    float timingQuality = std::clamp(1.0f - (speedPenalty * (1.0f - skillNorm)), 0.15f, 1.0f);
    float maxCharge = 0.8f * timingQuality;
    float minCharge = 0.5f * timingQuality;
    float simulatedCharge = minCharge + ((rand() % 100) / 100.f) * (maxCharge - minCharge);

    float basePower = 0.f;
    float vzOut = 0.f;
    float finalBackspin = 0.f;

    if (isHeader) {
        basePower = (30.f + (activeStat * 0.5f)) * std::max(0.3f, simulatedCharge);
        if (isShot) vzOut = 100.f - (activeStat * 3.0f);
        else vzOut = 150.f + (activeStat * 1.5f);
        finalBackspin = 10.f;
    }
    else {
        basePower = npc.getKickPower() * simulatedCharge * 1.1f;
        float techniqueError = (1.0f - skillNorm);
        vzOut = 100.f + (techniqueError * 350.f) + ((1.0f - timingQuality) * 200.f);
        finalBackspin = 30.f;
    }

    float errorMagnitude = (1.0f - skillNorm);
    errorMagnitude += (1.0f - timingQuality) * 0.5f;
    float maxErrorAngle = isHeader ? 15.f : 12.f;
    float radOffset = (errorMagnitude * maxErrorAngle * (((rand() % 200) - 100) / 100.f)) * 3.14159f / 180.f;

    float cosA = std::cos(radOffset);
    float sinA = std::sin(radOffset);
    sf::Vector2f finalDir = sf::Vector2f(
        aimDir.x * cosA - aimDir.y * sinA,
        aimDir.x * sinA + aimDir.y * cosA
    );

    float kickVol = std::clamp(0.f + (basePower / npc.getKickPower()) * 20.0f, 10.f, 100.f);
    soundManager.playRandomSound("kick", 3, kickVol, 0.15f);

    ball.shoot(finalDir, basePower, 0.0f, vzOut, finalBackspin);
    npc.resetKickCooldown();
    return true;
}

// ==========================================
// --- DEAD BALLS ---
// ==========================================

void PlayerAI::executeThrowIn(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates) {
    Player* bestTarget = nullptr;
    float bestScore = -9999.f;

    for (Player* mate : teammates) {
        if (mate == &npc) continue;
        float d = dist(npc.getPosition(), mate->getPosition());
        if (d < 2500.f) {
            float score = 2500.f - d;
            if (score > bestScore) {
                bestScore = score;
                bestTarget = mate;
            }
        }
    }

    sf::Vector2f targetPos = bestTarget ? bestTarget->getPosition() : sf::Vector2f(5000.f, 3500.f);
    sf::Vector2f throwDir = normalize(targetPos - npc.getPosition());

    float throwPower = 35.0f;
    float vzPower = 450.0f;
    float backspin = 5.0f;

    ball.shoot(throwDir, throwPower, 0.0f, vzPower, backspin);
    npc.resetKickCooldown();
}




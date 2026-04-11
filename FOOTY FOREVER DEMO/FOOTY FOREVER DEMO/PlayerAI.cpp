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
    dynamicAnchor.x += teamAI.getDefensiveLineOffset();

    TacticalZone zone = teamAI.getEffectiveTacticalZone(npc.getPlaystyle());

    // ==========================================
    // 2. TACTICAL POSITIONING (Relative to Anchor)
    // ==========================================
    // Use the dynamicAnchor as the base for the tactical target
    sf::Vector2f target = applyTacticalPositioning(npc, dynamicAnchor, ballPos, goalPos, state, zone, pitch, team, opponents, teamAI);

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

sf::Vector2f PlayerAI::evaluateShapeAndSpace(NPCPlayer& npc, sf::Vector2f currentTarget, sf::Vector2f ballPos, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, const TeamAI& teamAI, const TacticalZone& zone)
{
    sf::Vector2f spatialCorrection(0.f, 0.f);
    sf::Vector2f npcPos = npc.getPosition();
    float awareness = npc.getAwareness() / 100.0f;

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

                    float longBallPref = 1.0f - teamAI.getPassingLengthPref();
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
        // 2. ESCAPING COVER SHADOWS (DNA: Roaming Freedom)
        // ---------------------------------------------------------
        sf::Vector2f toBall = ballPos - npcPos;
        float distToBall = std::sqrt(toBall.x * toBall.x + toBall.y * toBall.y);
        if (distToBall > 10.f) {
            sf::Vector2f dirToBall = toBall / distToBall;

            for (Player* opp : opponents) {
                sf::Vector2f toOpp = opp->getPosition() - npcPos;
                float oppDist = std::sqrt(toOpp.x * toOpp.x + toOpp.y * toOpp.y);

                if (oppDist < distToBall) {
                    float dot = (toOpp.x * dirToBall.x) + (toOpp.y * dirToBall.y);
                    if (dot > 0.f) {
                        sf::Vector2f projection = dirToBall * dot;
                        sf::Vector2f rejection = toOpp - projection;
                        float distFromLane = std::sqrt(rejection.x * rejection.x + rejection.y * rejection.y);

                        if (distFromLane < 150.f) {
                            sf::Vector2f slideDir(-dirToBall.y, dirToBall.x);
                            float side = ((slideDir.x * rejection.x + slideDir.y * rejection.y) > 0) ? -1.f : 1.f;

                            // INJECTION: Target Men (Low Freedom) won't step out of shadows, they wait for a lob. 
                            // Playmakers (High Freedom) will slide laterally to get to feet.
                            float evadeDesire = 200.f + (zone.roamingFreedom * 300.f);
                            spatialCorrection += slideDir * side * evadeDesire * awareness;
                        }
                    }
                }
            }
        }
        // ---------------------------------------------------------
        // 3. TIKI-TAKA SUPPORT (Show to Feet)
        // ---------------------------------------------------------
        float tikiTakaPref = teamAI.getPassingLengthPref(); // 1.0 = Tiki-Taka, 0.0 = Long Ball
        float distToBallExact = dist(npcPos, ballPos);

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
    }
    else {
        // ---------------------------------------------------------
        // 3. DEFENSIVE CHAINING (Lateral Gaps)
        // ---------------------------------------------------------
        Player* adjacentPartner = nullptr;
        float minLateralGap = 9999.f;

        for (Player* tm : team) {
            if (tm == &npc || tm->getPositionRole() == PositionRole::Goalkeeper) continue;

            if (std::abs(tm->getPosition().x - npcPos.x) < 600.f) {
                float dy = std::abs(tm->getPosition().y - npcPos.y);
                if (dy < minLateralGap) {
                    minLateralGap = dy;
                    adjacentPartner = tm;
                }
            }
        }

        if (adjacentPartner) {
            float maxSafeGap = 1000.f - (teamAI.getDefensiveDepthPref() * 400.f);

            if (minLateralGap > maxSafeGap) {
                sf::Vector2f pinchDir = adjacentPartner->getPosition() - npcPos;
                pinchDir.x = 0.f;

                float discipline = 1.0f - (zone.roamingFreedom * 0.5f);
                float pinchMagnitude = (minLateralGap - maxSafeGap) * 0.6f * discipline;

                if (pinchDir.y > 0) spatialCorrection.y += pinchMagnitude * awareness;
                else spatialCorrection.y -= pinchMagnitude * awareness;
            }
        }

        // ==========================================
        // 4. VERTICAL LINE SYNC (Holding the Offside Trap)
        // ==========================================
        bool isDefender = (npc.getPositionRole() == PositionRole::CenterBack ||
            npc.getPositionRole() == PositionRole::LeftBack ||
            npc.getPositionRole() == PositionRole::RightBack ||
            npc.getPositionRole() == PositionRole::LeftWingBack ||
            npc.getPositionRole() == PositionRole::RightWingBack);

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
                if (tmIsDef) {
                    avgLineX += tm->getPosition().x;
                    defCount++;
                }
            }

            if (defCount > 0) {
                avgLineX /= defCount;
                float xDiff = avgLineX - npcPos.x;

                // DNA INJECTION: A rigid CB (0.1 freedom) holds the line perfectly.
                // A fluid attacking FB (0.8 freedom) might lag slightly behind or push up unevenly.
                float discipline = 1.0f - (zone.roamingFreedom * 0.6f);

                // Only forcefully sync if the line is noticeably breaking (> 60px gap)
                if (std::abs(xDiff) > 60.f) {
                    // If a fast player drops too quickly, xDiff pulls their target BACK towards the slow players.
                    // This creates a "rubber-band" effect where fast players wait for the line to catch up!
                    spatialCorrection.x += xDiff * 0.85f * discipline * awareness;
                }
            }
        }
    }

    return spatialCorrection;
}

sf::Vector2f PlayerAI::applyTacticalPositioning(NPCPlayer& npc, sf::Vector2f homePos, sf::Vector2f ballPos, sf::Vector2f goalPos, TeamState state, TacticalZone zone, const Pitch& pitch, const std::vector<Player*>& team, const std::vector<Player*>& opposition, const TeamAI& teamAI)
{
    float ballProgress = teamAI.getBallProgress();
    float offsideLineX = teamAI.getOffsideLineX();
    bool isHomeSide = teamAI.isHome();

    bool isForward = (npc.getPositionRole() == PositionRole::Striker || npc.getPositionRole() == PositionRole::CenterForward || npc.getPositionRole() == PositionRole::LeftWing || npc.getPositionRole() == PositionRole::RightWing);
    bool isMid = (npc.getPositionRole() == PositionRole::CenterMid || npc.getPositionRole() == PositionRole::AttackingMid || npc.getPositionRole() == PositionRole::LeftMid || npc.getPositionRole() == PositionRole::RightMid ||
        npc.getPositionRole() == PositionRole::DefensiveMid);
    bool isDefender = (!isForward && !isMid);

    sf::Vector2f tacticalTarget = homePos;
    float pitchCenterY = pitch.totalHeight / 2.0f;

    // --- FETCH MANAGER'S SLIDERS ---
    float passLengthPref = teamAI.getPassingLengthPref();
    float longBallPref = 1.0f - passLengthPref;
    float counterSpeed = teamAI.getPassingSpeedPref();
    float depthPref = teamAI.getDefensiveDepthPref();
    float freedomPref = teamAI.getPositionalFreedomPref();

    // --- FETCH PLAYER DNA ---
    PlayerBehavior behavior = npc.getPlaystyle().behavior;

    if (state == TeamState::Attacking) {
        float depthPush = isHomeSide ? 1.0f : -1.0f;

        // 1. PROACTIVE BALL SENSITIVITY
        float targetBallX = ballPos.x;
        if ((isForward || npc.getPositionRole() == PositionRole::AttackingMid) && zone.supportDepth > -0.3f) {
            float leadDist = 400.f + (longBallPref * 1000.f) + (zone.supportDepth * 600.f);
            targetBallX += depthPush * leadDist;
        }
        else if (isMid && zone.supportDepth > -0.3f) {
            float leadDist = 100.f + (longBallPref * 500.f) + (zone.supportDepth * 300.f);
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
        float idealSupportDist = 600.f + (longBallPref * 800.f);
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
            if (isHomeSide && tacticalTarget.x > offsideLineX + awarenessError) {
                tacticalTarget.x = offsideLineX + awarenessError - 50.f;
            }
            else if (!isHomeSide && tacticalTarget.x < offsideLineX - awarenessError) {
                tacticalTarget.x = offsideLineX - awarenessError + 50.f;
            }
        }
    }
    else {
        // ==========================================
        // --- 1. SLIDER-DRIVEN DEFENDING (Quicker) ---
        // ==========================================
        float dropIntensity = 1.0f - ballProgress;
        float dropMultiplier = 0.0f;

        if (isDefender) {
            // BUFFED: Defenders now actively sprint back off the ball
            // High Line (0.0) = 0.30 drop. Low Block (1.0) = 0.60 drop.
            dropMultiplier = 0.30f + (depthPref * 0.30f);
        }
        else if (isMid) {
            dropMultiplier = 0.15f - (counterSpeed * 0.1f);
        }
        else {
            dropMultiplier = std::max(0.0f, 0.05f - (counterSpeed * 0.05f));
        }

        float dropDist = (zone.backwardLeash * dropMultiplier) * dropIntensity;
        sf::Vector2f dropDir = isHomeSide ? sf::Vector2f(-1.f, 0.f) : sf::Vector2f(1.f, 0.f);
        tacticalTarget += (dropDir * dropDist);

        // ==========================================
        // --- 2. PROACTIVE HIGH LINE (Earlier) ---
        // ==========================================
        float threatX = teamAI.getHighestAttackerX();

        if (isDefender) {
            // BUFFED BUFFER: This makes them drop EARLIER. 
            // By increasing the buffer from 400->600 base, they won't wait until the 
            // striker is parallel to them. They will cushion the run 600-1400px in advance!
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

        // 3. DEFENSIVE COMPACTNESS
        float yOffset = tacticalTarget.y - pitchCenterY;
        float squeezeFactor = 0.0f;
        if (isDefender) squeezeFactor = (0.3f + (depthPref * 0.3f)) * dropIntensity;
        else if (isMid) squeezeFactor = (0.1f + (depthPref * 0.2f)) * dropIntensity;
        tacticalTarget.y -= yOffset * squeezeFactor;

        // 4. THE SWARM
        float maxLateralShift = 600.f + (depthPref * 400.f);
        float desiredYShift = (ballPos.y - tacticalTarget.y) * (zone.ballInfluence * 0.7f);
        desiredYShift = std::clamp(desiredYShift, -maxLateralShift, maxLateralShift);
        tacticalTarget.y += desiredYShift;

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
            // FIX 3: Keep the strikers up the pitch!
            else if (isForward) {
                float forwardLine = isHomeSide ? penaltyBoxEdgeX + 2500.f : penaltyBoxEdgeX - 2500.f;
                if (isHomeSide && tacticalTarget.x < forwardLine) tacticalTarget.x = forwardLine;
                else if (!isHomeSide && tacticalTarget.x > forwardLine) tacticalTarget.x = forwardLine;
            }
        }
    }

    // ==========================================
    // --- FINAL LAYER: SPATIAL INTELLIGENCE ---
    // ==========================================
    // INJECTION: Pass the 'zone' explicitly into the engine!
    sf::Vector2f spatialAdjustments = evaluateShapeAndSpace(npc, tacticalTarget, ballPos, state, team, opposition, teamAI, zone);

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
    float stamRatio = npc.getCurrentStamina() / npc.getMaxStamina();
    if (npc.getCurrentStamina() < 2.0f) return false;

    PositionRole role = npc.getPositionRole();
    bool isAttacker = (role == PositionRole::Striker || role == PositionRole::CenterForward || role == PositionRole::LeftWing || role == PositionRole::RightWing);
    bool isDefender = (role == PositionRole::CenterBack || role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);

    switch (urgency) {
    case AIUrgency::Critical:
        return true; // First responder to the ball ALWAYS sprints

    case AIUrgency::AttackingRun:
        // Defenders shouldn't waste energy sprinting forward unless they are massively out of position
        if (isDefender) return (stamRatio > 0.6f && distToTarget > 800.f);
        // Attackers and Midfielders eagerly sprint into the box
        return (stamRatio > 0.4f && distToTarget > 500.f);

    case AIUrgency::Pressing:
        if (isAttacker) return (stamRatio > 0.6f && distToBall < 500.f);
        else return (stamRatio > 0.35f && distToBall < 1000.f);

    case AIUrgency::Recovery:
    default:
        // ==========================================
        // --- BUFFED DEFENSIVE RECOVERY ---
        // ==========================================
        if (isDefender) {
            // Defenders will sprint back even if they are dead tired (20% stamina) 
            // and only a short distance away (300px). They prioritize the defensive line above all else!
            return (stamRatio > 0.2f && distToTarget > 300.f);
        }
        // Midfielders and Attackers drop back a bit less desperately to save energy for the counter
        return (stamRatio > 0.4f && distToTarget > 600.f);
    }
}

sf::Vector2f PlayerAI::calculateInterceptionPoint(NPCPlayer& npc, Ball& ball) {
    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f ballVel = ball.getVelocity();
    float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);

    if (ballSpeed < 50.f && ball.z <= 40.f) return ballPos;
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

    sf::Vector2f interceptPoint = ballPos;

    if (ball.z > 40.f) {
        // Exact landing spot physics calculation
        float misjudgment = 1.0f + (errorSeverity * 0.35f * ((ballVel.x > 0) ? 1.f : -1.f));
        float perceivedGravity = 980.f * misjudgment;

        float discriminant = (ball.vz * ball.vz) + (2.f * perceivedGravity * ball.z);
        if (discriminant > 0.f) {
            float t = (ball.vz + std::sqrt(discriminant)) / perceivedGravity;
            interceptPoint = ballPos + (ballVel * t);
        }
    }
    else {
        sf::Vector2f toNPC = npc.getPosition() - ballPos;
        float projection = (toNPC.x * ballDir.x + toNPC.y * ballDir.y);

        if (isTeammatePass) {
            // If it's a pass to us, run directly TO the ball to meet it, don't wait for it to cross us!
            float interceptDist = std::max(20.f, projection - 150.f);
            interceptPoint = ballPos + ballDir * interceptDist;
        }
        else {
            // General loose ball interception
            float distToLine = std::abs(toNPC.x * ballDir.y - toNPC.y * ballDir.x);
            float timeToLine = distToLine / ((npc.getTopSpeed() * 10.f) + 1.f);

            float interceptDist = std::max(20.f, projection + (ballSpeed * timeToLine * 0.7f) + (errorSeverity * ballSpeed * 0.4f));
            interceptPoint = ballPos + ballDir * interceptDist;

            float side = (toNPC.x * ballDir.y - toNPC.y * ballDir.x > 0) ? 1.f : -1.f;
            interceptPoint += sf::Vector2f(-ballDir.y, ballDir.x) * side * (errorSeverity * 180.f);
        }
    }

    interceptPoint.x = std::clamp(interceptPoint.x, 0.f, 10000.f);
    interceptPoint.y = std::clamp(interceptPoint.y, 0.f, 7000.f);
    return interceptPoint;
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

    if (isAttacker) basePressRadius *= 0.7f;
    if (isDefender) {
        basePressRadius *= 0.6f;
        basePressRadius *= (1.0f - (depthPref * 0.8f));
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

    bool isHomeTeam = (npc.getTeam() == Team::Home);
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

sf::Vector2f PlayerAI::handlePossession(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates, const std::vector<Player*>& opposition, UserPlayer& user, const Pitch& pitch, float dt, MatchState matchstate, const TeamAI& teamAI)
{
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);
    sf::Vector2f goalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);

    float bc = npc.getBallControl() / 100.f;
    float awareness = npc.getAwareness() / 100.f;
    float passingSkill = (npc.getShortPassing() + npc.getLongPassing()) / 200.f;
    float dribbleSkill = (npc.getBallControl() * 0.6f + npc.getAgility() * 0.4f) / 100.f;

    sf::Vector2f facingVec = getFacingVec(npc.getDirection()); // Convert enum to vector

    Player* nearestOpp = findNearestOpponent(npcPos, opposition);
    float closestOppDist = nearestOpp ? dist(npcPos, nearestOpp->getPosition()) : 9999.f;
    bool isUnderPressure = (closestOppDist < 600.f);
    bool isCrammed = (closestOppDist < (250.f - (bc * 100.f)));

    PlayerBehavior behavior = npc.getPlaystyle().behavior;

    // --- 1. SHOOTING ---
    float distToGoal = dist(npcPos, goalPos);
    float shotRange = 1000.f + ((npc.getFinishing() / 100.f) * 1000.f) + (behavior.shootBias * 1500.f);

    if (distToGoal < shotRange && npc.getKickCooldown() <= 0.0f && matchstate == MatchState::InPlay) {
        if (!isCrammed || distToGoal < (1000.f + (behavior.shootBias * 800.f))) {
            // ALIGNMENT CHECK: Ensure facing goal roughly before shooting
            sf::Vector2f toGoal = normalize(goalPos - npcPos);
            float alignment = dot(getFacingVec(npc.getDirection()), toGoal);

            if (alignment > 0.7f) { // Facing within ~45 degrees
                executeShot(npc, ball, goalPos, opposition, pitch, dt);
                return { 0.f, 0.f };
            }
        }
    }

    // --- 2. PASSING (DNA: Dribble Bias / Ball Hogging) ---
    npc.m_passTimer += dt;
    float managerPassDelay = 0.6f - (teamAI.getPassingSpeedPref() * 0.5f);
    float playerHogDelay = (behavior.dribbleBias - 0.5f) * 1.5f;
    float actualPassDelay = std::max(0.05f, managerPassDelay + playerHogDelay);

    if (isUnderPressure) actualPassDelay *= 0.3f;

    if (npc.getKickCooldown() <= 0.0f && npc.m_passTimer > actualPassDelay) {
        npc.m_passTimer = 0.f;
        Player* bestTarget = findBestPassOption(npc, teammates, opposition, user, teamAI, pitch);

        if (bestTarget) {
            sf::Vector2f toTarget = normalize(bestTarget->getPosition() - npcPos);
            float alignment = (facingVec.x * toTarget.x + facingVec.y * toTarget.y);

            // ==========================================
            // --- ORIENTATION-LOCKED PASSING ---
            // ==========================================
            // If the player isn't facing the target, return a "turn direction" 
            // instead of executing the pass immediately.
            if (alignment < 0.5f) { // Target is more than 60 degrees off
                return toTarget * 0.5f; // Turn towards target, slowing down slightly
            }

            Player* tOpp = findNearestOpponent(bestTarget->getPosition(), opposition);
            float targetOppDist = tOpp ? dist(bestTarget->getPosition(), tOpp->getPosition()) : 9999.f;

            if (isCrammed || (isUnderPressure && targetOppDist > closestOppDist + 200.f) ||
                (isUnderPressure && passingSkill > dribbleSkill * 1.2f) ||
                teamAI.getPassingSpeedPref() > 0.7f || behavior.dribbleBias < 0.3f)
            {
                executePass(npc, ball, bestTarget, opposition);
                return { 0.f, 0.f };
            }
        }
    }

    // --- 3. DRIBBLE ---
    if (matchstate == MatchState::InPlay) {
        sf::Vector2f dribbleDir = calculateDribbleDirection(npc, goalPos, opposition, pitch, teamAI);
        return dribbleDir;
    }
    return { 0.f, 0.f };
}

Player* PlayerAI::findBestPassOption(NPCPlayer& npc, const std::vector<Player*>& team, const std::vector<Player*>& opposition, UserPlayer& user, const TeamAI& teamAI, const Pitch& pitch) {
    Player* bestOption = nullptr;
    float bestScore = -1e9f;
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);

    float shortPassNorm = npc.getShortPassing() / 100.f;
    float longPassNorm = npc.getLongPassing() / 100.f;
    float curlNorm = npc.getCurl() / 100.f;
    float visionNorm = npc.getAwareness() / 100.f;

    float tikiTakaPref = teamAI.getPassingLengthPref();
    float routeOnePref = 1.0f - tikiTakaPref;
    float counterSpeed = teamAI.getPassingSpeedPref();

    // ==========================================
    // --- ON-THE-FLY OFFSIDE LINE SCANNER ---
    // ==========================================
    float deepestX = isHome ? 0.f : pitch.totalWidth;
    float secondDeepestX = deepestX;

    // Scan the opposition to find the second-last defender (usually the last CB + Goalkeeper)
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
    // The official offside line: The second deepest defender, bounded by the ball position and halfway line.
    float offsideLineX = isHome ? std::max(halfwayX, std::max(secondDeepestX, npcPos.x))
        : std::min(halfwayX, std::min(secondDeepestX, npcPos.x));

    // --- DNA INJECTION ---
    PlayerBehavior behavior = npc.getPlaystyle().behavior;
    float riskMultiplier = (behavior.passRiskBias - 0.5f) * 2.0f; // -1.0 (Safe) to +1.0 (Hollywood)

    bool inOwnDeepBox = isHome ? (npcPos.x < 2500.f) : (npcPos.x > 7500.f);
    Player* closestPresser = findNearestOpponent(npcPos, opposition);
    float presserDist = closestPresser ? dist(npcPos, closestPresser->getPosition()) : 9999.f;
    bool isCrammed = presserDist < 350.f;

    std::vector<Player*> receivers;
    for (auto& t : team) if (t != &npc) receivers.push_back(t);
    if (npc.getTeam() == user.getTeam()) receivers.push_back(&user);

    for (Player* target : receivers) {
        float distToTarget = dist(npcPos, target->getPosition());
        float arrivalSpeed = 500.f - (std::clamp(distToTarget / 4000.f, 0.f, 1.f) * 300.f);
        float requiredV0 = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * 800.f * distToTarget));
        if (requiredV0 / 52.0f > npc.getKickPower()) continue;

        sf::Vector2f predictedPos = target->getPosition() + (target->getVelocity() * (distToTarget / requiredV0));
        float predictedDist = dist(npcPos, predictedPos);
        sf::Vector2f passDir = normalize(predictedPos - npcPos);

        float forwardProgress = isHome ? (predictedPos.x - npcPos.x) : (npcPos.x - predictedPos.x);

        float score = 600.f;

        // ==========================================
        // --- THE OFFSIDE TRAP PENALTY ---
        // ==========================================
        // Offside is judged by where the player is NOW, not where they will be when the ball arrives!
        bool isOffside = isHome ? (target->getPosition().x > offsideLineX) : (target->getPosition().x < offsideLineX);

        if (isOffside) {
            // High Vision players (visionNorm ~ 1.0) will subtract 10,000 points and NEVER make this pass.
            // Low Vision players (visionNorm ~ 0.4) will only subtract 4,000 points, meaning a high 
            // "Hollywood Pass" risk multiplier might still tempt them to try it!
            score -= 10000.f * visionNorm;
        }

        // ==========================================
        // --- FOOT PREFERENCE SCORING ---
        // ==========================================
        bool isRightFooted = (npc.getPreferredFoot() == "Right");
        sf::Vector2f facing = getFacingVec(npc.getDirection());
        float cross = (facing.x * passDir.y - facing.y * passDir.x);
        bool favorsRight = (cross > 0);

        bool requiresWeakFoot = (isRightFooted && !favorsRight) || (!isRightFooted && favorsRight);

        if (requiresWeakFoot) {
            float wfPenalty = (5.0f - npc.getWeakFootAccuracy()) * 150.f;
            score -= wfPenalty;
        }

        float bodyAlignment = PlayerAI::dot(facing, passDir);
        if (bodyAlignment < -0.2f) score -= 2000.f;

        if (riskMultiplier > 0.0f) {
            score += forwardProgress * (0.3f + (counterSpeed * 0.4f) + (riskMultiplier * 1.5f));
        }
        else {
            score += std::abs(forwardProgress) * (riskMultiplier * 0.5f);
            score += (2000.f - distToTarget) * std::abs(riskMultiplier * 0.8f);
        }

        // TACTICAL OVERRIDES
        if (distToTarget < 1200.f) {
            score += (tikiTakaPref * 1500.f) * shortPassNorm;
        }
        else if (distToTarget > 2500.f) {
            score += (routeOnePref * 2500.f) * longPassNorm * visionNorm * (1.0f + std::max(0.0f, riskMultiplier));
        }

        // --- CROSSING DNA ---
        bool inFinalThird = isHome ? (npcPos.x > 7000.f) : (npcPos.x < 3000.f);
        bool isWide = (npcPos.y < 1500.f || npcPos.y > pitch.totalHeight - 1500.f);
        bool targetInBox = isHome ? (predictedPos.x > 8350.f) : (predictedPos.x < 1650.f);

        if (inFinalThird && isWide && targetInBox) {
            score += behavior.crossBias * 3000.f;
        }

        // SCAN FOR BLOCKERS
        bool directLaneBlocked = false, canCurlAround = false;
        for (auto* opp : opposition) {
            float dOpp = dist(npcPos, opp->getPosition());
            if (dOpp < predictedDist && dOpp > 100.f) {
                sf::Vector2f toOpp = opp->getPosition() - npcPos;
                float alignment = (passDir.x * (toOpp.x / dOpp) + passDir.y * (toOpp.y / dOpp));
                if (alignment > 0.92f) {
                    directLaneBlocked = true;
                    if (alignment < 0.97f && curlNorm > 0.6f) canCurlAround = true;
                }
            }
        }

        bool wantHigh = (distToTarget > 2200.f || (directLaneBlocked && !canCurlAround));
        if (wantHigh && distToTarget < 1500.f) score -= 5000.f;

        // Pressure handling
        float pressureFactor = std::clamp((600.f - presserDist) / 400.f, 0.f, 1.f);
        if (pressureFactor > 0.1f) {
            if (wantHigh && distToTarget < 2000.f) score -= 4000.f * pressureFactor;
            if (distToTarget < 1500.f) score += (3500.f * pressureFactor) * (directLaneBlocked ? 0.5f : 1.0f);
        }

        if (inOwnDeepBox && isCrammed) {
            if (target->getPositionRole() == PositionRole::Goalkeeper && !directLaneBlocked) score += 6000.f;
            else if (forwardProgress > 1500.f) score += 4000.f;
        }

        Player* marker = findNearestOpponent(predictedPos, opposition);
        float distToMarker = marker ? dist(predictedPos, marker->getPosition()) : 9999.f;

        if (wantHigh) {
            score -= (1.0f - longPassNorm) * 700.f;
            if (distToMarker < 400.f) score -= 1000.f;
        }
        else if (directLaneBlocked && canCurlAround) {
            score -= (1.0f - curlNorm) * 1000.f;
        }
        else if (!directLaneBlocked) {
            score += 400.f * shortPassNorm;
        }

        if (!wantHigh && distToMarker < 250.f) score -= 1000.f;

        if (score > bestScore) {
            bestScore = score;
            bestOption = target;
        }
    }
    return (bestScore > (isCrammed ? -2000.f : -400.f)) ? bestOption : nullptr;
}

sf::Vector2f PlayerAI::calculateDribbleDirection(NPCPlayer& npc, sf::Vector2f goalPos, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI) {
    sf::Vector2f npcPos = npc.getPosition();
    sf::Vector2f baseDir = normalize(goalPos - npcPos);
    sf::Vector2f bestDir = baseDir;
    float bestScore = -1e9f;

    float bcNorm = npc.getBallControl() / 100.f;
    float agilityNorm = npc.getAgility() / 100.f;
    float speedNorm = npc.getTopSpeed() / 10.f;

    float counterSpeed = teamAI.getPassingSpeedPref();

    // --- DNA INJECTION ---
    PlayerBehavior behavior = npc.getPlaystyle().behavior;

    bool isSpeedster = (speedNorm > 0.85f && speedNorm > bcNorm);
    bool isTrickster = (bcNorm > 0.8f && agilityNorm > 0.8f);

    Player* closestOpp = findNearestOpponent(npcPos, opposition);
    float overallMinOppDist = closestOpp ? dist(npcPos, closestOpp->getPosition()) : 9999.f;

    for (int i = 0; i < 16; ++i) {
        float angleDeg = -135.f + (i * (270.f / 15.f));
        float rad = angleDeg * 3.14159f / 180.f;
        sf::Vector2f testDir(baseDir.x * std::cos(rad) - baseDir.y * std::sin(rad), baseDir.x * std::sin(rad) + baseDir.y * std::cos(rad));

        // Base score: Forward momentum combined with run frequency DNA
        float score = (200.f + (counterSpeed * 300.f) + (behavior.runFrequency * 200.f)) * (testDir.x * baseDir.x + testDir.y * baseDir.y);

        sf::Vector2f npcPos = npc.getPosition();
        sf::Vector2f baseDir = normalize(goalPos - npcPos);

        // ==========================================
        // --- DYNAMIC SHIELDING (FOOT SWITCHING) ---
        // ==========================================
        Player* nearestOpp = findNearestOpponent(npcPos, opposition);
        if (nearestOpp) {
            sf::Vector2f toOpp = nearestOpp->getPosition() - npcPos;
            sf::Vector2f currentMoveDir = npc.getVelocity();
            if (length(currentMoveDir) < 5.f) currentMoveDir = baseDir;

            // Use the cross product to see if the defender is to our left or right
            float side = (currentMoveDir.x * toOpp.y - currentMoveDir.y * toOpp.x);

            // If opponent is on our RIGHT (side > 0), ball should be on our LEFT
            // If opponent is on our LEFT (side < 0), ball should be on our RIGHT
            if (side > 0 && npc.usingRightFoot()) {
                npc.changeFoot(); // Switch to Left foot
            }
            else if (side < 0 && !npc.usingRightFoot()){
                npc.changeFoot();  // Switch to Right foot
            }

            // SHIELDING BIAS: If we are a high Ball Control player, 
            // we can actually lean into the shield.
            float bc = npc.getBallControl() / 100.f;
            if (dist(npcPos, nearestOpp->getPosition()) < 300.f) {
                // Nudge our dribble direction slightly away from the opponent
                sf::Vector2f pushAway = normalize(npcPos - nearestOpp->getPosition());
                baseDir = normalize(baseDir + (pushAway * 0.4f * bc));
            }
        }

        // --- SHIELDING vs ISOLATION ---
        if (overallMinOppDist < 400.f) {
            float backwardAlignment = (testDir.x * -baseDir.x + testDir.y * -baseDir.y);

            // If they have LOW dribble bias, they immediately turn their back to the defender to shield the ball.
            // If they have HIGH dribble bias, they try to isolate the defender and take them on!
            if (backwardAlignment > 0.5f) {
                score += (1.0f - behavior.dribbleBias) * 800.f * bcNorm;
            }
        }

        if (isTrickster && overallMinOppDist < 300.f) {
            float turnAngle = std::abs(angleDeg);
            if (turnAngle > 70.f && turnAngle < 110.f) score += 500.f * bcNorm * agilityNorm * behavior.dribbleBias;
            score -= (testDir.x * npc.getDribbleTargetDir().x + testDir.y * npc.getDribbleTargetDir().y) * 200.f;
        }
        else if (isSpeedster && overallMinOppDist < 400.f && closestOpp) {
            float oppAlignment = (testDir.x * normalize(closestOpp->getPosition() - npcPos).x + testDir.y * normalize(closestOpp->getPosition() - npcPos).y);
            if (oppAlignment < 0.5f && oppAlignment > -0.2f) score += 600.f * speedNorm * behavior.dribbleBias;
        }
        else {
            sf::Vector2f currentDir = (std::sqrt(npc.getVelocity().x * npc.getVelocity().x + npc.getVelocity().y * npc.getVelocity().y) > 10.f) ? normalize(npc.getVelocity()) : baseDir;
            score -= (1.0f - (testDir.x * currentDir.x + testDir.y * currentDir.y)) * 300.f * (1.0f - agilityNorm);
            if ((testDir.x * npc.getDribbleTargetDir().x + testDir.y * npc.getDribbleTargetDir().y) > 0.90f) score += 200.f;
        }

        // Defender Proximity Penalty
        for (auto* opp : opposition) {
            float d = dist(npcPos, opp->getPosition());
            if (d < 500.f) {
                float oppAlignment = (testDir.x * normalize(opp->getPosition() - npcPos).x + testDir.y * normalize(opp->getPosition() - npcPos).y);
                // Tricksters are slightly less afraid of tight spaces
                float fearFactor = 800.f - (behavior.dribbleBias * 200.f);
                if (oppAlignment > 0.3f) score -= (1.0f - (d / 500.f)) * fearFactor * oppAlignment;
            }
        }

        // Pitch Boundary Safety
        sf::Vector2f projectedPos = npcPos + testDir * 300.f;
        if (projectedPos.x < pitch.margin || projectedPos.x > pitch.totalWidth - pitch.margin || projectedPos.y < pitch.margin || projectedPos.y > pitch.totalHeight - pitch.margin) {
            score -= 5000.f;
        }

        if (score > bestScore) {
            bestScore = score;
            bestDir = testDir;
        }
    }
    npc.setDribbleTargetDir(bestDir);
    return bestDir;
}

void PlayerAI::executePass(NPCPlayer& npc, Ball& ball, Player* target, const std::vector<Player*>& opposition) {
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
                // If blocked, attempt curl for mid-range, otherwise lob it
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

    // 2. GET PERFECT AIM & POWER: Use the universal AimAssist engine
    sf::Vector2f aimDir = directDir;
    float finalPower = 0.f;
    AimAssist::applyPassAssist(npc, target, aimDir, finalPower, goHigh, true);

    // ==========================================
    // --- WEAK FOOT LOGIC (AI) ---
    // ==========================================
    bool usingRight = npc.usingRightFoot();
    bool isRightFooted = (npc.getPreferredFoot() == "Right");
    bool isWeakFoot = (isRightFooted && !usingRight) || (!isRightFooted && usingRight);

    float errorAngle = (1.0f - (npc.getShortPassing() / 100.0f)) * 5.0f;

    if (isWeakFoot) {
        float pMod, eMod;
        float shank = getWeakFootPenalty(npc.getWeakFootAccuracy(), pMod, eMod);
        finalPower *= pMod;
        errorAngle = (errorAngle * eMod) + shank;

        // Apply error to aimDir for AI (User already has this in executeKickRelease)
        float randError = ((rand() % 100) / 100.f - 0.5f) * errorAngle;
        float rad = randError * 3.14159f / 180.f;
        aimDir = sf::Vector2f(aimDir.x * std::cos(rad) - aimDir.y * std::sin(rad), aimDir.x * std::sin(rad) + aimDir.y * std::cos(rad));
    }

    // 3. TRAJECTORY (VZ): Calculate loft and backspin for high passes
    float vzPower = 5.f;
    float finalBackspin = 0.f;
    if (goHigh) {
        float maxLoft = std::clamp(500.f + (distToTarget * 0.25f), 700.f, 1150.f);
        vzPower = std::max(maxLoft - ((npc.getLongPassing() / 100.f) * 80.f), 300.f);
        finalBackspin = 60.f + (npc.getLongPassing() * 0.5f);
    }

    // 4. CURL OFFSET: Apply the Magnus effect if we decided to curve it
    float finalSpin = 0.f;
    if (needsCurl && !goHigh) {
        float kickStrength = std::clamp(finalPower / npc.getKickPower(), 0.0f, 1.0f);
        float multiplier = (1.1f + kickStrength / 2.f);
        bool isLeftFoot = !npc.usingRightFoot();

        finalSpin = (curlSide < 0) ? (isLeftFoot ? (npc.getCurl() * multiplier) : (-(npc.getCurl() / 2.f) * multiplier))
            : (isLeftFoot ? ((npc.getCurl() / 2.f) * multiplier) : (-npc.getCurl() * multiplier));

        float offsetRad = -((finalSpin / 100.f) * 14.f) * (3.14159f / 180.f);
        aimDir = sf::Vector2f(
            aimDir.x * std::cos(offsetRad) - aimDir.y * std::sin(offsetRad),
            aimDir.x * std::sin(offsetRad) + aimDir.y * std::cos(offsetRad)
        );
    }

    ball.shoot(aimDir, finalPower, finalSpin, vzPower, finalBackspin);
    npc.resetKickCooldown();
}

void PlayerAI::executeShot(NPCPlayer& npc, Ball& ball, sf::Vector2f goalPos, const std::vector<Player*>& opposition, const Pitch& pitch, float dt) {
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

    // 2. INITIAL AIM: Pick a general spot on the goal line
    float topPostY = 3500.f - 366.f;
    float rawTargetY = topPostY + (static_cast<float>(rand()) / RAND_MAX * 732.f);
    sf::Vector2f aimDir = normalize(sf::Vector2f(goalPos.x, rawTargetY) - npcPos);

    // 3. CORNER SNAPPING: Use AimAssist to pull the shot into the corners
    float simulatedCharge = 0.6f + ((rand() % 41) / 100.f);
    float finalPower = npc.getKickPower() * simulatedCharge;
    float vzPower = 0.f;
    AimAssist::applyShotAssist(npc, aimDir, vzPower, finalPower, pitch);

    float finalBackspin = 0.f;
    float finalSpin = 0.f;

    // 4. OVERRIDE TRAJECTORY: Handle Chip vs High vs Low Driven
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
        bool isHighShot = (rand() % 100 > 50);
        if (isHighShot) {
            finalPower *= (1.1f - (simulatedCharge * 0.4f)) * 1.2f;
            finalBackspin = 50.f + (npc.getFinishing() * 0.8f);
        }
        else {
            finalPower *= 1.2f;
            vzPower *= 0.25f;   // Stay on the deck
        }

        // INTENTIONAL BEND: High finishing players curve the ball into the corners
        if ((npc.getFinishing() / 100.f) > 0.6f) {
            float curlDir = (aimDir.y < 3500.f) ? -1.0f : 1.0f;
            float rawSpin = npc.getCurl() * (0.8f + (npc.getFinishing() / 100.f) * 0.4f);
            finalSpin = curlDir * rawSpin * (1.1f + simulatedCharge / 2.f);

            float offsetRad = -curlDir * (15.0f * (npc.getCurl() / 100.f)) * (3.14159f / 180.f);
            aimDir = sf::Vector2f(
                aimDir.x * std::cos(offsetRad) - aimDir.y * std::sin(offsetRad),
                aimDir.x * std::sin(offsetRad) + aimDir.y * std::cos(offsetRad)
            );
        }
        finalPower = std::min(finalPower, npc.getKickPower() * (vzPower > 100.f ? 1.0f : 1.1f));
    }

    // ==========================================
    // --- WEAK FOOT LOGIC (AI SHOT) ---
    // ==========================================
    bool usingRight = npc.usingRightFoot();
    bool isRightFooted = (npc.getPreferredFoot() == "Right");
    bool isWeakFoot = (isRightFooted && !usingRight) || (!isRightFooted && usingRight);

    if (isWeakFoot) {
        float pMod, eMod;
        // Shots have higher base error than passes
        float baseError = (1.0f - (npc.getFinishing() / 100.0f)) * 10.0f;
        float shank = getWeakFootPenalty(npc.getWeakFootAccuracy(), pMod, eMod);

        finalPower *= pMod;
        float totalError = (baseError * eMod) + shank;

        float randError = ((rand() % 100) / 100.f - 0.5f) * totalError;
        float rad = randError * 3.14159f / 180.f;
        aimDir = sf::Vector2f(aimDir.x * std::cos(rad) - aimDir.y * std::sin(rad), aimDir.x * std::sin(rad) + aimDir.y * std::cos(rad));
    }

    ball.shoot(aimDir, finalPower, finalSpin, vzPower, finalBackspin);
    npc.resetKickCooldown();
}


// ==========================================
// --- GOALKEEPER AI ---
// ==========================================

void PlayerAI::handleGoalkeeping(NPCPlayer& npc, Ball& ball, const Pitch& pitch, const std::vector<Player*>& team, const std::vector<Player*>& opposition, float dt, const TeamAI& teamAI) {
    if (npc.getState() == PlayerState::Diving) {
        PhysicsEngine::applyKeeperDiveFriction(npc, dt);
        if (npc.getState() == PlayerState::Diving) return;
    }

    if (npc.getBallPossession()) {
        npc.m_possessionTimer += dt;

        Player* opp = findNearestOpponent(npc.getPosition(), opposition);
        float oppDist = opp ? dist(npc.getPosition(), opp->getPosition()) : 9999.f;
        PlaystyleType type = npc.getPlaystyle().type;

        // --- THE NEUER/ALISSON DNA ---
        // Calculate how comfortable the GK is with the ball at their feet (0.0 to 1.0)
        float gkDribbleSkill = (npc.getBallControl() * 0.7f + npc.getAgility() * 0.3f) / 100.f;

        // Sweeper keepers are comfortable, line keepers panic fast.
        float patience = (type == PlaystyleType::SweeperKeeper) ? 2.5f : 1.2f;

        // Elite dribbling GKs get "Ice in their Veins" - adding up to 1.5s of extra patience!
        patience += (gkDribbleSkill * 1.5f);

        // ==========================================
        // 1. HIGH PRESSURE (The Panic Zone vs The Turn)
        // ==========================================
        if (oppDist < 350.f) {

            // THE ALISSON TURN: 
            // If they are elite on the ball (>75 stat) and the striker isn't literally tackling them yet (>120px)
            if (gkDribbleSkill > 0.75f && oppDist > 120.f) {
                // Move away from the attacker to shield the ball
                sf::Vector2f toOpp = opp->getPosition() - npc.getPosition();
                sf::Vector2f awayDir = normalize(-toOpp);

                // Add a lateral side-step (Cruyff turn) to break the striker's ankles
                sf::Vector2f sideStep(-awayDir.y, awayDir.x);
                if ((rand() % 100) > 50) sideStep = -sideStep;

                sf::Vector2f dribbleDir = normalize(awayDir + sideStep * 0.5f);

                // Explode away from the pressure
                npc.setVelocity(dribbleDir * (npc.getTopSpeed() * 8.0f));

                // Return early so we DON'T clear the ball. We hold it and keep calculating!
                return;
            }

            // PANIC CLEARANCE: The striker got too close, or we lack the skill to turn
            float clearPower = npc.getKickPower() * 0.6f;
            float vzPower = 900.f;

            sf::Vector2f clearDir = teamAI.isHome() ? sf::Vector2f(1.f, 0.f) : sf::Vector2f(-1.f, 0.f);

            // High-skill GKs shank the ball much less under pressure
            float maxError = 35.0f - (gkDribbleSkill * 20.0f);
            float randError = ((rand() % 200) - 100) / 100.f * maxError;
            float rad = randError * 3.14159f / 180.f;

            sf::Vector2f finalDir(
                clearDir.x * std::cos(rad) - clearDir.y * std::sin(rad),
                clearDir.x * std::sin(rad) + clearDir.y * std::cos(rad)
            );

            ball.shoot(finalDir, clearPower, 0.f, vzPower, 50.f);
            npc.m_possessionTimer = 0.0f;
            npc.resetKickCooldown();
            npc.setVelocity({ 0.f, 0.f }); // Plant feet after kick
        }
        // ==========================================
        // 2. DISTRIBUTION (Patience Expired)
        // ==========================================
        else if (npc.m_possessionTimer > patience) {
            distributeBallAsGoalie(npc, ball, team, opposition, pitch, teamAI);
            npc.m_possessionTimer = 0.0f;
            npc.setVelocity({ 0.f, 0.f }); // Plant feet after kick
        }
        // ==========================================
        // 3. FREE SPACE (Carrying the Ball Out)
        // ==========================================
        else {
            // If they have space, a Sweeper Keeper with good feet will jog forward with the ball
            // to draw the press and gain territory before launching a pass.
            if (type == PlaystyleType::SweeperKeeper && gkDribbleSkill > 0.6f) {
                sf::Vector2f forwardDir = teamAI.isHome() ? sf::Vector2f(1.f, 0.f) : sf::Vector2f(-1.f, 0.f);
                float carrySpeed = npc.getTopSpeed() * 4.0f * gkDribbleSkill; // Confident jog
                npc.setVelocity(forwardDir * carrySpeed);
            }
            else {
                npc.setVelocity({ 0.f, 0.f }); // Traditional GKs just stand still and scan
            }
        }
        return;
    }
    npc.m_possessionTimer = 0.0f;

    sf::Vector2f myGoalCenter = teamAI.isHome() ? pitch.homeGoalCenter : pitch.awayGoalCenter;
    sf::Vector2f targetPos;
    bool sprint = false;

    bool ballInBoxX = teamAI.isHome() ? (ball.getPosition().x < 1650.f) : (ball.getPosition().x > pitch.totalWidth - 1650.f);
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

    float distToTarget = dist(npc.getPosition(), targetPos);
    if (distToTarget > 5.0f) {
        float actualSpeed = std::min(sprint ? (npc.getTopSpeed() * 10.0f) : 400.0f + ((std::max(npc.getAgility(), npc.getGkReactions()) / 100.0f) * 200.0f), distToTarget / dt);
        npc.setVelocity(normalize(targetPos - npc.getPosition()) * actualSpeed);
        npc.setRotationToward(ball.getPosition());
    }
    else {
        npc.setVelocity({ 0.f, 0.f });
        npc.setRotationToward(ball.getPosition());
    }

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
    float maxDiveSpeed = 600.0f + ((activeStat / 100.0f) * 1000.0f);

    float keeperTTI = diveDistance / maxDiveSpeed;
    if (ballTTI > keeperTTI + 0.15f) return;

    float attemptDiveRadius = 800.0f;

    if (diveDistance <= attemptDiveRadius && interceptZ <= (npc.height + 120.0f)) {
        float optimalSpeed = maxDiveSpeed;
        if (ballTTI > 0.05f) optimalSpeed = diveDistance / ballTTI;
        float finalSpeed = std::clamp(optimalSpeed, 150.0f, maxDiveSpeed);

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

void PlayerAI::distributeBallAsGoalie(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI)
{
    Player* bestTarget = nullptr;
    float bestScore = -1e9f;
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);

    PlayerBehavior behavior = npc.getPlaystyle().behavior;
    float passRisk = behavior.passRiskBias; // Distributors = high risk, Shot Stoppers = low risk
    float tikiTakaPref = teamAI.getPassingLengthPref();

    for (Player* mate : teammates) {
        if (mate == &npc) continue;
        sf::Vector2f matePos = mate->getPosition();
        float d = dist(npcPos, matePos);
        float progress = isHome ? (matePos.x - npcPos.x) : (npcPos.x - matePos.x);

        // Base score favors moving the ball forward safely
        float score = (progress * 0.5f);

        // --- DNA & TACTICAL OVERRIDES ---
        if (d < 1500.f) {
            // Short pass to Center Back / Fullback
            score += (tikiTakaPref * 1500.f);
            score += (1.0f - passRisk) * 800.f; // Safe GKs love short passes
        }
        else if (d > 3000.f) {
            // Long boot to Target Man / Winger
            score += ((1.0f - tikiTakaPref) * 1500.f);
            score += (passRisk * 1200.f) * (npc.getLongPassing() / 100.f); // Distributors look for counters
        }

        // --- PASSING LANE SAFETY CHECK ---
        sf::Vector2f passDir = normalize(matePos - npcPos);
        bool laneBlocked = false;

        for (Player* opp : opposition) {
            float dOpp = dist(npcPos, opp->getPosition());
            if (dOpp < d && dOpp > 100.f) {
                sf::Vector2f toOpp = opp->getPosition() - npcPos;
                // If opponent is directly in the path
                if ((passDir.x * (toOpp.x / dOpp) + passDir.y * (toOpp.y / dOpp)) > 0.95f) {
                    laneBlocked = true;
                    break;
                }
            }
        }

        // Goalkeepers are heavily punished for trying to pass through blocked lanes
        if (laneBlocked) score -= 5000.f;

        // Opponent proximity to receiver
        Player* nearestToTarget = findNearestOpponent(matePos, opposition);
        if (nearestToTarget) {
            float distToTarget = dist(matePos, nearestToTarget->getPosition());
            if (distToTarget < 400.f) score -= 1000.f; // Receiver is marked
        }

        if (d > 500.0f && d < 6500.0f) {
            if (score > bestScore) {
                bestScore = score;
                bestTarget = mate;
            }
        }
    }

    if (!bestTarget && !teammates.empty()) bestTarget = teammates[0];
    if (!bestTarget) return;

    // --- EXECUTION ---
    sf::Vector2f targetPos = bestTarget->getPosition();
    float rawDist = dist(npcPos, targetPos);
    bool goHigh = (rawDist > 2000.f); // GKs lob anything past 20 meters

    sf::Vector2f directDir = normalize(targetPos - npcPos);

    // Throwing vs Kicking determination
    bool useThrow = (rawDist < 2500.f && npc.getGkThrowing() > npc.getLongPassing());
    float statToUse = useThrow ? npc.getGkThrowing() : npc.getLongPassing();

    // Use standard AimAssist for perfect targeting
    float finalPower = 0.f;
    AimAssist::applyPassAssist(npc, bestTarget, directDir, finalPower, goHigh, true);

    float vzPower = 0.f;
    float finalBackspin = 0.f;

    if (goHigh) {
        float maxLoft = std::clamp(500.f + (rawDist * 0.25f), 700.f, 1150.f);
        vzPower = std::max(maxLoft - ((statToUse / 100.f) * 80.f), 300.f);
        finalBackspin = 60.f + (statToUse * 0.5f);
    }
    else {
        vzPower = 50.f + (finalPower * 0.5f); // Bouncy ground pass / fast roll
        finalBackspin = 10.f;
    }

    // Apply GK Error
    float errorMagnitude = (1.0f - (statToUse / 100.f));
    float randError = ((rand() % 200) - 100) / 100.f * errorMagnitude * 15.0f;
    float rad = randError * 3.14159f / 180.f;

    sf::Vector2f finalDir(
        directDir.x * std::cos(rad) - directDir.y * std::sin(rad),
        directDir.x * std::sin(rad) + directDir.y * std::cos(rad)
    );

    ball.shoot(finalDir, finalPower, 0.0f, vzPower, finalBackspin);
    npc.resetKickCooldown();
}

// ==========================================
// --- AERIAL PHYSICS & STRIKES ---
// ==========================================

void PlayerAI::handleNPCJumpLogic(NPCPlayer& npc, Ball& ball) {
    if (npc.z > 0.0f || npc.getState() == PlayerState::Tackling) return;

    float d = dist(npc.getPosition(), ball.getPosition());

    if (d < 350.f && ball.z > 150.f && ball.vz < 0.f) {
        float awarenessNorm = npc.getAwareness() / 100.f;
        float idealInterceptZ = 170.f;
        float error = (1.0f - awarenessNorm) * 100.f;

        if (ball.z < (idealInterceptZ + error)) {
            float jumpingNorm = npc.getJumpingStrength() / 100.0f;
            float jumpVz = 240.f + (jumpingNorm * 160.f);
            sf::Vector2f futureBallPos = ball.getPosition() + ball.getVelocity();
            sf::Vector2f jumpDir = normalize(futureBallPos - npc.getPosition());
            float speed = std::sqrt(npc.getVelocity().x * npc.getVelocity().x + npc.getVelocity().y * npc.getVelocity().y);

            npc.setVelocity(jumpDir * speed);
            npc.vz = jumpVz;
            npc.setState(PlayerState::Jumping);
        }
    }
}

bool PlayerAI::tryNPCAerialStrike(NPCPlayer& npc, Ball& ball, sf::Vector2f aimDir, bool isShot) {
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


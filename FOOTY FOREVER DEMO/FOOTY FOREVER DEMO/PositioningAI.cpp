#include "PositioningAI.h"
#include "SpatialGrid.h"
#include "PlayerAI.h"

sf::Vector2f PositioningAI::decideTargetPosition(NPCPlayer& npc, Ball& ball, const Pitch& pitch, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, Player* firstResponder, PositioningMask mask, const TeamAI& teamAI, const SpatialGrid& spatialGrid)
{
    if (mask.useManualTarget) return mask.manualTarget;

    bool isHomeSide = (npc.getTeam() == Team::Home);
    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f npcPos = npc.getPosition();
    float distToBall = PlayerAI::dist(npcPos, ballPos);
    sf::Vector2f goalPos = isHomeSide ? sf::Vector2f(pitch.margin, 3500.f) : sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f);

    sf::Vector2f dynamicAnchor = npc.getHomePosition();

    PositionRole role = npc.getPositionRole();
    bool isDefender = (role == PositionRole::CenterBack || role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);
    dynamicAnchor.x += teamAI.getDefensiveLineOffset(isDefender);

    TacticalZone zone = teamAI.getEffectiveTacticalZone(npc.getPlaystyle());

    sf::Vector2f target = applyTacticalPositioning(npc, ball, dynamicAnchor, ballPos, goalPos, state, zone, pitch, team, opponents, teamAI, spatialGrid);

    target += mask.homeOffset;
    if (mask.lateralSqueeze > 0.0f) {
        target.y = target.y + ((ballPos.y - target.y) * mask.lateralSqueeze);
    }

    sf::Vector2f finalTarget = target;
    bool isChasing = false;

    if (shouldEmergencyChase(npc, firstResponder, distToBall, pitch, ball, MatchState::InPlay, teamAI)) {
        isChasing = true;
        if (ball.hasOwner() && ball.getOwner()->getTeam() != npc.getTeam()) {
            float pressDistance = std::max(70.f, 200.f - (npc.getAggression() * 1.5f));
            finalTarget = ballPos + (PlayerAI::normalize(goalPos - ballPos) * pressDistance);
        }
        else {
            // THE FIX: Passed pitch here!
            finalTarget = calculateInterceptionPoint(npc, ball, pitch);
        }
    }
    else {
        finalTarget = clampToTacticalZone(target, dynamicAnchor, zone, distToBall, isHomeSide, teamAI);
    }

    float goalCenterY = 3500.f;
    float goalHalfWidth = 450.f;

    bool behindHomeGoal = (npcPos.x < pitch.margin + 20.f && std::abs(npcPos.y - goalCenterY) < goalHalfWidth);
    bool behindAwayGoal = (npcPos.x > pitch.totalWidth - pitch.margin - 20.f && std::abs(npcPos.y - goalCenterY) < goalHalfWidth);

    if ((behindHomeGoal || behindAwayGoal) && !isChasing) {
        finalTarget.x = behindHomeGoal ? (pitch.margin - 20.f) : (pitch.totalWidth - pitch.margin + 20.f);

        if (npcPos.y < goalCenterY) {
            finalTarget.y = goalCenterY - goalHalfWidth - 100.f;
        }
        else {
            finalTarget.y = goalCenterY + goalHalfWidth + 100.f;
        }
    }
    else {
        bool targetingHomeGoal = (finalTarget.x < pitch.margin + 50.f && std::abs(finalTarget.y - goalCenterY) < goalHalfWidth);
        bool targetingAwayGoal = (finalTarget.x > pitch.totalWidth - pitch.margin - 50.f && std::abs(finalTarget.y - goalCenterY) < goalHalfWidth);

        if (targetingHomeGoal) finalTarget.x = pitch.margin + 50.f;
        if (targetingAwayGoal) finalTarget.x = pitch.totalWidth - pitch.margin - 50.f;
    }

    return finalTarget;
}

sf::Vector2f PositioningAI::evaluateShapeAndSpace(NPCPlayer& npc, sf::Vector2f currentTarget, sf::Vector2f ballPos, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, const TeamAI& teamAI, const TacticalZone& zone, Ball& ball, const Pitch& pitch, const SpatialGrid& spatialGrid)
{
    sf::Vector2f spatialCorrection(0.f, 0.f);
    sf::Vector2f npcPos = npc.getPosition();
    float awareness = npc.getAwareness() / 100.0f;

    PositionRole role = npc.getPositionRole();
    bool isForward = (role == PositionRole::Striker || role == PositionRole::CenterForward || role == PositionRole::LeftWing || role == PositionRole::RightWing);
    bool isMid = (role == PositionRole::CenterMid || role == PositionRole::AttackingMid || role == PositionRole::LeftMid || role == PositionRole::RightMid || role == PositionRole::DefensiveMid);
    bool isDefender = (role == PositionRole::CenterBack || role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);

    bool isHomeSide = (npc.getTeam() == Team::Home);
    float pitchCenterY = pitch.totalHeight / 2.f;
    float halfwayX = pitch.totalWidth / 2.f;

    // ==========================================
    // --- THE KEEPER SPACE BUFFER ---
    // ==========================================
    if (ball.hasOwner() && ball.getOwner()->getPositionRole() == PositionRole::Goalkeeper && ball.getOwner()->getTeam() == npc.getTeam()) {
        Player* gk = ball.getOwner();
        float distToGk = PlayerAI::dist(npcPos, gk->getPosition());

        if (distToGk < 1200.f && &npc != gk) {
            sf::Vector2f repelDir = PlayerAI::normalize(npcPos - gk->getPosition());
            if (isDefender) {
                repelDir.x *= 0.2f; repelDir.y *= 1.8f;
                repelDir = PlayerAI::normalize(repelDir);
            }
            else {
                repelDir.x *= 1.8f; repelDir.y *= 0.2f;
                repelDir = PlayerAI::normalize(repelDir);
            }
            spatialCorrection += repelDir * (1200.f - distToGk);
        }
    }

    if (state == TeamState::Attacking) {

        // ==========================================
        // --- 1. GRID-BASED DYNAMIC SUPPORT ---
        // ==========================================
        if (ball.hasOwner() && ball.getOwner()->getTeam() == npc.getTeam() && ball.getOwner() != &npc) {
            Player* carrier = ball.getOwner();
            float distToCarrier = PlayerAI::dist(npcPos, carrier->getPosition());

            // If we are in the supporting radius (5m to 25m)
            if (distToCarrier > 500.f && distToCarrier < 2500.f) {
                // Let the Spatial Grid do the heavy lifting! 
                // It organically checks enemy influence, passing lanes, and proximity.
                sf::Vector2f bestPocket = spatialGrid.findBestSupportPocket(carrier->getPosition(), npcPos, npc.getTeam(), pitch);
                float distToPocket = PlayerAI::dist(npcPos, bestPocket);

                if (distToPocket > 100.f) {
                    Player* presser = PlayerAI::findNearestOpponent(carrier->getPosition(), opponents);
                    float presserDist = presser ? PlayerAI::dist(carrier->getPosition(), presser->getPosition()) : 9999.f;

                    // If the carrier is actively being swarmed, urgency spikes and we sprint to the pocket! 
                    // Otherwise, jog gently into the space to offer the lane.
                    float urgency = (presserDist < 500.f) ? 1.8f : 0.8f;

                    sf::Vector2f shiftVec = PlayerAI::normalize(bestPocket - npcPos);
                    spatialCorrection += shiftVec * std::min(distToPocket, 1000.f) * awareness * urgency;
                }
            }
        }

        // ==========================================
        // --- 2. POSITIONAL ROTATION & COVERING ---
        // ==========================================
        // 1. FULLBACK OVERLAP
        bool isLB = (role == PositionRole::LeftBack || role == PositionRole::LeftWingBack);
        bool isRB = (role == PositionRole::RightBack || role == PositionRole::RightWingBack);

        if (isLB || isRB) {
            Player* winger = nullptr;
            for (Player* tm : team) {
                if ((isLB && (tm->getPositionRole() == PositionRole::LeftWing || tm->getPositionRole() == PositionRole::LeftMid)) ||
                    (isRB && (tm->getPositionRole() == PositionRole::RightWing || tm->getPositionRole() == PositionRole::RightMid))) {
                    winger = tm; break;
                }
            }
            if (winger && (isHomeSide ? winger->getPosition().x > halfwayX : winger->getPosition().x < halfwayX)) {
                float wingerDistFromSide = isLB ? (winger->getPosition().y - pitch.margin) : ((pitch.totalHeight - pitch.margin) - winger->getPosition().y);

                if (wingerDistFromSide > 1200.f) {
                    spatialCorrection.x += isHomeSide ? 800.f : -800.f;
                    spatialCorrection.y += isLB ? -1000.f : 1000.f;
                }
            }
        }

        // 2. MIDFIELD DEFENSIVE COVER
        if (role == PositionRole::DefensiveMid || role == PositionRole::CenterMid) {
            for (Player* tm : team) {
                if (tm->getPositionRole() == PositionRole::LeftBack || tm->getPositionRole() == PositionRole::RightBack) {
                    bool fbAdvanced = isHomeSide ? (tm->getPosition().x > halfwayX) : (tm->getPosition().x < halfwayX);

                    if (fbAdvanced && PlayerAI::dist(npcPos, tm->getPosition()) < 3000.f) {
                        sf::Vector2f coverPos = tm->getHomePosition();
                        spatialCorrection += (coverPos - npcPos) * 0.7f * awareness;
                        break;
                    }
                }
            }
        }

        // 3. STRIKER FALSE-9 DROP
        if (role == PositionRole::Striker || role == PositionRole::CenterForward) {
            for (Player* tm : team) {
                if (tm->getPositionRole() == PositionRole::CenterMid || tm->getPositionRole() == PositionRole::AttackingMid) {
                    bool midWentWide = std::abs(tm->getPosition().y - pitchCenterY) > 1500.f;

                    if (midWentWide && PlayerAI::dist(npcPos, tm->getPosition()) < 2000.f) {
                        sf::Vector2f dropVec = isHomeSide ? sf::Vector2f(-1.f, 0.f) : sf::Vector2f(1.f, 0.f);
                        spatialCorrection += dropVec * 800.f * awareness;
                    }
                }
            }
        }

        // 4. MIDFIELD POCKET FINDING (Minor tactical shift)
        if (isMid) {
            Player* nearestOpp = PlayerAI::findNearestOpponent(currentTarget, opponents);
            if (nearestOpp && PlayerAI::dist(currentTarget, nearestOpp->getPosition()) < 700.f) {
                sf::Vector2f toOpp = nearestOpp->getPosition() - currentTarget;
                float escapeY = (toOpp.y > 0) ? -1.0f : 1.0f;
                spatialCorrection.y += escapeY * 350.f * awareness * zone.roamingFreedom;
            }
        }
    }
    else {
        // ==========================================
        // --- THE FIX: THE DEFENSIVE FORTRESS ---
        // ==========================================
        float ballDistFromCenterY = std::abs(ballPos.y - pitchCenterY);
        bool inDangerZone = isHomeSide ? (ballPos.x < pitch.margin + 3000.f) : (ballPos.x > pitch.totalWidth - pitch.margin - 3000.f);

        if (inDangerZone && isMid) {
            // Midfielders form a "Screen" in front of the backline.
            // Calculate the exact vector from the ball to the center of the goal
            sf::Vector2f myGoalCenter = isHomeSide ? sf::Vector2f(pitch.margin, pitchCenterY) : sf::Vector2f(pitch.totalWidth - pitch.margin, pitchCenterY);
            sf::Vector2f ballToGoal = PlayerAI::normalize(myGoalCenter - ballPos);

            // The ideal screen position is ~12 meters (1200px) away from the goal, intersecting the ball's path
            sf::Vector2f screenPos = myGoalCenter - (ballToGoal * 1200.f);

            // Midfielders aggressively pull towards this screening position to block shots and passes
            spatialCorrection += (screenPos - npcPos) * 0.8f * awareness;
        }
        else if (ballDistFromCenterY < 1200.f) {
            // ATTACK IS DOWN THE MIDDLE (Normal play)
            if (role == PositionRole::DefensiveMid || role == PositionRole::CenterMid) {
                sf::Vector2f dropVec = isHomeSide ? sf::Vector2f(-1.f, 0.f) : sf::Vector2f(1.f, 0.f);
                spatialCorrection += dropVec * 600.f * awareness;

                float pinch = (pitchCenterY - npcPos.y);
                spatialCorrection.y += pinch * 0.6f * awareness;
            }
        }
        else {
            // ATTACK IS OUT WIDE
            bool ballOnLeft = ballPos.y < pitchCenterY;
            if ((ballOnLeft && (role == PositionRole::LeftMid || role == PositionRole::LeftWing)) ||
                (!ballOnLeft && (role == PositionRole::RightMid || role == PositionRole::RightWing))) {

                sf::Vector2f dropVec = isHomeSide ? sf::Vector2f(-1.f, 0.f) : sf::Vector2f(1.f, 0.f);
                spatialCorrection += dropVec * 900.f * awareness;

                float pullWide = ballOnLeft ? -1.0f : 1.0f;
                spatialCorrection.y += pullWide * 400.f * awareness;
            }
        }

        // STRUCTURAL INTEGRITY & GRID GAP FILLING
        if (isDefender || isMid) {
            float sectorRadius = isDefender ? 1200.f : 1600.f;
            sf::Vector2f bestCover = spatialGrid.findBestCoverSpace(npcPos, npc.getHomePosition(), sectorRadius, npc.getTeam(), pitch);

            float distToCover = PlayerAI::dist(npcPos, bestCover);
            if (distToCover > 300.f) {
                float coverWillingness = awareness * (0.4f + (zone.roamingFreedom * 0.4f));
                sf::Vector2f shiftVec = PlayerAI::normalize(bestCover - npcPos);
                spatialCorrection += shiftVec * std::min(distToCover, 800.f) * coverWillingness;
            }
        }

        // RUNNER TRACKING OVERRIDE
        bool trackingRunner = false;
        if (isDefender) {
            float myGoalX = isHomeSide ? pitch.margin : pitch.totalWidth - pitch.margin;
            Player* mostDangerousRunner = nullptr;
            float highestDangerScore = 0.f;

            for (Player* opp : opponents) {
                if (opp->getPositionRole() == PositionRole::Goalkeeper || opp == ball.getOwner()) continue;

                sf::Vector2f oppVel = opp->getVelocity();
                float oppSpeed = std::sqrt(oppVel.x * oppVel.x + oppVel.y * oppVel.y);

                if (oppSpeed > 500.f) {
                    bool runningAtGoal = isHomeSide ? (oppVel.x < -200.f) : (oppVel.x > 200.f);

                    if (runningAtGoal) {
                        float distToDefender = PlayerAI::dist(npcPos, opp->getPosition());
                        if (distToDefender < 1500.f) {
                            float danger = oppSpeed + (1500.f - distToDefender);
                            if (danger > highestDangerScore) {
                                highestDangerScore = danger;
                                mostDangerousRunner = opp;
                            }
                        }
                    }
                }
            }

            if (mostDangerousRunner && (awareness + zone.roamingFreedom) > 0.8f) {
                trackingRunner = true;
                sf::Vector2f toGoal = PlayerAI::normalize(sf::Vector2f(myGoalX, 3500.f) - mostDangerousRunner->getPosition());
                sf::Vector2f trackTarget = mostDangerousRunner->getPosition() + (mostDangerousRunner->getVelocity() * 0.4f) + (toGoal * 100.f);
                spatialCorrection += (trackTarget - npcPos) * 0.85f * awareness;
            }
        }

        // UNIVERSAL HORIZONTAL LINE SYNC
        float avgLineX = 0.f;
        int lineCount = 0;

        for (Player* tm : team) {
            if (tm->isSentOff()) continue;

            bool tmIsForward = (tm->getPositionRole() == PositionRole::Striker || tm->getPositionRole() == PositionRole::CenterForward || tm->getPositionRole() == PositionRole::LeftWing || tm->getPositionRole() == PositionRole::RightWing);
            bool tmIsMid = (tm->getPositionRole() == PositionRole::CenterMid || tm->getPositionRole() == PositionRole::AttackingMid || tm->getPositionRole() == PositionRole::LeftMid || tm->getPositionRole() == PositionRole::RightMid || tm->getPositionRole() == PositionRole::DefensiveMid);
            bool tmIsDef = (tm->getPositionRole() == PositionRole::CenterBack || tm->getPositionRole() == PositionRole::LeftBack || tm->getPositionRole() == PositionRole::RightBack || tm->getPositionRole() == PositionRole::LeftWingBack || tm->getPositionRole() == PositionRole::RightWingBack);

            if ((isDefender && tmIsDef) || (isMid && tmIsMid) || (isForward && tmIsForward)) {
                avgLineX += tm->getPosition().x;
                lineCount++;
            }
        }

        if (lineCount > 0) {
            avgLineX /= lineCount;
            float xDiff = avgLineX - npcPos.x;
            float discipline = 1.0f - (zone.roamingFreedom * 0.4f);
            float distToBall = PlayerAI::dist(npcPos, ballPos);

            bool isStopper = false;
            if (distToBall < 1800.f) {
                isStopper = true;
                for (Player* tm : team) {
                    if (tm == &npc || tm->getPositionRole() == PositionRole::Goalkeeper || tm->isSentOff()) continue;

                    bool tmIsForward = (tm->getPositionRole() == PositionRole::Striker || tm->getPositionRole() == PositionRole::CenterForward || tm->getPositionRole() == PositionRole::LeftWing || tm->getPositionRole() == PositionRole::RightWing);
                    bool tmIsMid = (tm->getPositionRole() == PositionRole::CenterMid || tm->getPositionRole() == PositionRole::AttackingMid || tm->getPositionRole() == PositionRole::LeftMid || tm->getPositionRole() == PositionRole::RightMid || tm->getPositionRole() == PositionRole::DefensiveMid);
                    bool tmIsDef = (tm->getPositionRole() == PositionRole::CenterBack || tm->getPositionRole() == PositionRole::LeftBack || tm->getPositionRole() == PositionRole::RightBack || tm->getPositionRole() == PositionRole::LeftWingBack || tm->getPositionRole() == PositionRole::RightWingBack);

                    if (((isDefender && tmIsDef) || (isMid && tmIsMid) || (isForward && tmIsForward)) &&
                        PlayerAI::dist(tm->getPosition(), ballPos) < distToBall) {
                        isStopper = false;
                        break;
                    }
                }
            }

            if (std::abs(xDiff) > 50.f && !isStopper && !trackingRunner) {
                float snapStrength = isDefender ? 0.90f : 0.75f;
                spatialCorrection.x += xDiff * snapStrength * discipline * awareness;
            }
        }
    }

    float correctionMagnitude = std::sqrt(spatialCorrection.x * spatialCorrection.x + spatialCorrection.y * spatialCorrection.y);
    float maxAllowedDrift = 300.f + (zone.roamingFreedom * 500.f); // THE FIX: Shrunk absolute max drift!

    if (correctionMagnitude > maxAllowedDrift) {
        spatialCorrection = (spatialCorrection / correctionMagnitude) * maxAllowedDrift;
    }

    return spatialCorrection;
}

sf::Vector2f PositioningAI::applyTacticalPositioning(NPCPlayer& npc, Ball& ball, sf::Vector2f homePos, sf::Vector2f ballPos, sf::Vector2f goalPos, TeamState state, TacticalZone zone, const Pitch& pitch, const std::vector<Player*>& team, const std::vector<Player*>& opposition, const TeamAI& teamAI, const SpatialGrid& spatialGrid)
{
    // ==========================================
    // --- 0. THE S-CURVE PROGRESSION ---
    // ==========================================
    float rawProgress = teamAI.getBallProgress();
    rawProgress = std::clamp(rawProgress, 0.0f, 1.0f);
    float ballProgress = rawProgress * rawProgress * rawProgress * (rawProgress * (rawProgress * 6.0f - 15.0f) + 10.0f);
    bool isHomeSide = (npc.getTeam() == Team::Home);

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

            if (isHomeSide) {
                if (px < threatX) threatX = px; // Home defends 0 (Left)
            }
            else {
                if (px > threatX) threatX = px; // Away defends 10000 (Right)
            }
        }

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

    if (isHomeSide && enemyOffsideLineX < pitch.totalWidth / 2.f) enemyOffsideLineX = pitch.totalWidth / 2.f;
    if (!isHomeSide && enemyOffsideLineX > pitch.totalWidth / 2.f) enemyOffsideLineX = pitch.totalWidth / 2.f;

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

        if (ball.hasOwner() && ball.getOwner()->getPositionRole() == PositionRole::Goalkeeper) {
            float pushAmount = 2500.f + (teamAI.getPassingLengthPref() * 2000.f);
            targetBallX += (isHomeSide ? pushAmount : -pushAmount);
        }

        if ((isForward || npc.getPositionRole() == PositionRole::AttackingMid) && zone.supportDepth > -0.3f) {
            float leadDist = 400.f + (passLengthPref * 1000.f) + (zone.supportDepth * 600.f);
            targetBallX += depthPush * leadDist;
        }
        else if (isMid && zone.supportDepth > -0.3f) {
            // THE FIX 1: Increased Midfield lead distance so they join the attack faster!
            float leadDist = 300.f + (passLengthPref * 800.f) + (zone.supportDepth * 500.f);
            targetBallX += depthPush * leadDist;
        }

        float xDiff = targetBallX - tacticalTarget.x;
        tacticalTarget.x += (xDiff * zone.ballInfluence * 0.6f);

        // 2. TRANSITION SPEED & PUSH
        float unitExpansion = 0.0f;

        if (isForward) unitExpansion = std::clamp((ballProgress - 0.1f) * (1.8f + (counterSpeed * 0.5f)), 0.0f, 1.0f);
        // THE FIX 2: Midfielders spring to life immediately! 
        // We added a baseline +0.10f so they expand even when deep in their own half, and raised the floor to 0.15f.
        else if (isMid) unitExpansion = std::clamp((ballProgress + 0.10f) * (1.5f + (counterSpeed * 1.5f)), 0.15f, 1.0f);
        else unitExpansion = std::clamp((ballProgress - 0.2f) * (1.0f + (counterSpeed * 0.4f)), 0.0f, 0.4f + (freedomPref * 0.3f));

        float runModifier = (behavior.runFrequency - 0.5f) * 0.4f;
        unitExpansion = std::clamp(unitExpansion + runModifier, 0.0f, 1.0f);

        float pushFactor = (ballProgress - 0.5f) * 2.0f;
        pushFactor = std::max(0.0f, pushFactor);

        float leashMultiplier = isForward ? 0.5f : (isMid ? 0.3f : 0.15f);
        tacticalTarget.x += depthPush * (zone.forwardLeash * leashMultiplier * unitExpansion);
        tacticalTarget.x += depthPush * (zone.supportDepth * 600.f);

        // ---------------------------------------------------------
        // 3. ATTACKING SHAPE (Extreme Fanning Out)
        // ---------------------------------------------------------
        float yOffset = tacticalTarget.y - pitchCenterY;
        tacticalTarget.y += yOffset * (0.25f * pushFactor);

        if (std::abs(zone.widthPreference) > 0.1f) {
            float dirToTouchline = (tacticalTarget.y > pitchCenterY) ? 1.0f : -1.0f;

            // THE FIX 4: WINGER TOUCHLINE HUGGING
            bool isWidePlayer = (npc.getPositionRole() == PositionRole::LeftWing ||
                npc.getPositionRole() == PositionRole::RightWing ||
                npc.getPositionRole() == PositionRole::LeftMid ||
                npc.getPositionRole() == PositionRole::RightMid ||
                npc.getPositionRole() == PositionRole::LeftWingBack ||
                npc.getPositionRole() == PositionRole::RightWingBack);

            // Center players push out 500px. Wide players push out a massive 2200px to chalk their boots!
            float widthPush = isWidePlayer ? 2200.f : 500.f;

            tacticalTarget.y += (dirToTouchline * zone.widthPreference * widthPush);
        }

        // 4. DYNAMIC PASSING POCKETS
        float idealSupportDist = 600.f + (passLengthPref * 800.f);
        float distToBallTarget = PlayerAI::dist(tacticalTarget, ballPos);

        if (distToBallTarget < idealSupportDist - 150.f) {
            sf::Vector2f repelDir = PlayerAI::normalize(tacticalTarget - ballPos);
            repelDir.x *= 0.3f;
            repelDir.y *= 1.8f;
            repelDir = PlayerAI::normalize(repelDir);
            tacticalTarget += repelDir * (idealSupportDist - distToBallTarget) * 0.7f;
        }
        else if (distToBallTarget > idealSupportDist + 300.f && !isForward) {
            sf::Vector2f attractDir = PlayerAI::normalize(ballPos - tacticalTarget);
            tacticalTarget += attractDir * (distToBallTarget - idealSupportDist) * passLengthPref * 0.6f;
        }

        // 5. ATTACKING BOX LIMITS & OFFSIDE
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
            // Increased base drop so the backline retreats faster against counters
            dropMultiplier = 0.40f + (depthPref * 0.35f);
        }
        else if (isMid) {
            // THE FIX 2: MASSIVE MIDFIELD DROP BUFF
            // Midfielders now drop almost as fast as defenders to compress the space!
            dropMultiplier = 0.45f + (depthPref * 0.35f);
            if (npc.getPositionRole() == PositionRole::DefensiveMid) dropMultiplier += 0.15f;
        }
        else {
            dropMultiplier = 0.20f + (depthPref * 0.20f);
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
            float outletDepth = 400.f + (depthPref * 800.f);
            float targetX = isHomeSide ? (ballPos.x - outletDepth) : (ballPos.x + outletDepth);

            if (isHomeSide) {
                targetX = std::min(targetX, enemyOffsideLineX - 150.f);
            }
            else {
                targetX = std::max(targetX, enemyOffsideLineX + 150.f);
            }

            float minStrikerDepth = isHomeSide ? (myDefLineX + 2500.f) : (myDefLineX - 2500.f);
            if (isHomeSide) {
                targetX = std::max(targetX, minStrikerDepth);
            }
            else {
                targetX = std::min(targetX, minStrikerDepth);
            }

            tacticalTarget.x = targetX;
        }

        // 3. DEFENSIVE COMPACTNESS (Slamming the Door)
        float yOffset = tacticalTarget.y - pitchCenterY;
        float squeezeFactor = 0.0f;

        if (isDefender) squeezeFactor = (0.5f + (depthPref * 0.4f)) * dropIntensity;
        else if (isMid) squeezeFactor = (0.2f + (depthPref * 0.3f)) * dropIntensity;

        float ballDistFromCenterY = std::abs(ballPos.y - pitchCenterY);
        float distToBallExact = PlayerAI::dist(npc.getPosition(), ballPos);

        if (distToBallExact < 2000.f && ballDistFromCenterY < 1200.f) {
            squeezeFactor *= 1.8f;
        }
        tacticalTarget.y -= yOffset * squeezeFactor;

        // 4. THE SWARM
        float maxLateralShift = 800.f + (depthPref * 400.f);
        float desiredYShift = (ballPos.y - tacticalTarget.y) * (zone.ballInfluence * 0.85f);
        desiredYShift = std::clamp(desiredYShift, -maxLateralShift, maxLateralShift);
        tacticalTarget.y += desiredYShift;
        sf::Vector2f npcPos = npc.getPosition();

        // ==========================================
        // --- 4.5 THE STOPPER / SWEEPER SYSTEM ---
        // ==========================================
        if (isDefender && distToBallExact < 1000.f) {
            bool amIClosestDef = true;
            for (Player* tm : team) {
                if (tm == &npc || tm->getPositionRole() == PositionRole::Goalkeeper || tm->isSentOff()) continue;

                bool tmIsDef = (tm->getPositionRole() == PositionRole::CenterBack ||
                    tm->getPositionRole() == PositionRole::LeftBack ||
                    tm->getPositionRole() == PositionRole::RightBack ||
                    tm->getPositionRole() == PositionRole::LeftWingBack ||
                    tm->getPositionRole() == PositionRole::RightWingBack);

                if (tmIsDef && PlayerAI::dist(tm->getPosition(), ballPos) < distToBallExact) {
                    amIClosestDef = false;
                    break;
                }
            }

            if (amIClosestDef) {
                sf::Vector2f toBall = ballPos - tacticalTarget;

                float defensiveUrgency = 1.0f;
                float penaltyBoxX = isHomeSide ? pitch.margin + 1650.f : pitch.totalWidth - pitch.margin - 1650.f;
                if (std::abs(ballPos.x - penaltyBoxX) < 1200.f) defensiveUrgency = 1.6f;

                // Nerfed the base stopper bite from 0.4 down to 0.25 so they don't over-commit
                float stopperBite = std::min(0.85f, (0.25f + (npc.getAggression() / 100.f * 0.35f)) * defensiveUrgency);
                sf::Vector2f biteVector = toBall * stopperBite;

                // ==========================================
                // --- THE FIX 3: THE LINE-BREAK CLAMP ---
                // ==========================================
                // DO NOT allow the defender to step more than 600px ahead of the average defensive line!
                if (isHomeSide && tacticalTarget.x + biteVector.x > myDefLineX + 600.f) {
                    biteVector.x = (myDefLineX + 600.f) - tacticalTarget.x;
                }
                else if (!isHomeSide && tacticalTarget.x + biteVector.x < myDefLineX - 600.f) {
                    biteVector.x = (myDefLineX - 600.f) - tacticalTarget.x;
                }

                tacticalTarget += biteVector;
            }
            else {
                float coverDrop = 250.f + (depthPref * 150.f);
                tacticalTarget.x += isHomeSide ? -coverDrop : coverDrop;

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
            // THE FIX 3: THE BUILD-UP INVERSION
            float idealDistance = 0.f;

            // If the ball is in the first 35% of the pitch (Build-Up phase)
            if (ballProgress < 0.35f) {
                // Show IN FRONT of the ball carrier by 10 to 15 meters to offer an outlet!
                // (Negative values put the player ahead of the ball)
                idealDistance = -(1000.f + (passLengthPref * 500.f));
            }
            else {
                // Final Third / Opponent Half: Sit 6 to 10 meters BEHIND the ball as a safety pivot
                idealDistance = 600.f + (passLengthPref * 400.f);
                if (npc.getPositionRole() == PositionRole::AttackingMid) idealDistance = 150.f;
            }

            float pocketX = isHomeSide ? (ballPos.x - idealDistance) : (ballPos.x + idealDistance);

            // Pull strength increases when deep so we aggressively break out of the backline to get open
            float deepUrgency = (ballProgress < 0.35f) ? 0.3f : 0.0f;
            float pullStrength = 0.4f + (zone.supportDepth * 0.2f) + deepUrgency;

            tacticalTarget.x = (tacticalTarget.x * (1.0f - pullStrength)) + (pocketX * pullStrength);
        }
        else {
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
    sf::Vector2f spatialAdjustments = evaluateShapeAndSpace(npc, tacticalTarget, ballPos, state, team, opposition, teamAI, zone, ball, pitch, spatialGrid);

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

bool PositioningAI::evaluateSprintUrgency(NPCPlayer& npc, AIUrgency urgency, float distToTarget, float distToBall) {
    if (npc.getBallPossession()) return true;

    float stamRatio = npc.getCurrentStamina() / npc.getMaxStamina();

    if (urgency != AIUrgency::Critical && npc.getCurrentStamina() < 2.0f) return false;

    PositionRole role = npc.getPositionRole();
    bool isAttacker = (role == PositionRole::Striker || role == PositionRole::CenterForward || role == PositionRole::LeftWing || role == PositionRole::RightWing);
    bool isDefender = (role == PositionRole::CenterBack || role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);
    bool isMid = (!isAttacker && !isDefender);

    // ==========================================
    // --- THE FIX: REMOVED HESITATION TIMER ---
    // ==========================================
    // Professional players don't walk when tracking back! (Drastically reduced)
    float hesitationTimer = (100.f - npc.getTacticalFamiliarity()) / 100.f * 1.0f;
    if (hesitationTimer > 0.0f && (rand() % 100) < 5) {
        return false;
    }

    switch (urgency) {
    case AIUrgency::Critical:
        return true;

    case AIUrgency::AttackingRun:
        if (isDefender) return (stamRatio > 0.6f && distToTarget > 1200.f);
        return (stamRatio > 0.3f && distToTarget > 600.f);

    case AIUrgency::Pressing:
        if (isAttacker) return (stamRatio > 0.6f && distToBall < 300.f);
        else return (stamRatio > 0.35f && distToBall < 800.f);

    case AIUrgency::Recovery:
    default:
        // Defenders and Midfielders MUST hustle to cover shape
        if (isDefender) {
            return (stamRatio > 0.15f && distToTarget > 150.f);
        }
        else if (isMid) {
            return (stamRatio > 0.20f && distToTarget > 200.f);
        }
        return (stamRatio > 0.4f && distToTarget > 800.f);
    }
}

sf::Vector2f PositioningAI::calculateInterceptionPoint(NPCPlayer& npc, Ball& ball, const Pitch& pitch) {
    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f ballVel = ball.getVelocity();
    float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);

    if (ballSpeed < 30.f && ball.z <= 40.f) return ballPos;

    sf::Vector2f npcPos = npc.getPosition();
    sf::Vector2f npcVel = npc.getVelocity();
    float npcSpeedSq = npcVel.x * npcVel.x + npcVel.y * npcVel.y;
    float turnPenalty = 0.f;

    if (npcSpeedSq > 400.f) {
        sf::Vector2f npcDir = PlayerAI::normalize(npcVel);
        sf::Vector2f targetDir = PlayerAI::normalize(ballPos - npcPos);
        float dot = npcDir.x * targetDir.x + npcDir.y * targetDir.y;

        if (dot < 0.4f) {
            float agilityFactor = 2.0f - (npc.getAgility() / 100.f);
            turnPenalty = ((0.4f - dot) / 1.4f) * 1.2f * agilityFactor;
        }
    }

    bool isTeammatePass = (!ball.hasOwner() && ball.getLastOwner() && ball.getLastOwner()->getTeam() == npc.getTeam());
    float awarenessNorm = npc.getAwareness() / 100.f;
    float errorSeverity = isTeammatePass ? 0.0f : std::clamp(1.0f - awarenessNorm, 0.f, 1.f);
    float perceivedGravity = 980.f * (1.0f + (errorSeverity * 0.2f));

    float dtSim = 0.05f;
    float maxTime = 4.0f;

    sf::Vector2f simPos = ballPos;
    sf::Vector2f simVel = ballVel;
    float simZ = ball.z, simVz = ball.vz, simSpin = ball.spin, simBs = ball.bs;
    float avgRunSpeed = npc.getTopSpeed() * 8.5f + 1.f;

    float minX = pitch.margin, maxX = pitch.totalWidth - pitch.margin;
    float minY = pitch.margin, maxY = pitch.totalHeight - pitch.margin;

    // THE FIX 1: Track if the ball is a high pass!
    bool isAerialBall = (ball.z > 160.f || ball.vz > 100.f);

    for (float t = 0.f; t < maxTime; t += dtSim) {
        float speed = std::sqrt(simVel.x * simVel.x + simVel.y * simVel.y);
        float trueSpeed = std::sqrt((speed * speed) + (simVz * simVz));

        if (speed > 50.f && std::abs(simSpin) > 0.1f) {
            sf::Vector2f perp(-simVel.y / speed, simVel.x / speed);
            float grip = (simZ <= 5.f) ? 1.35f : 1.0f;
            float spinForce = 15.0f * (1.0f - std::clamp(simZ / 400.f, 0.f, 1.f)) * std::clamp(speed / 1000.f, 0.2f, 1.f) * grip;
            simVel += perp * simSpin * spinForce * dtSim;
            float newSpd = std::sqrt(simVel.x * simVel.x + simVel.y * simVel.y);
            simVel = (simVel / newSpd) * speed;
        }

        if (simZ > 0.f && trueSpeed > 5.f) {
            float Cd = 0.25f - (0.13f * std::clamp((trueSpeed - 1200.f) / 600.f, 0.f, 1.f));
            float newTs = std::max(0.f, trueSpeed - ((trueSpeed * trueSpeed) * Cd * 0.0003f) * dtSim);
            if (trueSpeed > 0.1f) {
                speed *= (newTs / trueSpeed);
                float cLen = std::sqrt(simVel.x * simVel.x + simVel.y * simVel.y);
                if (cLen > 0.1f) simVel = (simVel / cLen) * speed;
            }
        }

        if (simZ > 0.f || simVz != 0.f) {
            simVz -= perceivedGravity * dtSim;
            simZ += simVz * dtSim;
            if (simZ < 0.f) {
                simZ = 0.f; simSpin = 0.f;
                if (simBs > 20.f && speed > 100.f) {
                    simVel *= (1.0f - ((simBs / 100.f) * 0.3f));
                    simVz = -simVz * (0.05f + (simBs / 200.f));
                }
                else {
                    simVz = -simVz * 0.35f; simVel *= 0.8f;
                }
                if (simVz < 15.f) simVz = 0.f;
            }
        }

        float currentSpinFric = (simZ <= 0.f) ? 35.0f : 0.5f;
        if (simSpin > 0) simSpin = std::max(0.f, simSpin - currentSpinFric * dtSim);
        else if (simSpin < 0) simSpin = std::min(0.f, simSpin + currentSpinFric * dtSim);

        if (simZ <= 0.f && speed > 0.f) {
            speed = std::max(0.f, speed - ball.friction * dtSim);
            if (speed > 0.f) simVel = (simVel / std::sqrt(simVel.x * simVel.x + simVel.y * simVel.y)) * speed;
            else simVel = { 0.f, 0.f };
        }
        simPos += simVel * dtSim;

        if (simPos.x < minX || simPos.x > maxX || simPos.y < minY || simPos.y > maxY) {
            simPos.x = std::clamp(simPos.x, minX, maxX);
            simPos.y = std::clamp(simPos.y, minY, maxY);
            return simPos;
        }

        bool inOwnBox = (simPos.x < minX + 1650.f) && (std::abs(simPos.y - 3500.f) < 2016.f);
        bool inOppBox = (simPos.x > maxX - 1650.f) && (std::abs(simPos.y - 3500.f) < 2016.f);
        float interceptZ = (inOwnBox || inOppBox) ? 160.f : 35.f;

        if (simZ <= interceptZ && simVz <= 50.f) {
            float distToSim = PlayerAI::dist(npcPos, simPos);
            float availableRunTime = t - turnPenalty;
            float maxReach = (availableRunTime > 0.f ? availableRunTime * avgRunSpeed : 0.f) + 60.f;

            if (maxReach >= distToSim) {
                if (errorSeverity > 0.f && speed > 100.f) {
                    sf::Vector2f errDir(-simVel.y, simVel.x);
                    float errLen = std::sqrt(errDir.x * errDir.x + errDir.y * errDir.y);
                    if (errLen > 0.1f) {
                        float side = (rand() % 2 == 0) ? 1.f : -1.f;
                        simPos += (errDir / errLen) * side * (errorSeverity * 150.f);
                    }
                }
                return simPos;
            }

            // THE FIX 2: THE DROP ZONE OVERRIDE
            // If the ball is dropping out of the sky, this exact spot IS the drop zone.
            // We return it immediately so the player sprints here, instead of letting it bounce.
            if (isAerialBall) {
                return simPos;
            }
        }
        if (simZ == 0.f && speed < 5.f) break;
    }

    simPos.x = std::clamp(simPos.x, minX, maxX);
    simPos.y = std::clamp(simPos.y, minY, maxY);
    return simPos;
}

sf::Vector2f PositioningAI::calculateSeparation(NPCPlayer& npc, const std::vector<Player*>& team, const std::vector<Player*>& opponents, sf::Vector2f ballPos, const TeamAI& teamAI) {
    sf::Vector2f force(0.f, 0.f);
    sf::Vector2f npcPos = npc.getPosition();
    Player* myTarget = PlayerAI::findNearestOpponent(npcPos, opponents);

    PlayerBehavior behavior = npc.getPlaystyle().behavior;
    float widthMultiplier = 0.5f + teamAI.getAttackingWidthPref();
    float freedomMultiplier = 0.8f + (teamAI.getPositionalFreedomPref() * 0.4f);
    float movementActivity = 0.7f + (behavior.runFrequency * 0.6f);

    for (auto& teammate : team) {
        if (teammate == &npc) continue;
        sf::Vector2f teamPos = teammate->getPosition();
        sf::Vector2f diff = npcPos - teamPos;
        float distSq = diff.x * diff.x + diff.y * diff.y;

        Player* teammateTarget = PlayerAI::findNearestOpponent(teamPos, opponents);
        bool markingSameGuy = (myTarget != nullptr && myTarget == teammateTarget);

        // THE FIX 2: EXTREME ANTI-CRAMPING
        // Base bubble increased to 800px (8m) if marking same guy, 1400px (14m) otherwise!
        float baseBubble = markingSameGuy ? 800.f : 1400.f;
        float currentBubble = baseBubble * widthMultiplier * freedomMultiplier * movementActivity;

        if (distSq < currentBubble * currentBubble && distSq > 0.1f) {
            float d = std::sqrt(distSq);
            float overlap = (currentBubble - d) / currentBubble;
            // Multiply the final outward push by 2.5 so they violently snap away from each other
            force += (diff / d) * (overlap * overlap) * 2.5f;
        }
    }
    return force;
}

bool PositioningAI::shouldEmergencyChase(NPCPlayer& npc, Player* firstResponder, float distToBall, const Pitch& pitch, Ball& ball, MatchState matchstate, const TeamAI& teamAI) {
    Player* owner = ball.getOwner();
    if ((owner != nullptr && owner->getTeam() != npc.getTeam() && owner->getPositionRole() == PositionRole::Goalkeeper) ||
        matchstate != MatchState::InPlay) {
        return false;
    }

    bool isHomeTeam = npc.getTeam() == Team::Home;
    sf::Vector2f ballPos = ball.getPosition();
    bool inDangerZone = isHomeTeam ? (ballPos.x < pitch.margin + 3000.f) : (ballPos.x > pitch.totalWidth - pitch.margin - 3000.f);

    PositionRole role = npc.getPositionRole();
    bool isAttacker = (role == PositionRole::Striker || role == PositionRole::CenterForward || role == PositionRole::LeftWing || role == PositionRole::RightWing);
    bool isDefender = (role == PositionRole::CenterBack || role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);
    bool isMid = (!isAttacker && !isDefender);

    // ==========================================
    // --- THE FIX: DEFENSIVE EMERGENCY OVERRIDE ---
    // ==========================================
    // If the ball is loose in our defensive third, EVERY nearby defender chases it, overriding the 'firstResponder' lock!
    bool isDefensiveEmergency = (owner == nullptr && isDefender && inDangerZone && distToBall < 2000.f);

    if (&npc != firstResponder && !isDefensiveEmergency) return false;

    if (owner == nullptr) return true; // Loose ball override

    float depthPref = teamAI.getDefensiveDepthPref();
    float pressPref = teamAI.getPressingIntensityPref();

    PlayerBehavior behavior = npc.getPlaystyle().behavior;

    float basePressRadius = 400.f;
    basePressRadius += (pressPref * 800.f);
    basePressRadius += (npc.getAggression() / 100.f * 300.f);
    basePressRadius += (behavior.tackleAggression * 400.f);

    if (isAttacker) basePressRadius *= 0.7f;
    if (isDefender) {
        bool ballInOwnHalf = isHomeTeam ? (ball.getPosition().x < 5000.f) : (ball.getPosition().x > 5000.f);

        if (!ballInOwnHalf) {
            basePressRadius *= 0.4f;
            basePressRadius *= (1.0f - (depthPref * 0.8f));
        }
        else {
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

sf::Vector2f PositioningAI::calculateMarkingPosition(NPCPlayer& npc, Player* threat, sf::Vector2f goalPos, const TacticalZone& zone, const TeamAI& teamAI) {
    sf::Vector2f threatPos = threat->getPosition();
    sf::Vector2f threatVel = threat->getVelocity();
    float threatSpeed = std::sqrt(threatVel.x * threatVel.x + threatVel.y * threatVel.y);

    float aggressionNorm = npc.getAggression() / 100.0f;
    float awarenessNorm = npc.getAwareness() / 100.0f;
    sf::Vector2f toGoal = PlayerAI::normalize(goalPos - threatPos);
    float pressPref = teamAI.getPressingIntensityPref();

    PlayerBehavior behavior = npc.getPlaystyle().behavior;

    if (threat->getBallPossession()) {
        // ==========================================
        // --- THE FIX 1: TIGHTER JOCKEY BUFFER ---
        // ==========================================
        // Shrunk the base buffer from 300px down to 220px.
        // High aggression/pressing teams will now stand practically on top of the attacker (down to 20px gap!).
        float jockeyBuffer = 220.f - (aggressionNorm * 80.f) - (pressPref * 100.f) - (behavior.tackleAggression * 80.f);
        jockeyBuffer = std::max(20.f, jockeyBuffer);

        float dxToGoal = std::abs(npc.getPosition().x - goalPos.x);
        float dyToGoal = std::abs(npc.getPosition().y - goalPos.y);
        bool inOwnBox = (dxToGoal < 1650.f && dyToGoal < 2050.f);

        sf::Vector2f toThreat = threatPos - npc.getPosition();
        float distToThreat = std::sqrt(toThreat.x * toThreat.x + toThreat.y * toThreat.y);

        sf::Vector2f threatDir = (threatSpeed > 10.f) ? (threatVel / threatSpeed) : sf::Vector2f(0.f, 0.f);
        sf::Vector2f npcDir = (distToThreat > 0.1f) ? (toThreat / distToThreat) : sf::Vector2f(0.f, 0.f);

        float approachAngle = (npcDir.x * threatDir.x) + (npcDir.y * threatDir.y);
        bool isTacklingFromBehind = (approachAngle > 0.4f);
        bool isCertain = !isTacklingFromBehind && (awarenessNorm > 0.4f);

        // ==========================================
        // --- THE FIX 2: AGGRESSIVE BACKPEDAL LIMIT ---
        // ==========================================
        float speedThreat = (threatSpeed / 1000.f);

        // Reduced the speed penalty from 500px to 300px so defenders don't drop too deep against pace!
        float dynamicBuffer = jockeyBuffer + (speedThreat * 300.f);

        // Increased the "fear" threshold. They will only backpedal if the attacker is going > 650px/s.
        if (inOwnBox || !isCertain || threatSpeed > 650.f) {
            float safeBuffer = inOwnBox ? std::max(80.f, dynamicBuffer) : std::max(60.f, dynamicBuffer);

            // Reduced the look-ahead from 0.5s to 0.3s so the defender stays tighter to the current position.
            sf::Vector2f predictedThreatPos = threatPos + (threatVel * 0.3f);
            sf::Vector2f predictedToGoal = PlayerAI::normalize(goalPos - predictedThreatPos);

            return predictedThreatPos + (predictedToGoal * safeBuffer);
        }

        // ==========================================
        // --- THE FIX 3: THE COMMITMENT TO TACKLE ---
        // ==========================================
        // If we aren't backpedaling, we are stepping in!
        sf::Vector2f baseDefendPos = threatPos + (toGoal * dynamicBuffer);

        // Increased the tackle lead from 0.3 to 0.45. The defender will actively try to intercept the run!
        sf::Vector2f tacticalTarget = baseDefendPos + (threatVel * 0.45f);

        // Increased the "dive in" radius from 80px to 140px! 
        // This forces the defender to bite and attempt the tackle much earlier.
        float tackleRadius = 140.f + ((1.0f - awarenessNorm) * 60.f) + (behavior.tackleAggression * 50.f);

        if (PlayerAI::dist(npc.getPosition(), threatPos) < tackleRadius && npc.canTackle()) {

            // If they are strong and aware, they step directly in front of the attacker (Body Check)
            if (awarenessNorm > 0.5f && (npc.getBodyStrength() / 100.f) > 0.6f) {
                sf::Vector2f diveDir = (threatSpeed > 10.f) ? threatDir : toGoal;
                // Cut off the exact vector the attacker is running on
                return threatPos + (diveDir * 120.f);
            }
            else {
                // Otherwise they just lunge straight at the ball
                return threatPos + (toGoal * (120.f - (aggressionNorm * 40.f)));
            }
        }
        return tacticalTarget;
    }
    else {
        // Off-ball marking: Drop off faster players to avoid getting beaten over the top
        float offBallGap = std::max(350.f - (aggressionNorm * 150.f) - (pressPref * 200.f) - (behavior.tackleAggression * 100.f), 30.f);

        // Reduced the pace buffer from 0.4 to 0.25 so defenders mark tight to wingers!
        offBallGap += (threatSpeed * 0.25f);

        return threatPos + (threatVel * (awarenessNorm * 0.4f)) + (toGoal * offBallGap);
    }
}

sf::Vector2f PositioningAI::clampToTacticalZone(sf::Vector2f target, sf::Vector2f homePos, const TacticalZone& zone, float distToBall, bool isHomeSide, const TeamAI& teamAI) {
    // ==========================================
    // --- THE FIX: STRICTER SHAPE DISCIPLINE ---
    // ==========================================
    // Cut the stretch multiplier down. Even highly fluid teams shouldn't see
    // left-backs wandering into the right-wing position.
    float stretch = teamAI.getPositionalFreedomPref() * 500.f * (0.2f + zone.roamingFreedom);

    float minX = (isHomeSide ? homePos.x - zone.backwardLeash : homePos.x - zone.forwardLeash) - stretch;
    float maxX = (isHomeSide ? homePos.x + zone.forwardLeash : homePos.x + zone.backwardLeash) + stretch;
    float minY = (homePos.y - zone.lateralLeash) - stretch;
    float maxY = (homePos.y + zone.lateralLeash) + stretch;

    // Tighten the convergence radius when the ball gets close so they don't break the line
    if (distToBall < 600.f) {
        minX -= 200.f; maxX += 200.f;
        minY -= 150.f; maxY += 150.f;
    }

    target.x = std::clamp(target.x, minX, maxX);
    target.y = std::clamp(target.y, minY, maxY);
    return target;
}

bool PositioningAI::shouldRecoverPosition(const sf::Vector2f& npcPos, const sf::Vector2f& homePos, const TacticalZone& zone, float distToBall, bool isHomeSide, const TeamAI& teamAI) {
    float currentDX = std::abs(npcPos.x - homePos.x);
    float currentDY = std::abs(npcPos.y - homePos.y);

    bool isAheadOfHome = isHomeSide ? (npcPos.x > homePos.x) : (npcPos.x < homePos.x);
    float maxReachX = isAheadOfHome ? zone.forwardLeash : zone.backwardLeash;

    // --- DNA INJECTION ---
    // A fluid player (high roamingFreedom) won't instantly panic and recover if they step out of bounds.
    float freedomTolerance = teamAI.getPositionalFreedomPref() * 800.f * (0.3f + zone.roamingFreedom);

    return ((currentDX > maxReachX + 200.f + freedomTolerance || currentDY > zone.lateralLeash + 200.f + freedomTolerance) && distToBall > 400.f);
}

Player* PositioningAI::identifyTargetReceiver(Ball& ball, const std::vector<Player*>& team, const Pitch& pitch) {
    if (ball.hasOwner() || ball.getLastOwner() == nullptr) return nullptr;

    sf::Vector2f ballVel = ball.getVelocity();
    float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);
    if (ballSpeed < 30.f) return nullptr;

    struct ReceiverProfile {
        Player* ptr;
        float sprintSpeed;
        float turnPenalty;
        float reactionDelay;
    };
    std::vector<ReceiverProfile> activeRunners;

    for (Player* p : team) {
        if (p == ball.getLastOwner() || p->getPositionRole() == PositionRole::Goalkeeper || p->isSentOff()) continue;

        ReceiverProfile profile;
        profile.ptr = p;
        profile.sprintSpeed = p->getTopSpeed() * 8.5f + 1.f;

        sf::Vector2f pVel = p->getVelocity();
        float pSpeedSq = pVel.x * pVel.x + pVel.y * pVel.y;
        profile.turnPenalty = 0.f;

        if (pSpeedSq > 400.f) {
            sf::Vector2f pDir = PlayerAI::normalize(pVel);
            sf::Vector2f toBallDir = PlayerAI::normalize(ball.getPosition() - p->getPosition());
            float dot = pDir.x * toBallDir.x + pDir.y * toBallDir.y;

            if (dot < 0.4f) {
                float agilityFactor = 2.0f - (p->getAgility() / 100.f);
                profile.turnPenalty = ((0.4f - dot) / 1.4f) * 1.2f * agilityFactor;
            }
        }

        profile.reactionDelay = (100.f - p->getTeamChemistry()) / 100.f * 0.40f;
        activeRunners.push_back(profile);
    }

    float dtSim = 0.05f;
    float maxSimTime = 4.0f;

    sf::Vector2f simPos = ball.getPosition();
    sf::Vector2f simVel = ballVel;
    float simZ = ball.z, simVz = ball.vz, simSpin = ball.spin, simBs = ball.bs;

    float minX = pitch.margin, maxX = pitch.totalWidth - pitch.margin;
    float minY = pitch.margin, maxY = pitch.totalHeight - pitch.margin;

    for (float t = 0.f; t < maxSimTime; t += dtSim) {
        float speed = std::sqrt(simVel.x * simVel.x + simVel.y * simVel.y);
        float trueSpeed = std::sqrt((speed * speed) + (simVz * simVz));

        if (speed > 50.f && std::abs(simSpin) > 0.1f) {
            sf::Vector2f perp(-simVel.y / speed, simVel.x / speed);
            float grip = (simZ <= 5.f) ? 1.35f : 1.0f;
            float spinForce = 15.0f * (1.0f - std::clamp(simZ / 400.f, 0.f, 1.f)) * std::clamp(speed / 1000.f, 0.2f, 1.f) * grip;
            simVel += perp * simSpin * spinForce * dtSim;
            float newSpd = std::sqrt(simVel.x * simVel.x + simVel.y * simVel.y);
            simVel = (simVel / newSpd) * speed;
        }

        if (simZ > 0.f && trueSpeed > 5.f) {
            float Cd = 0.25f - (0.13f * std::clamp((trueSpeed - 1200.f) / 600.f, 0.f, 1.f));
            float newTs = std::max(0.f, trueSpeed - ((trueSpeed * trueSpeed) * Cd * 0.0003f) * dtSim);
            if (trueSpeed > 0.1f) {
                speed *= (newTs / trueSpeed);
                float cLen = std::sqrt(simVel.x * simVel.x + simVel.y * simVel.y);
                if (cLen > 0.1f) simVel = (simVel / cLen) * speed;
            }
        }

        if (simZ > 0.f || simVz != 0.f) {
            simVz -= 980.f * dtSim;
            simZ += simVz * dtSim;
            if (simZ < 0.f) {
                simZ = 0.f; simSpin = 0.f;
                if (simBs > 20.f && speed > 100.f) {
                    simVel *= (1.0f - ((simBs / 100.f) * 0.3f));
                    simVz = -simVz * (0.05f + (simBs / 200.f));
                }
                else {
                    simVz = -simVz * 0.35f; simVel *= 0.8f;
                }
                if (simVz < 15.f) simVz = 0.f;
            }
        }

        float currentSpinFric = (simZ <= 0.f) ? 35.0f : 0.5f;
        if (simSpin > 0) simSpin = std::max(0.f, simSpin - currentSpinFric * dtSim);
        else if (simSpin < 0) simSpin = std::min(0.f, simSpin + currentSpinFric * dtSim);

        if (simZ <= 0.f && speed > 0.f) {
            speed = std::max(0.f, speed - ball.friction * dtSim);
            if (speed > 0.f) simVel = (simVel / std::sqrt(simVel.x * simVel.x + simVel.y * simVel.y)) * speed;
            else simVel = { 0.f, 0.f };
        }
        simPos += simVel * dtSim;

        if (simPos.x < minX || simPos.x > maxX || simPos.y < minY || simPos.y > maxY) return nullptr;

        bool inOwnBox = (simPos.x < minX + 1650.f) && (std::abs(simPos.y - 3500.f) < 2016.f);
        bool inOppBox = (simPos.x > maxX - 1650.f) && (std::abs(simPos.y - 3500.f) < 2016.f);
        float interceptZ = (inOwnBox || inOppBox) ? 160.f : 35.f;

        if (simZ <= interceptZ && simVz <= 50.f) {
            Player* bestCandidate = nullptr;
            // THE FIX: We start with a deeply negative margin so SOMEONE is always picked for the drop zone!
            float bestMargin = -99999.f;

            for (const auto& profile : activeRunners) {
                float distToSim = PlayerAI::dist(profile.ptr->getPosition(), simPos);
                float availableRunTime = t - profile.reactionDelay - profile.turnPenalty;

                float maxReach = (availableRunTime > 0.f ? availableRunTime * profile.sprintSpeed : 0.f) + 60.f;
                float timeMargin = maxReach - distToSim;

                sf::Vector2f passOriginToSim = simPos - ball.getPosition();
                float passLen = std::sqrt(passOriginToSim.x * passOriginToSim.x + passOriginToSim.y * passOriginToSim.y);

                if (passLen > 50.f) {
                    sf::Vector2f passDir = passOriginToSim / passLen;
                    sf::Vector2f toPlayerOrigin = profile.ptr->getPosition() - ball.getPosition();
                    float dot = toPlayerOrigin.x * passDir.x + toPlayerOrigin.y * passDir.y;

                    if (dot > 0.f && timeMargin > bestMargin) {
                        bestMargin = timeMargin;
                        bestCandidate = profile.ptr;
                    }
                }
                else {
                    if (timeMargin > bestMargin) {
                        bestMargin = timeMargin;
                        bestCandidate = profile.ptr;
                    }
                }
            }

            // The exact moment the ball drops, whoever is closest to arriving on time is the receiver.
            if (bestCandidate != nullptr) return bestCandidate;
        }

        if (simZ == 0.f && speed < 5.f) {
            Player* closest = nullptr;
            float minDist = 99999.f;
            for (const auto& profile : activeRunners) {
                float d = PlayerAI::dist(profile.ptr->getPosition(), simPos);
                if (d < minDist) {
                    minDist = d;
                    closest = profile.ptr;
                }
            }
            return closest;
        }
    }
    return nullptr;
}
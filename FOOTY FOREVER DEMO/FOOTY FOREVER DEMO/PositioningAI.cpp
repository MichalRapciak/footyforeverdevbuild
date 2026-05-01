#include "PositioningAI.h"
#include "SpatialGrid.h"
#include "PlayerAI.h"

// Main Positioning

sf::Vector2f PositioningAI::decideTargetPosition(NPCPlayer& npc, Ball& ball, const Pitch& pitch, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, Player* firstResponder, PositioningMask mask, const TeamAI& teamAI, const SpatialGrid& spatialGrid)
{
    if (mask.useManualTarget) return mask.manualTarget;

    bool isTeammatePass = (!ball.hasOwner() && ball.getLastOwner() && ball.getLastOwner()->getTeam() == npc.getTeam());
    if (isTeammatePass && &npc == firstResponder) {
        return calculateInterceptionPoint(npc, ball, pitch);
    }

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
        if (npcPos.y < goalCenterY) finalTarget.y = goalCenterY - goalHalfWidth - 100.f;
        else finalTarget.y = goalCenterY + goalHalfWidth + 100.f;
    }
    else {
        bool targetingHomeGoal = (finalTarget.x < pitch.margin + 50.f && std::abs(finalTarget.y - goalCenterY) < goalHalfWidth);
        bool targetingAwayGoal = (finalTarget.x > pitch.totalWidth - pitch.margin - 50.f && std::abs(finalTarget.y - goalCenterY) < goalHalfWidth);

        if (targetingHomeGoal) finalTarget.x = pitch.margin + 50.f;
        if (targetingAwayGoal) finalTarget.x = pitch.totalWidth - pitch.margin - 50.f;
    }

    // ==========================================
    // --- THE FIX: GOAL NET REPULSION ---
    // ==========================================
    // If the final calculated target puts them INSIDE the goal net, force the target back out!
    // The goal depth is ~225px. We'll use 250px to give them a safe boundary.
    float homeGoalDepthX = pitch.margin - 250.f;
    float awayGoalDepthX = pitch.totalWidth - pitch.margin + 250.f;

    bool targetInsideHomeNet = (finalTarget.x < pitch.margin && finalTarget.x > homeGoalDepthX && std::abs(finalTarget.y - goalCenterY) < goalHalfWidth);
    bool targetInsideAwayNet = (finalTarget.x > pitch.totalWidth - pitch.margin && finalTarget.x < awayGoalDepthX && std::abs(finalTarget.y - goalCenterY) < goalHalfWidth);

    if (targetInsideHomeNet) {
        // Push the target out to the edge of the 6-yard box
        finalTarget.x = pitch.margin + 150.f;
    }
    else if (targetInsideAwayNet) {
        finalTarget.x = pitch.totalWidth - pitch.margin - 150.f;
    }

    return finalTarget;
}

sf::Vector2f PositioningAI::evaluateShapeAndSpace(NPCPlayer& npc, sf::Vector2f currentTarget, sf::Vector2f ballPos, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, const TeamAI& teamAI, const TacticalZone& zone, Ball& ball, const Pitch& pitch, const SpatialGrid& spatialGrid)
{
    sf::Vector2f spatialCorrection(0.f, 0.f);
    sf::Vector2f npcPos = npc.getPosition();

    // 1. Universal Goalkeeper Repulsion
    if (ball.hasOwner() && ball.getOwner()->getPositionRole() == PositionRole::Goalkeeper && ball.getOwner()->getTeam() == npc.getTeam()) {
        Player* gk = ball.getOwner();
        float distToGk = PlayerAI::dist(npcPos, gk->getPosition());
        if (distToGk < 1200.f && &npc != gk) {
            sf::Vector2f repelDir = PlayerAI::normalize(npcPos - gk->getPosition());
            bool isDefender = (npc.getPositionRole() == PositionRole::CenterBack || npc.getPositionRole() == PositionRole::LeftBack || npc.getPositionRole() == PositionRole::RightBack || npc.getPositionRole() == PositionRole::LeftWingBack || npc.getPositionRole() == PositionRole::RightWingBack);
            if (isDefender) { repelDir.x *= 0.2f; repelDir.y *= 1.8f; }
            else { repelDir.x *= 1.8f; repelDir.y *= 0.2f; }
            spatialCorrection += PlayerAI::normalize(repelDir) * (1200.f - distToGk);
        }
    }

    // 2. Delegate to Phase-Specific Logic
    if (state.phase == MatchPhase::Attacking) {
        spatialCorrection += evaluateAttackingShape(npc, currentTarget, ballPos, state, team, opponents, teamAI, zone, ball, pitch, spatialGrid);
    }
    else {
        float avgLineX = getAverageDefensiveLineX(npc, team);
        spatialCorrection += evaluateDefendingShape(npc, currentTarget, ballPos, state, team, opponents, teamAI, zone, ball, pitch, spatialGrid, avgLineX);
    }

    // 3. Final Magnitude Clamping
    float correctionMagnitude = std::sqrt(spatialCorrection.x * spatialCorrection.x + spatialCorrection.y * spatialCorrection.y);
    float maxAllowedDrift = 300.f + (zone.roamingFreedom * 500.f);
    if (state.phase == MatchPhase::Attacking) {
        maxAllowedDrift += 400.f + ((1.0f - teamAI.getPassingLengthPref()) * 1000.f);
    }

    if (correctionMagnitude > maxAllowedDrift) {
        spatialCorrection = (spatialCorrection / correctionMagnitude) * maxAllowedDrift;
    }

    return spatialCorrection;
}

sf::Vector2f PositioningAI::evaluateAttackingShape(NPCPlayer& npc, sf::Vector2f currentTarget, sf::Vector2f ballPos, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, const TeamAI& teamAI, const TacticalZone& zone, Ball& ball, const Pitch& pitch, const SpatialGrid& spatialGrid) {
    sf::Vector2f correction(0.f, 0.f);
    sf::Vector2f npcPos = npc.getPosition();
    float awareness = npc.getAwareness() / 100.0f;
    bool isHomeSide = (npc.getTeam() == Team::Home);
    float pitchCenterY = pitch.totalHeight / 2.f;
    float halfwayX = pitch.totalWidth / 2.f;
    PositionRole role = npc.getPositionRole();
    bool isMid = (role == PositionRole::CenterMid || role == PositionRole::AttackingMid || role == PositionRole::LeftMid || role == PositionRole::RightMid || role == PositionRole::DefensiveMid);

    if (ball.hasOwner() && ball.getOwner()->getTeam() == npc.getTeam() && ball.getOwner() != &npc) {
        Player* carrier = ball.getOwner();
        float distToCarrier = PlayerAI::dist(npcPos, carrier->getPosition());

        if (distToCarrier > 300.f && distToCarrier < 3000.f) {
            sf::Vector2f bestPocket = spatialGrid.findBestSupportPocket(carrier->getPosition(), npcPos, npc.getTeam(), pitch, state, teamAI.getPassingLengthPref());
            float distToPocket = PlayerAI::dist(npcPos, bestPocket);

            if (distToPocket > 100.f) {
                Player* presser = PlayerAI::findNearestOpponent(carrier->getPosition(), opponents);
                float presserDist = presser ? PlayerAI::dist(carrier->getPosition(), presser->getPosition()) : 9999.f;
                float urgency = (presserDist < 500.f || state.subState == TacticalSubState::KeepPossession) ? 1.8f : 1.0f;
                if (state.subState == TacticalSubState::Transition) urgency = 2.0f;
                urgency += ((1.0f - teamAI.getPassingLengthPref()) * 0.8f);
                correction += PlayerAI::normalize(bestPocket - npcPos) * std::min(distToPocket, 1500.f) * awareness * urgency;
            }
        }

        Player* myMarker = PlayerAI::findNearestOpponent(npcPos, opponents);
        if (myMarker && distToCarrier < 2500.f) {
            float markerDist = PlayerAI::dist(npcPos, myMarker->getPosition());
            if (markerDist < 800.f) {
                sf::Vector2f toBallNorm = PlayerAI::normalize(carrier->getPosition() - npcPos);
                sf::Vector2f toMarkerNorm = PlayerAI::normalize(myMarker->getPosition() - npcPos);
                if ((toBallNorm.x * toMarkerNorm.x + toBallNorm.y * toMarkerNorm.y) > 0.7f) {
                    sf::Vector2f stepDir(-toBallNorm.y, toBallNorm.x);
                    if ((stepDir.x * toMarkerNorm.x + stepDir.y * toMarkerNorm.y) > 0.f) stepDir = -stepDir;
                    float unmarkUrgency = 1.0f + ((1.0f - teamAI.getPassingLengthPref()) * 2.0f);
                    correction += stepDir * 1800.f * awareness * unmarkUrgency;
                }
                else if (markerDist < 250.f) {
                    correction -= toMarkerNorm * 600.f * awareness;
                }
            }
        }
    }

    bool isLB = (role == PositionRole::LeftBack || role == PositionRole::LeftWingBack);
    bool isRB = (role == PositionRole::RightBack || role == PositionRole::RightWingBack);

    if ((isLB || isRB) && state.subState != TacticalSubState::TimeWasting && state.subState != TacticalSubState::KeepPossession) {
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
                correction.x += isHomeSide ? 800.f : -800.f;
                correction.y += isLB ? -1000.f : 1000.f;
            }
        }
    }

    if (role == PositionRole::DefensiveMid || role == PositionRole::CenterMid) {
        for (Player* tm : team) {
            if (tm->getPositionRole() == PositionRole::LeftBack || tm->getPositionRole() == PositionRole::RightBack) {
                if ((isHomeSide ? (tm->getPosition().x > halfwayX) : (tm->getPosition().x < halfwayX)) && PlayerAI::dist(npcPos, tm->getPosition()) < 3000.f) {
                    correction += (tm->getHomePosition() - npcPos) * 0.7f * awareness;
                    break;
                }
            }
        }
    }

    if (role == PositionRole::Striker || role == PositionRole::CenterForward) {
        for (Player* tm : team) {
            if (tm->getPositionRole() == PositionRole::CenterMid || tm->getPositionRole() == PositionRole::AttackingMid) {
                if (std::abs(tm->getPosition().y - pitchCenterY) > 1500.f && PlayerAI::dist(npcPos, tm->getPosition()) < 2000.f) {
                    correction += (isHomeSide ? sf::Vector2f(-1.f, 0.f) : sf::Vector2f(1.f, 0.f)) * 800.f * awareness;
                }
            }
        }
    }

    if (isMid) {
        Player* nearestOpp = PlayerAI::findNearestOpponent(currentTarget, opponents);
        if (nearestOpp && PlayerAI::dist(currentTarget, nearestOpp->getPosition()) < 700.f) {
            float escapeY = ((nearestOpp->getPosition().y - currentTarget.y) > 0) ? -1.0f : 1.0f;
            correction.y += escapeY * 350.f * awareness * zone.roamingFreedom;
        }
    }

    return correction;
}

sf::Vector2f PositioningAI::evaluateDefendingShape(NPCPlayer& npc, sf::Vector2f currentTarget, sf::Vector2f ballPos, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, const TeamAI& teamAI, const TacticalZone& zone, Ball& ball, const Pitch& pitch, const SpatialGrid& spatialGrid, float avgLineX) {
    sf::Vector2f correction(0.f, 0.f);
    float awareness = npc.getAwareness() / 100.0f;
    PositionRole role = npc.getPositionRole();

    bool isMid = (role == PositionRole::CenterMid || role == PositionRole::AttackingMid || role == PositionRole::LeftMid || role == PositionRole::RightMid || role == PositionRole::DefensiveMid);
    bool isForward = (role == PositionRole::Striker || role == PositionRole::CenterForward || role == PositionRole::LeftWing || role == PositionRole::RightWing);
    bool isDefender = (!isForward && !isMid);

    // 1. Base Zonal & Screen Positioning
    if (state.subState != TacticalSubState::Transition && state.subState != TacticalSubState::AllOut) {
        correction += evaluateZonalShifts(npc, ballPos, pitch, zone, spatialGrid, awareness, isMid, isDefender);
    }

    // 2. Dynamic Runner Tracking & Offside Traps
    bool trackingRunner = false;
    bool triggerOffsideTrap = false;
    correction += evaluateRunnerTracking(npc, ballPos, state, opponents, teamAI, zone, avgLineX, awareness, isMid, isDefender, trackingRunner, triggerOffsideTrap);

    // 3. Defensive Line Cohesion & Tight Squeeze
    correction += evaluateLineIntegrity(npc, ballPos, team, zone, pitch, avgLineX, awareness, isMid, isForward, isDefender, trackingRunner, triggerOffsideTrap);

    return correction;
}

sf::Vector2f PositioningAI::evaluateZonalShifts(NPCPlayer& npc, sf::Vector2f ballPos, const Pitch& pitch, const TacticalZone& zone, const SpatialGrid& spatialGrid, float awareness, bool isMid, bool isDefender) {
    sf::Vector2f correction(0.f, 0.f);
    sf::Vector2f npcPos = npc.getPosition();
    bool isHomeSide = (npc.getTeam() == Team::Home);
    float pitchCenterY = pitch.totalHeight / 2.f;
    PositionRole role = npc.getPositionRole();

    float ballDistFromCenterY = std::abs(ballPos.y - pitchCenterY);
    bool inDangerZone = isHomeSide ? (ballPos.x < pitch.margin + 3000.f) : (ballPos.x > pitch.totalWidth - pitch.margin - 3000.f);

    if (inDangerZone && isMid) {
        sf::Vector2f myGoalCenter = isHomeSide ? sf::Vector2f(pitch.margin, pitchCenterY) : sf::Vector2f(pitch.totalWidth - pitch.margin, pitchCenterY);
        sf::Vector2f screenPos = myGoalCenter - (PlayerAI::normalize(myGoalCenter - ballPos) * 1200.f);
        correction += (screenPos - npcPos) * 0.8f * awareness;
    }
    else if (ballDistFromCenterY < 1200.f) {
        if (role == PositionRole::DefensiveMid || role == PositionRole::CenterMid) {
            correction += (isHomeSide ? sf::Vector2f(-1.f, 0.f) : sf::Vector2f(1.f, 0.f)) * 600.f * awareness;
            correction.y += (pitchCenterY - npcPos.y) * 0.6f * awareness;
        }
    }
    else {
        bool ballOnLeft = ballPos.y < pitchCenterY;
        if ((ballOnLeft && (role == PositionRole::LeftMid || role == PositionRole::LeftWing)) ||
            (!ballOnLeft && (role == PositionRole::RightMid || role == PositionRole::RightWing))) {
            correction += (isHomeSide ? sf::Vector2f(-1.f, 0.f) : sf::Vector2f(1.f, 0.f)) * 900.f * awareness;
            correction.y += (ballOnLeft ? -1.0f : 1.0f) * 400.f * awareness;
        }
    }

    if (isDefender || isMid) {
        float sectorRadius = isDefender ? 1200.f : 1600.f;
        sf::Vector2f bestCover = spatialGrid.findBestCoverSpace(npcPos, npc.getHomePosition(), sectorRadius, npc.getTeam(), pitch);
        float distToCover = PlayerAI::dist(npcPos, bestCover);
        if (distToCover > 300.f) {
            correction += PlayerAI::normalize(bestCover - npcPos) * std::min(distToCover, 800.f) * (awareness * (0.4f + (zone.roamingFreedom * 0.4f)));
        }
    }

    return correction;
}

sf::Vector2f PositioningAI::evaluateRunnerTracking(NPCPlayer& npc, sf::Vector2f ballPos, TeamState state, const std::vector<Player*>& opponents, const TeamAI& teamAI, const TacticalZone& zone, float avgLineX, float awareness, bool isMid, bool isDefender, bool& outTrackingRunner, bool& outTriggerTrap) {
    sf::Vector2f correction(0.f, 0.f);
    sf::Vector2f npcPos = npc.getPosition();
    bool isHomeSide = (npc.getTeam() == Team::Home);
    float pitchCenterY = 3500.f;

    outTrackingRunner = false;
    outTriggerTrap = false;

    if (isDefender || (isMid && state.subState == TacticalSubState::Transition)) {
        float myGoalX = isHomeSide ? 600.f : 9400.f; // Example margins
        Player* mostDangerousRunner = nullptr;
        float highestDangerScore = 0.f;
        float depthPref = teamAI.getDefensiveDepthPref();

        for (Player* opp : opponents) {
            if (opp->getPositionRole() == PositionRole::Goalkeeper) continue;
            sf::Vector2f oppVel = opp->getVelocity();
            float oppSpeed = std::sqrt(oppVel.x * oppVel.x + oppVel.y * oppVel.y);

            if (oppSpeed > 500.f) {
                bool runningAtGoal = isHomeSide ? (oppVel.x < -200.f) : (oppVel.x > 200.f);
                float offsideTrapThreshold = (depthPref < 0.4f) ? 20.f : 50.f + (depthPref * 450.f);
                bool isBehindLine = isHomeSide ? (opp->getPosition().x < avgLineX - offsideTrapThreshold) : (opp->getPosition().x > avgLineX + offsideTrapThreshold);

                if (isDefender && depthPref < 0.4f && runningAtGoal) {
                    float distToLine = isHomeSide ? (avgLineX - opp->getPosition().x) : (opp->getPosition().x - avgLineX);
                    if (distToLine > -50.f && distToLine < 800.f) outTriggerTrap = true;
                }

                if (runningAtGoal && !isBehindLine) {
                    float distToDefender = PlayerAI::dist(npcPos, opp->getPosition());
                    float detectRadius = isMid ? 3500.f : 1500.f;
                    if (distToDefender < detectRadius) {
                        float danger = oppSpeed + (detectRadius - distToDefender);
                        if (danger > highestDangerScore) {
                            highestDangerScore = danger;
                            mostDangerousRunner = opp;
                        }
                    }
                }
            }
        }

        // DM Suffocation
        if (npc.getPositionRole() == PositionRole::DefensiveMid && mostDangerousRunner) {
            if (std::abs(mostDangerousRunner->getPosition().y - pitchCenterY) < 1000.f) {
                bool runningAtDefense = isHomeSide ? (mostDangerousRunner->getVelocity().x < -200.f) : (mostDangerousRunner->getVelocity().x > 200.f);
                if (runningAtDefense && highestDangerScore > 400.f) {
                    correction += isHomeSide ? sf::Vector2f(200.f, 0.f) : sf::Vector2f(-200.f, 0.f);
                }
            }
        }

        if (mostDangerousRunner && (awareness + zone.roamingFreedom) > 0.6f && !outTriggerTrap) {
            outTrackingRunner = true;
            sf::Vector2f toGoal = PlayerAI::normalize(sf::Vector2f(myGoalX, 3500.f) - mostDangerousRunner->getPosition());
            sf::Vector2f insideShade(0.f, (mostDangerousRunner->getPosition().y < 3500.f) ? 80.f : -80.f);
            if (std::abs(mostDangerousRunner->getPosition().y - 3500.f) < 500.f) insideShade.y = 0.f;
            sf::Vector2f trackTarget = mostDangerousRunner->getPosition() + insideShade + (mostDangerousRunner->getVelocity() * 0.6f) + (toGoal * 100.f);
            correction += (trackTarget - npcPos) * 0.95f * awareness;
        }
        else if (outTriggerTrap && isDefender && state.subState != TacticalSubState::AllOut) {
            float trapPush = 350.f * awareness * (1.0f - depthPref);
            correction.x += isHomeSide ? trapPush : -trapPush;
        }
    }
    return correction;
}

sf::Vector2f PositioningAI::evaluateLineIntegrity(NPCPlayer& npc, sf::Vector2f ballPos, const std::vector<Player*>& team, const TacticalZone& zone, const Pitch& pitch, float avgLineX, float awareness, bool isMid, bool isForward, bool isDefender, bool trackingRunner, bool triggerOffsideTrap) {
    sf::Vector2f correction(0.f, 0.f);
    sf::Vector2f npcPos = npc.getPosition();
    bool isHomeSide = (npc.getTeam() == Team::Home);

    if (std::abs(avgLineX - npcPos.x) > 50.f && !trackingRunner && !triggerOffsideTrap) {
        float xDiff = avgLineX - npcPos.x;
        float distToBall = PlayerAI::dist(npcPos, ballPos);
        bool isStopper = false;

        if (distToBall < 1800.f) {
            isStopper = true;
            for (Player* tm : team) {
                if (tm == &npc || tm->getPositionRole() == PositionRole::Goalkeeper || tm->isSentOff()) continue;
                PositionRole tr = tm->getPositionRole();
                bool tmIsForward = (tr == PositionRole::Striker || tr == PositionRole::CenterForward || tr == PositionRole::LeftWing || tr == PositionRole::RightWing);
                bool tmIsMid = (tr == PositionRole::CenterMid || tr == PositionRole::AttackingMid || tr == PositionRole::LeftMid || tr == PositionRole::RightMid || tr == PositionRole::DefensiveMid);
                bool tmIsDef = (!tmIsForward && !tmIsMid);

                if (((isDefender && tmIsDef) || (isMid && tmIsMid) || (isForward && tmIsForward)) && PlayerAI::dist(tm->getPosition(), ballPos) < distToBall) {
                    isStopper = false; break;
                }
            }
        }

        // Zonal Engagement Override
        if (isDefender && distToBall < 800.f) {
            isStopper = true;
        }

        // ==========================================
        // --- THE FIX: DEFENSIVE SQUEEZE & COVER ---
        // ==========================================
        float halfwayX = pitch.totalWidth / 2.0f;
        bool ballInOurHalf = isHomeSide ? (ballPos.x < halfwayX) : (ballPos.x > halfwayX);

        if (ballInOurHalf && (isDefender || isMid)) {
            // 1. The Lateral Squeeze (Choke the central space)
            float yPinch = (ballPos.y - npcPos.y);
            float pinchFactor = isDefender ? 0.35f : 0.20f;

            // Squeeze tighter the closer the ball gets to the goal
            float depthRatio = isHomeSide ? (1.0f - (ballPos.x / halfwayX)) : ((ballPos.x - halfwayX) / halfwayX);
            depthRatio = std::clamp(depthRatio, 0.0f, 1.0f);

            float lateralShift = yPinch * pinchFactor * depthRatio * awareness;
            // Prevent defenders from totally abandoning their flank
            lateralShift = std::clamp(lateralShift, -1200.f, 1200.f);
            correction.y += lateralShift;

            // 2. The Defensive Cover Drop (The "V" Shape)
            // If the stopper steps up, the adjacent defenders must drop slightly behind them to provide cover.
            if (!isStopper && isDefender) {
                float coverDrop = isHomeSide ? -250.f : 250.f; // Drop back ~2.5 meters
                correction.x += coverDrop * depthRatio * awareness;
            }
        }

        // 3. Line Snapping (Only if not actively engaging the ball)
        if (!isStopper) {
            float snapStrength = isDefender ? 0.90f : 0.75f;
            correction.x += xDiff * snapStrength * (1.0f - (zone.roamingFreedom * 0.4f)) * awareness;
        }
    }

    return correction;
}

sf::Vector2f PositioningAI::applyTacticalPositioning(NPCPlayer& npc, Ball& ball, sf::Vector2f homePos, sf::Vector2f ballPos, sf::Vector2f goalPos, TeamState state, TacticalZone zone, const Pitch& pitch, const std::vector<Player*>& team, const std::vector<Player*>& opposition, const TeamAI& teamAI, const SpatialGrid& spatialGrid)
{
    float rawProgress = std::clamp(teamAI.getBallProgress(), 0.0f, 1.0f);
    float ballProgress = rawProgress * rawProgress * rawProgress * (rawProgress * (rawProgress * 6.0f - 15.0f) + 10.0f);
    bool isHomeSide = (npc.getTeam() == Team::Home);

    // Calculate Global Extremes
    float enemyOffsideLineX = pitch.totalWidth / 2.f;
    float threatX = isHomeSide ? pitch.totalWidth : 0.f;

    if (!opposition.empty()) {
        std::vector<float> oppX;
        for (Player* opp : opposition) {
            if (opp->getPositionRole() == PositionRole::Goalkeeper || opp->isSentOff()) continue;
            float px = opp->getPosition().x;
            oppX.push_back(px);
            if (isHomeSide) { if (px < threatX) threatX = px; }
            else { if (px > threatX) threatX = px; }
        }
        if (!oppX.empty()) {
            if (isHomeSide) std::sort(oppX.begin(), oppX.end(), std::greater<float>());
            else std::sort(oppX.begin(), oppX.end(), std::less<float>());
            enemyOffsideLineX = oppX[0];
        }
    }

    if (isHomeSide && enemyOffsideLineX < pitch.totalWidth / 2.f) enemyOffsideLineX = pitch.totalWidth / 2.f;
    if (!isHomeSide && enemyOffsideLineX > pitch.totalWidth / 2.f) enemyOffsideLineX = pitch.totalWidth / 2.f;

    sf::Vector2f tacticalTarget = homePos;

    // Delegate to Phase-Specific Targeting
    if (state.phase == MatchPhase::Attacking) {
        tacticalTarget = calculateAttackingTarget(npc, ball, tacticalTarget, ballPos, state, zone, pitch, teamAI, enemyOffsideLineX, ballProgress);
    }
    else {
        tacticalTarget = calculateDefendingTarget(npc, ball, tacticalTarget, ballPos, state, zone, pitch, team, teamAI, ballProgress, threatX, homePos.x);
    }

    // Apply Local Shape Adjustments
    tacticalTarget += evaluateShapeAndSpace(npc, tacticalTarget, ballPos, state, team, opposition, teamAI, zone, ball, pitch, spatialGrid);

    // The Dynamic Rogue Sweeper Clamp
    bool isForward = (npc.getPositionRole() == PositionRole::Striker || npc.getPositionRole() == PositionRole::CenterForward || npc.getPositionRole() == PositionRole::LeftWing || npc.getPositionRole() == PositionRole::RightWing);
    bool isMid = (npc.getPositionRole() == PositionRole::CenterMid || npc.getPositionRole() == PositionRole::AttackingMid || npc.getPositionRole() == PositionRole::LeftMid || npc.getPositionRole() == PositionRole::RightMid || npc.getPositionRole() == PositionRole::DefensiveMid);
    bool isDefender = (!isForward && !isMid);

    if (isDefender && state.subState != TacticalSubState::AllOut) {
        float maxDrop = (teamAI.getDefensiveDepthPref() < 0.35f) ? 10.f : 100.f + (teamAI.getDefensiveDepthPref() * 500.f);
        if (isHomeSide && tacticalTarget.x < homePos.x - maxDrop) tacticalTarget.x = homePos.x - maxDrop;
        else if (!isHomeSide && tacticalTarget.x > homePos.x + maxDrop) tacticalTarget.x = homePos.x + maxDrop;
    }

    // Final Pitch Clamp
    float safeZone = 150.f;
    tacticalTarget.x = std::clamp(tacticalTarget.x, pitch.margin + safeZone, pitch.totalWidth - pitch.margin - safeZone);
    tacticalTarget.y = std::clamp(tacticalTarget.y, pitch.margin + safeZone, pitch.totalHeight - pitch.margin - safeZone);

    return tacticalTarget;
}

sf::Vector2f PositioningAI::calculateAttackingTarget(NPCPlayer& npc, Ball& ball, sf::Vector2f tacticalTarget, sf::Vector2f ballPos, TeamState state, TacticalZone zone, const Pitch& pitch, const TeamAI& teamAI, float enemyOffsideLineX, float ballProgress) {
    bool isHomeSide = (npc.getTeam() == Team::Home);
    float pitchCenterY = pitch.totalHeight / 2.0f;
    float passLengthPref = teamAI.getPassingLengthPref();
    float attackSpeedPref = teamAI.getAttackingSpeedPref();
    PositionRole role = npc.getPositionRole();
    bool isForward = (role == PositionRole::Striker || role == PositionRole::CenterForward || role == PositionRole::LeftWing || role == PositionRole::RightWing);
    bool isMid = (role == PositionRole::CenterMid || role == PositionRole::AttackingMid || role == PositionRole::LeftMid || role == PositionRole::RightMid || role == PositionRole::DefensiveMid);

    if (state.subState == TacticalSubState::TimeWasting) {
        float cornerY = (ballPos.y > pitchCenterY) ? pitch.totalHeight - pitch.margin : pitch.margin;
        float cornerX = isHomeSide ? pitch.totalWidth - pitch.margin : pitch.margin;
        tacticalTarget += (sf::Vector2f(cornerX, cornerY) - tacticalTarget) * 0.4f;
    }

    float depthPush = isHomeSide ? 1.0f : -1.0f;
    float targetBallX = ballPos.x;

    if (ball.hasOwner() && ball.getOwner()->getPositionRole() == PositionRole::Goalkeeper) {
        targetBallX += (isHomeSide ? 1 : -1) * (2500.f + (passLengthPref * 2000.f));
    }

    if ((isForward || role == PositionRole::AttackingMid) && zone.supportDepth > -0.3f) {
        targetBallX += depthPush * (200.f + (attackSpeedPref * 1000.f) + (passLengthPref * 400.f) + (zone.supportDepth * 600.f));
    }
    else if (isMid && zone.supportDepth > -0.3f) {
        if (role == PositionRole::DefensiveMid) {
            targetBallX -= depthPush * (600.f + (passLengthPref * 400.f));
        }
        else {
            targetBallX += depthPush * (150.f + (attackSpeedPref * 700.f) + (passLengthPref * 300.f) + (zone.supportDepth * 500.f));
        }
    }

    tacticalTarget.x += ((targetBallX - tacticalTarget.x) * zone.ballInfluence * 0.6f);

    float unitExpansion = 0.0f;
    float pushFactor = (ballProgress - 0.5f) * 2.0f;

    if (state.subState == TacticalSubState::Transition) {
        unitExpansion = 0.4f + (attackSpeedPref * 0.6f);
        pushFactor = 1.0f + (attackSpeedPref * 0.5f);
    }
    else if (state.subState == TacticalSubState::AllOut) {
        unitExpansion = 1.2f; pushFactor = 2.0f;
    }
    else if (state.subState == TacticalSubState::KeepPossession || state.subState == TacticalSubState::TimeWasting) {
        unitExpansion = std::clamp((ballProgress - 0.2f) * 0.3f, 0.0f, 0.4f);
        pushFactor *= 0.3f;
    }
    else {
        if (isForward) unitExpansion = std::clamp((ballProgress - 0.1f) * (0.8f + (attackSpeedPref * 1.8f)), 0.0f, 1.0f);
        else if (isMid) unitExpansion = std::clamp((ballProgress + 0.10f) * (0.6f + (attackSpeedPref * 1.6f)), 0.15f, 1.0f);
        else unitExpansion = std::clamp((ballProgress - 0.2f) * (0.4f + (attackSpeedPref * 1.2f)), 0.0f, 0.4f + (teamAI.getPositionalFreedomPref() * 0.3f));
    }

    unitExpansion = std::clamp(unitExpansion + ((npc.getPlaystyle().behavior.runFrequency - 0.5f) * 0.4f), 0.0f, 1.0f);
    pushFactor = std::max(0.0f, pushFactor);

    float leashMultiplier = isForward ? 0.5f : (isMid ? 0.3f : 0.15f);
    tacticalTarget.x += depthPush * (zone.forwardLeash * leashMultiplier * unitExpansion);
    tacticalTarget.x += depthPush * (zone.supportDepth * 600.f);

    if (state.subState == TacticalSubState::AllOut && !isForward && !isMid) tacticalTarget.x += depthPush * 1200.f;

    tacticalTarget.y += (tacticalTarget.y - pitchCenterY) * (0.25f * pushFactor);

    if (std::abs(zone.widthPreference) > 0.1f) {
        float dirToTouchline = (tacticalTarget.y > pitchCenterY) ? 1.0f : -1.0f;
        bool isWidePlayer = (role == PositionRole::LeftWing || role == PositionRole::RightWing || role == PositionRole::LeftMid || role == PositionRole::RightMid || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);
        tacticalTarget.y += (dirToTouchline * zone.widthPreference * (isWidePlayer ? 2200.f : 500.f));
    }

    float idealSupportDist = 350.f + (passLengthPref * 400.f) + (attackSpeedPref * 500.f);
    if (state.subState == TacticalSubState::KeepPossession) idealSupportDist *= 0.6f;

    float distToBallTarget = PlayerAI::dist(tacticalTarget, ballPos);
    if (distToBallTarget < idealSupportDist - 150.f) {
        sf::Vector2f repelDir = PlayerAI::normalize(tacticalTarget - ballPos);
        repelDir.x *= 0.3f; repelDir.y *= 1.8f;
        tacticalTarget += PlayerAI::normalize(repelDir) * (idealSupportDist - distToBallTarget) * 0.7f;
    }
    else if (distToBallTarget > idealSupportDist + 300.f && !isForward) {
        tacticalTarget += PlayerAI::normalize(ballPos - tacticalTarget) * (distToBallTarget - idealSupportDist) * passLengthPref * 0.6f;
    }

    float attackingMaxPush = isHomeSide ? (pitch.totalWidth - pitch.margin) - 800.f : pitch.margin + 800.f;
    if (isHomeSide && tacticalTarget.x > attackingMaxPush) tacticalTarget.x = attackingMaxPush;
    else if (!isHomeSide && tacticalTarget.x < attackingMaxPush) tacticalTarget.x = attackingMaxPush;

    if (zone.supportDepth > -0.5f) {
        float awErr = (1.0f - (npc.getAwareness() / 100.0f)) * 300.f;
        if (isHomeSide && tacticalTarget.x > enemyOffsideLineX + awErr) tacticalTarget.x = enemyOffsideLineX + awErr - 50.f;
        else if (!isHomeSide && tacticalTarget.x < enemyOffsideLineX - awErr) tacticalTarget.x = enemyOffsideLineX - awErr + 50.f;
    }

    // Midfield Pocket Adjustment
    if (isMid) {
        float idealDistance = (ballProgress < 0.35f) ? -(500.f + (attackSpeedPref * 900.f) + (passLengthPref * 200.f)) : (600.f + (passLengthPref * 400.f) - (attackSpeedPref * 250.f));
        if (role == PositionRole::AttackingMid) idealDistance = 150.f - (attackSpeedPref * 100.f);
        float pocketX = isHomeSide ? (ballPos.x - idealDistance) : (ballPos.x + idealDistance);
        float pullStrength = 0.4f + (zone.supportDepth * 0.2f) + ((ballProgress < 0.35f) ? 0.3f : 0.0f);
        tacticalTarget.x = (tacticalTarget.x * (1.0f - pullStrength)) + (pocketX * pullStrength);
    }

    return tacticalTarget;
}

sf::Vector2f PositioningAI::calculateDefendingTarget(NPCPlayer& npc, Ball& ball, sf::Vector2f tacticalTarget, sf::Vector2f ballPos, TeamState state, TacticalZone zone, const Pitch& pitch, const std::vector<Player*>& team, const TeamAI& teamAI, float ballProgress, float threatX, float myDefLineX) {
    bool isHomeSide = (npc.getTeam() == Team::Home);
    float pitchCenterY = pitch.totalHeight / 2.0f;
    PositionRole role = npc.getPositionRole();
    bool isForward = (role == PositionRole::Striker || role == PositionRole::CenterForward || role == PositionRole::LeftWing || role == PositionRole::RightWing);
    bool isMid = (role == PositionRole::CenterMid || role == PositionRole::AttackingMid || role == PositionRole::LeftMid || role == PositionRole::RightMid || role == PositionRole::DefensiveMid);
    bool isDefender = (!isForward && !isMid);
    float depthPref = teamAI.getDefensiveDepthPref();
    bool ballIsCentral = std::abs(ballPos.y - pitchCenterY) < 1200.f;
    float distToBallExact = PlayerAI::dist(npc.getPosition(), ballPos);

    if (role == PositionRole::DefensiveMid && state.subState != TacticalSubState::Transition && state.subState != TacticalSubState::AllOut) {
        float shieldDist = 700.f + ((1.0f - depthPref) * 400.f);
        float shieldX = isHomeSide ? myDefLineX + shieldDist : myDefLineX - shieldDist;
        if (std::abs(ballPos.y - pitchCenterY) < 1400.f && distToBallExact < 3000.f) {
            tacticalTarget.x = shieldX; tacticalTarget.y = ballPos.y;
            return tacticalTarget;
        }
    }

    float dropIntensity = 1.0f - ballProgress;
    float dropMultiplier = 0.0f;
    bool isCounterPressing = false;

    if (state.subState == TacticalSubState::Transition) {
        if (teamAI.getPressingIntensityPref() > 0.6f && !isDefender && distToBallExact < 1500.f) {
            isCounterPressing = true; dropIntensity = -1.0f; dropMultiplier = 0.5f + (teamAI.getPressingIntensityPref() * 0.5f);
        }
        else {
            dropIntensity = 1.0f; dropMultiplier = ballIsCentral ? (0.3f - (depthPref * 0.1f)) : (0.8f - (depthPref * 0.4f));
            if (isMid) tacticalTarget.x += isHomeSide ? -1000.f : 1000.f;
        }
    }
    else if (state.subState == TacticalSubState::AllOut) {
        dropIntensity = 1.0f; dropMultiplier = 1.0f;
    }
    else {
        if (isDefender) dropMultiplier = 0.40f + (depthPref * 0.35f);
        else if (isMid) dropMultiplier = 0.45f + (depthPref * 0.35f) + ((role == PositionRole::DefensiveMid) ? 0.15f : 0.f);
        else dropMultiplier = 0.20f + (depthPref * 0.20f);
    }

    tacticalTarget += (isHomeSide ? sf::Vector2f(-1.f, 0.f) : sf::Vector2f(1.f, 0.f)) * (zone.backwardLeash * dropMultiplier * dropIntensity);

    if (isDefender) {
        float buffer = (state.subState == TacticalSubState::AllOut) ? 0.f : 600.f + (depthPref * 800.f);
        if (state.subState == TacticalSubState::Transition && depthPref > 0.6f) buffer = ballIsCentral ? -600.f : -300.f;
        if (isHomeSide && tacticalTarget.x < threatX - buffer) tacticalTarget.x = threatX - buffer;
        else if (!isHomeSide && tacticalTarget.x > threatX + buffer) tacticalTarget.x = threatX + buffer;
    }
    else if (isMid) {
        float midBuffer = (state.subState == TacticalSubState::AllOut) ? 200.f : 800.f;
        float referenceX = isHomeSide ? std::max(threatX, ballPos.x) : std::min(threatX, ballPos.x);
        if (!isCounterPressing) {
            if (isHomeSide && tacticalTarget.x < referenceX + midBuffer) tacticalTarget.x = referenceX + midBuffer;
            else if (!isHomeSide && tacticalTarget.x > referenceX - midBuffer) tacticalTarget.x = referenceX - midBuffer;
        }
    }
    else if (isForward && !isCounterPressing) {
        float outletDepth = (state.subState == TacticalSubState::AllOut) ? -1500.f : 400.f + (depthPref * 800.f);
        float targetX = isHomeSide ? (ballPos.x - outletDepth) : (ballPos.x + outletDepth);
        float minStrikerDepth = isHomeSide ? (myDefLineX + 2500.f) : (myDefLineX - 2500.f);
        tacticalTarget.x = isHomeSide ? std::max(targetX, minStrikerDepth) : std::min(targetX, minStrikerDepth);
    }

    float squeezeFactor = isDefender ? (0.5f + (depthPref * 0.4f)) * dropIntensity : (isMid ? (0.2f + (depthPref * 0.3f)) * dropIntensity : 0.f);
    if (distToBallExact < 2000.f) squeezeFactor *= 2.1f;
    if (state.subState == TacticalSubState::Transition) squeezeFactor *= ballIsCentral ? 4.0f : 2.1f;
    if (state.subState == TacticalSubState::AllOut) squeezeFactor = 0.9f;
    tacticalTarget.y -= (tacticalTarget.y - pitchCenterY) * squeezeFactor;

    float flankRatio = std::abs(ballPos.y - pitchCenterY) / (pitch.totalHeight / 2.f);
    float insideOffset = ballIsCentral ? 0.f : ((ballPos.y < pitchCenterY) ? (200.f + flankRatio * 300.f) : -(200.f + flankRatio * 300.f));
    float desiredYShift = ((ballPos.y + insideOffset) - tacticalTarget.y) * (zone.ballInfluence * 0.85f);

    if (isDefender) {
        bool ballOnLeft = (ballPos.y < pitchCenterY);
        bool amIOnLeft = (npc.getHomePosition().y < pitchCenterY);
        if (ballOnLeft != amIOnLeft && flankRatio > 0.4f) {
            desiredYShift += (amIOnLeft ? (1200.f * flankRatio) : -(1200.f * flankRatio));
        }
    }

    if (isCounterPressing) desiredYShift = (ballPos.y - tacticalTarget.y) * (zone.ballInfluence * 1.5f);
    tacticalTarget.y += std::clamp(desiredYShift, -(800.f + (depthPref * 400.f)), 800.f + (depthPref * 400.f));

    float engageDist = (ballIsCentral && state.subState == TacticalSubState::Transition) ? 600.f : 300.f;
    if ((isDefender || isMid) && distToBallExact < engageDist && state.subState != TacticalSubState::AllOut) {
        bool amIClosestDef = true;
        Player* closestDefPtr = &npc;
        float minDefDist = distToBallExact - ((isHomeSide ? (npc.getPosition().x - myDefLineX) : (myDefLineX - npc.getPosition().x)) > 80.f ? 250.f : 0.f);

        for (Player* tm : team) {
            if (tm == &npc || tm->getPositionRole() == PositionRole::Goalkeeper || tm->isSentOff()) continue;
            PositionRole tr = tm->getPositionRole();
            if (tr == PositionRole::CenterBack || tr == PositionRole::LeftBack || tr == PositionRole::RightBack || tr == PositionRole::LeftWingBack || tr == PositionRole::RightWingBack || tr == PositionRole::DefensiveMid || tr == PositionRole::CenterMid) {
                float tmDist = PlayerAI::dist(tm->getPosition(), ballPos);
                if (((isHomeSide ? (tm->getPosition().x - myDefLineX) : (myDefLineX - tm->getPosition().x)) > 80.f)) tmDist -= 250.f;
                if (tmDist < minDefDist) { amIClosestDef = false; minDefDist = tmDist; closestDefPtr = tm; }
            }
        }

        if (amIClosestDef) {
            sf::Vector2f shadedBallPos = ballPos;

            // ==========================================
            // --- THE FIX 1: KILL THE POLITE SHADE ---
            // ==========================================
            // Only maintain a tactical shade if the ball is far away.
            // If they are within 600px, aim squarely for the ball to block their path!
            if (distToBallExact > 600.f) {
                shadedBallPos.y += (ballPos.y < pitchCenterY) ? 80.f : -80.f;
            }
            if (std::abs(ballPos.y - pitchCenterY) < 400.f) shadedBallPos.y = ballPos.y;

            float defUrgency = 1.0f;
            float penaltyBoxX = isHomeSide ? pitch.margin + 1650.f : pitch.totalWidth - pitch.margin - 1650.f;
            if (std::abs(ballPos.x - penaltyBoxX) < 1200.f) defUrgency = 1.6f;
            if (state.subState == TacticalSubState::Transition && ballIsCentral) defUrgency = 2.0f;

            // ==========================================
            // --- THE FIX 2: THE "CRASH" COMMITMENT ---
            // ==========================================
            // stopperBite dictates how much of the distance they close. 
            float stopperBite = isCounterPressing ? 1.0f : std::min(1.0f, (0.25f + (npc.getAggression() / 100.f * 0.35f)) * defUrgency);

            // If the ball carrier enters tackle range, abandon the base shape completely!
            // Over-commit (1.2x bite) and lead the target's velocity to step IN FRONT of them.
            if (distToBallExact < 500.f) {
                stopperBite = 1.2f;
                shadedBallPos = ballPos + (ball.getVelocity() * 0.15f); // Cut off their run!
            }

            if (std::abs(tacticalTarget.x - myDefLineX) > 800.f && !isCounterPressing && distToBallExact > 500.f) {
                stopperBite *= 0.1f;
            }

            sf::Vector2f biteVector = (shadedBallPos - tacticalTarget) * stopperBite;

            float maxStepOut = ballIsCentral ? 1400.f : 800.f;
            if (isHomeSide && tacticalTarget.x + biteVector.x > myDefLineX + maxStepOut) biteVector.x = (myDefLineX + maxStepOut) - tacticalTarget.x;
            else if (!isHomeSide && tacticalTarget.x + biteVector.x < myDefLineX - maxStepOut) biteVector.x = (myDefLineX - maxStepOut) - tacticalTarget.x;

            tacticalTarget += biteVector;
        }
        else {
            float coverDrop = 0.f;
            if (depthPref > 0.35f) {
                coverDrop = (depthPref - 0.35f) * 400.f;
                if (closestDefPtr) coverDrop += std::clamp((std::abs(closestDefPtr->getPosition().x - myDefLineX) - 150.f) * 0.6f, 0.f, (depthPref - 0.35f) * 500.f);
            }
            else if (closestDefPtr && std::abs(closestDefPtr->getPosition().x - myDefLineX) > 100.f) {
                coverDrop = -150.f;
            }
            tacticalTarget.x += isHomeSide ? -coverDrop : coverDrop;
            float pinchStrength = (role == PositionRole::CenterBack) ? 0.65f : 0.35f;
            if (ballIsCentral && state.subState == TacticalSubState::Transition) pinchStrength *= 2.0f;
            tacticalTarget.y += ((closestDefPtr->getPosition().y - tacticalTarget.y) * std::clamp(pinchStrength, 0.0f, 0.9f));
        }
    }

    float penaltyBoxEdgeX = isHomeSide ? pitch.margin + 1650.f : pitch.totalWidth - pitch.margin - 1650.f;
    if ((isHomeSide ? (ballPos.x > penaltyBoxEdgeX) : (ballPos.x < penaltyBoxEdgeX)) && !isCounterPressing) {
        float lineError = (1.0f - (npc.getAwareness() / 100.0f)) * 150.f;
        if (isDefender) tacticalTarget.x = isHomeSide ? std::max(tacticalTarget.x, penaltyBoxEdgeX - lineError) : std::min(tacticalTarget.x, penaltyBoxEdgeX + lineError);
        else if (isMid) tacticalTarget.x = isHomeSide ? std::max(tacticalTarget.x, penaltyBoxEdgeX + 600.f) : std::min(tacticalTarget.x, penaltyBoxEdgeX - 600.f);
    }

    // Midfield Screen Adjustment
    if (isMid) {
        float screenDist = (state.subState == TacticalSubState::AllOut) ? 300.f : 1000.f - (depthPref * 300.f);
        if (state.subState == TacticalSubState::Transition) screenDist *= 0.5f;
        if (role == PositionRole::DefensiveMid) screenDist = 500.f;
        else if (role == PositionRole::AttackingMid) screenDist = 1600.f;
        float discipline = 0.8f - (zone.roamingFreedom * 0.3f);
        tacticalTarget.x = (tacticalTarget.x * (1.0f - discipline)) + ((isHomeSide ? (myDefLineX + screenDist) : (myDefLineX - screenDist)) * discipline);
    }

    return tacticalTarget;
}

// Smaller Functions

bool PositioningAI::evaluateSprintUrgency(NPCPlayer& npc, AIUrgency urgency, float distToTarget, float distToBall, TeamState state) {
    if (npc.getBallPossession()) return true;

    float stamRatio = npc.getCurrentStamina() / npc.getMaxStamina();

    if (urgency != AIUrgency::Critical && npc.getCurrentStamina() < 2.0f) return false;

    PositionRole role = npc.getPositionRole();
    bool isAttacker = (role == PositionRole::Striker || role == PositionRole::CenterForward || role == PositionRole::LeftWing || role == PositionRole::RightWing);
    bool isDefender = (role == PositionRole::CenterBack || role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);
    bool isMid = (!isAttacker && !isDefender);

    float hesitationTimer = (100.f - npc.getTacticalFamiliarity()) / 100.f * 1.0f;

    if (hesitationTimer > 0.0f && (rand() % 100) < 5) {
        return false;
    }

    if (state.subState == TacticalSubState::Transition) {
        return (stamRatio > 0.15f && distToTarget > 150.f);
    }
    else if (state.subState == TacticalSubState::KeepPossession || state.subState == TacticalSubState::TimeWasting) {
        if (urgency != AIUrgency::Critical) {
            return (stamRatio > 0.5f && distToTarget > 1000.f);
        }
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

    float stamRatio = npc.getCurrentStamina() / npc.getMaxStamina();
    float awarenessNorm = npc.getAwareness() / 100.f;

    float speedMultiplier = 0.7f + (0.3f * stamRatio);
    float absoluteTopSpeed = 450.f + (npc.getTopSpeed() * 4.5f);
    float avgRunSpeed = absoluteTopSpeed * speedMultiplier;

    bool isTeammatePass = (!ball.hasOwner() && ball.getLastOwner() && ball.getLastOwner()->getTeam() == npc.getTeam());

    float errorSeverity = isTeammatePass ? 0.0f : std::clamp(0.25f + ((1.0f - awarenessNorm) * 0.70f), 0.f, 1.f);
    float perceivedGravity = 980.f * (1.0f + (errorSeverity * 0.2f));

    float baseReaction = (1.0f - awarenessNorm) * 0.35f;
    float reactionDelay = isTeammatePass ? baseReaction : baseReaction + 0.15f + (errorSeverity * 0.15f);

    float turnPenalty = 0.f;
    if (npcSpeedSq > 400.f) {
        sf::Vector2f npcDir = PlayerAI::normalize(npcVel);
        sf::Vector2f futureBallRough = ballPos + (ballVel * 0.5f);
        sf::Vector2f targetDir = PlayerAI::normalize(futureBallRough - npcPos);
        float dot = npcDir.x * targetDir.x + npcDir.y * targetDir.y;

        if (dot < 0.4f) {
            float agilityFactor = 2.0f - (npc.getAgility() / 100.f);
            turnPenalty = ((0.4f - dot) / 1.4f) * 1.2f * agilityFactor;
        }
    }

    float dtSim = 0.05f;
    float maxTime = 4.0f;

    sf::Vector2f simPos = ballPos;
    sf::Vector2f simVel = ballVel;
    float simZ = ball.z, simVz = ball.vz, simSpin = ball.spin, simBs = ball.bs;

    float minX = pitch.margin, maxX = pitch.totalWidth - pitch.margin;
    float minY = pitch.margin, maxY = pitch.totalHeight - pitch.margin;

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
            float availableRunTime = t - turnPenalty - reactionDelay;
            float maxReach = (availableRunTime > 0.f ? availableRunTime * avgRunSpeed : 0.f) + 60.f;

            if (maxReach >= distToSim) {
                if (errorSeverity > 0.f && speed > 100.f) {
                    sf::Vector2f errDir(-simVel.y, simVel.x);
                    float errLen = std::sqrt(errDir.x * errDir.x + errDir.y * errDir.y);
                    if (errLen > 0.1f) {
                        float side = (rand() % 2 == 0) ? 1.f : -1.f;
                        simPos += (errDir / errLen) * side * (errorSeverity * 250.f);
                    }
                }
                return simPos;
            }

            if (isAerialBall) {
                float timeDeficit = (distToSim / avgRunSpeed) - availableRunTime;
                if (timeDeficit < 0.4f) {
                    return simPos;
                }
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

        float baseBubble = markingSameGuy ? 800.f : 1400.f;
        float currentBubble = baseBubble * widthMultiplier * freedomMultiplier * movementActivity;

        if (distSq < currentBubble * currentBubble && distSq > 0.1f) {
            float d = std::sqrt(distSq);
            float overlap = (currentBubble - d) / currentBubble;
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

    bool isDefensiveEmergency = (owner == nullptr && isDefender && inDangerZone && distToBall < 2000.f);

    if (&npc != firstResponder && !isDefensiveEmergency) return false;

    if (owner == nullptr) return true;

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

sf::Vector2f PositioningAI::calculateMarkingPosition(NPCPlayer& npc, Player* threat, sf::Vector2f goalPos, const TacticalZone& zone, const TeamAI& teamAI, Ball& ball) {
    sf::Vector2f threatPos = threat->getPosition();
    sf::Vector2f threatVel = threat->getVelocity();
    float threatSpeed = std::sqrt(threatVel.x * threatVel.x + threatVel.y * threatVel.y);

    float aggressionNorm = npc.getAggression() / 100.0f;
    float awarenessNorm = npc.getAwareness() / 100.0f;
    sf::Vector2f toGoal = PlayerAI::normalize(goalPos - threatPos);
    float pressPref = teamAI.getPressingIntensityPref();

    PlayerBehavior behavior = npc.getPlaystyle().behavior;
    bool isHomeSide = npc.getTeam() == Team::Home;
    PositionRole myRole = npc.getPositionRole();

    float pitchCenterY = 3500.f;
    float shadeAmount = 70.f + (awarenessNorm * 60.f);

    bool isFullback = (myRole == PositionRole::LeftBack || myRole == PositionRole::RightBack ||
        myRole == PositionRole::LeftWingBack || myRole == PositionRole::RightWingBack);

    bool isWideThreat = std::abs(threatPos.y - pitchCenterY) > 1200.f;

    if (isFullback && isWideThreat) {
        shadeAmount += 150.f;
    }

    bool rightFooted = (threat->getPreferredFoot() == "Right");

    if (threatPos.y < pitchCenterY && rightFooted) shadeAmount += 80.f;
    else if (threatPos.y > pitchCenterY && !rightFooted) shadeAmount += 80.f;

    sf::Vector2f insideShade(0.f, (threatPos.y < pitchCenterY) ? shadeAmount : -shadeAmount);
    if (std::abs(threatPos.y - pitchCenterY) < 500.f) insideShade.y = 0.f;

    if (threat->getBallPossession()) {
        float baseBuffer = 250.f - (aggressionNorm * 60.f) - (pressPref * 80.f);

        float paceDifference = threatSpeed - (npc.getTopSpeed() * 5.0f);
        if (paceDifference > 0.f) {
            baseBuffer += (paceDifference * 0.5f);
        }

        baseBuffer = std::max(50.f, baseBuffer);

        float dxToGoal = std::abs(npc.getPosition().x - goalPos.x);
        float dyToGoal = std::abs(npc.getPosition().y - goalPos.y);

        bool inOwnBox = (dxToGoal < 1650.f && dyToGoal < 2050.f);

        // Identify if we are in the "Danger Zone" outside the box
        bool inFinalThird = (dxToGoal < 3000.f && dyToGoal < 2500.f);

        sf::Vector2f toThreat = threatPos - npc.getPosition();
        float distToThreat = std::sqrt(toThreat.x * toThreat.x + toThreat.y * toThreat.y);
        sf::Vector2f threatDir = (threatSpeed > 10.f) ? (threatVel / threatSpeed) : sf::Vector2f(0.f, 0.f);

        bool isHeavyTouch = false;
        float ballControlNorm = threat->getBallControl() / 100.f;
        if (threatSpeed > 600.f && ballControlNorm < 0.7f && (rand() % 100 > 80)) {
            isHeavyTouch = true;
        }

        bool mustBackpedal = (!inOwnBox && threatSpeed > 500.f && !isHeavyTouch);

        if (isFullback && isWideThreat) {
            baseBuffer = 20.f;
            mustBackpedal = false;
        }

        // ==========================================
        // --- THE FIX 1: CENTER BACK BRAVERY ---
        // ==========================================
        bool isCenterBack = (myRole == PositionRole::CenterBack);

        // If a CB is in the final third, they stop backpedaling and plant their feet!
        // Running backward into the box just gives the striker a free shot.
        if (isCenterBack && inFinalThird && std::abs(threatPos.y - pitchCenterY) < 1200.f) {
            mustBackpedal = false;
            baseBuffer *= 0.4f; // Crush the gap down so they are breathing on the striker
        }

            if (mustBackpedal) {
                if (paceDifference > 200.f && aggressionNorm > 0.7f && distToThreat < 200.f) {
                    if (npc.canTackle()) return threatPos;
                }
                sf::Vector2f backpedalVec = threatVel * 0.4f;
                return threatPos + insideShade + (toGoal * baseBuffer) + backpedalVec;
            }

        sf::Vector2f baseDefendPos = threatPos + insideShade + (toGoal * baseBuffer);
        sf::Vector2f tacticalTarget = baseDefendPos + (threatVel * 0.35f);

        // ==========================================
        // --- THE FIX 2: TACKLE AGGRESSION ---
        // ==========================================
        // We boost the base tackle radius slightly so defenders lunge in *before* // the attacker is physically clipping into them.
        float tackleRadius = 180.f + (aggressionNorm * 60.f) + (behavior.tackleAggression * 80.f);

        if (isFullback && isWideThreat) tackleRadius += 60.f;
        if (isCenterBack) tackleRadius += 40.f; // CBs lunge earlier to block shots

        if ((isHeavyTouch || distToThreat < tackleRadius) && npc.canTackle()) {
            if (awarenessNorm > 0.6f && (npc.getBodyStrength() / 100.f) > 0.6f) {
                sf::Vector2f diveDir = (threatSpeed > 10.f) ? threatDir : toGoal;
                return threatPos + insideShade + (diveDir * 90.f);
            }
            else {
                return threatPos + insideShade + (toGoal * 60.f);
            }
        }
        return tacticalTarget;
    }
    else {
        float offBallGap = std::max(400.f - (aggressionNorm * 150.f) - (pressPref * 200.f), 50.f);

        if (threatSpeed > 400.f) {
            offBallGap += (threatSpeed * 0.4f);
        }

        if (isFullback && isWideThreat) {
            sf::Vector2f ballPos = ball.getPosition();
            sf::Vector2f ballVel = ball.getVelocity();
            float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);

            bool isBallOnMyFlank = (threatPos.y > pitchCenterY && ballPos.y > pitchCenterY) ||
                (threatPos.y < pitchCenterY && ballPos.y < pitchCenterY);

            bool incomingPass = false;
            if (ballSpeed > 400.f) {
                sf::Vector2f ballToThreat = threatPos - ballPos;
                float distToThreat = std::sqrt(ballToThreat.x * ballToThreat.x + ballToThreat.y * ballToThreat.y);

                if (distToThreat > 0.1f) {
                    float dot = ((ballVel.x / ballSpeed) * (ballToThreat.x / distToThreat)) +
                        ((ballVel.y / ballSpeed) * (ballToThreat.y / distToThreat));

                    if (dot > 0.85f && distToThreat < 2500.f) incomingPass = true;
                }
            }

            if (incomingPass) {
                offBallGap = -100.f;
            }
            else if (isBallOnMyFlank) {
                offBallGap *= 0.1f;
            }
            else {
                offBallGap *= 0.8f;
            }
        }

        if (myRole == PositionRole::DefensiveMid) {
            offBallGap *= 0.5f;
            if (std::abs(threatPos.y - pitchCenterY) < 1000.f) {
                offBallGap = std::max(10.f, offBallGap - 150.f);
                bool runningAtDefense = isHomeSide ? (threatVel.x < -200.f) : (threatVel.x > 200.f);
                if (runningAtDefense && threatSpeed > 400.f) {
                    offBallGap = -80.f;
                }
            }
        }

        float frustration = teamAI.getFrustration();
        if (frustration > 0.0f) {
            offBallGap = std::max(20.f, offBallGap - (frustration * 350.f));
        }

        return threatPos + insideShade + (threatVel * (awarenessNorm * 0.3f)) + (toGoal * offBallGap);
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
        profile.sprintSpeed = 450.f + (p->getTopSpeed() * 4.5f);

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

        profile.reactionDelay = (100.f - p->getTeamChemistry()) / 100.f * 0.80f;
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

float PositioningAI::getAverageDefensiveLineX(NPCPlayer& npc, const std::vector<Player*>& team) {
    float avgLineX = 0.f;
    int lineCount = 0;
    PositionRole myRole = npc.getPositionRole();
    bool isForward = (myRole == PositionRole::Striker || myRole == PositionRole::CenterForward || myRole == PositionRole::LeftWing || myRole == PositionRole::RightWing);
    bool isMid = (myRole == PositionRole::CenterMid || myRole == PositionRole::AttackingMid || myRole == PositionRole::LeftMid || myRole == PositionRole::RightMid || myRole == PositionRole::DefensiveMid);
    bool isDefender = (!isForward && !isMid);

    for (Player* tm : team) {
        if (tm->isSentOff() || tm->getPositionRole() == PositionRole::Goalkeeper) continue;
        PositionRole tmRole = tm->getPositionRole();
        bool tmIsForward = (tmRole == PositionRole::Striker || tmRole == PositionRole::CenterForward || tmRole == PositionRole::LeftWing || tmRole == PositionRole::RightWing);
        bool tmIsMid = (tmRole == PositionRole::CenterMid || tmRole == PositionRole::AttackingMid || tmRole == PositionRole::LeftMid || tmRole == PositionRole::RightMid || tmRole == PositionRole::DefensiveMid);
        bool tmIsDef = (!tmIsForward && !tmIsMid);

        if ((isDefender && tmIsDef) || (isMid && tmIsMid) || (isForward && tmIsForward)) {
            avgLineX += tm->getPosition().x;
            lineCount++;
        }
    }
    return (lineCount > 0) ? (avgLineX / lineCount) : npc.getPosition().x;
}
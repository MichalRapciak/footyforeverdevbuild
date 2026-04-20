#include "PositioningAI.h"
#include "SpatialGrid.h"
#include "PlayerAI.h"

sf::Vector2f PositioningAI::decideTargetPosition(NPCPlayer& npc, Ball& ball, const Pitch& pitch, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, Player* firstResponder, PositioningMask mask, const TeamAI& teamAI, const SpatialGrid& spatialGrid)
{
    if (mask.useManualTarget) return mask.manualTarget;

    // THE FIX: True team-agnostic check so UserPlayer works on Away Team!
    bool isHomeSide = (npc.getTeam() == Team::Home);
    sf::Vector2f ballPos = ball.getPosition();
    sf::Vector2f npcPos = npc.getPosition();
    float distToBall = PlayerAI::dist(npcPos, ballPos);
    sf::Vector2f goalPos = isHomeSide ? sf::Vector2f(pitch.margin, 3500.f) : sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f);

    // ==========================================
    // 1. CALCULATE DYNAMIC ANCHOR
    // ==========================================
    sf::Vector2f dynamicAnchor = npc.getHomePosition();

    PositionRole role = npc.getPositionRole();
    bool isDefender = (role == PositionRole::CenterBack || role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);
    dynamicAnchor.x += teamAI.getDefensiveLineOffset(isDefender);

    TacticalZone zone = teamAI.getEffectiveTacticalZone(npc.getPlaystyle());

    // ==========================================
    // 2. TACTICAL POSITIONING (Relative to Anchor)
    // ==========================================
    // Pass the spatialGrid down into the tactical positioning
    sf::Vector2f target = applyTacticalPositioning(npc, ball, dynamicAnchor, ballPos, goalPos, state, zone, pitch, team, opponents, teamAI, spatialGrid);

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
            target = ballPos + (PlayerAI::normalize(goalPos - ballPos) * pressDistance);
        }
        else {
            target = calculateInterceptionPoint(npc, ball);
        }
        return target;
    }

    // ==========================================
    // 4. CLAMP TO THE DYNAMIC ZONE
    // ==========================================
    return clampToTacticalZone(target, dynamicAnchor, zone, distToBall, isHomeSide, teamAI);
}

sf::Vector2f PositioningAI::evaluateShapeAndSpace(NPCPlayer& npc, sf::Vector2f currentTarget, sf::Vector2f ballPos, TeamState state, const std::vector<Player*>& team, const std::vector<Player*>& opponents, const TeamAI& teamAI, const TacticalZone& zone, Ball& ball, const Pitch& pitch, const SpatialGrid& spatialGrid)
{
    sf::Vector2f spatialCorrection(0.f, 0.f);
    sf::Vector2f npcPos = npc.getPosition();
    float awareness = npc.getAwareness() / 100.0f;

    bool isForward = (npc.getPositionRole() == PositionRole::Striker || npc.getPositionRole() == PositionRole::CenterForward || npc.getPositionRole() == PositionRole::LeftWing || npc.getPositionRole() == PositionRole::RightWing);

    bool isMid = (npc.getPositionRole() == PositionRole::CenterMid || npc.getPositionRole() == PositionRole::AttackingMid || npc.getPositionRole() == PositionRole::LeftMid || npc.getPositionRole() == PositionRole::RightMid ||
        npc.getPositionRole() == PositionRole::DefensiveMid);

    bool isDefender = (npc.getPositionRole() == PositionRole::CenterBack ||
        npc.getPositionRole() == PositionRole::LeftBack ||
        npc.getPositionRole() == PositionRole::RightBack ||
        npc.getPositionRole() == PositionRole::LeftWingBack ||
        npc.getPositionRole() == PositionRole::RightWingBack);


    // ==========================================
    // --- THE FIX 3: GIVE THE KEEPER SPACE ---
    // ==========================================
    if (ball.hasOwner() && ball.getOwner()->getPositionRole() == PositionRole::Goalkeeper && ball.getOwner()->getTeam() == npc.getTeam()) {
        Player* gk = ball.getOwner();
        float distToGk = PlayerAI::dist(npcPos, gk->getPosition());

        // If we are within 12 meters (1200px) of our goalkeeper who has the ball...
        if (distToGk < 1200.f && &npc != gk) {
            // Push violently away from the keeper to open up the passing lane!
            sf::Vector2f repelDir = PlayerAI::normalize(npcPos - gk->getPosition());

            // Defenders fan out laterally (wide) to offer safe short passes.
            // Midfielders push vertically up the pitch.
            if (isDefender) {
                repelDir.x *= 0.2f;
                repelDir.y *= 1.8f;
                repelDir = PlayerAI::normalize(repelDir);
            }
            else {
                repelDir.x *= 1.8f;
                repelDir.y *= 0.2f;
                repelDir = PlayerAI::normalize(repelDir);
            }

            spatialCorrection += repelDir * (1200.f - distToGk);
        }
    }

    if (state == TeamState::Attacking) {
        // ---------------------------------------------------------
        // 1. PASSING TRIANGLES (DNA: Ball Influence & Roaming)
        // ---------------------------------------------------------
        if (zone.ballInfluence > 0.3f) {
            Player* nearestTeammate = nullptr;
            float minDist = 9999.f;

            for (Player* tm : team) {
                if (tm == &npc || tm->getBallPossession()) continue;
                float d = PlayerAI::dist(npcPos, tm->getPosition());
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

                    float d1 = PlayerAI::dist(currentTarget, idealPocket1);
                    float d2 = PlayerAI::dist(currentTarget, idealPocket2);
                    sf::Vector2f bestPocket = (d1 < d2) ? idealPocket1 : idealPocket2;

                    sf::Vector2f steer = bestPocket - currentTarget;
                    float steerLen = std::sqrt(steer.x * steer.x + steer.y * steer.y);
                    if (steerLen > 0.1f) {
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

                if (oppDist < distToBallExact) {
                    float dot = (toOpp.x * dirToBall.x) + (toOpp.y * dirToBall.y);
                    if (dot > 0.f) {
                        sf::Vector2f projection = dirToBall * dot;
                        sf::Vector2f rejection = toOpp - projection;
                        float distFromLane = std::sqrt(rejection.x * rejection.x + rejection.y * rejection.y);

                        if (distFromLane < 200.f) {
                            isInShadow = true;
                            sf::Vector2f slideDir(-dirToBall.y, dirToBall.x);
                            float side = ((slideDir.x * rejection.x + slideDir.y * rejection.y) > 0) ? -1.f : 1.f;
                            float evadeDesire = 600.f * awareness;
                            spatialCorrection += (slideDir * side * evadeDesire) + (dirToBall * evadeDesire * 0.5f);
                            break;
                        }
                    }
                }
            }

            if (!isInShadow && distToBallExact < 2500.f) {
                spatialCorrection *= 0.5f;
            }
        }

        // ---------------------------------------------------------
        // 3. TIKI-TAKA SUPPORT (Show to Feet)
        // ---------------------------------------------------------
        float tikiTakaPref = 1.0 - teamAI.getPassingLengthPref();
        distToBallExact = PlayerAI::dist(npcPos, ballPos);

        if (tikiTakaPref > 0.3f && distToBallExact > 700.f && distToBallExact < 3500.f) {
            bool amIClosest = true;
            for (Player* tm : team) {
                if (tm == &npc || tm->getBallPossession()) continue;
                if (PlayerAI::dist(tm->getPosition(), ballPos) < distToBallExact) {
                    amIClosest = false;
                    break;
                }
            }

            if (amIClosest) {
                sf::Vector2f toBallVec = ballPos - npcPos;
                float shiftMagnitude = distToBallExact * 0.40f;
                float willingness = 0.4f + (zone.roamingFreedom * 0.6f);
                spatialCorrection += PlayerAI::normalize(toBallVec) * shiftMagnitude * tikiTakaPref * willingness * awareness;
            }
        }

        // ---------------------------------------------------------
        // 4. MIDFIELD POCKET FINDING
        // ---------------------------------------------------------
        if (isMid) {
            Player* nearestOpp = PlayerAI::findNearestOpponent(currentTarget, opponents);
            if (nearestOpp && PlayerAI::dist(currentTarget, nearestOpp->getPosition()) < 700.f) {
                sf::Vector2f toOpp = nearestOpp->getPosition() - currentTarget;
                float escapeY = (toOpp.y > 0) ? -1.0f : 1.0f;
                spatialCorrection.y += escapeY * 350.f * awareness * zone.roamingFreedom;
            }
        }

        // ---------------------------------------------------------
        // 5. GRID-BASED SPACE INVADER (Finding Pockets)
        // ---------------------------------------------------------
        if (isForward || isMid) {
            // How far is the player willing to look for empty space?
            float sectorRadius = isForward ? 2500.f : 1800.f;

            // Ask the Grid: Where is the cell with the absolute lowest enemy presence near my tactical target?
            sf::Vector2f bestPocket = spatialGrid.findBestAttackingPocket(npcPos, currentTarget, sectorRadius, npc.getTeam(), pitch);

            float distToPocket = PlayerAI::dist(npcPos, bestPocket);

            if (distToPocket > 200.f) {
                // High awareness players effortlessly glide into these empty pockets
                float freedomPref = teamAI.getPositionalFreedomPref();
                float attackWillingness = awareness * (0.6f + (zone.roamingFreedom * 0.4f) + (freedomPref * 0.4f));

                sf::Vector2f shiftVec = PlayerAI::normalize(bestPocket - npcPos);

                // Actively bend their run into the open grid cell
                spatialCorrection += shiftVec * std::min(distToPocket, 1200.f) * attackWillingness;
            }
        }
    }
    else {
        // ---------------------------------------------------------
        // 3. STRUCTURAL INTEGRITY & GRID GAP FILLING
        // ---------------------------------------------------------
        if (isDefender || isMid) {
            float sectorRadius = isDefender ? 1200.f : 1600.f;
            sf::Vector2f bestCover = spatialGrid.findBestCoverSpace(npcPos, npc.getHomePosition(), sectorRadius, npc.getTeam(), pitch);

            float distToCover = PlayerAI::dist(npcPos, bestCover);
            if (distToCover > 300.f) {
                float coverWillingness = awareness * (0.2f + (zone.roamingFreedom * 0.4f));
                sf::Vector2f shiftVec = PlayerAI::normalize(bestCover - npcPos);
                spatialCorrection += shiftVec * std::min(distToCover, 600.f) * coverWillingness;
            }
        }

        // ==========================================
        // --- THE FIX 2: RUNNER TRACKING OVERRIDE ---
        // ==========================================
        bool trackingRunner = false;
        if (isDefender) {
            bool isHomeSide = npc.getTeam() == Team::Home;
            float myGoalX = isHomeSide ? pitch.margin : pitch.totalWidth - pitch.margin;

            Player* mostDangerousRunner = nullptr;
            float highestDangerScore = 0.f;

            for (Player* opp : opponents) {
                if (opp->getPositionRole() == PositionRole::Goalkeeper || opp == ball.getOwner()) continue;

                sf::Vector2f oppVel = opp->getVelocity();
                float oppSpeed = std::sqrt(oppVel.x * oppVel.x + oppVel.y * oppVel.y);

                // If they are sprinting (> 500px/s)
                if (oppSpeed > 500.f) {
                    // Are they running towards our goal? 
                    // (Home defends X-, so negative velocity is a threat. Away defends X+)
                    bool runningAtGoal = isHomeSide ? (oppVel.x < -200.f) : (oppVel.x > 200.f);

                    if (runningAtGoal) {
                        float distToDefender = PlayerAI::dist(npcPos, opp->getPosition());

                        // If they are within 15 meters of this defender
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

            // High awareness defenders are much more reliable at spotting and tracking runs!
            if (mostDangerousRunner && (awareness + zone.roamingFreedom) > 0.8f) {
                trackingRunner = true;

                // Abandon the zone! Drop and stick to the runner's shoulder!
                sf::Vector2f toGoal = PlayerAI::normalize(sf::Vector2f(myGoalX, 3500.f) - mostDangerousRunner->getPosition());

                // Sit roughly 1 meter (100px) goal-side of the sprinting runner to legally body-check them
                sf::Vector2f trackTarget = mostDangerousRunner->getPosition() + (mostDangerousRunner->getVelocity() * 0.4f) + (toGoal * 100.f);

                spatialCorrection += (trackTarget - npcPos) * 0.85f * awareness;
            }
        }

        // ==========================================
        // 4. UNIVERSAL HORIZONTAL LINE SYNC (Rigid Banks)
        // ==========================================
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

            // THE FIX 3: Exempt the runner-tracker from the rigid line sync!
            if (std::abs(xDiff) > 50.f && !isStopper && !trackingRunner) {
                float snapStrength = isDefender ? 0.90f : 0.75f;
                spatialCorrection.x += xDiff * snapStrength * discipline * awareness;
            }
        }

        bool isHomeSide = (npc.getTeam() == Team::Home);



        // ==========================================
        // 5. MIDFIELD SPACE COVERAGE & MARKING
        // ==========================================
        if (isMid) {
            Player* nearestOpp = PlayerAI::findNearestOpponent(npcPos, opponents);
            if (nearestOpp && nearestOpp->getPositionRole() != PositionRole::Goalkeeper && nearestOpp != ball.getOwner()) {
                sf::Vector2f oppPos = nearestOpp->getPosition();

                if (PlayerAI::dist(npcPos, oppPos) < 1600.f) {
                    sf::Vector2f goalPos = isHomeSide ? sf::Vector2f(pitch.margin, 3500.f) : sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f);
                    sf::Vector2f toGoal = PlayerAI::normalize(goalPos - oppPos);

                    sf::Vector2f idealMarkingPos = oppPos + (toGoal * 300.f);
                    sf::Vector2f markCorrection = idealMarkingPos - npcPos;

                    float markPull = 0.5f * awareness;
                    spatialCorrection += markCorrection * markPull;
                }
            }
        }
    }

    // ==========================================
    // --- THE ELASTIC TETHER (Rigidity Clamp) ---
    // ==========================================
    float correctionMagnitude = std::sqrt(spatialCorrection.x * spatialCorrection.x + spatialCorrection.y * spatialCorrection.y);

    float maxAllowedDrift = 400.f + (zone.roamingFreedom * 800.f);

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

    // THE FIX: If you have the ball, adrenaline takes over! Always sprint!
    if (npc.getBallPossession()) return true;

    float stamRatio = npc.getCurrentStamina() / npc.getMaxStamina();

    // If it's a Critical emergency (like receiving a pass), bypass the stamina lock!
    if (urgency != AIUrgency::Critical && npc.getCurrentStamina() < 2.0f) return false;

    PositionRole role = npc.getPositionRole();
    bool isAttacker = (role == PositionRole::Striker || role == PositionRole::CenterForward || role == PositionRole::LeftWing || role == PositionRole::RightWing);
    bool isDefender = (role == PositionRole::CenterBack || role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);
    bool isMid = (!isAttacker && !isDefender);

    // A player with 50 Familiarity reacts 0.5 seconds slower to tactical shifts!
    float hesitationTimer = (100.f - npc.getTacticalFamiliarity()) / 100.f * 1.0f;

    if (hesitationTimer > 0.0f && (rand() % 100) < 50) {
        // They are confused by the system and momentarily freeze instead of sprinting!
        return false;
    }

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
        // ==========================================
        // --- THE FIX 1: INSTANT RECOVERY SPRINT ---
        // ==========================================
        if (isDefender) {
            // Lowered from 300px to 150px! Defenders will sprint immediately to fix the line.
            return (stamRatio > 0.15f && distToTarget > 150.f);
        }
        else if (isMid) {
            // Lowered from 400px to 200px! Midfielders haul ass back to support.
            return (stamRatio > 0.20f && distToTarget > 200.f);
        }
        return (stamRatio > 0.4f && distToTarget > 600.f);
    }
}

sf::Vector2f PositioningAI::calculateInterceptionPoint(NPCPlayer& npc, Ball& ball) {
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

        // ==========================================
        // --- THE FIX 2: CASCADING AERIAL INTERCEPTION ---
        // ==========================================
        // The AI checks if it can reach the ball at Head height, then Chest, then Feet.
        float targetHeights[] = { 160.f, 110.f, 20.f };
        float avgRunSpeed = npc.getTopSpeed() * 8.5f + 1.f;

        // Calculate Turn Penalty (Same logic as ground tracking!)
        sf::Vector2f npcVel = npc.getVelocity();
        float turnPenalty = 0.f;
        if (npcVel.x * npcVel.x + npcVel.y * npcVel.y > 400.f) {
            sf::Vector2f npcDir = PlayerAI::normalize(npcVel);
            sf::Vector2f targetDir = PlayerAI::normalize(ballPos - npc.getPosition()); // Rough estimate for turn angle
            float dot = npcDir.x * targetDir.x + npcDir.y * targetDir.y;

            if (dot < 0.3f) {
                float agilityFactor = 1.8f - (npc.getAgility() / 100.f);
                turnPenalty = ((0.3f - dot) / 1.3f) * 1.2f * agilityFactor;
            }
        }

        for (float targetZ : targetHeights) {
            // If the ball is already below this target and falling, skip to the next, lower target!
            if (ball.z < targetZ && ball.vz <= 0.f) continue;

            // Solve: 0.5 * g * t^2 - vz * t + (TargetZ - CurrentZ) = 0
            float a = 0.5f * perceivedGravity;
            float b = -ball.vz;
            float c = targetZ - ball.z;

            float discriminant = (b * b) - (4.f * a * c);
            if (discriminant > 0.f) {
                // The '+' root gives us the time the ball falls DOWN to the target height
                float t = (-b + std::sqrt(discriminant)) / (2.f * a);

                if (t > 0.f) {
                    sf::Vector2f predictedPos = ballPos + (ballVel * t);
                    float distToPredicted = PlayerAI::dist(npc.getPosition(), predictedPos);
                    float runTime = turnPenalty + (distToPredicted / avgRunSpeed);

                    // Can we reach this coordinate BEFORE the ball falls past this height?
                    // We add a tiny 0.2s buffer so they stretch/lunge for it
                    if (runTime <= t + 0.2f) {
                        predictedPos.x = std::clamp(predictedPos.x, 0.f, 10000.f);
                        predictedPos.y = std::clamp(predictedPos.y, 0.f, 7000.f);
                        return predictedPos;
                    }
                }
            }
        }

        // ==========================================
        // --- 3. THE BOUNCE CATCHER (Last Resort) ---
        // ==========================================
        // If they cannot reach it in the air at all, predict exactly where it will hit the floor!
        float a = 0.5f * perceivedGravity;
        float b = -ball.vz;
        float c = 0.f - ball.z; // Floor is 0.f
        float discriminant = (b * b) - (4.f * a * c);

        if (discriminant > 0.f) {
            float t = (-b + std::sqrt(discriminant)) / (2.f * a);
            if (t > 0.f) {
                sf::Vector2f landingPos = ballPos + (ballVel * t);
                landingPos.x = std::clamp(landingPos.x, 0.f, 10000.f);
                landingPos.y = std::clamp(landingPos.y, 0.f, 7000.f);
                return landingPos;
            }
        }

        return ballPos;
    }
    else {
        // ==========================================
        // --- PHYSICS-INFORMED GROUND TRACKING ---
        // ==========================================
        float ballFriction = ball.friction;
        float maxStopDist = (ballSpeed * ballSpeed) / (2.f * ballFriction);

        sf::Vector2f toNPC = npc.getPosition() - ballPos;
        float distToBall = std::sqrt(toNPC.x * toNPC.x + toNPC.y * toNPC.y);
        float projection = (toNPC.x * ballDir.x + toNPC.y * ballDir.y);
        float interceptDist = 0.f;

        // ==========================================
        // --- THE FIX 1: MOMENTUM & TURN PENALTY ---
        // ==========================================
        sf::Vector2f npcVel = npc.getVelocity();
        float npcSpeedSq = npcVel.x * npcVel.x + npcVel.y * npcVel.y;
        float turnPenalty = 0.f;

        // If the player is actively moving, check their facing direction
        if (npcSpeedSq > 400.f) {
            sf::Vector2f npcDir = npcVel / std::sqrt(npcSpeedSq);
            sf::Vector2f targetDir = -toNPC / distToBall; // Direction FROM player TO ball

            float dot = npcDir.x * targetDir.x + npcDir.y * targetDir.y;

            // If the player's momentum is carrying them away from the target (Angle > ~72 degrees)
            if (dot < 0.3f) {
                // Normalizes the severity from 0.0 (slightly off) to 1.0 (running exact opposite way)
                float turnSeverity = (0.3f - dot) / 1.3f;

                // Clumsy players take much longer to stop, turn, and re-accelerate
                float agilityFactor = 1.8f - (npc.getAgility() / 100.f);

                // Maximum penalty for a low-agility player running the wrong way is ~2.0 seconds
                turnPenalty = turnSeverity * 1.2f * agilityFactor;
            }
        }

        if (isTeammatePass) {
            // Meet the ball on its path, but NEVER run past where it will physically stop rolling!
            interceptDist = std::max(20.f, projection - 150.f);

            // Push the interception point further down the line if the player needs time to turn around!
            interceptDist += (turnPenalty * ballSpeed * 0.85f);
            interceptDist = std::min(interceptDist, maxStopDist);
        }
        else {
            // ==========================================
            // --- THE FIX 2: KINEMATIC ITERATION ---
            // ==========================================
            float estimatedTime = turnPenalty;
            float estimatedBallTravel = 0.f;
            float avgRunSpeed = npc.getTopSpeed() * 8.5f + 1.f; // Average sprinting speed

            // 3 prediction passes is enough to reliably converge on the exact meeting point
            for (int i = 0; i < 3; i++) {
                // 1. Where will the ball be at the estimated time?
                estimatedBallTravel = (ballSpeed * estimatedTime) - (0.5f * ballFriction * estimatedTime * estimatedTime);

                // The ball physically cannot travel further than its friction limit
                if (estimatedBallTravel > maxStopDist || estimatedBallTravel < 0.f) {
                    estimatedBallTravel = maxStopDist;
                }

                sf::Vector2f predictedPos = ballPos + (ballDir * estimatedBallTravel);

                // 2. How long will it take the player to run to THAT specific future spot?
                sf::Vector2f toPredicted = predictedPos - npc.getPosition();
                float distToPredicted = std::sqrt(toPredicted.x * toPredicted.x + toPredicted.y * toPredicted.y);
                float runTime = distToPredicted / avgRunSpeed;

                // 3. Refine the estimated time for the next loop
                estimatedTime = turnPenalty + runTime;
            }

            // Apply awareness error to make low-tier players slightly misjudge the final spot
            interceptDist = estimatedBallTravel + (errorSeverity * ballSpeed * 0.4f);
            interceptDist = std::min(interceptDist, maxStopDist);
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

Player* PositioningAI::identifyTargetReceiver(Ball& ball, const std::vector<Player*>& team) {
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

        // ==========================================
        // --- THE FIX: CHEMISTRY HESITATION ---
        // ==========================================
        // A telepathic team (100 Chem) reacts to the pass instantly (1.0x).
        // A disjointed team (0 Chem) hesitates to claim the ball, making them up to 30% slower to realize it's their ball!
        float miscommunication = (100.f - p->getTeamChemistry()) / 100.f * 0.30f;

        // Apply the penalty. Low chemistry directly inflates their time to intercept.
        playerTime *= (1.0f + miscommunication);

        float totalScore = ballTime + (playerTime * 1.5f);

        if (totalScore < minTime) {
            minTime = totalScore;
            bestReceiver = p;
        }
    }
    return bestReceiver;
}
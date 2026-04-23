#include "PossessionAI.h"
#include "PlayerAI.h"
#include "AimAssist.h"
#include "MatchStatistics.h"

sf::Vector2f PossessionAI::handlePossession(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates, const std::vector<Player*>& opposition, UserPlayer* user, const Pitch& pitch, float dt, MatchState matchstate, const TeamAI& teamAI, SoundManager& soundManager, MatchStatistics& stats)
{
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);
    sf::Vector2f goalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);

    float bc = npc.getBallControl() / 100.f;
    float awareness = npc.getAwareness() / 100.f;
    float passingSkill = (npc.getShortPassing() + npc.getLongPassing()) / 200.f;
    float dribbleSkill = (npc.getBallControl() * 0.6f + npc.getAgility() * 0.4f) / 100.f;

    sf::Vector2f facingVec = PlayerAI::getFacingVec(npc.getDirection());

    Player* nearestOpp = PlayerAI::findNearestOpponent(npcPos, opposition);
    float closestOppDist = nearestOpp ? PlayerAI::dist(npcPos, nearestOpp->getPosition()) : 9999.f;
    bool isUnderPressure = (closestOppDist < 600.f);
    bool isCrammed = (closestOppDist < (250.f - (bc * 100.f)));

    PlayerBehavior behavior = npc.getPlaystyle().behavior;

    sf::Vector2f npcScale = npc.getSprite().getScale();
    sf::Vector2f feetPos = npcPos;
    feetPos.x -= 150.0f * std::abs(npcScale.x);

    sf::Vector2f toBall = ball.getPosition() - feetPos;
    float distToBall = std::sqrt(toBall.x * toBall.x + toBall.y * toBall.y);

    sf::Vector2f toBallNorm = (distToBall > 0.001f) ? toBall / distToBall : sf::Vector2f(0.f, 0.f);
    float ballAlignment = (facingVec.x * toBallNorm.x + facingVec.y * toBallNorm.y);

    bool ballInKickingReach = (distToBall <= 120.f && ballAlignment > -0.1f);

    if (ballInKickingReach)
    {
        float distToGoal = PlayerAI::dist(npcPos, goalPos);
        float kickPowerNorm = npc.getKickPower() / 100.f;
        float finishingNorm = npc.getFinishing() / 100.f;

        float yDistFromCenter = std::abs(goalPos.y - npcPos.y);
        float xDistFromGoal = std::abs(goalPos.x - npcPos.x);

        bool isDeadAngle = (xDistFromGoal < 400.f && yDistFromCenter > 1100.f);
        bool bypassShooting = false;

        if (isDeadAngle && awareness > 0.6f) {
            bypassShooting = true;
        }

        float maxShotRange = 800.f + (kickPowerNorm * 1400.f) + (behavior.shootBias * 500.f);

        if (!bypassShooting && distToGoal < maxShotRange && npc.getKickCooldown() <= 0.0f && matchstate == MatchState::InPlay) {
            bool isGoodAngle = (yDistFromCenter < xDistFromGoal * 2.0f) || (distToGoal < 1800.f);

            if (isGoodAngle) {
                bool isLongShot = (distToGoal > 2000.f);
                bool hasOpenSpaceToDrive = (isLongShot && closestOppDist > 700.f);
                bool takeLongShotAnyway = (isLongShot && behavior.shootBias > 0.7f && kickPowerNorm > 0.8f && (rand() % 100 < 20));

                if (!hasOpenSpaceToDrive || takeLongShotAnyway) {
                    bool isFirstTime = (npc.m_possessionTimer < 0.4f);
                    bool wantsToShoot = false;

                    if (distToGoal < 1500.f) {
                        wantsToShoot = true;
                    }
                    else if (!isFirstTime && behavior.shootBias > 0.4f) {
                        wantsToShoot = true;
                    }
                    else if (isFirstTime && finishingNorm > 0.8f && behavior.shootBias > 0.7f) {
                        if ((rand() % 100) < 50) wantsToShoot = true;
                    }

                    if (isCrammed && distToGoal > 1800.f) wantsToShoot = false;

                    if (wantsToShoot) {
                        sf::Vector2f toGoal = PlayerAI::normalize(goalPos - npcPos);
                        float alignment = (facingVec.x * toGoal.x + facingVec.y * toGoal.y);
                        float requiredAlignment = (distToGoal < 1000.f) ? 0.0f : 0.4f;

                        if (alignment > requiredAlignment) {
                            PossessionAI::executeShot(npc, ball, goalPos, opposition, pitch, dt, soundManager, stats);
                            return { 0.f, 0.f };
                        }
                    }
                }
            }
        }

        npc.m_passTimer += dt;

        // ==========================================
        // --- THE FIX 1: EXPONENTIAL HOG MATH ---
        // ==========================================
        float passSpeedPref = teamAI.getPassingSpeedPref();
        // Fast teams expect the ball to move in ~0.15s. Slow teams in ~0.5s.
        float managerPassDelay = 0.5f - (passSpeedPref * 0.35f);

        float playerHogDelay = 0.f;
        if (behavior.dribbleBias > 0.5f) {
            // High dribble bias players want to hold it. 
            // Team tactics heavily suppress this UNLESS they are the main superstar (>0.85)
            float tacticalSuppression = (behavior.dribbleBias > 0.85f) ? 1.2f : std::max(0.1f, 1.0f - passSpeedPref);

            // Using std::pow creates an exponential curve. A 0.6 bias player only gets ~0.2s extra delay.
            // A 0.95 bias player gets a massive ~1.2s extra delay.
            playerHogDelay = std::pow(behavior.dribbleBias, 3.f) * 3.5f * tacticalSuppression;
        }
        else {
            // Unselfish players actively scan for passes instantly
            playerHogDelay = -0.2f;
        }

        float actualPassDelay = std::max(0.05f, managerPassDelay + playerHogDelay);

        bool isMid = (npc.getPositionRole() == PositionRole::CenterMid ||
            npc.getPositionRole() == PositionRole::DefensiveMid ||
            npc.getPositionRole() == PositionRole::AttackingMid);

        if (isMid && isUnderPressure) {
            if (awareness > 0.7f || behavior.dribbleBias < 0.4f) {
                actualPassDelay = 0.0f;
            }
        }

        if (isUnderPressure) actualPassDelay *= 0.3f;
        if (isDeadAngle && awareness > 0.65f) actualPassDelay = 0.0f;

        if (npc.getKickCooldown() <= 0.0f && npc.m_passTimer > actualPassDelay) {
            Player* bestTarget = PossessionAI::findBestPassOption(npc, teammates, opposition, user, teamAI, pitch);

            if (bestTarget) {
                sf::Vector2f toTarget = PlayerAI::normalize(bestTarget->getPosition() - npcPos);
                float alignment = (facingVec.x * toTarget.x + facingVec.y * toTarget.y);

                bool isBackwardPass = isHome ? (toTarget.x < -0.1f) : (toTarget.x > 0.1f);
                bool isFirstTime = (npc.m_possessionTimer < 0.6f);
                float bodyStrengthNorm = npc.getBodyStrength() / 100.f;

                bool vetoPassToTurn = (isBackwardPass && isFirstTime && !isUnderPressure && behavior.dribbleBias > 0.4f);

                bool vetoPassToHoldUp = false;
                if (isCrammed && bodyStrengthNorm > 0.75f && npc.m_possessionTimer < 1.5f) {
                    vetoPassToHoldUp = true;
                }

                // ==========================================
                // --- THE FIX 2: VETO RESTRICTION ---
                // ==========================================
                bool vetoPassToDribble = false;

                // You must be an elite dribbler AND the manager must prefer slow build-up
                float requiredDribbleBias = 0.75f + (passSpeedPref * 0.20f);

                if ((behavior.dribbleBias > requiredDribbleBias && dribbleSkill > 0.85f) && !isCrammed) {
                    if (closestOppDist > 800.f) {
                        float forwardAlignment = isHome ? facingVec.x : -facingVec.x;
                        bool isPassForward = isHome ? (bestTarget->getPosition().x > npcPos.x + 300.f) : (bestTarget->getPosition().x < npcPos.x - 300.f);

                        // ONLY ignore the pass if we are running straight at goal and the pass is backward
                        if (forwardAlignment > 0.6f && !isPassForward) {
                            vetoPassToDribble = true;
                        }
                    }
                }

                if (!vetoPassToTurn && !vetoPassToHoldUp && !vetoPassToDribble) {

                    if (alignment < 0.3f) {
                        return toTarget * 0.8f;
                    }

                    Player* tOpp = PlayerAI::findNearestOpponent(bestTarget->getPosition(), opposition);
                    float targetOppDist = tOpp ? PlayerAI::dist(bestTarget->getPosition(), tOpp->getPosition()) : 9999.f;

                    float maxHoldTime = 1.0f + (behavior.dribbleBias * 2.0f) + (dribbleSkill * 1.5f); // Reduced max hold time

                    if (isCrammed || (isUnderPressure && targetOppDist > closestOppDist + 200.f) ||
                        (isUnderPressure && passingSkill > dribbleSkill * 1.2f) ||
                        teamAI.getPassingSpeedPref() > 0.6f || behavior.dribbleBias < 0.35f ||
                        isDeadAngle || npc.m_possessionTimer > maxHoldTime)
                    {
                        executePass(npc, ball, bestTarget, opposition, pitch, soundManager, stats);
                        return { 0.f, 0.f };
                    }
                }
            }
        }
    }

    if (matchstate == MatchState::InPlay) {
        return calculateDribbleDirection(npc, goalPos, opposition, pitch, teamAI);
    }
    return { 0.f, 0.f };
}

Player* PossessionAI::findBestPassOption(NPCPlayer& npc, const std::vector<Player*>& team, const std::vector<Player*>& opposition, UserPlayer* user, const TeamAI& teamAI, const Pitch& pitch) {
    Player* bestOption = nullptr;
    float bestScore = -1e9f;
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);

    bool inOwnDeepBox = isHome ? (npcPos.x < 2500.f) : (npcPos.x > 7500.f);
    bool inEnemyBox = isHome ? (npcPos.x > pitch.totalWidth - pitch.margin - 1650.f) : (npcPos.x < pitch.margin + 1650.f);

    float shortPassNorm = npc.getShortPassing() / 100.f;
    float longPassNorm = npc.getLongPassing() / 100.f;
    float curlNorm = npc.getCurl() / 100.f;
    float visionNorm = npc.getAwareness() / 100.f;

    float tikiTakaPref = 1.0f - teamAI.getPassingLengthPref();
    float routeOnePref = teamAI.getPassingLengthPref();
    float counterSpeed = teamAI.getPassingSpeedPref();

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
    float offsideLineX = isHome ? std::max(halfwayX, std::max(secondDeepestX, npcPos.x)) : std::min(halfwayX, std::min(secondDeepestX, npcPos.x));

    PlayerBehavior behavior = npc.getPlaystyle().behavior;
    float riskMultiplier = (behavior.passRiskBias - 0.5f) * 2.0f;

    Player* closestPresser = PlayerAI::findNearestOpponent(npcPos, opposition);
    float presserDist = closestPresser ? PlayerAI::dist(npcPos, closestPresser->getPosition()) : 9999.f;
    bool isCrammed = presserDist < 350.f;

    std::vector<Player*> receivers;
    for (auto& t : team) {
        if (t != &npc && t->getState() != PlayerState::Injured && !t->isSentOff()) receivers.push_back(t);
    }
    if (user != nullptr && npc.getTeam() == user->getTeam() && user->getState() != PlayerState::Injured && !user->isSentOff()) receivers.push_back(user);

    for (Player* target : receivers) {
        sf::Vector2f targetPos = target->getPosition();
        sf::Vector2f targetVel = target->getVelocity();
        float rawDist = PlayerAI::dist(npcPos, targetPos);

        float baseForwardProgress = isHome ? (targetPos.x - npcPos.x) : (npcPos.x - targetPos.x);
        bool goHigh = (rawDist > 2000.f);
        if (baseForwardProgress < -300.f) goHigh = false;

        float estV0 = std::sqrt(2.f * 800.f * rawDist) + 500.f;
        float leadTime = rawDist / (estV0 * 0.6f);
        leadTime = std::min(leadTime, 1.2f);

        if (goHigh) {
            float estVz = std::clamp(400.f + (rawDist * 0.15f), 500.f, 1050.f);
            float timeInAir = (2.f * estVz) / 980.f;
            leadTime = std::max(0.1f, timeInAir - 0.15f);
        }

        sf::Vector2f leadVec = targetVel * leadTime;
        float maxLeadDist = target->getTopSpeed() * 10.f * leadTime;
        float leadLen = std::sqrt(leadVec.x * leadVec.x + leadVec.y * leadVec.y);

        if (leadLen > maxLeadDist && leadLen > 0.001f) leadVec = (leadVec / leadLen) * maxLeadDist;

        sf::Vector2f aimSpot = targetPos + leadVec;

        if (aimSpot.x < pitch.margin + 50.f || aimSpot.x > pitch.totalWidth - pitch.margin - 50.f ||
            aimSpot.y < pitch.margin + 50.f || aimSpot.y > pitch.totalHeight - pitch.margin - 50.f) {
            continue;
        }

        float exactDist = PlayerAI::dist(npcPos, aimSpot);
        float requiredV0Sq = 0.f;

        if (goHigh) {
            float finalVz = std::clamp(400.f + (exactDist * 0.15f), 500.f, 1050.f);
            float timeInAir = (2.f * finalVz) / 980.f;
            float reqSpeed = exactDist / timeInAir;
            requiredV0Sq = (reqSpeed * 1.03f) * (reqSpeed * 1.03f);
        }
        else {
            float arrivalSpeed = std::clamp(exactDist * 1.0f, 400.f, 800.f);
            if (baseForwardProgress < -300.f) arrivalSpeed = 250.f;

            requiredV0Sq = (arrivalSpeed * arrivalSpeed) + (2.f * 800.f * exactDist);
            float minV0 = 1200.f;
            requiredV0Sq = std::max(requiredV0Sq, minV0 * minV0);
        }

        float idealPower = std::sqrt(requiredV0Sq) / 52.0f;
        if (idealPower > npc.getKickPower() * 1.1f) continue;

        sf::Vector2f passDir = PlayerAI::normalize(aimSpot - npcPos);
        float forwardProgress = isHome ? (aimSpot.x - npcPos.x) : (npcPos.x - aimSpot.x);

        // Base score normalized
        float score = 600.f;

        bool isOffside = isHome ? (aimSpot.x > offsideLineX) : (aimSpot.x < offsideLineX);
        if (isOffside) score -= 10000.f * visionNorm; // Hard Veto

        if (target->getPositionRole() == PositionRole::Goalkeeper) {
            bool deepInOwnHalf = isHome ? (npcPos.x < 3500.f) : (npcPos.x > 6500.f);
            if (!deepInOwnHalf) score -= 10000.f; // Hard Veto
        }

        bool isRightFooted = (npc.getPreferredFoot() == "Right");
        sf::Vector2f facing = PlayerAI::getFacingVec(npc.getDirection());
        float cross = (facing.x * passDir.y - facing.y * passDir.x);
        bool requiresWeakFoot = (isRightFooted && cross < 0) || (!isRightFooted && cross > 0);

        if (requiresWeakFoot) score -= (5.0f - npc.getWeakFootAccuracy()) * 100.f;

        float bodyAlignment = PlayerAI::dot(facing, passDir);
        if (bodyAlignment < -0.2f) score -= 1000.f;

        bool directLaneBlocked = false;
        bool canCurlAround = false;

        for (auto* opp : opposition) {
            float dOpp = PlayerAI::dist(npcPos, opp->getPosition());
            if (dOpp < exactDist && dOpp > 100.f) {
                sf::Vector2f toOpp = opp->getPosition() - npcPos;
                float alignment = (passDir.x * (toOpp.x / dOpp) + passDir.y * (toOpp.y / dOpp));

                if (alignment > 0.92f) {
                    directLaneBlocked = true;
                    if (alignment < 0.98f && curlNorm > 0.5f && forwardProgress > -300.f) {
                        canCurlAround = true;
                    }
                }
            }
        }

        bool wantHigh = (goHigh || (directLaneBlocked && !canCurlAround));

        if (forwardProgress < -200.f) wantHigh = false;

        // Veto blocked paths reliably without overflowing the float scale
        if (directLaneBlocked && !wantHigh && !canCurlAround) {
            score -= 10000.f;
        }

        if (wantHigh && exactDist < 1500.f) score -= 2000.f;

        Player* marker = PlayerAI::findNearestOpponent(aimSpot, opposition);
        float rawDistToMarker = marker ? PlayerAI::dist(aimSpot, marker->getPosition()) : 9999.f;
        float defenderClosingDistance = 850.f * leadTime;
        float effectiveMarkerDist = rawDistToMarker - defenderClosingDistance;

        // Squished Contested Penalties
        if (effectiveMarkerDist < 250.f) {
            score -= 4000.f * visionNorm;
        }
        else if (effectiveMarkerDist < 500.f) {
            score -= 1500.f * visionNorm;
        }

        // Squished Safe Pass Supremacy
        if (effectiveMarkerDist > 800.f && !directLaneBlocked && !isOffside && !inEnemyBox) {
            score += 2000.f * visionNorm;

            // Tiki-Taka dream is now a solid +3000, putting it reliably at the top without being infinite
            if (exactDist < 1800.f && !goHigh) {
                score += 3000.f * visionNorm * shortPassNorm * (1.0f + tikiTakaPref);
            }
        }

        bool isPasserCB = (npc.getPositionRole() == PositionRole::CenterBack);
        float pitchThirdX = isHome ? (pitch.totalWidth / 3.f) : (pitch.totalWidth * 0.66f);
        bool outOfPosition = isHome ? (npcPos.x > pitchThirdX) : (npcPos.x < pitchThirdX);

        if (isPasserCB && outOfPosition) {
            if (effectiveMarkerDist > 800.f && forwardProgress > -200.f && !goHigh) {
                score += 1500.f * visionNorm;
            }
            if (goHigh || forwardProgress > 2500.f) score -= 3000.f;
        }

        bool isPasserGK = (npc.getPositionRole() == PositionRole::Goalkeeper);

        if ((isPasserCB || isPasserGK) && inOwnDeepBox) {
            PositionRole targetRole = target->getPositionRole();
            bool isTargetFB = (targetRole == PositionRole::LeftBack || targetRole == PositionRole::RightBack || targetRole == PositionRole::LeftWingBack || targetRole == PositionRole::RightWingBack);
            bool isTargetMid = (targetRole == PositionRole::CenterMid || targetRole == PositionRole::DefensiveMid);
            bool isTargetCB = (targetRole == PositionRole::CenterBack);
            bool isTargetGK = (targetRole == PositionRole::Goalkeeper);

            if (isTargetMid && effectiveMarkerDist > 800.f && !goHigh) {
                score += 2000.f * visionNorm;
            }
            else if (isTargetFB && effectiveMarkerDist > 600.f) {
                score += 1200.f * visionNorm;
            }
            else if (isTargetCB || isTargetGK) {
                if (effectiveMarkerDist < 1000.f) {
                    score -= 4000.f;
                }
                else {
                    score -= 500.f;
                }
            }
        }

        float timeScaleNorm = std::clamp(npc.getMatchTimeScale() / 10.0f, 0.2f, 3.0f);

        if (riskMultiplier > 0.0f) {
            score += forwardProgress * (0.3f + (counterSpeed * 0.4f) + (riskMultiplier * 1.5f)) * timeScaleNorm;
        }
        else {
            score += std::abs(forwardProgress) * (riskMultiplier * 0.5f);
            score += (1000.f - exactDist) * std::abs(riskMultiplier * 0.8f);
            if (forwardProgress > 0.f) score += forwardProgress * 0.3f * timeScaleNorm;
        }

        if (exactDist < 1200.f) score += (tikiTakaPref * 800.f) * shortPassNorm;
        else if (exactDist > 2500.f) score += (routeOnePref * 1200.f) * longPassNorm * visionNorm * (1.0f + std::max(0.0f, riskMultiplier)) * timeScaleNorm;

        bool inFinalThird = isHome ? (npcPos.x > 7000.f) : (npcPos.x < 3000.f);
        bool isWide = (npcPos.y < 1500.f || npcPos.y > pitch.totalHeight - 1500.f);
        bool targetInBox = isHome ? (aimSpot.x > 8350.f) : (aimSpot.x < 1650.f);

        if (inFinalThird && isWide && targetInBox) score += behavior.crossBias * 1500.f;

        if (forwardProgress < -1500.f) score -= 2000.f;
        else if (inFinalThird && forwardProgress < -800.f && !targetInBox) score -= 3000.f;

        if (inFinalThird && targetInBox) {
            float cutbackAngle = -forwardProgress;
            if (cutbackAngle > -200.f && cutbackAngle < 1200.f) {
                score += 2500.f * visionNorm;
                if (effectiveMarkerDist > 300.f) score += 3500.f * visionNorm;
            }
        }

        bool isMid = (npc.getPositionRole() == PositionRole::CenterMid || npc.getPositionRole() == PositionRole::DefensiveMid || npc.getPositionRole() == PositionRole::AttackingMid);

        if (isMid && visionNorm > 0.7f && isCrammed) {
            float pitchMidY = pitch.totalHeight / 2.f;
            if ((npcPos.y > pitchMidY) != (aimSpot.y > pitchMidY)) {
                if (std::abs(npcPos.y - aimSpot.y) > 2500.f && (goHigh || !directLaneBlocked)) {
                    if (effectiveMarkerDist > 1000.f) {
                        score += 1500.f * visionNorm * longPassNorm;
                    }
                    else {
                        score -= 3000.f;
                    }
                }
            }
        }

        if (wantHigh) score -= (1.0f - longPassNorm) * 500.f;
        else if (directLaneBlocked && canCurlAround) score -= (1.0f - curlNorm) * 800.f;
        else if (!directLaneBlocked) score += 300.f * shortPassNorm;

        float pressureFactor = std::clamp((600.f - presserDist) / 400.f, 0.f, 1.f);
        if (pressureFactor > 0.1f) {
            if (wantHigh && exactDist < 2000.f) score -= 2000.f * pressureFactor;
            if (exactDist < 1500.f) score += 1500.f * pressureFactor * (directLaneBlocked ? 0.5f : 1.0f);
        }

        if (inOwnDeepBox && isCrammed) {
            if (target->getPositionRole() == PositionRole::Goalkeeper && !directLaneBlocked) score += 2000.f;
            else if (forwardProgress > 1500.f) score += 1500.f;
        }

        if (score > bestScore) {
            bestScore = score;
            bestOption = target;
        }
    }

    // Normalized acceptable floor:
    // If Crammed: -2500 (Accept slightly bad passes just to clear it)
    // General: -1000 (Takes standard safe passes easily, rejects heavily blocked passes)
    // Enemy Box: 500 (More critical, holds up for a cutback)
    float minimumAcceptableScore = -1000.f;
    if (isCrammed) minimumAcceptableScore = -2500.f;
    if (inEnemyBox) minimumAcceptableScore = 500.f;

    return (bestScore > minimumAcceptableScore) ? bestOption : nullptr;
}

sf::Vector2f PossessionAI::calculateDribbleDirection(NPCPlayer& npc, sf::Vector2f goalPos, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI) {
    sf::Vector2f npcPos = npc.getPosition();
    sf::Vector2f baseDir = PlayerAI::normalize(goalPos - npcPos);

    float bcNorm = npc.getBallControl() / 100.f;
    float agilityNorm = npc.getAgility() / 100.f;
    float speedNorm = npc.getTopSpeed() / 100.f;
    float counterSpeed = teamAI.getPassingSpeedPref();
    float awarenessNorm = npc.getAwareness() / 100.f;

    float spongeDist = 1200.f;
    sf::Vector2f boundaryPush(0.f, 0.f);

    float leftEdge = pitch.margin;
    float rightEdge = pitch.totalWidth - pitch.margin;
    float topEdge = pitch.margin;
    float bottomEdge = pitch.totalHeight - pitch.margin;

    if (npcPos.x < leftEdge + spongeDist) {
        float factor = 1.0f - ((npcPos.x - leftEdge) / spongeDist);
        boundaryPush.x += factor * factor;
    }
    else if (npcPos.x > rightEdge - spongeDist) {
        float factor = 1.0f - ((rightEdge - npcPos.x) / spongeDist);
        boundaryPush.x -= factor * factor;
    }

    if (npcPos.y < topEdge + spongeDist) {
        float factor = 1.0f - ((npcPos.y - topEdge) / spongeDist);
        boundaryPush.y += factor * factor;
    }
    else if (npcPos.y > bottomEdge - spongeDist) {
        float factor = 1.0f - ((bottomEdge - npcPos.y) / spongeDist);
        boundaryPush.y -= factor * factor;
    }

    if (boundaryPush.x != 0.f || boundaryPush.y != 0.f) {
        float bounceStrength = 3.0f + (speedNorm * 4.0f);
        baseDir = PlayerAI::normalize(baseDir + (boundaryPush * bounceStrength));
    }

    sf::Vector2f bestDir = baseDir;
    float bestScore = -1e9f;

    PlayerBehavior behavior = npc.getPlaystyle().behavior;
    bool isSpeedster = (speedNorm > 0.85f && speedNorm > bcNorm);
    bool isTrickster = (bcNorm > 0.80f && agilityNorm > 0.80f);

    Player* closestOpp = PlayerAI::findNearestOpponent(npcPos, opposition);
    float overallMinOppDist = closestOpp ? PlayerAI::dist(npcPos, closestOpp->getPosition()) : 9999.f;
    sf::Vector2f toClosestOpp = closestOpp ? PlayerAI::normalize(closestOpp->getPosition() - npcPos) : sf::Vector2f(0.f, 0.f);

    if (closestOpp) {
        sf::Vector2f currentMoveDir = npc.getVelocity();
        if (PlayerAI::length(currentMoveDir) < 5.f) currentMoveDir = baseDir;

        float side = (currentMoveDir.x * toClosestOpp.y - currentMoveDir.y * toClosestOpp.x);
        if (side > 0 && npc.usingRightFoot()) npc.changeFoot();
        else if (side < 0 && !npc.usingRightFoot()) npc.changeFoot();

        if (overallMinOppDist < 300.f) {
            baseDir = PlayerAI::normalize(baseDir + (-toClosestOpp * 0.4f * bcNorm));
        }
    }

    bool isFirstTouch = (npc.m_possessionTimer < 0.5f);
    // Determine if we must trigger the "Survival Escape" Protocol
    bool isUnderSeverePressure = (overallMinOppDist < 350.f);

    for (int i = 0; i < 16; ++i) {
        float angleDeg = -135.f + (i * (270.f / 15.f));
        float rad = angleDeg * 3.14159f / 180.f;
        sf::Vector2f testDir(baseDir.x * std::cos(rad) - baseDir.y * std::sin(rad), baseDir.x * std::sin(rad) + baseDir.y * std::cos(rad));

        float score = 0.f;

        // ==========================================
        // --- 1. SURVIVAL ESCAPE VS FORWARD DRIVE ---
        // ==========================================
        if (isUnderSeverePressure && closestOpp) {
            // SURVIVAL MODE: The main goal is putting our body between the ball and the defender
            float dotAway = (testDir.x * -toClosestOpp.x + testDir.y * -toClosestOpp.y);
            score = dotAway * 5000.f * awarenessNorm;

            // Still give a modest bonus to forward momentum if we CAN safely escape forward
            score += (testDir.x * baseDir.x + testDir.y * baseDir.y) * 500.f;
        }
        else {
            // OPEN HIGHWAY: The main goal is driving toward the goal/momentum
            score = (200.f + (counterSpeed * 300.f) + (behavior.runFrequency * 200.f)) * (testDir.x * baseDir.x + testDir.y * baseDir.y);
        }

        sf::Vector2f currentTargetDir = npc.getDribbleTargetDir();
        float stickiness = (testDir.x * currentTargetDir.x + testDir.y * currentTargetDir.y);
        if (stickiness > 0.95f) score += 400.f;

        if (overallMinOppDist < 250.f && !isFirstTouch) {
            float escapeAlignment = (testDir.x * -toClosestOpp.x + testDir.y * -toClosestOpp.y);
            float holdUpIntelligence = std::max(1.0f - behavior.dribbleBias, awarenessNorm);

            if (escapeAlignment > 0.5f) {
                float bodyStrengthNorm = npc.getBodyStrength() / 100.f;
                score += holdUpIntelligence * 2500.f * bcNorm * bodyStrengthNorm;
            }
        }

        float maxThreatInLane = 0.f;
        for (auto* opp : opposition) {
            float d = PlayerAI::dist(npcPos, opp->getPosition());
            if (d < 800.f) {
                sf::Vector2f toOppLocal = PlayerAI::normalize(opp->getPosition() - npcPos);
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

        if (closestOpp && overallMinOppDist < 400.f) {
            float testToOppDot = (testDir.x * toClosestOpp.x + testDir.y * toClosestOpp.y);

            if (testToOppDot < -0.2f) {
                score += 2500.f * speedNorm * behavior.dribbleBias * std::abs(testToOppDot);
            }
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

        float projectionDist = 400.f + (speedNorm * 600.f);
        sf::Vector2f projectedPos = npcPos + testDir * projectionDist;

        float distToLeft = projectedPos.x - leftEdge;
        float distToRight = rightEdge - projectedPos.x;
        float distToTop = projectedPos.y - topEdge;
        float distToBottom = bottomEdge - projectedPos.y;

        float minBoundDist = std::min({ distToLeft, distToRight, distToTop, distToBottom });

        if (minBoundDist < 0.f) {
            score -= 50000.f;
        }
        else if (minBoundDist < 500.f) {
            score -= (500.f - minBoundDist) * 35.f;
        }

        if (score > bestScore) {
            bestScore = score;
            bestDir = testDir;
        }
    }

    npc.setDribbleTargetDir(bestDir);
    return bestDir;
}

// ==========================================
// --- PASSING ASSIST ---
// ==========================================
void PossessionAI::executePass(NPCPlayer& npc, Ball& ball, Player* target, const std::vector<Player*>& opposition, const Pitch& pitch, SoundManager& soundManager, MatchStatistics& stats) {
    sf::Vector2f npcPos = npc.getPosition();
    sf::Vector2f targetPos = target->getPosition();
    sf::Vector2f targetVel = target->getVelocity();

    stats.recordPassAttempt(npc.getTeam());

    bool isHome = (npc.getTeam() == Team::Home);
    float rawDistToTarget = PlayerAI::dist(npcPos, targetPos);
    bool goHigh = (rawDistToTarget > 2000.f);

    float forwardProgress = isHome ? (targetPos.x - npcPos.x) : (npcPos.x - targetPos.x);
    bool isBackpass = (forwardProgress < -300.f);
    if (isBackpass) goHigh = false;

    float estTime = rawDistToTarget / 800.f;
    float maxLeadTime = goHigh ? 1.5f : 0.8f;
    float leadTime = std::min(estTime, maxLeadTime);

    sf::Vector2f leadVec = targetVel * leadTime;
    float maxLeadDist = target->getTopSpeed() * 10.f * leadTime;
    float leadLen = std::sqrt(leadVec.x * leadVec.x + leadVec.y * leadVec.y);

    if (leadLen > maxLeadDist && leadLen > 0.001f) {
        leadVec = (leadVec / leadLen) * maxLeadDist;
    }

    sf::Vector2f aimSpot = targetPos + leadVec;
    float exactDist = PlayerAI::dist(npcPos, aimSpot);
    sf::Vector2f directDir = PlayerAI::normalize(aimSpot - npcPos);

    // Replace this specific section inside executePass:
    bool needsCurl = false;
    float curlSide = 0.f;

    for (auto* opp : opposition) {
        sf::Vector2f toOpp = opp->getPosition() - npcPos;
        float dOpp = PlayerAI::dist(npcPos, opp->getPosition());

        if (dOpp < exactDist && dOpp > 100.f) {
            float alignment = (directDir.x * (toOpp.x / dOpp) + directDir.y * (toOpp.y / dOpp));
            if (alignment > 0.90f) {
                if (exactDist < 2500.f && (npc.getCurl() / 100.f) > 0.5f && !isBackpass) {
                    needsCurl = true;
                    goHigh = false;
                    curlSide = ((directDir.x * toOpp.y - directDir.y * toOpp.x) > 0) ? -1.0f : 1.0f;
                }
                else if (!isBackpass) {
                    // THE FIX: Only switch to a chipped pass if it is going forward!
                    goHigh = true;
                }
            }
        }
    }

    bool usingRight = npc.usingRightFoot();

    if (!needsCurl && (rand() % 100 < npc.getCurl())) {
        needsCurl = true;
        curlSide = (rand() % 100 < 70) ? (usingRight ? -1.0f : 1.0f) : (usingRight ? 1.0f : -1.0f);
    }

    float intentionalSpin = 0.f;
    if (needsCurl) {
        float multiplier = 1.6f;
        intentionalSpin = (!usingRight) ? ((curlSide > 0) ? (npc.getCurl() * multiplier) : (-(npc.getCurl() / 2.f) * multiplier))
            : ((curlSide < 0) ? (-npc.getCurl() * multiplier) : ((npc.getCurl() / 2.f) * multiplier));
    }

    // ==========================================
    // --- EXACT KINEMATIC POWER CALCULATION ---
    // ==========================================
    float finalVz = 100.f;
    float finalBs = 0.f;
    float idealPowerWorld = 0.f;

    if (goHigh) {
        finalVz = std::clamp(500.f + (exactDist * 0.2f), 600.f, 1150.f);
        finalBs = 60.f + (npc.getLongPassing() * 0.5f);

        float timeInAir = (2.f * finalVz) / 980.f;
        float reqSpeed = exactDist / timeInAir;
        idealPowerWorld = reqSpeed * 1.15f;
    }
    else {
        float arrivalSpeed = std::clamp(exactDist * 0.8f, 550.f, 750.f);
        if (isBackpass) arrivalSpeed = std::clamp(exactDist * 0.5f, 300.f, 450.f);

        float requiredV0Sq = (arrivalSpeed * arrivalSpeed) + (2.f * ball.friction * exactDist);
        idealPowerWorld = std::sqrt(requiredV0Sq);
        idealPowerWorld = std::max(idealPowerWorld, 850.f);
    }

    float perfectPower = idealPowerWorld / 52.0f;

    float passingStat = goHigh ? npc.getLongPassing() : npc.getShortPassing();
    bool isRightFooted = (npc.getPreferredFoot() == "Right");
    bool isWeakFoot = (isRightFooted && !usingRight) || (!isRightFooted && usingRight);

    // ==========================================
    // --- THE FIX 3: TIGHTEN THE ERROR MARGIN ---
    // ==========================================
    // Reduced from 15 degrees to 6 degrees. Even poor players will mostly hit the lane.
    float errorAngle = (1.0f - (passingStat / 100.0f)) * 6.0f;
    float wfPowerMod = 1.0f;

    if (isWeakFoot) {
        float eMod;
        float shank = getWeakFootPenalty(npc.getWeakFootAccuracy(), wfPowerMod, eMod);
        errorAngle = (errorAngle * eMod) + shank;
    }

    float randError = ((rand() % 100) / 100.f - 0.5f) * errorAngle;
    float rad = randError * 3.14159f / 180.f;

    sf::Vector2f finalAim(
        directDir.x * std::cos(rad) - directDir.y * std::sin(rad),
        directDir.x * std::sin(rad) + directDir.y * std::cos(rad)
    );

    // Reduced power variance from 15% to just 5% so passes don't fall wildly short
    float weightErrorFactor = (1.0f - (passingStat / 100.f)) * 0.05f;
    float randomWeight = 1.0f + (((rand() % 200) - 100) / 100.f) * weightErrorFactor;
    float finalPower = perfectPower * wfPowerMod * randomWeight;

    AimAssist::applyPassAssist(npc, target, finalAim, finalPower, goHigh, true, pitch);

    if (needsCurl) {
        float distanceScale = std::clamp(exactDist / 1500.f, 0.4f, 1.8f);
        float baseOffsetDegrees = 26.f;
        float offsetRad = -((intentionalSpin / 100.f) * baseOffsetDegrees * distanceScale) * (3.14159f / 180.f);

        finalAim = sf::Vector2f(
            finalAim.x * std::cos(offsetRad) - finalAim.y * std::sin(offsetRad),
            finalAim.x * std::sin(offsetRad) + finalAim.y * std::cos(offsetRad)
        );
    }

    finalPower = std::clamp(finalPower, 5.0f, npc.getKickPower());
    float kickStrength = std::clamp(finalPower / npc.getKickPower(), 0.0f, 1.0f);

    finalVz *= 0.8f;

    if (needsCurl) intentionalSpin *= (1.1f + kickStrength / 2.f);
    if (isWeakFoot) intentionalSpin *= (0.4f + (npc.getWeakFootAccuracy() / 5.0f) * 0.6f);

    float kickVol = std::clamp(0.0f + (finalPower / npc.getKickPower()) * 40.0f, 10.f, 100.f);
    soundManager.playRandomSound("kick", 3, kickVol, 0.15f);

    ball.shoot(finalAim, finalPower, intentionalSpin, finalVz, finalBs);
    npc.resetKickCooldown();
}

void PossessionAI::executeShot(NPCPlayer& npc, Ball& ball, sf::Vector2f goalPos, const std::vector<Player*>& opposition, const Pitch& pitch, float dt, SoundManager& soundManager, MatchStatistics& stats) {
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

    // 2. INITIAL AIM: The "Halves" Method
    float goalCenter = 3500.f;
    float halfGoalWidth = 366.f;
    float rawTargetY;

    // THE FIX: Aim exclusively at the Left or Right half of the goal!
    if (rand() % 2 == 0) {
        // Aim anywhere in the Top Half (Top Post to Center)
        // Bias it slightly towards the post by skipping the exact center 50px
        rawTargetY = goalCenter - 50.f - (static_cast<float>(rand()) / RAND_MAX * (halfGoalWidth - 90.f));
    }
    else {
        // Aim anywhere in the Bottom Half
        rawTargetY = goalCenter + 50.f + (static_cast<float>(rand()) / RAND_MAX * (halfGoalWidth - 90.f));
    }

    sf::Vector2f aimDir = PlayerAI::normalize(sf::Vector2f(goalPos.x, rawTargetY) - npcPos);

    // ==========================================
     // --- 3. APPLY RAW STAT ERROR & POWER ---
     // ==========================================
    bool usingRight = npc.usingRightFoot();
    bool isRightFooted = (npc.getPreferredFoot() == "Right");
    bool isWeakFoot = (isRightFooted && !usingRight) || (!isRightFooted && usingRight);

    float finishingNorm = npc.getFinishing() / 100.f;
    float distToGoal = PlayerAI::dist(npcPos, goalPos);

    // THE FIX 1: PROXIMITY POWER SCALING (No more random pea-rollers!)
    // If they are inside the penalty box (< 1650px), the absolute minimum charge is 85%.
    // If they are outside the box, they might try to place it (60% min charge).
    float minCharge = (distToGoal < 1650.f) ? 0.85f : 0.60f;

    // Elite finishers are far more consistent with their ball striking
    minCharge += (finishingNorm * 0.10f);
    minCharge = std::min(minCharge, 0.95f); // Cap min charge to preserve a tiny bit of human variance

    float varianceRange = 1.0f - minCharge;
    float simulatedCharge = minCharge + (((rand() % 100) / 100.f) * varianceRange);

    float basePower = npc.getKickPower() * simulatedCharge;
    float finalPower = basePower;

    // THE FIX 1: DRASTICALLY REDUCE BASE ANGULAR ERROR
    // Even a bad striker (50 Finishing) should only have ~3.5 degrees of pure mathematical error.
    // 3.5 degrees from the penalty spot is still enough to miss the post!
    float baseError = (1.0f - finishingNorm) * 7.0f;
    float wfPowerMod = 1.0f;

    // ==========================================
    // --- 4. FIRST TIME SHOT PENALTY (Elite Exemption) ---
    // ==========================================
    bool isFirstTimeShot = (npc.m_possessionTimer < 0.4f);
    if (isFirstTimeShot) {
        // First time shots are wild for poor finishers, but elite strikers control them well
        float rushPenalty = (1.0f - finishingNorm) * 8.0f; // Reduced from 15.0f
        baseError += 2.0f + rushPenalty; // Reduced baseline rush error from 10.0f to 2.0f

        float powerRetention = 0.70f + (finishingNorm * 0.30f);
        finalPower *= std::clamp(powerRetention, 0.70f, 1.0f);
    }

    if (isWeakFoot) {
        float eMod;
        float shank = getWeakFootPenalty(npc.getWeakFootAccuracy(), wfPowerMod, eMod);
        finalPower *= wfPowerMod;

        // Weak foot shanks are slightly randomized
        float wfShankDir = (rand() % 2 == 0) ? 1.0f : -1.0f;
        baseError += (shank * wfShankDir);
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
        float distToGoal = PlayerAI::dist(npcPos, goalPos);
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
        
        // THE FIX 3: THE "LACES" STRIKE
        // Standard shots get a baseline 1.2x boost. 
        // But if they are close to the goal, they put their laces through it for up to 1.5x!
        float proximityVenom = (distToGoal < 2000.f) ? (1.3f + (finishingNorm * 0.2f)) : 1.2f;
        finalPower *= proximityVenom;

        if (isHighShot) {
            finalBackspin = 50.f + (npc.getFinishing() * 0.8f);
        }
        else {
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
            float distToGoal = PlayerAI::dist(npcPos, goalPos);
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

    // ==========================================
    // --- THE FIX 2: GEOMETRIC SHOT TRACKING ---
    // ==========================================
    bool isHomeSide = (npc.getTeam() == Team::Home);
    float targetLineX = isHomeSide ? pitch.totalWidth - pitch.margin : pitch.margin;

    bool onTarget = false;

    // Prevent divide-by-zero if they somehow shoot exactly parallel to the goal line
    if (std::abs(aimDir.x) > 0.001f) {
        // Calculate the time 't' it takes for the X vector to reach the goal line
        float t = (targetLineX - npcPos.x) / aimDir.x;

        // If t is positive, the ball is actually moving TOWARDS the goal!
        if (t > 0.f) {
            // Project the Y coordinate using 't'
            float intersectY = npcPos.y + (aimDir.y * t);

            float goalCenterY = 3500.f;
            float halfGoalWidth = 366.f;

            // Did the trajectory cross the line between the two posts?
            if (intersectY > goalCenterY - halfGoalWidth && intersectY < goalCenterY + halfGoalWidth) {
                onTarget = true;
            }
        }
    }

    // Record the physically accurate shot result!
    stats.recordShot(npc.getTeam(), onTarget);

    ball.shoot(aimDir, finalPower, finalSpin, vzPower, finalBackspin);
    npc.resetKickCooldown();
}

void PossessionAI::executeThrowIn(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates) {
    Player* bestTarget = nullptr;
    float bestScore = -9999.f;
    sf::Vector2f npcPos = npc.getPosition();

    for (Player* mate : teammates) {
        if (mate == &npc) continue;
        float d = PlayerAI::dist(npcPos, mate->getPosition());

        // Throw-ins shouldn't be too short (interceptable) or too long (inaccurate)
        if (d < 2500.f && d > 400.f) {
            float score = 2500.f - std::abs(d - 1200.f); // Ideal throw is ~12 meters
            if (score > bestScore) {
                bestScore = score;
                bestTarget = mate;
            }
        }
    }

    sf::Vector2f targetPos = bestTarget ? bestTarget->getPosition() : sf::Vector2f(5000.f, 3500.f);
    float distToTarget = PlayerAI::dist(npcPos, targetPos);
    sf::Vector2f throwDir = PlayerAI::normalize(targetPos - npcPos);

    // ==========================================
    // --- EXACT KINEMATIC THROW-IN SOLVER ---
    // ==========================================
    // Longer throws require a higher arc to stay in the air
    float vzPower = 300.0f + (distToTarget * 0.15f);
    vzPower = std::clamp(vzPower, 300.f, 650.f);

    // How long will it stay in the air? (t = 2v/g)
    float timeInAir = (2.f * vzPower) / 980.f;

    // What horizontal speed is required to cover the distance in that time?
    float reqHorizSpeed = distToTarget / timeInAir;
    float dragTax = 1.15f;

    float throwPower = (reqHorizSpeed * dragTax) / 52.0f;
    throwPower = std::clamp(throwPower, 10.f, 45.f); // Humans can't throw as hard as they kick

    float backspin = 15.0f;

    ball.shoot(throwDir, throwPower, 0.0f, vzPower, backspin);
    npc.resetKickCooldown();
}

void PossessionAI::handleNPCJumpLogic(NPCPlayer& npc, Ball& ball) {
    if (npc.z > 0.0f || npc.getState() == PlayerState::Tackling) return;

    float d = PlayerAI::dist(npc.getPosition(), ball.getPosition());
    if (d > 400.f || ball.z < 100.f || ball.vz > 150.f) return;

    float awarenessNorm = npc.getAwareness() / 100.f;
    float jumpingNorm = npc.getJumpingStrength() / 100.0f;

    float jumpVz = 240.f + (jumpingNorm * 160.f);
    float timeToApex = jumpVz / 980.f;

    float headBaseZ = 160.f;
    float playerApexZ = headBaseZ + ((jumpVz * jumpVz) / (2.f * 980.f));

    float a = 0.5f * 980.f;
    float b = -ball.vz;
    float c = playerApexZ - ball.z;

    float timeToBall = -1.f;
    float discriminant = (b * b) - (4.f * a * c);

    if (discriminant >= 0.f) {
        float t1 = (-b - std::sqrt(discriminant)) / (2.f * a);
        float t2 = (-b + std::sqrt(discriminant)) / (2.f * a);
        if (t1 > 0.f) timeToBall = t1;
        else if (t2 > 0.f) timeToBall = t2;
    }
    else {
        c = headBaseZ - ball.z;
        discriminant = (b * b) - (4.f * a * c);
        if (discriminant >= 0.f) {
            float t1 = (-b - std::sqrt(discriminant)) / (2.f * a);
            float t2 = (-b + std::sqrt(discriminant)) / (2.f * a);
            if (t1 > 0.f) timeToBall = t1;
            else if (t2 > 0.f) timeToBall = t2;
        }
    }

    if (timeToBall > 0.f) {
        float timeDiff = timeToBall - timeToApex;
        float errorMargin = 0.05f + ((1.0f - awarenessNorm) * 0.15f);

        if (timeDiff <= errorMargin) {
            sf::Vector2f futureBallPos = ball.getPosition() + (ball.getVelocity() * timeToBall);
            float distToDropZone = PlayerAI::dist(npc.getPosition(), futureBallPos);
            float maxJumpReach = (npc.getTopSpeed() * 10.f) * timeToBall;

            if (distToDropZone < maxJumpReach + 30.f) {
                sf::Vector2f jumpDir = PlayerAI::normalize(futureBallPos - npc.getPosition());

                float currentSpeed = std::sqrt(npc.getVelocity().x * npc.getVelocity().x + npc.getVelocity().y * npc.getVelocity().y);
                float requiredJumpSpeed = distToDropZone / timeToBall;

                // THE FIX: Do not allow them to jump horizontally faster than their top speed!
                float maxSpeed = npc.getTopSpeed() * 10.f;
                float jumpSpeed = std::min(maxSpeed, std::max(currentSpeed, requiredJumpSpeed));

                npc.setVelocity(jumpDir * jumpSpeed);
                npc.vz = jumpVz;
                npc.setState(PlayerState::Jumping);
            }
        }
    }
}

bool PossessionAI::tryNPCAerialStrike(NPCPlayer& npc, Ball& ball, sf::Vector2f aimDir, bool isShot, SoundManager& soundManager) {
    if (npc.getKickCooldown() > 0.0f) return false;
    if (ball.hasOwner()) return false;

    Player* lastOwner = ball.getLastOwner();
    if (lastOwner != nullptr) {
        if (lastOwner == &npc) return false;
        if (!isShot && lastOwner->getTeam() == npc.getTeam()) {
            float distFromPasser = PlayerAI::dist(npc.getPosition(), lastOwner->getPosition());
            if (distFromPasser < 1500.f) return false;
        }
    }

    sf::Vector2f npcPos = npc.getPosition();
    sf::Vector2f ballPos = ball.getPosition();

    float dx = std::abs(npcPos.x - ballPos.x);
    float dy = std::abs(npcPos.y - ballPos.y);
    float d = std::sqrt(dx * dx + dy * dy);

    float relativeHeight = ball.z - npc.z;
    bool isHeader = (relativeHeight >= 140.f && relativeHeight <= 220.f);
    bool isVolley = (relativeHeight >= 40.f && relativeHeight < 140.f);

    if (!isHeader && !isVolley) return false;

    if (isHeader) {
        if (dy > 35.f || dx > 100.f) return false;
    }
    else if (isVolley) {
        if (d > 90.f) return false;
    }

    if (isVolley && relativeHeight < 110.f && !isShot) {
        if (d > 45.f) return false;

        float bcNorm = npc.getBallControl() / 100.f;

        if (((rand() % 100) / 100.f) < bcNorm) {
            float touchError = 1.0f - bcNorm;

            if (bcNorm > 0.85f && (rand() % 100 < 80)) {
                ball.possess(&npc);
            }
            else {
                // THE FIX 1: Tamed the "chest explosion" heavy touch. 
                // Reduced 400px/s knock down to 140px/s so it looks like a realistic fumble!
                float knockSpeed = 15.f + (touchError * 140.f);
                float bounceZ = 10.f + (touchError * 80.f);

                sf::Vector2f npcVel = npc.getVelocity();
                float currentSpeed = std::sqrt(npcVel.x * npcVel.x + npcVel.y * npcVel.y);
                sf::Vector2f baseDir = (currentSpeed > 10.f) ? PlayerAI::normalize(npcVel) : aimDir;

                float randError = ((rand() % 200) - 100) / 100.f * (touchError * 90.f);
                float rad = randError * 3.14159f / 180.f;
                sf::Vector2f knockDir(
                    baseDir.x * std::cos(rad) - baseDir.y * std::sin(rad),
                    baseDir.x * std::sin(rad) + baseDir.y * std::cos(rad)
                );

                ball.shoot(knockDir, knockSpeed, 0.f, bounceZ, 0.f);
            }
            npc.resetKickCooldown();
            return true;
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

    // THE FIX 2: CONTEXTUAL AERIAL CLEARANCES
    bool isHome = npc.getTeam() == Team::Home;
    bool inOwnBox = isHome ? (npcPos.x < 1650.f) : (npcPos.x > 8350.f);
    bool isClearance = (!isShot && inOwnBox && incomingSpeed > 300.f);

    if (isHeader) {
        if (isShot) {
            basePower = (30.f + (activeStat * 0.5f)) * std::max(0.3f, simulatedCharge);
            vzOut = 100.f - (activeStat * 3.0f);
        }
        else if (isClearance) {
            basePower = 55.f + (activeStat * 0.3f); // Boot it away!
            vzOut = 450.f;
        }
        else {
            // THE FIX 3: Soft Header Pass (Dropped from ~70 power to ~20 power!)
            basePower = 15.f + (activeStat * 0.15f);
            vzOut = 200.f + (activeStat * 1.5f);
        }
        finalBackspin = 10.f;
    }
    else {
        if (isShot) {
            basePower = npc.getKickPower() * simulatedCharge * 1.1f;
            float techniqueError = (1.0f - skillNorm);
            vzOut = 100.f + (techniqueError * 350.f) + ((1.0f - timingQuality) * 200.f);
        }
        else if (isClearance) {
            basePower = npc.getKickPower() * 0.85f; // Volley clearance!
            vzOut = 600.f;
        }
        else {
            // Volley Pass
            basePower = 20.f + (activeStat * 0.2f);
            vzOut = 250.f + ((1.0f - skillNorm) * 150.f);
        }
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
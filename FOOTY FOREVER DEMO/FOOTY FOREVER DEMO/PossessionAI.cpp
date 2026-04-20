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

    // --- 1. SHOOTING (Preserving xG & Shot Quality) ---
    float distToGoal = PlayerAI::dist(npcPos, goalPos);
    float kickPowerNorm = npc.getKickPower() / 100.f;
    float finishingNorm = npc.getFinishing() / 100.f;

    float yDistFromCenter = std::abs(goalPos.y - npcPos.y);
    float xDistFromGoal = std::abs(goalPos.x - npcPos.x);

    // ==========================================
        // --- THE FIX 3A: RELAXED xG VETO ---
        // ==========================================
        // Shrink the dead angle! Only veto if they are almost touching the corner flag.
    bool isDeadAngle = (xDistFromGoal < 400.f && yDistFromCenter > 1100.f);
    bool bypassShooting = false;

    if (isDeadAngle && awareness > 0.6f) {
        bypassShooting = true;
    }

    float maxShotRange = 800.f + (kickPowerNorm * 1400.f) + (behavior.shootBias * 500.f);

    if (!bypassShooting && distToGoal < maxShotRange && npc.getKickCooldown() <= 0.0f && matchstate == MatchState::InPlay) {
        // Massive buff to good angle calculation. If they are within 18 meters, it's a good angle!
        bool isGoodAngle = (yDistFromCenter < xDistFromGoal * 2.0f) || (distToGoal < 1800.f);

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

                // THE FIX 1: THE CRAMMED VETO
                                // If they are crammed by a defender, panic ruins the shot chance ONLY if they are outside the box!
                                // Inside the box (< 1800px), they will shoot even if a defender is breathing down their neck.
                if (isCrammed && distToGoal > 1800.f) wantsToShoot = false;

                if (wantsToShoot) {
                    sf::Vector2f toGoal = PlayerAI::normalize(goalPos - npcPos);
                    float alignment = (facingVec.x * toGoal.x + facingVec.y * toGoal.y);

                    // THE FIX 2: TAP-IN ALIGNMENT
                    // If we are close to goal (< 1000px), we don't need to be perfectly aligned to shoot. 
                    // We just poke it in! Further out, we need at least 0.4 alignment to strike it clean.
                    float requiredAlignment = (distToGoal < 1000.f) ? 0.0f : 0.4f;

                    if (alignment > requiredAlignment) {
                        executeShot(npc, ball, goalPos, opposition, pitch, dt, soundManager, stats);
                        return { 0.f, 0.f };
                    }
                }
            }
        }
    }



    // --- 2. PASSING ---
    npc.m_passTimer += dt;
    float managerPassDelay = 0.6f - (teamAI.getPassingSpeedPref() * 0.5f);

    // ==========================================
    // --- THE FIX 1: MASSIVE HOG DELAY ---
    // ==========================================
    // Dribblers demand the ball. We use an exponential curve so players with 
    // > 0.8 dribble bias will hold the ball for several seconds before even looking up!
    float playerHogDelay = 0.f;
    if (behavior.dribbleBias > 0.5f) {
        playerHogDelay = std::pow(behavior.dribbleBias, 2.f) * 3.0f;
    }
    else {
        playerHogDelay = (behavior.dribbleBias - 0.5f) * 1.0f; // Quick passers
    }

    float actualPassDelay = std::max(0.05f, managerPassDelay + playerHogDelay);

    // ==========================================
    // --- THE METRONOME (Press Resistance) ---
    // ==========================================
    bool isMid = (npc.getPositionRole() == PositionRole::CenterMid ||
        npc.getPositionRole() == PositionRole::DefensiveMid ||
        npc.getPositionRole() == PositionRole::AttackingMid);

    // If a midfielder gets the ball and is instantly pressured...
    if (isMid && isUnderPressure) {
        if (awareness > 0.7f || behavior.dribbleBias < 0.4f) {
            actualPassDelay = 0.0f; // One touch escape
        }
    }

    if (isUnderPressure) actualPassDelay *= 0.3f;
    if (isDeadAngle && awareness > 0.65f) actualPassDelay = 0.0f;

    if (npc.getKickCooldown() <= 0.0f && npc.m_passTimer > actualPassDelay) {
        Player* bestTarget = PossessionAI::findBestPassOption(npc, teammates, opposition, user, teamAI, pitch);

        if (bestTarget) {
            sf::Vector2f toTarget = PlayerAI::normalize(bestTarget->getPosition() - npcPos);
            float alignment = (facingVec.x * toTarget.x + facingVec.y * toTarget.y);

            // ==========================================
            // --- THE FIRST-TOUCH TURN ---
            // ==========================================
            bool isBackwardPass = isHome ? (toTarget.x < -0.1f) : (toTarget.x > 0.1f);
            bool isFirstTime = (npc.m_possessionTimer < 0.6f);
            float bodyStrengthNorm = npc.getBodyStrength() / 100.f;

            bool vetoPassToTurn = (isBackwardPass && isFirstTime && !isUnderPressure && behavior.dribbleBias > 0.3f);

            // ==========================================
            // --- TARGET MAN HOLD-UP ---
            // ==========================================
            bool vetoPassToHoldUp = false;
            if (isCrammed && bodyStrengthNorm > 0.75f && npc.m_possessionTimer < 1.5f) {
                vetoPassToHoldUp = true;
            }

            // ==========================================
            // --- THE FIX 2: THE DRIBBLE DRIVE VETO ---
            // ==========================================
            // Elite dribblers will completely ignore good passes if they have open space!
            bool vetoPassToDribble = false;
            if ((behavior.dribbleBias > 0.65f || dribbleSkill > 0.8f) && !isCrammed) {
                // If there's no defender within 5 meters, and they aren't facing backward, KEEP RUNNING!
                if (closestOppDist > 500.f) {
                    float forwardAlignment = isHome ? facingVec.x : -facingVec.x;
                    if (forwardAlignment > -0.2f) {
                        vetoPassToDribble = true;
                    }
                }
            }

            // ONLY execute the pass if we haven't vetoed it to hold/turn/dribble!
            if (!vetoPassToTurn && !vetoPassToHoldUp && !vetoPassToDribble) {

                if (alignment < 0.5f) {
                    return toTarget * 0.5f; // Steer to face the pass
                }

                Player* tOpp = PlayerAI::findNearestOpponent(bestTarget->getPosition(), opposition);
                float targetOppDist = tOpp ? PlayerAI::dist(bestTarget->getPosition(), tOpp->getPosition()) : 9999.f;

                // ==========================================
                // --- THE FIX 3: SCALED FORCED RELEASE ---
                // ==========================================
                // Top dribblers can legally hold the ball for 6+ seconds before panicking. 
                // Bad dribblers will panic and force a release after 1.5s.
                float maxHoldTime = 1.5f + (behavior.dribbleBias * 3.0f) + (dribbleSkill * 2.0f);

                if (isCrammed || (isUnderPressure && targetOppDist > closestOppDist + 200.f) ||
                    (isUnderPressure && passingSkill > dribbleSkill * 1.2f) ||
                    teamAI.getPassingSpeedPref() > 0.7f || behavior.dribbleBias < 0.3f ||
                    isDeadAngle || npc.m_possessionTimer > maxHoldTime)
                {
                    executePass(npc, ball, bestTarget, opposition, soundManager, stats);
                    return { 0.f, 0.f };
                }
            }
        }
    }

    // --- 3. DRIBBLE / HOLD UP ---
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
    bool inEnemyBox = isHome ? (npcPos.x > pitch.totalWidth - pitch.margin - 1650.f)
        : (npcPos.x < pitch.margin + 1650.f);

    float shortPassNorm = npc.getShortPassing() / 100.f;
    float longPassNorm = npc.getLongPassing() / 100.f;
    float curlNorm = npc.getCurl() / 100.f;
    float visionNorm = npc.getAwareness() / 100.f;

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
    float riskMultiplier = (behavior.passRiskBias - 0.5f) * 2.0f;

    Player* closestPresser = PlayerAI::findNearestOpponent(npcPos, opposition);
    float presserDist = closestPresser ? PlayerAI::dist(npcPos, closestPresser->getPosition()) : 9999.f;
    bool isCrammed = presserDist < 350.f;

    std::vector<Player*> receivers;
    for (auto& t : team) {
        if (t != &npc && t->getState() != PlayerState::Injured && !t->isSentOff()) {
            receivers.push_back(t);
        }
    }
    if (user != nullptr && npc.getTeam() == user->getTeam() && user->getState() != PlayerState::Injured && !user->isSentOff()) {
        receivers.push_back(user);
    }

    for (Player* target : receivers) {
        float distToTarget = PlayerAI::dist(npcPos, target->getPosition());

        float ballFriction = 800.f;
        float maxKickVel = npc.getKickPower() * 52.0f;
        float maxPhysicalRange = (maxKickVel * maxKickVel) / (2.f * ballFriction);

        if (distToTarget > maxPhysicalRange * 0.9f) continue;

        float arrivalSpeed = 400.f;
        float requiredV0 = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * ballFriction * distToTarget));
        if (requiredV0 / 52.0f > npc.getKickPower()) continue;

        float travelTime = (requiredV0 - arrivalSpeed) / ballFriction;
        sf::Vector2f predictedPos = target->getPosition() + (target->getVelocity() * travelTime);
        float predictedDist = PlayerAI::dist(npcPos, predictedPos);
        sf::Vector2f passDir = PlayerAI::normalize(predictedPos - npcPos);

        float forwardProgress = isHome ? (predictedPos.x - npcPos.x) : (npcPos.x - predictedPos.x);
        float score = 600.f;

        bool isOffside = isHome ? (target->getPosition().x > offsideLineX) : (target->getPosition().x < offsideLineX);
        if (isOffside) score -= 10000.f * visionNorm;

        // ==========================================
        // --- NEW: THE GOALKEEPER VETO ---
        // ==========================================
        if (target->getPositionRole() == PositionRole::Goalkeeper) {
            // Only allow passing to the GK if we are actively in our own defensive third!
            bool deepInOwnHalf = isHome ? (npcPos.x < 3500.f) : (npcPos.x > 6500.f);
            if (!deepInOwnHalf) {
                score -= 25000.f; // Hard ban on returning the ball to the GK from midfield/attack
            }
        }

        bool isRightFooted = (npc.getPreferredFoot() == "Right");
        sf::Vector2f facing = PlayerAI::getFacingVec(npc.getDirection());
        float cross = (facing.x * passDir.y - facing.y * passDir.x);
        bool favorsRight = (cross > 0);
        bool requiresWeakFoot = (isRightFooted && !favorsRight) || (!isRightFooted && favorsRight);

        if (requiresWeakFoot) score -= (5.0f - npc.getWeakFootAccuracy()) * 150.f;

        float bodyAlignment = PlayerAI::dot(facing, passDir);
        if (bodyAlignment < -0.2f) score -= 2000.f;

        // ==========================================
        // --- NEW: TIME-SCALED URGENCY LOGIC ---
        // ==========================================
        // Assuming 10.0f is the baseline time scale.
        // A scale of 20 (short game) heavily amplifies forward urgency.
        // A scale of 2 (long game) cuts urgency down, resulting in slow, methodical build-up!
        float timeScaleNorm = std::clamp(npc.getMatchTimeScale() / 10.0f, 0.2f, 3.0f);

        if (riskMultiplier > 0.0f) {
            // Fast matches: Massive reward for forward progress, massive penalty for backpasses.
            score += forwardProgress * (0.3f + (counterSpeed * 0.4f) + (riskMultiplier * 1.5f)) * timeScaleNorm;
        }
        else {
            // Safe passers still prioritize short/lateral passes
            score += std::abs(forwardProgress) * (riskMultiplier * 0.5f);
            score += (2000.f - distToTarget) * std::abs(riskMultiplier * 0.8f);

            if (forwardProgress > 0.f) {
                score += forwardProgress * 0.3f * timeScaleNorm;
            }
        }

        if (distToTarget < 1200.f) {
            score += (tikiTakaPref * 1500.f) * shortPassNorm;
        }
        else if (distToTarget > 2500.f) {
            // Long, Route-One passes are much more common in short, frantic matches
            score += (routeOnePref * 2500.f) * longPassNorm * visionNorm * (1.0f + std::max(0.0f, riskMultiplier)) * timeScaleNorm;
        }

        // --- CROSSING DNA ---
        bool inFinalThird = isHome ? (npcPos.x > 7000.f) : (npcPos.x < 3000.f);
        bool isWide = (npcPos.y < 1500.f || npcPos.y > pitch.totalHeight - 1500.f);
        bool targetInBox = isHome ? (predictedPos.x > 8350.f) : (predictedPos.x < 1650.f);

        if (inFinalThird && isWide && targetInBox) {
            score += behavior.crossBias * 3000.f;
        }

        // ==========================================
        // --- THE BACKPASS BAN ---
        // ==========================================

        // 1. GLOBAL LONG BACKPASS BAN
        // Nobody should be booting the ball 25+ meters backward to reset play.
        if (forwardProgress < -2500.f) {
            score -= 25000.f;
        }
        // 2. FINAL THIRD STRICTNESS
        // If we are attacking in the final third, don't blast it back to the defense.
        else if (inFinalThird && forwardProgress < -1200.f) {
            score -= 15000.f;
        }

        // 3. REWARD DANGEROUS CUTBACKS
        if (inFinalThird && targetInBox) {
            float cutbackAngle = -forwardProgress;

            if (cutbackAngle > -200.f && cutbackAngle < 1200.f) {
                score += 8000.f * visionNorm;

                Player* marker = PlayerAI::findNearestOpponent(predictedPos, opposition);
                float distToMarker = marker ? PlayerAI::dist(predictedPos, marker->getPosition()) : 9999.f;

                if (distToMarker > 300.f) {
                    score += 12000.f * visionNorm;
                }
            }
        }

        bool directLaneBlocked = false, canCurlAround = false;
        for (auto* opp : opposition) {
            float dOpp = PlayerAI::dist(npcPos, opp->getPosition());
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
        Player* marker = PlayerAI::findNearestOpponent(predictedPos, opposition);
        float distToMarker = marker ? PlayerAI::dist(predictedPos, marker->getPosition()) : 9999.f;

        // 1. REWARD PATIENCE (The Safe Pass)
        if (distToMarker > 1000.f && !directLaneBlocked && !isOffside && !inEnemyBox) {
            score += 2500.f * visionNorm;

            // ==========================================
            // --- NEW: TIKI-TAKA LATERAL BUFF ---
            // ==========================================
            // If it's a lateral or slightly backward pass (-800px to 200px), it's the perfect reset!
            if (forwardProgress < 200.f && forwardProgress > -800.f) {
                // Massive reward for shifting the ball sideways or slightly back
                score += 4500.f * visionNorm;
            }
        }

        bool isMid = (npc.getPositionRole() == PositionRole::CenterMid ||
            npc.getPositionRole() == PositionRole::DefensiveMid ||
            npc.getPositionRole() == PositionRole::AttackingMid);

        // ==========================================
        // --- THE PLAYMAKER'S SWITCH ---
        // ==========================================
        if (isMid && visionNorm > 0.7f && isCrammed) {
            float pitchMidY = pitch.totalHeight / 2.f;
            bool ballOnRightSide = (npcPos.y > pitchMidY);
            bool targetOnRightSide = (predictedPos.y > pitchMidY);

            if (ballOnRightSide != targetOnRightSide) {
                float lateralDistance = std::abs(npcPos.y - predictedPos.y);

                if (lateralDistance > 2500.f) {
                    score += 4000.f * visionNorm * longPassNorm;
                }
            }
        }

        // 2. PUNISH THE HERO BALL
        if (distToMarker < 450.f) {
            score -= 3500.f * visionNorm;

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
    // --- THE FIX 5: HIGHER STANDARDS IN THE BOX ---
    // ==========================================
    float minimumAcceptableScore = isCrammed ? -2000.f : (-400.f + (visionNorm * 800.f));

    if (inEnemyBox) {
        minimumAcceptableScore = std::max(minimumAcceptableScore, 1500.f);
    }

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

    // ==========================================
    // --- DYNAMIC SHIELDING ---
    // ==========================================
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
            // 2. High-Awareness players put their body between the ball and defender.
            float holdUpIntelligence = std::max(1.0f - behavior.dribbleBias, awarenessNorm);

            // If this direction slice points directly AWAY from the defender's chest:
            if (escapeAlignment > 0.5f) {
                float bodyStrengthNorm = npc.getBodyStrength() / 100.f;

                // BUFF: Reward shielding the ball heavily based on physical strength!
                // A strong Target Man will heavily bias toward these directions to plant their feet.
                score += holdUpIntelligence * 2500.f * bcNorm * bodyStrengthNorm;
            }
        }

        // ==========================================
        // --- THE "OPEN HIGHWAY" SCANNER ---
        // ==========================================
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

void PossessionAI::executePass(NPCPlayer& npc, Ball& ball, Player* target, const std::vector<Player*>& opposition, SoundManager& soundManager, MatchStatistics& stats) {
    sf::Vector2f npcPos = npc.getPosition();
    sf::Vector2f targetPos = target->getPosition();

    stats.recordPassAttempt(npc.getTeam());

    // ==========================================
    // --- THE FIX 1: DYNAMIC BASE AIMING ---
    // ==========================================
    // Predict where the player will be in ~1 second so our base aim leads them!
    sf::Vector2f targetVel = target->getVelocity();
    float roughTravelTime = (PlayerAI::dist(npcPos, targetPos) > 2000.f) ? 1.5f : 0.8f;
    sf::Vector2f predictedPos = targetPos + (targetVel * roughTravelTime);

    float distToTarget = PlayerAI::dist(npcPos, predictedPos);
    sf::Vector2f directDir = PlayerAI::normalize(predictedPos - npcPos);

    bool isHome = (npc.getTeam() == Team::Home);
    float forwardProgress = isHome ? (predictedPos.x - npcPos.x) : (npcPos.x - predictedPos.x);

    // Passes that travel more than 3 meters backward are classified as Backpasses
    bool isBackpass = (forwardProgress < -300.f);
    bool goHigh = (distToTarget > 2000.f);

    // Goalkeepers and defenders hate receiving lofted backpasses. Keep them on the deck!
    if (isBackpass) goHigh = false;

    bool needsCurl = false;
    float curlSide = 0.f;

    // 1. SCAN FOR BLOCKERS: Decide if we need to go over or around
    for (auto* opp : opposition) {
        sf::Vector2f toOpp = opp->getPosition() - npcPos;
        float dOpp = PlayerAI::dist(npcPos, opp->getPosition());

        if (dOpp < distToTarget && dOpp > 100.f) {
            float alignment = (directDir.x * (toOpp.x / dOpp) + directDir.y * (toOpp.y / dOpp));
            if (alignment > 0.90f) {
                // Try to curl around if it's a forward pass. Don't risk curling backpasses!
                if (distToTarget < 2500.f && (npc.getCurl() / 100.f) > 0.5f && !isBackpass) {
                    needsCurl = true;
                    goHigh = false;
                    float cross = (directDir.x * toOpp.y - directDir.y * toOpp.x);
                    curlSide = (cross > 0) ? -1.0f : 1.0f;
                }
                else {
                    goHigh = true; // Forced high due to blocker
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

    float errorAngle = (1.0f - (passingStat / 100.0f)) * 15.0f;
    float wfPowerMod = 1.0f;

    if (isWeakFoot) {
        float eMod;
        float shank = getWeakFootPenalty(npc.getWeakFootAccuracy(), wfPowerMod, eMod);
        errorAngle = (errorAngle * eMod) + shank;
    }

    float randError = ((rand() % 100) / 100.f - 0.5f) * errorAngle;
    float rad = randError * 3.14159f / 180.f;
    sf::Vector2f shankedAim(
        directDir.x * std::cos(rad) - directDir.y * std::sin(rad),
        directDir.x * std::sin(rad) + directDir.y * std::cos(rad)
    );

    // ==========================================
    // --- 3. APPLY AIM ASSIST (The Corrector) ---
    // ==========================================
    float finalPower = 0.f;
    sf::Vector2f finalAim = shankedAim;
    AimAssist::applyPassAssist(npc, target, finalAim, finalPower, goHigh, true);

    // Friction Compensation and Weak Foot Power Drop
    finalPower *= wfPowerMod;

    // --- THE FIX 2: TAME LONG GROUND PASSES ---
    if (isBackpass && !goHigh) {
        float distanceScale = std::clamp(distToTarget / 3000.f, 0.0f, 1.0f);
        float punchBoost = 1.4f - (distanceScale * 0.25f);
        finalPower *= punchBoost; // Take massive pace off backpasses so they are easy to trap!
    }
    else if (!goHigh) {
        // BUFFED: Short passes (0m) now get a 1.80x boost to zip across the grass quickly.
        // Long passes (30m+) still beautifully scale down to exactly 1.0x to trust the physics math!
        float distanceScale = std::clamp(distToTarget / 3000.f, 0.0f, 1.0f);
        float punchBoost = 1.9f - (distanceScale * 0.8f);

        finalPower *= punchBoost;
    }

    finalPower = std::min(finalPower, npc.getKickPower());
    float kickStrength = std::clamp(finalPower / npc.getKickPower(), 0.0f, 1.0f);

    // ==========================================
    // --- 4. CURL LOGIC & MAGNUS EFFECT ---
    // ==========================================
    float finalSpin = 0.f;

    if (!needsCurl && (rand() % 100 < npc.getCurl())) {
        needsCurl = true;
        bool wantInsideFoot = (rand() % 100 < 70);
        curlSide = wantInsideFoot ? (usingRight ? -1.0f : 1.0f) : (usingRight ? 1.0f : -1.0f);
    }

    if (needsCurl) {
        float multiplier = (1.1f + kickStrength / 2.f);
        bool isLeftFoot = !usingRight;

        if (isLeftFoot) {
            finalSpin = (curlSide > 0) ? (npc.getCurl() * multiplier) : (-(npc.getCurl() / 2.f) * multiplier);
        }
        else {
            finalSpin = (curlSide < 0) ? (-npc.getCurl() * multiplier) : ((npc.getCurl() / 2.f) * multiplier);
        }

        if (isWeakFoot) finalSpin *= (0.4f + (npc.getWeakFootAccuracy() / 5.0f) * 0.6f);

        // THE FIX 4: SANE CURL OFFSETS
        // Capped the distance scale to 1.5f so they don't aim 40 degrees away from the target!
        float distanceScale = std::clamp(distToTarget / 1500.f, 0.4f, 1.5f);
        float baseOffsetDegrees = 14.f;

        float offsetRad = -((finalSpin / 100.f) * baseOffsetDegrees * distanceScale) * (3.14159f / 180.f);

        finalAim = sf::Vector2f(
            finalAim.x * std::cos(offsetRad) - finalAim.y * std::sin(offsetRad),
            finalAim.x * std::sin(offsetRad) + finalAim.y * std::cos(offsetRad)
        );
    }

    // ==========================================
    // --- 5. TRAJECTORY (VZ) ---
    // ==========================================
    float vzPower = 10.f + (std::pow(kickStrength, 2.f) * 650.f);
    float finalBackspin = 0.f;

    if (goHigh) {
        float maxLoft = std::clamp(500.f + (distToTarget * 0.25f), 700.f, 1150.f);
        vzPower = std::max(maxLoft - ((passingStat / 100.f) * 70.f), 275.f);
        finalBackspin = 60.f + (passingStat * 0.5f);
    }

    float kickVol = std::clamp(0.0f + (finalPower / npc.getKickPower()) * 40.0f, 10.f, 100.f);
    soundManager.playRandomSound("kick", 3, kickVol, 0.15f);

    ball.shoot(finalAim, finalPower, finalSpin, vzPower, finalBackspin);
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

    for (Player* mate : teammates) {
        if (mate == &npc) continue;
        float d = PlayerAI::dist(npc.getPosition(), mate->getPosition());
        if (d < 2500.f) {
            float score = 2500.f - d;
            if (score > bestScore) {
                bestScore = score;
                bestTarget = mate;
            }
        }
    }

    sf::Vector2f targetPos = bestTarget ? bestTarget->getPosition() : sf::Vector2f(5000.f, 3500.f);
    sf::Vector2f throwDir = PlayerAI::normalize(targetPos - npc.getPosition());

    float throwPower = 35.0f;
    float vzPower = 450.0f;
    float backspin = 5.0f;

    ball.shoot(throwDir, throwPower, 0.0f, vzPower, backspin);
    npc.resetKickCooldown();
}

void PossessionAI::handleNPCJumpLogic(NPCPlayer& npc, Ball& ball) {
    if (npc.z > 0.0f || npc.getState() == PlayerState::Tackling) return;

    float d = PlayerAI::dist(npc.getPosition(), ball.getPosition());

    // Ignore balls that are too far, already on the ground, or flying upwards fast
    if (d > 400.f || ball.z < 100.f || ball.vz > 150.f) return;

    float awarenessNorm = npc.getAwareness() / 100.f;
    float jumpingNorm = npc.getJumpingStrength() / 100.0f;

    // Physics parameters
    float jumpVz = 240.f + (jumpingNorm * 160.f);
    float timeToApex = jumpVz / 980.f;

    // ==========================================
    // --- 1. EXACT KINEMATIC INTERSECTION ---
    // ==========================================
    // Calculate the absolute highest point the player's head can reach
    float headBaseZ = 160.f; // Base head height
    float playerApexZ = headBaseZ + ((jumpVz * jumpVz) / (2.f * 980.f));

    // Solve for T when the ball will reach our apex: 0.5*g*t^2 - Vz*t + (TargetZ - currentZ) = 0
    float a = 0.5f * 980.f;
    float b = -ball.vz;
    float c = playerApexZ - ball.z;

    float timeToBall = -1.f;
    float discriminant = (b * b) - (4.f * a * c);

    if (discriminant >= 0.f) {
        float t1 = (-b - std::sqrt(discriminant)) / (2.f * a);
        float t2 = (-b + std::sqrt(discriminant)) / (2.f * a);
        // Take the earliest positive time it crosses our apex
        if (t1 > 0.f) timeToBall = t1;
        else if (t2 > 0.f) timeToBall = t2;
    }
    else {
        // The ball won't reach our absolute max height. Will it cross our standing height?
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
        // ==========================================
        // --- 2. LAUNCH COMMAND (With X/Y Lock) ---
        // ==========================================
        float timeDiff = timeToBall - timeToApex;

        // High awareness players jump with perfect anticipation. 
        // Low awareness players hesitate and jump slightly late.
        float errorMargin = 0.05f + ((1.0f - awarenessNorm) * 0.15f);

        // If the time until the ball arrives is LESS than or EQUAL to the time it takes to jump, TAKE OFF!
        if (timeDiff <= errorMargin) {

            // Predict the exact X/Y position of the ball at that future time
            sf::Vector2f futureBallPos = ball.getPosition() + (ball.getVelocity() * timeToBall);

            // How far is the drop zone?
            float distToDropZone = PlayerAI::dist(npc.getPosition(), futureBallPos);

            // Calculate our maximum physical reach during the jump time
            float maxJumpReach = (npc.getTopSpeed() * 10.f) * timeToBall;

            // ONLY jump if we can actually reach the ball in the air!
            if (distToDropZone < maxJumpReach + 30.f) {
                sf::Vector2f jumpDir = PlayerAI::normalize(futureBallPos - npc.getPosition());

                // Conserve momentum, or speed up slightly to reach the exact coordinate
                float speed = std::sqrt(npc.getVelocity().x * npc.getVelocity().x + npc.getVelocity().y * npc.getVelocity().y);
                float jumpSpeed = std::max(speed, distToDropZone / timeToBall);

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

    float d = PlayerAI::dist(npc.getPosition(), ball.getPosition());
    if (d > 120.f) return false;

    float relativeHeight = ball.z - npc.z;
    bool isHeader = (relativeHeight >= 140.f && relativeHeight <= 220.f);
    bool isVolley = (relativeHeight >= 40.f && relativeHeight < 140.f);

    if (!isHeader && !isVolley) return false;

    // ==========================================
    // --- NEW: THE "BRING IT DOWN" LOGIC (Taking a Touch) ---
    // ==========================================
    // If the ball is roughly chest/hip height (z < 110) and we aren't explicitly shooting...
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
                sf::Vector2f baseDir = (currentSpeed > 10.f) ? PlayerAI::normalize(npcVel) : aimDir;

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

    // ... [Rest of your tryNPCAerialStrike code (headingStat, finishingStat, etc.) goes here] ...

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
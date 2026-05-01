#include "PossessionAI.h"
#include "PlayerAI.h"
#include "AimAssist.h"
#include "MatchStatistics.h"


// 1. Receiver Orientation
static float getReceiverOrientationBonus(Player* target, sf::Vector2f goalPos) {
    sf::Vector2f targetPos = target->getPosition();
    sf::Vector2f toGoal = PlayerAI::normalize(goalPos - targetPos);
    sf::Vector2f facing = PlayerAI::getFacingVec(target->getDirection());

    float alignment = (facing.x * toGoal.x + facing.y * toGoal.y);
    float agilityNorm = target->getAgility() / 100.f;

    if (alignment > 0.5f) return 2000.f; // Facing goal: ready to attack
    if (alignment > -0.2f) return 500.f; // Side-on: can turn quickly

    // Facing away from goal. Agile players mitigate this penalty.
    return -2000.f * (1.0f - agilityNorm);
}

// 2. Defensive Gravity (The Trap Check)
static float calculateReceiverPressureTrend(sf::Vector2f aimSpot, float leadTime, const std::vector<Player*>& opposition) {
    float trapPenalty = 0.f;
    int closingDefenders = 0;

    for (Player* opp : opposition) {
        if (opp->getPositionRole() == PositionRole::Goalkeeper || opp->isSentOff()) continue;

        sf::Vector2f oppPos = opp->getPosition();
        sf::Vector2f oppVel = opp->getVelocity();
        float oppSpeed = std::sqrt(oppVel.x * oppVel.x + oppVel.y * oppVel.y);

        float distToAim = PlayerAI::dist(oppPos, aimSpot);

        if (distToAim < 1500.f && oppSpeed > 150.f) {
            sf::Vector2f toAim = PlayerAI::normalize(aimSpot - oppPos);
            sf::Vector2f oppDir = oppVel / oppSpeed;
            float closingAlignment = (toAim.x * oppDir.x + toAim.y * oppDir.y);

            // If they are sprinting directly at the landing spot
            if (closingAlignment > 0.7f) {
                float arrivalTime = distToAim / oppSpeed;
                // If they will arrive at the same time or shortly after the ball
                if (arrivalTime < leadTime + 0.8f) {
                    closingDefenders++;
                    trapPenalty += 6000.f * closingAlignment * (1.0f - (distToAim / 1500.f));
                }
            }
        }
    }

    if (closingDefenders >= 2) trapPenalty *= 2.5f; // Exponential penalty if swarmed
    return trapPenalty;
}

// 3. Line Breaker Logic
static float calculateLineBreakerValue(sf::Vector2f startPos, sf::Vector2f aimSpot, const std::vector<Player*>& opposition, bool isHomeTeam) {
    float oppDefLineX = 0.f;
    float oppMidLineX = 0.f;
    int defCount = 0;
    int midCount = 0;

    for (Player* opp : opposition) {
        if (opp->isSentOff()) continue;
        PositionRole role = opp->getPositionRole();
        if (role == PositionRole::CenterBack || role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack) {
            oppDefLineX += opp->getPosition().x;
            defCount++;
        }
        else if (role == PositionRole::CenterMid || role == PositionRole::DefensiveMid || role == PositionRole::AttackingMid || role == PositionRole::LeftMid || role == PositionRole::RightMid) {
            oppMidLineX += opp->getPosition().x;
            midCount++;
        }
    }

    if (defCount > 0) oppDefLineX /= defCount;
    if (midCount > 0) oppMidLineX /= midCount;

    float bonus = 0.f;
    bool crossedMid = isHomeTeam ? (startPos.x < oppMidLineX && aimSpot.x > oppMidLineX) : (startPos.x > oppMidLineX && aimSpot.x < oppMidLineX);
    bool crossedDef = isHomeTeam ? (startPos.x < oppDefLineX && aimSpot.x > oppDefLineX) : (startPos.x > oppDefLineX && aimSpot.x < oppDefLineX);

    if (crossedMid) bonus += 4000.f;
    if (crossedDef) bonus += 12000.f; // Massive reward for playing someone in behind

    return bonus;
}

// 4. Triangulation (Dead End Check)
static float evaluateTriangulation(Player* target, const std::vector<Player*>& team, const std::vector<Player*>& opposition) {
    int safeOptions = 0;
    sf::Vector2f targetPos = target->getPosition();

    for (Player* teammate : team) {
        if (teammate == target || teammate->isSentOff()) continue;
        float dist = PlayerAI::dist(targetPos, teammate->getPosition());

        // Are they in safe passing range?
        if (dist > 400.f && dist < 2000.f) {
            Player* nearestOpp = PlayerAI::findNearestOpponent(teammate->getPosition(), opposition);
            float oppDist = nearestOpp ? PlayerAI::dist(teammate->getPosition(), nearestOpp->getPosition()) : 9999.f;
            if (oppDist > 600.f) {
                safeOptions++;
            }
        }
    }

    if (safeOptions == 0) return -4000.f; // Passing to a dead end
    if (safeOptions >= 2) return 3000.f;  // Great flow state
    return 0.f;
}

// ==========================================
// --- NEW: THE NPC KICK DISPATCHER ---
// ==========================================
static void dispatchNPCKick(NPCPlayer& npc, Ball& ball, sf::Vector2f aimDir, float power, float spin, float vz, float backspin, bool isPass, bool isShot, bool isOnTarget, SoundManager& soundManager) 
{

    int currentFrame = npc.getAnimator().getCurrentFrameIndex();
    bool usingRight = npc.usingRightFoot();
    int targetFrame = 7;

    if (!usingRight) {
        if (currentFrame < 3 || currentFrame > 9) targetFrame = 3;
        else targetFrame = 11;
    }

    npc.m_pendingKick.aimDir = aimDir;
    npc.m_pendingKick.power = power;
    npc.m_pendingKick.spin = spin;
    npc.m_pendingKick.vz = vz;
    npc.m_pendingKick.backspin = backspin;
    npc.m_pendingKick.targetFrame = targetFrame;
    npc.m_pendingKick.failsafeTimer = 0.0f;

    // ==========================================
    // --- THE FIX: STORE DEFERRED INTENT ---
    // ==========================================
    npc.m_pendingKick.isPassIntent = isPass;
    npc.m_pendingKick.isShotIntent = isShot;
    npc.m_pendingKick.isShotOnTarget = isOnTarget;
    npc.m_pendingKick.assistCandidate = ball.assistCandidate; // Grab the live assist candidate!

    npc.m_pendingKick.isActive = true;

    npc.setState(PlayerState::Kicking);

    // Force a tiny step forward to drive the running animation
    sf::Vector2f curVel = npc.getVelocity();
    float curSpeed = std::sqrt(curVel.x * curVel.x + curVel.y * curVel.y);
    if (curSpeed < 150.f) {
        npc.setVelocity(npc.getAimDirection() * 250.f);
    }
}

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

    bool isDeepDefender = (npc.getPositionRole() == PositionRole::CenterBack ||
        npc.getPositionRole() == PositionRole::LeftBack ||
        npc.getPositionRole() == PositionRole::RightBack ||
        npc.getPositionRole() == PositionRole::LeftWingBack ||
        npc.getPositionRole() == PositionRole::RightWingBack ||
        npc.getPositionRole() == PositionRole::DefensiveMid);

    float pitchHalfX = pitch.totalWidth / 2.f;
    float noseBleedFactor = 0.0f;

    if (isDeepDefender) {
        float excursion = isHome ? (npcPos.x - pitchHalfX) : (pitchHalfX - npcPos.x);
        float tolerance = (npc.getPositionRole() == PositionRole::CenterBack) ? -800.f : 800.f;

        if (excursion > tolerance) {
            noseBleedFactor = std::clamp((excursion - tolerance) / 2500.f, 0.0f, 1.0f);
        }
    }

    Player* gk = nullptr;
    float distToOutfieldOpp = 9999.f;
    float deepestOutfieldX = isHome ? 0.f : pitch.totalWidth;

    for (auto* opp : opposition) {
        if (opp->getPositionRole() == PositionRole::Goalkeeper) {
            gk = opp;
        }
        else {
            float d = PlayerAI::dist(npcPos, opp->getPosition());
            if (d < distToOutfieldOpp) distToOutfieldOpp = d;

            if (isHome && opp->getPosition().x > deepestOutfieldX) deepestOutfieldX = opp->getPosition().x;
            else if (!isHome && opp->getPosition().x < deepestOutfieldX) deepestOutfieldX = opp->getPosition().x;
        }
    }

    float distToGk = gk ? PlayerAI::dist(npcPos, gk->getPosition()) : 9999.f;

    bool inAttackingHalf = isHome ? (npcPos.x > pitch.totalWidth / 2.f) : (npcPos.x < pitch.totalWidth / 2.f);
    bool isCleanThrough = inAttackingHalf && (isHome ? (npcPos.x > deepestOutfieldX - 150.f) : (npcPos.x < deepestOutfieldX + 150.f));

    float closestOppDist = std::min(distToOutfieldOpp, distToGk);
    bool isUnderPressure = (closestOppDist < 600.f);
    bool isCrammed = (closestOppDist < (250.f - (bc * 100.f)));

    // ==========================================
    // --- THE FIX 1: DEFENSIVE THIRD CONTEXT ---
    // ==========================================
    bool inDefensiveThird = isHome ? (npcPos.x < pitch.totalWidth / 3.f) : (npcPos.x > pitch.totalWidth * 0.66f);

    PlayerBehavior behavior = npc.getPlaystyle().behavior;

    if (noseBleedFactor > 0.0f) {
        behavior.dribbleBias *= (1.0f - (noseBleedFactor * 0.8f));
    }

    TeamState state = teamAI.getCurrentState();

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

        if (state.subState == TacticalSubState::TimeWasting && distToGoal > 1200.f) {
            bypassShooting = true;
        }
        else if (state.subState == TacticalSubState::AllOut) {
            behavior.shootBias += 0.3f;
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

                    if (distToGoal < 1500.f) wantsToShoot = true;
                    else if (!isFirstTime && behavior.shootBias > 0.4f) wantsToShoot = true;
                    else if (isFirstTime && finishingNorm > 0.8f && behavior.shootBias > 0.7f) {
                        if ((rand() % 100) < 50) wantsToShoot = true;
                    }

                    if (isCrammed && distToGoal > 1800.f) wantsToShoot = false;

                    if (wantsToShoot && isCleanThrough) {
                        if (distToGoal > 1200.f) wantsToShoot = false;
                        if (distToGk <= 600.f) wantsToShoot = true;
                    }
                    else if (wantsToShoot && distToGoal > 500.f) {
                        if (distToOutfieldOpp > 350.f && distToGk > 600.f && distToGoal > 700.f) {
                            float composure = (finishingNorm * 0.6f) + (bc * 0.4f);
                            if ((rand() % 100) / 100.f < composure) {
                                wantsToShoot = false;
                            }
                        }

                        if (distToGk <= 600.f && distToGoal < 2000.f) {
                            sf::Vector2f toGoal = PlayerAI::normalize(goalPos - npcPos);
                            if ((facingVec.x * toGoal.x + facingVec.y * toGoal.y) > 0.4f) {
                                wantsToShoot = true;
                            }
                        }
                    }

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

        float passSpeedPref = teamAI.getPassingSpeedPref();

        // ==========================================
        // --- THE FIX: PATIENT POSSESSION ---
        // ==========================================
        // Increased base delay from 0.3s to 0.8s. 
        // A team with 100 passing speed (passSpeedPref = 1.0) will now pass every 0.3s.
        // A team with 0 passing speed (passSpeedPref = 0.0) will hold the ball for 0.8s!
        float managerPassDelay = 0.8f - (passSpeedPref * 0.5f);

        if (state.subState == TacticalSubState::Transition) managerPassDelay *= 0.4f; // Counter attacks are still fast
        else if (state.subState == TacticalSubState::KeepPossession) managerPassDelay *= 1.8f; // Heavy time wasting

        float playerHogDelay = 0.f;
        if (behavior.dribbleBias > 0.5f) {
            playerHogDelay = (behavior.dribbleBias - 0.5f) * 1.5f; // Selfish dribblers hold it even longer!
        }

        if (state.subState == TacticalSubState::TimeWasting) playerHogDelay += 2.5f;

        float actualPassDelay = std::max(0.05f, managerPassDelay + playerHogDelay);

        if (noseBleedFactor > 0.0f) {
            actualPassDelay *= (1.0f - noseBleedFactor);
        }

        bool isMid = (npc.getPositionRole() == PositionRole::CenterMid ||
            npc.getPositionRole() == PositionRole::DefensiveMid ||
            npc.getPositionRole() == PositionRole::AttackingMid);

        if (isMid && isUnderPressure) {
            if (awareness > 0.7f || behavior.dribbleBias < 0.4f) actualPassDelay = 0.0f; // Elite mids will one-touch out of trouble
        }

        if (isUnderPressure) actualPassDelay *= 0.4f; // Slight panic reduction
        if (isDeadAngle && awareness > 0.65f) actualPassDelay = 0.0f;

        if (npc.getKickCooldown() <= 0.0f && npc.m_passTimer > actualPassDelay) {
            Player* bestTarget = PossessionAI::findBestPassOption(npc, teammates, opposition, user, teamAI, pitch);
            bool passExecuted = false;

            if (bestTarget) {
                sf::Vector2f toTarget = PlayerAI::normalize(bestTarget->getPosition() - npcPos);
                float alignment = (facingVec.x * toTarget.x + facingVec.y * toTarget.y);

                bool isBackwardPass = isHome ? (toTarget.x < -0.1f) : (toTarget.x > 0.1f);
                bool isFirstTime = (npc.m_possessionTimer < 0.6f);
                float bodyStrengthNorm = npc.getBodyStrength() / 100.f;

                bool vetoPassToTurn = (isBackwardPass && isFirstTime && !isUnderPressure && behavior.dribbleBias > 0.4f);
                bool vetoPassToHoldUp = (isCrammed && bodyStrengthNorm > 0.75f && npc.m_possessionTimer < 1.5f);
                bool vetoPassToDribble = false;

                if (state.subState == TacticalSubState::TimeWasting && closestOppDist > 400.f) {
                    vetoPassToDribble = true;
                }
                else if (state.subState == TacticalSubState::Transition && isBackwardPass && closestOppDist > 400.f) {
                    vetoPassToDribble = true;
                }
                else if (behavior.dribbleBias > 0.85f && dribbleSkill > 0.85f && !isCrammed) {
                    if (closestOppDist > 1200.f) {
                        float forwardAlignment = isHome ? facingVec.x : -facingVec.x;
                        bool isPassForward = isHome ? (bestTarget->getPosition().x > npcPos.x + 300.f) : (bestTarget->getPosition().x < npcPos.x - 300.f);
                        if (forwardAlignment > 0.6f && !isPassForward) {
                            vetoPassToDribble = true;
                        }
                    }
                }

                if (!vetoPassToTurn && !vetoPassToHoldUp && !vetoPassToDribble) {
                    if (alignment < 0.1f) {
                        return toTarget * 0.8f;
                    }

                    Player* tOpp = PlayerAI::findNearestOpponent(bestTarget->getPosition(), opposition);
                    float targetOppDist = tOpp ? PlayerAI::dist(bestTarget->getPosition(), tOpp->getPosition()) : 9999.f;
                    float maxHoldTime = 1.5f + (behavior.dribbleBias * 3.0f) + (dribbleSkill * 2.0f);

                    bool forcedByTimer = (!isCleanThrough && npc.m_possessionTimer > maxHoldTime);

                    if (isCrammed || (isUnderPressure && targetOppDist > closestOppDist + 200.f) ||
                        (isUnderPressure && passingSkill > dribbleSkill * 1.2f) ||
                        teamAI.getPassingSpeedPref() > 0.7f || behavior.dribbleBias < 0.3f ||
                        isDeadAngle || forcedByTimer || noseBleedFactor > 0.5f)
                    {
                        executePass(npc, ball, bestTarget, opposition, pitch, soundManager, stats, teamAI);
                        passExecuted = true;
                        return { 0.f, 0.f };
                    }
                }
            }

            // ==========================================
            // --- THE FIX 2: THE PANIC CLEARANCE ---
            // ==========================================
            // If we found no safe pass (or it was vetoed) AND we are crammed in our defensive third... BOOM IT.
            if (!passExecuted && inDefensiveThird && isCrammed) {

                // 1. Calculate the Center of Mass of upfield teammates to aim generally towards safety
                sf::Vector2f clearAim = isHome ? sf::Vector2f(pitch.totalWidth * 0.7f, pitch.totalHeight / 2.f)
                    : sf::Vector2f(pitch.totalWidth * 0.3f, pitch.totalHeight / 2.f);
                int upfieldCount = 0;
                sf::Vector2f centerOfMass(0.f, 0.f);

                for (Player* tm : teammates) {
                    if (tm == &npc || tm->isSentOff() || tm->getPositionRole() == PositionRole::Goalkeeper) continue;

                    bool isUpfield = isHome ? (tm->getPosition().x > npcPos.x + 800.f) : (tm->getPosition().x < npcPos.x - 800.f);
                    if (isUpfield) {
                        centerOfMass += tm->getPosition();
                        upfieldCount++;
                    }
                }

                if (upfieldCount > 0) clearAim = centerOfMass / static_cast<float>(upfieldCount);

                sf::Vector2f clearDir = PlayerAI::normalize(clearAim - npcPos);

                // 2. The "Step to the Side": Evade the immediate presser physically in the kick trajectory
                Player* nearestOpp = PlayerAI::findNearestOpponent(npcPos, opposition);
                if (nearestOpp) {
                    sf::Vector2f toOpp = nearestOpp->getPosition() - npcPos;
                    if (PlayerAI::dist(npcPos, nearestOpp->getPosition()) < 300.f) {
                        float dot = clearDir.x * toOpp.x + clearDir.y * toOpp.y;
                        if (dot > -0.5f) {
                            // The opponent is directly in the clearance path. Bend the kick perpendicularly away!
                            sf::Vector2f evadeDir(-toOpp.y, toOpp.x);

                            // Ensure the evasion steps *forward*, not backward into our own goal
                            if ((evadeDir.x > 0.f) != isHome) evadeDir = -evadeDir;

                            clearDir = PlayerAI::normalize(clearDir + (evadeDir * 1.5f));
                        }
                    }
                }

                // 3. Hoof it
                float clearPower = npc.getKickPower() * 0.55f;
                float clearVz = 750.f; // High lofted ball
                float clearSpin = ((rand() % 100) / 100.f - 0.5f) * 10.f; // Chaotic spin

                stats.recordPassAttempt(npc.getTeam());
                ball.shoot(clearDir, clearPower, clearSpin, clearVz, 30.f);

                float kickVol = std::clamp(0.0f + (clearPower / 100.f) * 40.f, 10.f, 100.f);
                soundManager.playRandomSound("kick", 3, kickVol, 0.15f);

                npc.resetKickCooldown();
                return { 0.f, 0.f };
            }
        }
    }

    if (matchstate == MatchState::InPlay) {
        if (noseBleedFactor > 0.3f && (rand() % 100) < (noseBleedFactor * 40.f)) {
            return { 0.f, 0.f };
        }
        return calculateDribbleDirection(npc, goalPos, opposition, pitch, teamAI);
    }
    return { 0.f, 0.f };
}

Player* PossessionAI::findBestPassOption(NPCPlayer& npc, const std::vector<Player*>& team, const std::vector<Player*>& opposition, UserPlayer* user, const TeamAI& teamAI, const Pitch& pitch) {
    Player* bestOption = nullptr;
    float bestScore = -1e9f;
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);

    bool isDeepDefender = (npc.getPositionRole() == PositionRole::CenterBack ||
        npc.getPositionRole() == PositionRole::LeftBack ||
        npc.getPositionRole() == PositionRole::RightBack ||
        npc.getPositionRole() == PositionRole::LeftWingBack ||
        npc.getPositionRole() == PositionRole::RightWingBack ||
        npc.getPositionRole() == PositionRole::DefensiveMid);

    float pitchHalfX = pitch.totalWidth / 2.f;
    float noseBleedFactor = 0.0f;
    if (isDeepDefender) {
        float excursion = isHome ? (npcPos.x - pitchHalfX) : (pitchHalfX - npcPos.x);
        float tolerance = (npc.getPositionRole() == PositionRole::CenterBack) ? -800.f : 800.f;
        if (excursion > tolerance) {
            noseBleedFactor = std::clamp((excursion - tolerance) / 2500.f, 0.0f, 1.0f);
        }
    }

    TeamState state = teamAI.getCurrentState();

    bool inOwnDeepBox = isHome ? (npcPos.x < 2500.f) : (npcPos.x > 7500.f);

    // ==========================================
    // --- THE FIX 3: DEFENSIVE THIRD PARANOIA ---
    // ==========================================
    bool inDefensiveThird = isHome ? (npcPos.x < pitch.totalWidth / 3.f) : (npcPos.x > pitch.totalWidth * 0.66f);

    bool inEnemyBox = isHome ? (npcPos.x > pitch.totalWidth - pitch.margin - 1650.f) : (npcPos.x < pitch.margin + 1650.f);
    sf::Vector2f goalPos = isHome ? sf::Vector2f(pitch.totalWidth - pitch.margin, 3500.f) : sf::Vector2f(pitch.margin, 3500.f);

    float shortPassNorm = npc.getShortPassing() / 100.f;
    float longPassNorm = npc.getLongPassing() / 100.f;
    float curlNorm = npc.getCurl() / 100.f;
    float visionNorm = npc.getAwareness() / 100.f;

    float tikiTakaPref = 1.0f - teamAI.getPassingLengthPref();
    float routeOnePref = teamAI.getPassingLengthPref();
    float passSpeedPref = teamAI.getPassingSpeedPref();
    float attackSpeedPref = teamAI.getAttackingSpeedPref();

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
    float riskMultiplier = (behavior.passRiskBias - 0.5f) * 1.5f;

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
        float score = 500.f;

        if (state.subState == TacticalSubState::TimeWasting) {
            if (forwardProgress > 300.f) score -= 15000.f;
            if (forwardProgress < -300.f) score += 5000.f;
            if (goHigh) score -= 10000.f;
        }
        else if (state.subState == TacticalSubState::Transition) {
            if (forwardProgress > 800.f && !goHigh) score += (4000.f + (attackSpeedPref * 6000.f)) * visionNorm;
            if (forwardProgress < 0.f) score -= (1000.f + (attackSpeedPref * 3000.f));
        }
        else if (state.subState == TacticalSubState::AllOut) {
            bool targetInBox = isHome ? (aimSpot.x > 8350.f) : (aimSpot.x < 1650.f);
            if (targetInBox) score += 15000.f;
            if (forwardProgress < 0.f) score -= 5000.f;
        }

        bool isOffside = isHome ? (aimSpot.x > offsideLineX) : (aimSpot.x < offsideLineX);
        if (isOffside) score -= 20000.f;

        if (target->getPositionRole() == PositionRole::Goalkeeper) {
            bool deepInOwnHalf = isHome ? (npcPos.x < 3500.f) : (npcPos.x > 6500.f);
            if (!deepInOwnHalf) score -= 20000.f;
        }

        bool isRightFooted = (npc.getPreferredFoot() == "Right");
        sf::Vector2f facing = PlayerAI::getFacingVec(npc.getDirection());
        float cross = (facing.x * passDir.y - facing.y * passDir.x);
        bool requiresWeakFoot = (isRightFooted && cross < 0) || (!isRightFooted && cross > 0);

        if (requiresWeakFoot) score -= (5.0f - npc.getWeakFootAccuracy()) * 50.f;

        float bodyAlignment = PlayerAI::dot(facing, passDir);
        if (bodyAlignment < -0.2f) score -= 500.f;

        bool directLaneBlocked = false;
        bool canCurlAround = false;
        float worstEffectiveMarkerDist = 9999.f;

        sf::Vector2f passVector = aimSpot - npcPos;
        float passLengthSq = (passVector.x * passVector.x) + (passVector.y * passVector.y);

        for (auto* opp : opposition) {
            sf::Vector2f oppPos = opp->getPosition();

            if (passLengthSq > 0.001f) {
                sf::Vector2f toOpp = oppPos - npcPos;
                float t = std::clamp((toOpp.x * passVector.x + toOpp.y * passVector.y) / passLengthSq, 0.0f, 1.0f);
                sf::Vector2f closestPointOnLine = npcPos + (passVector * t);
                float orthoDist = PlayerAI::dist(oppPos, closestPointOnLine);

                if (orthoDist < 300.f && t > 0.05f && t < 0.95f) {
                    directLaneBlocked = true;
                    if (orthoDist > 150.f && curlNorm > 0.5f && forwardProgress > -300.f) {
                        canCurlAround = true;
                    }
                }
            }

            float rawDistToTarget = PlayerAI::dist(aimSpot, oppPos);
            if (rawDistToTarget < 1200.f) {
                float defenderClosingDistance = 850.f * leadTime;
                float effectiveDist = rawDistToTarget - defenderClosingDistance;

                sf::Vector2f toOppFromTarget = oppPos - aimSpot;
                sf::Vector2f toOppNorm = (rawDistToTarget > 0.1f) ? (toOppFromTarget / rawDistToTarget) : sf::Vector2f(0.f, 0.f);
                float passDotOpp = (passDir.x * toOppNorm.x + passDir.y * toOppNorm.y);

                if (!goHigh) {
                    if (passDotOpp < -0.3f) {
                        effectiveDist -= 300.f;
                    }
                    else if (passDotOpp > 0.3f) {
                        float bodyStrengthNorm = target->getBodyStrength() / 100.f;
                        effectiveDist += 150.f + (bodyStrengthNorm * 250.f);
                    }
                }

                if (effectiveDist < worstEffectiveMarkerDist) {
                    worstEffectiveMarkerDist = effectiveDist;
                }
            }
        }

        bool wantHigh = (goHigh || (directLaneBlocked && !canCurlAround));
        if (forwardProgress < -200.f) wantHigh = false;

        if (directLaneBlocked && !wantHigh && !canCurlAround) score -= 10000.f;
        if (wantHigh && exactDist < 1500.f) score -= 2500.f;

        float effectiveMarkerDist = worstEffectiveMarkerDist;

        // ==========================================
        // --- THE FIX 3: DEFENSIVE PARANOIA SCORING ---
        // ==========================================
        if (inDefensiveThird) {
            // Absolute Zero Tolerance for risk in our own third
            if (effectiveMarkerDist < 200.f) score -= 8000.f; // Excludes virtually all covered targets
            else if (effectiveMarkerDist < 400.f) score -= 500.f;

            // THE FIX 4: REWARDING THE WIDE OUTLET
            PositionRole targetRole = target->getPositionRole();
            bool isTargetFB = (targetRole == PositionRole::LeftBack || targetRole == PositionRole::RightBack ||
                targetRole == PositionRole::LeftWingBack || targetRole == PositionRole::RightWingBack);

            // If it's a fullback hugging the touchline (within 12m / 1200px of the edge)
            if (isTargetFB) {
                if (targetPos.y < pitch.margin + 1200.f || targetPos.y > pitch.totalHeight - pitch.margin - 1200.f) {
                    // Massive safety bonus to cycle the ball wide
                    score += 3000.f * shortPassNorm;
                }
            }
        }
        else {
            // Normal field logic
            if (effectiveMarkerDist < 250.f) score -= 3500.f * visionNorm;
            else if (effectiveMarkerDist < 500.f) score -= 1500.f * visionNorm;
        }

        if (effectiveMarkerDist > 800.f && !directLaneBlocked && !isOffside && !inEnemyBox) {
            score += 1500.f * visionNorm;
            if (exactDist < 1800.f && !goHigh) {
                score += 2500.f * visionNorm * shortPassNorm * (1.0f + tikiTakaPref);
            }
        }

        if (exactDist < 150.f) {
            score -= 20000.f;
        }
        else if (exactDist < 800.f && !goHigh && !directLaneBlocked && !isOffside) {
            if ((isCrammed || presserDist < 500.f) && effectiveMarkerDist > presserDist + 100.f) {
                score += 6000.f * shortPassNorm;
            }
            else if (effectiveMarkerDist > 400.f) {
                score += 2500.f * passSpeedPref * shortPassNorm;
            }
        }

        float lineBreakBonus = calculateLineBreakerValue(npcPos, aimSpot, opposition, isHome);
        score += lineBreakBonus * visionNorm * (0.2f + (attackSpeedPref * 0.8f));

        float spacePassProgress = isHome ? (aimSpot.x - targetPos.x) : (targetPos.x - aimSpot.x);
        bool inMiddlePark = (npcPos.x > (pitch.totalWidth * 0.33f) && npcPos.x < (pitch.totalWidth * 0.66f));

        if (spacePassProgress > 300.f && !goHigh) {
            if (inMiddlePark && effectiveMarkerDist < 1200.f) {
                score -= spacePassProgress * 5.0f;
            }
            else if (effectiveMarkerDist > 500.f) {
                score += spacePassProgress * 3.0f * visionNorm * attackSpeedPref;
            }
        }
        else if (inMiddlePark && std::abs(spacePassProgress) < 150.f) {
            score += 2000.f * visionNorm;
        }

        float trapPenalty = calculateReceiverPressureTrend(aimSpot, leadTime, opposition);
        score -= trapPenalty * visionNorm;

        float triangulationScore = evaluateTriangulation(target, team, opposition);
        score += triangulationScore * visionNorm;

        float orientationBonus = getReceiverOrientationBonus(target, goalPos);
        score += orientationBonus * visionNorm;

        bool isPasserCB = (npc.getPositionRole() == PositionRole::CenterBack);
        float pitchThirdX = isHome ? (pitch.totalWidth / 3.f) : (pitch.totalWidth * 0.66f);
        bool outOfPosition = isHome ? (npcPos.x > pitchThirdX) : (npcPos.x < pitchThirdX);

        if (isPasserCB && outOfPosition) {
            if (effectiveMarkerDist > 800.f && forwardProgress > -200.f && !goHigh) score += 1500.f * visionNorm;
            if (goHigh || forwardProgress > 2500.f) score -= 3000.f;
        }

        bool isPasserGK = (npc.getPositionRole() == PositionRole::Goalkeeper);

        if ((isPasserCB || isPasserGK) && inOwnDeepBox) {
            PositionRole targetRole = target->getPositionRole();
            bool isTargetFB = (targetRole == PositionRole::LeftBack || targetRole == PositionRole::RightBack || targetRole == PositionRole::LeftWingBack || targetRole == PositionRole::RightWingBack);
            bool isTargetMid = (targetRole == PositionRole::CenterMid || targetRole == PositionRole::DefensiveMid);
            bool isTargetCB = (targetRole == PositionRole::CenterBack);
            bool isTargetGK = (targetRole == PositionRole::Goalkeeper);

            if (isTargetMid && effectiveMarkerDist > 800.f && !goHigh) score += 2000.f * visionNorm;
            else if (isTargetFB && effectiveMarkerDist > 600.f) score += 1200.f * visionNorm;
            else if (isTargetCB || isTargetGK) {
                if (effectiveMarkerDist < 1000.f) score -= 4000.f;
                else score -= 500.f;
            }
        }

        float timeScaleNorm = std::clamp(npc.getMatchTimeScale() / 10.0f, 0.2f, 3.0f);

        if (riskMultiplier > 0.0f) {
            score += forwardProgress * (0.1f + (attackSpeedPref * 0.8f) + (riskMultiplier * 1.0f)) * timeScaleNorm;
        }
        else {
            score += std::abs(forwardProgress) * (riskMultiplier * 0.5f);
            score += (1000.f - exactDist) * std::abs(riskMultiplier * 0.8f);
            if (forwardProgress > 0.f) score += forwardProgress * (0.1f + (attackSpeedPref * 0.3f)) * timeScaleNorm;
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
                    if (effectiveMarkerDist > 1000.f) score += 1500.f * visionNorm * longPassNorm;
                    else score -= 3000.f;
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

        float penUrgency = teamAI.getPenetrationUrgency();
        if (penUrgency > 0.0f) {
            if (forwardProgress > 400.f && !goHigh) {
                score += (forwardProgress * penUrgency * 4.0f) * visionNorm;
            }
            if (forwardProgress < -200.f) {
                score -= (std::abs(forwardProgress) * penUrgency * 6.0f);
            }
            if (lineBreakBonus > 0.f) {
                score += (lineBreakBonus * penUrgency * 2.0f);
            }
        }

        if (score > bestScore) {
            bestScore = score;
            bestOption = target;
        }
    }

    float pickiness = 1.0f - passSpeedPref;
    float minimumAcceptableScore = 1200.f + (pickiness * 3000.f);

    float penUrgency = teamAI.getPenetrationUrgency();
    if (penUrgency > 0.0f) {
        minimumAcceptableScore -= (penUrgency * 2500.f);
    }

    if (noseBleedFactor > 0.0f) {
        minimumAcceptableScore -= (noseBleedFactor * 4000.f);
    }

    if (state.subState == TacticalSubState::TimeWasting) {
        minimumAcceptableScore = 4500.f;
    }
    else if (state.subState == TacticalSubState::KeepPossession) {
        minimumAcceptableScore = std::max(minimumAcceptableScore, 2500.f);
    }
    else if (state.subState == TacticalSubState::AllOut) {
        minimumAcceptableScore = -8000.f;
    }
    else if (isCrammed) {
        minimumAcceptableScore = -1500.f;
    }

    if (inEnemyBox && state.subState != TacticalSubState::TimeWasting && state.subState != TacticalSubState::AllOut) {
        minimumAcceptableScore = 1000.f;
    }

    return (bestScore > minimumAcceptableScore) ? bestOption : nullptr;
}

sf::Vector2f PossessionAI::calculateDribbleDirection(NPCPlayer& npc, sf::Vector2f goalPos, const std::vector<Player*>& opposition, const Pitch& pitch, const TeamAI& teamAI) {
    sf::Vector2f npcPos = npc.getPosition();
    bool isHomeSide = (npc.getTeam() == Team::Home);

    // ==========================================
    // --- NOSE-BLEED RE-CALC ---
    // ==========================================
    bool isDeepDefender = (npc.getPositionRole() == PositionRole::CenterBack ||
        npc.getPositionRole() == PositionRole::LeftBack ||
        npc.getPositionRole() == PositionRole::RightBack ||
        npc.getPositionRole() == PositionRole::LeftWingBack ||
        npc.getPositionRole() == PositionRole::RightWingBack ||
        npc.getPositionRole() == PositionRole::DefensiveMid);

    float pitchHalfX = pitch.totalWidth / 2.f;
    float noseBleedFactor = 0.0f;
    if (isDeepDefender) {
        float excursion = isHomeSide ? (npcPos.x - pitchHalfX) : (pitchHalfX - npcPos.x);
        float tolerance = (npc.getPositionRole() == PositionRole::CenterBack) ? -800.f : 800.f;
        if (excursion > tolerance) {
            noseBleedFactor = std::clamp((excursion - tolerance) / 2500.f, 0.0f, 1.0f);
        }
    }

    TeamState state = teamAI.getCurrentState();
    sf::Vector2f targetGoal = goalPos;

    if (state.subState == TacticalSubState::TimeWasting) {
        float cornerX = isHomeSide ? pitch.totalWidth - pitch.margin : pitch.margin;
        float topCornerY = pitch.margin;
        float botCornerY = pitch.totalHeight - pitch.margin;

        targetGoal = sf::Vector2f(cornerX, (npcPos.y < pitch.totalHeight / 2.f) ? topCornerY : botCornerY);
    }

    sf::Vector2f baseDir = PlayerAI::normalize(targetGoal - npcPos);

    float bcNorm = npc.getBallControl() / 100.f;
    float agilityNorm = npc.getAgility() / 100.f;
    float speedNorm = npc.getTopSpeed() / 100.f;

    float attackSpeedPref = teamAI.getAttackingSpeedPref();
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

    if ((boundaryPush.x != 0.f || boundaryPush.y != 0.f) && state.subState != TacticalSubState::TimeWasting) {
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
    bool isUnderSeverePressure = (overallMinOppDist < 350.f);

    for (int i = 0; i < 16; ++i) {
        float angleDeg = -135.f + (i * (270.f / 15.f));
        float rad = angleDeg * 3.14159f / 180.f;
        sf::Vector2f testDir(baseDir.x * std::cos(rad) - baseDir.y * std::sin(rad), baseDir.x * std::sin(rad) + baseDir.y * std::cos(rad));

        float score = 0.f;

        if (isUnderSeverePressure && closestOpp) {
            float dotAway = (testDir.x * -toClosestOpp.x + testDir.y * -toClosestOpp.y);
            score = dotAway * 5000.f * awarenessNorm;
            score += (testDir.x * baseDir.x + testDir.y * baseDir.y) * 500.f;
        }
        else {
            score = (100.f + (attackSpeedPref * 500.f) + (behavior.runFrequency * 200.f)) * (testDir.x * baseDir.x + testDir.y * baseDir.y);
        }

        sf::Vector2f currentTargetDir = npc.getDribbleTargetDir();
        float stickiness = (testDir.x * currentTargetDir.x + testDir.y * currentTargetDir.y);
        if (stickiness > 0.95f) score += 400.f;

        if (overallMinOppDist < 250.f && !isFirstTouch) {
            float escapeAlignment = (testDir.x * -toClosestOpp.x + testDir.y * -toClosestOpp.y);
            float holdUpIntelligence = std::max(1.0f - behavior.dribbleBias, awarenessNorm);

            if (escapeAlignment > 0.5f) {
                float bodyStrengthNorm = npc.getBodyStrength() / 100.f;
                if (state.subState == TacticalSubState::TimeWasting) holdUpIntelligence *= 2.0f;
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

        float dribbleBiasAdj = behavior.dribbleBias;
        if (state.subState == TacticalSubState::Transition) dribbleBiasAdj = std::max(0.7f, dribbleBiasAdj);

        if (maxThreatInLane < 0.1f) {
            score += 2000.f * speedNorm * dribbleBiasAdj;
        }
        else {
            float fearFactor = 800.f - (dribbleBiasAdj * 500.f) - (bcNorm * 300.f);
            fearFactor = std::max(50.f, fearFactor);

            // ==========================================
            // --- THE FIX 4: NOSE-BLEED TERROR ---
            // ==========================================
            if (noseBleedFactor > 0.0f) {
                fearFactor *= (1.0f + (noseBleedFactor * 4.0f)); // Up to 500% terror when dribbling at an opponent
            }

            score -= maxThreatInLane * fearFactor;
        }

        if (closestOpp && overallMinOppDist < 400.f) {
            float testToOppDot = (testDir.x * toClosestOpp.x + testDir.y * toClosestOpp.y);

            if (testToOppDot < -0.2f) {
                score += 2500.f * speedNorm * dribbleBiasAdj * std::abs(testToOppDot);
            }
            else if (testToOppDot > 0.0f) {
                // ==========================================
                // --- THE FIX 5: DON'T BE A HERO ---
                // ==========================================
                // If a defender is nose-bleeding, heavily punish ANY attempt to dribble towards an opponent
                if (noseBleedFactor > 0.0f && testToOppDot > 0.1f) {
                    score -= 15000.f * noseBleedFactor * testToOppDot;
                }
                else if (isTrickster && overallMinOppDist < 250.f) {
                    float orthogonality = 1.0f - std::abs(testToOppDot);
                    if (orthogonality > 0.8f) score += 2500.f * bcNorm * agilityNorm * dribbleBiasAdj * orthogonality;
                    if (testToOppDot > 0.6f) score -= 4000.f;
                }
                else if (isSpeedster) {
                    if (testToOppDot > 0.4f && testToOppDot < 0.85f) score += 1800.f * speedNorm * dribbleBiasAdj;
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
        else if (minBoundDist < 500.f && state.subState != TacticalSubState::TimeWasting) {
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

void PossessionAI::executePass(NPCPlayer& npc, Ball& ball, Player* target, const std::vector<Player*>& opposition, const Pitch& pitch, SoundManager& soundManager, MatchStatistics& stats, const TeamAI& teamAI) {
    sf::Vector2f npcPos = npc.getPosition();
    sf::Vector2f targetPos = target->getPosition();
    sf::Vector2f targetVel = target->getVelocity();

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
        if (isBackpass) arrivalSpeed = std::clamp(exactDist * 0.5f, 500.f, 650.f);

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

    // ==========================================
    // --- THE FIX 3: POSSESSION COMPLACENCY ---
    // ==========================================
    // If the team has held the ball for more than 20 seconds, mental fatigue and complacency set in.
    // The passing error angle slowly increases, ensuring infinite passing loops eventually fail.
    float phaseTime = teamAI.getCurrentState().phase == MatchPhase::Attacking ?
        (teamAI.getPhaseTimer()) : 0.0f;

    float complacencyModifier = 0.0f;

    if (phaseTime > 20.0f) complacencyModifier = std::clamp((phaseTime - 20.0f) / 30.0f, 0.0f, 1.0f);

    errorAngle += (complacencyModifier * 5.0f);

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
    dispatchNPCKick(npc, ball, finalAim, finalPower, intentionalSpin, finalVz, finalBs, true, false, false, soundManager);
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

    dispatchNPCKick(npc, ball, aimDir, finalPower, finalSpin, vzPower, finalBackspin, false, true, onTarget, soundManager);
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

bool PossessionAI::tryNPCAerialStrike(NPCPlayer& npc, Ball& ball, sf::Vector2f aimDir, bool isShot, SoundManager& soundManager, MatchStatistics& stats, const Pitch& pitch) {
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

    bool isHome = npc.getTeam() == Team::Home;
    bool inOwnBox = isHome ? (npcPos.x < 1650.f) : (npcPos.x > 8350.f);
    bool isClearance = (!isShot && inOwnBox && incomingSpeed > 300.f);

    if (isHeader) {
        if (isShot) {
            basePower = (30.f + (activeStat * 0.5f)) * std::max(0.3f, simulatedCharge);
            vzOut = 100.f - (activeStat * 3.0f);
        }
        else if (isClearance) {
            basePower = 55.f + (activeStat * 0.3f);
            vzOut = 450.f;
        }
        else {
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
            basePower = npc.getKickPower() * 0.85f;
            vzOut = 600.f;
        }
        else {
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

    // ==========================================
    // --- THE FIX 3: RECORD AERIAL STATS ---
    // ==========================================
    if (isShot) {
        float targetLineX = isHome ? pitch.totalWidth - pitch.margin : pitch.margin;
        bool onTarget = false;

        // Perform a quick geometric check to see if the aerial shot is on goal
        if (std::abs(finalDir.x) > 0.001f) {
            float t = (targetLineX - npc.getPosition().x) / finalDir.x;
            if (t > 0.f) {
                float intersectY = npc.getPosition().y + (finalDir.y * t);
                if (intersectY > 3500.f - 366.f && intersectY < 3500.f + 366.f) {
                    onTarget = true;
                }
            }
        }
        stats.recordShot(npc.getTeam(), onTarget);
    }
    else if (!isClearance) {
        // If it's not a shot and not a desperate clearance, it's a pass!
        stats.recordPassAttempt(npc.getTeam());
    }

    float kickVol = std::clamp(0.f + (basePower / npc.getKickPower()) * 20.0f, 10.f, 100.f);
    soundManager.playRandomSound("kick", 3, kickVol, 0.15f);

    ball.shoot(finalDir, basePower, 0.0f, vzOut, finalBackspin);
    npc.resetKickCooldown();
    return true;
}

void PossessionAI::executeSetPiece(NPCPlayer& npc, Ball& ball, const std::vector<Player*>& teammates, const std::vector<Player*>& opposition, const Pitch& pitch, MatchState state, SoundManager& soundManager, MatchStatistics& stats, const TeamAI& teamAI) {
    sf::Vector2f npcPos = npc.getPosition();
    bool isHome = (npc.getTeam() == Team::Home);
    float pitchCenterY = pitch.totalHeight / 2.f;

    // Home attacks X+, Away attacks X-
    float goalLineX = isHome ? (pitch.totalWidth - pitch.margin) : pitch.margin;

    sf::Vector2f aimSpot;
    float vzPower = 600.f;
    float spin = 0.f;
    float basePower = 0.f;
    bool isCross = false;

    if (state == MatchState::Corner) {
        isCross = true;

        // Target the "Mixer" - approx 8 meters (800px) out from the center of the goal
        float mixerX = isHome ? (goalLineX - 800.f) : (goalLineX + 800.f);

        // Find the best aerial threat currently inside the penalty box
        Player* bestTarget = nullptr;
        float bestHeading = 0.f;

        for (Player* tm : teammates) {
            if (tm == &npc || tm->isSentOff()) continue;

            bool inBox = isHome ? (tm->getPosition().x > pitch.totalWidth - pitch.margin - 1650.f) : (tm->getPosition().x < pitch.margin + 1650.f);
            if (inBox) {
                float headingScore = tm->getHeading() + (tm->getBodyStrength() * 0.5f);
                if (headingScore > bestHeading) {
                    bestHeading = headingScore;
                    bestTarget = tm;
                }
            }
        }

        if (bestTarget) {
            // Blend the generic mixer coordinates with the specific target's coordinates
            aimSpot = (sf::Vector2f(mixerX, pitchCenterY) * 0.4f) + (bestTarget->getPosition() * 0.6f);
        }
        else {
            aimSpot = sf::Vector2f(mixerX, pitchCenterY);
        }

        // High trajectory to clear the near-post defenders
        vzPower = 950.f;

        // Curl logic: In-swinger vs Out-swinger
        bool rightFooted = (npc.getPreferredFoot() == "Right");
        bool takingFromLeft = (npcPos.y < pitchCenterY); // Y- is the left side of the pitch

        float curlNorm = npc.getCurl() / 100.f;
        float maxSpin = 45.f * curlNorm;

        // Apply bend towards or away from the goal based on foot and pitch side
        if (takingFromLeft) {
            spin = rightFooted ? maxSpin : -maxSpin;
        }
        else {
            spin = rightFooted ? -maxSpin : maxSpin;
        }
    }
    else if (state == MatchState::FreeKick) {
        float distToGoalLine = std::abs(npcPos.x - goalLineX);
        bool isWide = (npcPos.y < pitchCenterY - 1200.f || npcPos.y > pitchCenterY + 1200.f);

        if (distToGoalLine < 3500.f && isWide) {
            // Wide free kick in the attacking half -> Treat like a deep corner
            isCross = true;
            float mixerX = isHome ? (goalLineX - 1000.f) : (goalLineX + 1000.f);
            aimSpot = sf::Vector2f(mixerX, pitchCenterY);
            vzPower = 850.f;
        }
        else if (distToGoalLine > 5000.f) {
            // Deep free kick in own half -> Boom it to the target man
            isCross = true;
            Player* bestTarget = nullptr;
            float bestHeading = 0.f;

            for (Player* tm : teammates) {
                if (tm == &npc || tm->isSentOff()) continue;
                // Only target players in the opponent's half
                if ((isHome && tm->getPosition().x > pitchCenterY) || (!isHome && tm->getPosition().x < pitchCenterY)) {
                    if (tm->getHeading() > bestHeading) {
                        bestHeading = tm->getHeading();
                        bestTarget = tm;
                    }
                }
            }

            if (bestTarget) {
                // Lead the target slightly towards goal
                sf::Vector2f lead = isHome ? sf::Vector2f(400.f, 0.f) : sf::Vector2f(-400.f, 0.f);
                aimSpot = bestTarget->getPosition() + lead;
            }
            else {
                aimSpot = sf::Vector2f(pitch.totalWidth / 2.f, pitchCenterY);
            }
            vzPower = 1000.f;
        }
        else {
            // Standard central free kick (too far to shoot) -> Pass it to feet
            Player* passTarget = PossessionAI::findBestPassOption(npc, teammates, opposition, nullptr, teamAI, pitch);
            if (passTarget) {
                PossessionAI::executePass(npc, ball, passTarget, opposition, pitch, soundManager, stats, teamAI);
                return;
            }
            else {
                // Fallback dump into the box
                aimSpot = isHome ? sf::Vector2f(goalLineX - 2000.f, pitchCenterY) : sf::Vector2f(goalLineX + 2000.f, pitchCenterY);
                isCross = true;
                vzPower = 800.f;
            }
        }
    }

    // --- EXACT KINEMATIC DELIVERY SOLVER ---
    if (isCross) {
        float exactDist = PlayerAI::dist(npcPos, aimSpot);

        float timeInAir = (2.f * vzPower) / 980.f;
        float reqSpeed = exactDist / timeInAir;

        float idealPower = reqSpeed * 1.15f; // Drag tax
        basePower = idealPower / 52.0f;
        basePower = std::clamp(basePower, 10.f, npc.getKickPower() * 1.1f);

        sf::Vector2f passDir = PlayerAI::normalize(aimSpot - npcPos);

        // Standard deviation based on passing stats
        float accuracyNorm = npc.getLongPassing() / 100.f;
        float errorAngle = (1.0f - accuracyNorm) * 5.0f;
        float randError = ((rand() % 100) / 100.f - 0.5f) * errorAngle;
        float rad = randError * 3.14159f / 180.f;

        sf::Vector2f finalAim(
            passDir.x * std::cos(rad) - passDir.y * std::sin(rad),
            passDir.x * std::sin(rad) + passDir.y * std::cos(rad)
        );

        float backspin = 40.f + (accuracyNorm * 30.f);

        float kickVol = std::clamp(0.0f + (basePower / npc.getKickPower()) * 40.0f, 10.f, 100.f);
        soundManager.playRandomSound("kick", 3, kickVol, 0.15f);

        dispatchNPCKick(npc, ball, finalAim, basePower, spin, vzPower, backspin, true, false, false, soundManager);
    }
}
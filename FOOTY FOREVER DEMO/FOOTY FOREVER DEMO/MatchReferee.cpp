#include "MatchReferee.h"
#include "Player.h"
#include "Ball.h"
#include "Pitch.h"
#include <iostream>
#include "SoundManager.h"
#include "MatchEnvironment.h"
#include "GlobalSettings.h"
#include "MatchInfo.h"

void MatchReferee::update(float dt, MatchEnvironment& env)
{

    if (m_matchState == MatchState::InPlay || m_matchState == MatchState::GoalScored) {
        m_matchMinute += (dt * m_timeScale) / 60.0f;

        if (m_matchMinute >= 45.0f && m_half == 1 && m_matchState == MatchState::InPlay) {
            m_matchState = MatchState::HalfTime;
            m_whistleTimer = 5.0f;
            updateMatchContexts();
            m_lastInfraction = FoulType::None;
            env.sound->playSound("ref_fulltime", 100.f);
            std::cout << "HALF TIME!\n";
        }
        else if (m_matchMinute >= 90.0f && m_half == 2 && m_matchState == MatchState::InPlay) {
            m_matchState = MatchState::FullTime;
            updateMatchContexts();
            env.sound->playSound("ref_fulltime", 100.f);
            std::cout << "FULL TIME!\n";
        }
    }

    if (m_matchState == MatchState::InPlay) {
        checkBoundaries(env);
        if (m_matchState != MatchState::InPlay) {
            prepareRestart(m_matchState, *env.ball, *env.pitch, *env.allPlayers, *env.sound);
        }
    }
    else if (m_matchState == MatchState::FoulDelay) {
        m_foulDelayTimer -= dt;
        if (m_foulDelayTimer <= 0.f) m_matchState = MatchState::RequestReplay;
    }
    else if (m_matchState == MatchState::GoalScored) {
        m_whistleTimer -= dt;
        if (m_whistleTimer <= 0.f) {
            m_restartPos = sf::Vector2f(env.pitch->totalWidth / 2.f, 3500.f);
            m_lastInfraction = FoulType::None;
            m_matchState = MatchState::RequestReplay;
        }
    }
    else if (m_matchState == MatchState::OutOfBoundsDelay) {
        m_whistleTimer -= dt;
        if (m_whistleTimer <= 0.f) {
            m_lastInfraction = FoulType::None;
            if ((rand() % 100) < 30) m_matchState = MatchState::RequestReplay;
            else prepareRestart(m_pendingState, *env.ball, *env.pitch, *env.allPlayers, *env.sound);
        }
    }
    else if (m_matchState == MatchState::HalfTime) {
        m_whistleTimer -= dt;
        if (m_whistleTimer <= 0.f) {
            m_half = 2;
            m_matchMinute = 45.0f;
            m_awardedTo = Team::Away;
            m_restartPos = sf::Vector2f(env.pitch->totalWidth / 2.f, 3500.f);
            m_lastInfraction = FoulType::None;
            prepareRestart(MatchState::KickOff, *env.ball, *env.pitch, *env.allPlayers, *env.sound);
        }
    }
    else if (m_matchState == MatchState::FullTime) {
        // Handled by MatchEngine
    }
    else {
        bool justBlew = (m_whistleTimer > 0.f && m_whistleTimer - dt <= 0.f);
        m_whistleTimer -= dt;

        if (justBlew) {
            env.sound->playSound("ref_whistle", 100.f);
        }

        if (m_whistleTimer <= 0.f) {
            float speed = std::sqrt(env.ball->velocity.x * env.ball->velocity.x + env.ball->velocity.y * env.ball->velocity.y + env.ball->vz * env.ball->vz);

            if (env.ball->getOwner() == nullptr && speed > 15.0f) {
                m_matchState = MatchState::InPlay;
                m_setPieceTaker = nullptr;
                updateMatchContexts();
                return;
            }
        }

        // ==========================================
        // --- THE FIX: BALL COLLISION IMMUNITY ---
        // ==========================================
        if (m_whistleTimer > 0.f) {
            if (m_setPieceTaker) m_setPieceTaker->setVelocity({ 0.f, 0.f });
            env.ball->setPosition(m_restartPos);
            env.ball->setVelocity({ 0.f, 0.f });
            env.ball->vz = 0.f;
        }
        else {
            float speed = std::sqrt(env.ball->velocity.x * env.ball->velocity.x + env.ball->velocity.y * env.ball->velocity.y + env.ball->vz * env.ball->vz);
            if (speed < 15.0f) {
                env.ball->setPosition(m_restartPos);
                env.ball->setVelocity({ 0.f, 0.f });
                env.ball->vz = 0.f;
            }
        }

        if (m_matchState == MatchState::FreeKick || m_matchState == MatchState::Corner ||
            m_matchState == MatchState::Penalty || m_matchState == MatchState::KickOff ||
            m_matchState == MatchState::ThrowIn)
        {
            sf::Vector2f targetGoal = (m_awardedTo == Team::Home) ? env.pitch->awayGoalCenter : env.pitch->homeGoalCenter;
            float distToTargetGoal = std::sqrt(std::pow(m_restartPos.x - targetGoal.x, 2) + std::pow(m_restartPos.y - targetGoal.y, 2));
            bool needsWall = (m_matchState == MatchState::FreeKick && distToTargetGoal < 2500.f);

            int wallCount = 0;
            int maxWallPlayers = (distToTargetGoal < 1800.f) ? 4 : 3;

            sf::Vector2f toGoal = targetGoal - m_restartPos;
            float lenToGoal = std::sqrt(toGoal.x * toGoal.x + toGoal.y * toGoal.y);
            if (lenToGoal > 0.1f) toGoal /= lenToGoal;
            sf::Vector2f wallCenter = m_restartPos + (toGoal * 915.f);
            sf::Vector2f perp(-toGoal.y, toGoal.x);

            for (Player* p : *env.allPlayers)
            {
                if (p == m_setPieceTaker || p->getTeam() == m_awardedTo || p->isSentOff() || p->getState() == PlayerState::Injured) continue;

                bool isDefenderOrMid = (p->getPositionRole() != PositionRole::Goalkeeper &&
                    p->getPositionRole() != PositionRole::Striker &&
                    p->getPositionRole() != PositionRole::CenterForward &&
                    p->getPositionRole() != PositionRole::LeftWing &&
                    p->getPositionRole() != PositionRole::RightWing);

                if (needsWall && wallCount < maxWallPlayers && isDefenderOrMid)
                {
                    float offset = (wallCount - (maxWallPlayers / 2.0f) + 0.5f) * 60.f;
                    sf::Vector2f targetPos = wallCenter + (perp * offset);

                    targetPos.x = std::clamp(targetPos.x, env.pitch->margin, env.pitch->totalWidth - env.pitch->margin);
                    targetPos.y = std::clamp(targetPos.y, env.pitch->margin, env.pitch->totalHeight - env.pitch->margin);

                    p->setPosition(targetPos);
                    p->setVelocity({ 0.f, 0.f });
                    wallCount++;
                    continue;
                }

                if (m_matchState != MatchState::Penalty) {
                    sf::Vector2f pPos = p->getPosition();
                    float dx = pPos.x - m_restartPos.x;
                    float dy = pPos.y - m_restartPos.y;
                    float distSq = dx * dx + dy * dy;

                    float reqDist = (m_matchState == MatchState::ThrowIn) ? 200.f : 915.f;

                    if (distSq < (reqDist * reqDist)) {
                        float dist = std::sqrt(distSq);
                        if (dist > 0.1f) {
                            sf::Vector2f pushDir = { dx / dist, dy / dist };
                            sf::Vector2f targetPos = m_restartPos + (pushDir * reqDist);

                            if (targetPos.x < env.pitch->margin || targetPos.x > env.pitch->totalWidth - env.pitch->margin ||
                                targetPos.y < env.pitch->margin || targetPos.y > env.pitch->totalHeight - env.pitch->margin)
                            {
                                sf::Vector2f center(env.pitch->totalWidth / 2.f, env.pitch->totalHeight / 2.f);
                                sf::Vector2f toCenter = center - pPos;
                                float centerLen = std::sqrt(toCenter.x * toCenter.x + toCenter.y * toCenter.y);

                                if (centerLen > 0.1f) {
                                    pushDir += (toCenter / centerLen) * 0.8f;
                                    float newLen = std::sqrt(pushDir.x * pushDir.x + pushDir.y * pushDir.y);
                                    pushDir /= newLen;
                                    targetPos = m_restartPos + (pushDir * reqDist);
                                }
                            }

                            targetPos.x = std::clamp(targetPos.x, env.pitch->margin, env.pitch->totalWidth - env.pitch->margin);
                            targetPos.y = std::clamp(targetPos.y, env.pitch->margin, env.pitch->totalHeight - env.pitch->margin);

                            p->setPosition(targetPos);
                            p->setVelocity({ 0.f, 0.f });
                        }
                    }
                }
            }
        }
    }
}

void MatchReferee::checkBoundaries(MatchEnvironment& env)
{
    sf::Vector2f bPos = env.ball->getPosition();
    sf::Vector2f bVel = env.ball->getVelocity();
    Player* lastTouch = env.ball->lastTouch;

    bool isOutY = (bPos.y < env.pitch->margin || bPos.y > env.pitch->totalHeight - env.pitch->margin);
    bool movingInwardsY = (bPos.y < env.pitch->margin && bVel.y > 0.f) ||
        (bPos.y > env.pitch->totalHeight - env.pitch->margin && bVel.y < 0.f);

    // ==========================================
    // --- THE FIX: ADD DELAY FOR THROW-INS ---
    // ==========================================
    if (isOutY && !movingInwardsY) {
        m_pendingState = MatchState::ThrowIn;
        float safeY = (bPos.y < env.pitch->margin) ? env.pitch->margin : env.pitch->totalHeight - env.pitch->margin;
        float safeX = std::clamp(bPos.x, env.pitch->margin, env.pitch->totalWidth - env.pitch->margin);
        m_restartPos = sf::Vector2f(safeX, safeY);

        if (lastTouch != nullptr) m_awardedTo = (lastTouch->getTeam() == Team::Home) ? Team::Away : Team::Home;
        else m_awardedTo = (bPos.x > env.pitch->halfwayLineX) ? Team::Home : Team::Away;

        m_matchState = MatchState::OutOfBoundsDelay;
        m_whistleTimer = 1.5f;
        return;
    }

    bool isOutX = (bPos.x < env.pitch->margin || bPos.x > env.pitch->totalWidth - env.pitch->margin);

    if (isOutX) {
        if (checkGoalScored(env)) {
            m_matchState = MatchState::GoalScored;
            m_pendingState = MatchState::KickOff;
            m_whistleTimer = 2.5f;
            return;
        }

        bool movingInwardsX = (bPos.x < env.pitch->margin && bVel.x > 0.f) ||
            (bPos.x > env.pitch->totalWidth - env.pitch->margin && bVel.x < 0.f);

        if (!movingInwardsX) {
            bool isHomeEnd = (bPos.x < env.pitch->margin);
            const Goal& activeGoal = isHomeEnd ? *env.homeGoal : *env.awayGoal;

            float goalTopY = activeGoal.center.y - 366.f;
            float goalBottomY = activeGoal.center.y + 366.f;

            if (bPos.y >= goalTopY - 2.f && bPos.y <= goalBottomY + 2.f && env.ball->z <= 249.f) {
                return;
            }

            if (lastTouch == nullptr) {
                m_pendingState = MatchState::GoalKick;
                m_awardedTo = isHomeEnd ? Team::Home : Team::Away;
                m_restartPos = isHomeEnd ? sf::Vector2f(env.pitch->margin + 600.f, 3500.f)
                    : sf::Vector2f(env.pitch->totalWidth - env.pitch->margin - 600.f, 3500.f);
            }
            else {
                bool lastTouchedByHome = (lastTouch->getTeam() == Team::Home);

                // ==========================================
                // --- THE FIX: EXACT CORNER COORDINATES ---
                // ==========================================
                if (isHomeEnd) {
                    if (lastTouchedByHome) {
                        m_pendingState = MatchState::Corner;
                        m_awardedTo = Team::Away;
                        m_restartPos = (bPos.y < 3500.f) ? sf::Vector2f(env.pitch->margin, env.pitch->margin)
                            : sf::Vector2f(env.pitch->margin, env.pitch->totalHeight - env.pitch->margin);
                    }
                    else {
                        m_pendingState = MatchState::GoalKick;
                        m_awardedTo = Team::Home;
                        m_restartPos = sf::Vector2f(env.pitch->margin + 600.f, 3500.f);
                    }
                }
                else {
                    if (lastTouchedByHome) {
                        m_pendingState = MatchState::GoalKick;
                        m_awardedTo = Team::Away;
                        m_restartPos = sf::Vector2f(env.pitch->totalWidth - env.pitch->margin - 600.f, 3500.f);
                    }
                    else {
                        m_pendingState = MatchState::Corner;
                        m_awardedTo = Team::Home;
                        m_restartPos = (bPos.y < 3500.f) ? sf::Vector2f(env.pitch->totalWidth - env.pitch->margin, env.pitch->margin)
                            : sf::Vector2f(env.pitch->totalWidth - env.pitch->margin, env.pitch->totalHeight - env.pitch->margin);
                    }
                }
            }

            if (m_pendingState == MatchState::GoalKick) {
                if (m_matchState != MatchState::GoalScored) {
                    env.sound->playSound("crowd_miss", 80.f);
                }
            }

            m_matchState = MatchState::OutOfBoundsDelay;
            m_whistleTimer = 1.5f;
        }
    }
}

bool MatchReferee::checkGoalScored(MatchEnvironment& env) 
{
    sf::Vector2f bPos = env.ball->getPosition();
    if (env.ball->z > 244.f + 5.f) return false;

    float goalLineBuffer = 24.f;

    // ==========================================
    // --- SCORING IN THE HOME GOAL (AWAY TEAM SCORES) ---
    // ==========================================
    if (bPos.x < env.pitch->margin - goalLineBuffer) {
        float goalTopY = (*env.homeGoal).center.y - 366.f;
        float goalBottomY = (*env.homeGoal).center.y + 366.f;

        if (bPos.y >= goalTopY - 2.f && bPos.y <= goalBottomY + 2.f) {

            Player* scorer = env.ball->lastTouch;
            Player* assister = env.ball->assistCandidate;
            bool isOwnGoal = false;

            // Did a Home player put the ball in the Home net?
            if (scorer && scorer->getTeam() == Team::Home) {
                isOwnGoal = true;
                assister = nullptr;

                // --- SHOT ON TARGET OVERRIDE ---
                // If the Away team shot it, and it was going in anyway, it's a deflection, NOT an Own Goal!
                if (env.ball->lastShooter && env.ball->lastShooter->getTeam() == Team::Away && env.ball->lastShotWasOnTarget) {
                    isOwnGoal = false;
                    scorer = env.ball->lastShooter;
                    assister = env.ball->lastShooterAssister;
                }
            }

            m_awayScore++;

            m_lastGoalScorerName = scorer ? scorer->getName() : "Unknown";
            m_lastGoalAssistName = assister ? assister->getName() : "";
            m_lastGoalWasOwnGoal = isOwnGoal;
            m_lastGoalScoringTeam = Team::Away;

            if (isOwnGoal) std::cout << "OWN GOAL by " << m_lastGoalScorerName << "!\n";
            else if (assister) std::cout << "GOAL by " << m_lastGoalScorerName << " (Assist: " << m_lastGoalAssistName << ")!\n";
            else std::cout << "GOAL by " << m_lastGoalScorerName << "!\n";

            std::string sId = scorer ? scorer->getId() : "";
            std::string aId = assister ? assister->getId() : "";
            env.info->recordGoal(sId, aId, env.info->getAwayTeamId(), static_cast<int>(m_matchMinute), isOwnGoal, false);
            env.stats->recordGoal(Team::Away, m_lastGoalScorerName, static_cast<int>(m_matchMinute), m_lastGoalAssistName, isOwnGoal);

            env.sound->playSound("ref_whistle", 100.f);
            env.sound->playSound("crowd_goal", 100.f);
            m_awardedTo = Team::Home;
            return true;
        }
    }

    // ==========================================
    // --- SCORING IN THE AWAY GOAL (HOME TEAM SCORES) ---
    // ==========================================
    if (bPos.x > env.pitch->totalWidth - env.pitch->margin + goalLineBuffer) {
        float goalTopY = (*env.awayGoal).center.y - 366.f;
        float goalBottomY = (*env.awayGoal).center.y + 366.f;

        if (bPos.y >= goalTopY - 2.f && bPos.y <= goalBottomY + 2.f) {

            Player* scorer = env.ball->lastTouch;
            Player* assister = env.ball->assistCandidate;
            bool isOwnGoal = false;

            // Did an Away player put the ball in the Away net?
            if (scorer && scorer->getTeam() == Team::Away) {
                isOwnGoal = true;
                assister = nullptr;

                // --- SHOT ON TARGET OVERRIDE ---
                if (env.ball->lastShooter && env.ball->lastShooter->getTeam() == Team::Home && env.ball->lastShotWasOnTarget) {
                    isOwnGoal = false;
                    scorer = env.ball->lastShooter;
                    assister = env.ball->lastShooterAssister;
                }
            }

            m_homeScore++;

            m_lastGoalScorerName = scorer ? scorer->getName() : "Unknown";
            m_lastGoalAssistName = assister ? assister->getName() : "";
            m_lastGoalWasOwnGoal = isOwnGoal;
            m_lastGoalScoringTeam = Team::Home;

            if (isOwnGoal) std::cout << "OWN GOAL by " << m_lastGoalScorerName << "!\n";
            else if (assister) std::cout << "GOAL by " << m_lastGoalScorerName << " (Assist: " << m_lastGoalAssistName << ")!\n";
            else std::cout << "GOAL by " << m_lastGoalScorerName << "!\n";

            std::string sId = scorer ? scorer->getId() : "";
            std::string aId = assister ? assister->getId() : "";
            env.info->recordGoal(sId, aId, env.info->getHomeTeamId(), static_cast<int>(m_matchMinute), isOwnGoal, false);
            env.stats->recordGoal(Team::Home, m_lastGoalScorerName, static_cast<int>(m_matchMinute), m_lastGoalAssistName, isOwnGoal);

            env.sound->playSound("ref_whistle", 100.f);
            env.sound->playSound("crowd_goal", 100.f);
            m_awardedTo = Team::Away;
            return true;
        }
    }

    return false;
}

void MatchReferee::updateMatchContexts() {
    m_attackCtx = TacticalContext();
    m_defendCtx = TacticalContext();
    m_attackMask = PositioningMask();
    m_defendMask = PositioningMask();
}

TacticalContext MatchReferee::getTacticalContext(Team team, bool isTaker) const {
    TacticalContext ctx = (team == m_awardedTo) ? m_attackCtx : m_defendCtx;
    ctx.state = m_matchState;

    if (m_matchState != MatchState::InPlay) {
        if (m_matchState == MatchState::RequestReplay || m_matchState == MatchState::ReplayPlaying) {
            ctx.isTaker = false;
            ctx.canPossess = false;
            ctx.ballInfluence = 0.0f;
            ctx.canTackle = false;
            ctx.maxSpeedLimit = 0.f;
        }
        else if (isTaker) {
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
            ctx.ballInfluence = 0.0f;

            if (m_matchState == MatchState::KickOff || m_matchState == MatchState::Penalty || m_matchState == MatchState::FreeKick) {
                ctx.maxSpeedLimit = 0.f;
            }
            else {
                ctx.maxSpeedLimit = 350.f;
            }
        }
    }

    return ctx;
}

PositioningMask MatchReferee::getPositioningMask(const Player* p, const Pitch& pitch) const {
    PositioningMask mask;
    Team team = p->getTeam();
    PositionRole role = p->getPositionRole();
    sf::Vector2f homePos = p->getHomePosition();

    bool isAttacking = (team == m_awardedTo);
    bool isHome = (team == Team::Home);
    bool attackingHomeEnd = (m_awardedTo == Team::Away);

    float attackingGoalX = attackingHomeEnd ? pitch.margin : pitch.totalWidth - pitch.margin;
    float defendingGoalX = attackingHomeEnd ? pitch.totalWidth - pitch.margin : pitch.margin;
    float halfwayX = pitch.totalWidth / 2.f;
    float centerY = pitch.totalHeight / 2.f;

    MatchState effectiveState = (m_matchState == MatchState::ReplayPlaying ||
        m_matchState == MatchState::OutOfBoundsDelay ||
        m_matchState == MatchState::FoulDelay)
        ? m_pendingState : m_matchState;

    // --- THE FIX: TAKER MASK REMOVED ---
    // The taker's position is managed 100% by MatchReferee::prepareRestart now, preventing overlap!

    if (effectiveState == MatchState::Corner) {
        mask.useManualTarget = true;

        bool isFullback = (role == PositionRole::LeftBack || role == PositionRole::RightBack || role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack);
        bool isStriker = (role == PositionRole::Striker || role == PositionRole::CenterForward);
        bool isCB = (role == PositionRole::CenterBack);

        float relativeY = (homePos.y - centerY) * 0.6f;
        float sign = attackingHomeEnd ? 1.0f : -1.0f;
        float ownHalfDir = isHome ? -1.0f : 1.0f;

        int seed = static_cast<int>(role) * 7 + (isHome ? 13 : 31);
        float scatterX = static_cast<float>((seed % 400) - 200);
        float scatterY = static_cast<float>((seed % 300) - 150);

        if (isAttacking) {
            if (role == PositionRole::Goalkeeper) {
                mask.manualTarget.x = defendingGoalX;
                mask.manualTarget.y = centerY;
            }
            else if (isFullback) {
                mask.manualTarget.x = halfwayX + (ownHalfDir * 400.f);
                mask.manualTarget.y = homePos.y;
            }
            else if (isCB) {
                mask.manualTarget.x = attackingGoalX + (sign * 800.f) + scatterX;
                mask.manualTarget.y = centerY + (relativeY * 0.5f) + scatterY;
            }
            else {
                mask.manualTarget.x = attackingGoalX + (sign * 1300.f) + scatterX;
                mask.manualTarget.y = centerY + relativeY + scatterY;
            }
        }
        else {
            if (role == PositionRole::Goalkeeper) {
                mask.manualTarget.x = attackingGoalX;
                mask.manualTarget.y = centerY;
            }
            else if (isStriker) {
                mask.manualTarget.x = halfwayX + (ownHalfDir * 400.f);
                mask.manualTarget.y = homePos.y;
            }
            else if (isCB) {
                mask.manualTarget.x = attackingGoalX + (sign * 400.f) + (scatterX * 0.5f);
                mask.manualTarget.y = centerY + (relativeY * 0.5f) + scatterY;
            }
            else {
                mask.manualTarget.x = attackingGoalX + (sign * 1000.f) + scatterX;
                mask.manualTarget.y = centerY + relativeY + scatterY;
            }
        }
    }
    else if (effectiveState == MatchState::Penalty) {
        if (role != PositionRole::Goalkeeper && p != m_setPieceTaker) {
            mask.useManualTarget = true;
            float relativeY = (homePos.y - centerY) * 0.8f;
            float dArcEdgeX = attackingHomeEnd ? pitch.margin + 2200.f : pitch.totalWidth - pitch.margin - 2200.f;
            mask.manualTarget = sf::Vector2f(dArcEdgeX + (attackingHomeEnd ? 100.f : -100.f), centerY + relativeY);
        }
    }
    else if (effectiveState == MatchState::KickOff) {
        mask.forwardLeashMod = 0.25f;
    }
    else if (effectiveState == MatchState::GoalKick) {
        mask.useManualTarget = true;

        float relativeY = (homePos.y - centerY) * 0.8f;
        float scatterY = static_cast<float>(((static_cast<int>(role) * 7) % 300) - 150);

        // ==========================================
        // --- THE FIX: DIRECTIONAL PITCH MATH ---
        // ==========================================
        // +1.0 if pushing towards Away Goal (Home attacking), -1.0 if pushing towards Home Goal (Away attacking)
        float upPitchDir = (m_awardedTo == Team::Home) ? 1.0f : -1.0f;

        if (isAttacking) {
            if (role == PositionRole::Goalkeeper) {
                mask.manualTarget.x = defendingGoalX + (upPitchDir * 100.f);
                mask.manualTarget.y = centerY;
            }
            else if (role == PositionRole::CenterBack) {
                // THE FIX: Push CBs FAR out of the goalkeeper's box (Penalty box is 1650px deep)
                float pushY = (homePos.y < centerY) ? -1100.f : 1100.f;
                mask.manualTarget.x = defendingGoalX + (upPitchDir * 1850.f);
                mask.manualTarget.y = centerY + pushY;
            }
            else if (role == PositionRole::LeftBack || role == PositionRole::RightBack ||
                role == PositionRole::LeftWingBack || role == PositionRole::RightWingBack) {
                // Fullbacks push high and wide up the touchlines
                float pushY = (homePos.y < centerY) ? -1800.f : 1800.f;
                mask.manualTarget.x = defendingGoalX + (upPitchDir * 2800.f);
                mask.manualTarget.y = centerY + pushY;
            }
            else if (role == PositionRole::Striker || role == PositionRole::CenterForward ||
                role == PositionRole::LeftWing || role == PositionRole::RightWing) {
                // Attackers wait near the halfway line to contest the long clearance
                mask.manualTarget.x = halfwayX - (upPitchDir * 500.f);
                mask.manualTarget.y = centerY + relativeY + scatterY;
            }
            else {
                // Midfielders form a structural line ahead of the defense to provide safe passing lanes
                mask.manualTarget.x = defendingGoalX + (upPitchDir * 4000.f);
                mask.manualTarget.y = centerY + relativeY + scatterY;
            }
        }
        else {
            if (role == PositionRole::Goalkeeper) {
                mask.manualTarget.x = attackingGoalX;
                mask.manualTarget.y = centerY;
            }
            else {
                // ==========================================
                // --- THE FIX: DEFENSIVE DROP-OFF ---
                // ==========================================
                // Instead of suicidally pressing the 18-yard box, the defending team drops into a mid/low block
                float dropBackX = 0.f;

                if (role == PositionRole::Striker || role == PositionRole::CenterForward ||
                    role == PositionRole::LeftWing || role == PositionRole::RightWing) {
                    // Forwards drop off the edge of the final third to congest the midfield
                    dropBackX = halfwayX - (upPitchDir * 1500.f);
                }
                else if (role == PositionRole::CenterBack || role == PositionRole::LeftBack ||
                    role == PositionRole::RightBack || role == PositionRole::LeftWingBack ||
                    role == PositionRole::RightWingBack) {
                    // Defenders drop deep into their own half to safeguard against a long ball
                    dropBackX = halfwayX + (upPitchDir * 2000.f);
                }
                else {
                    // Midfielders sit firmly on their side of the halfway line
                    dropBackX = halfwayX + (upPitchDir * 500.f);
                }

                mask.manualTarget.x = dropBackX;
                mask.manualTarget.y = centerY + relativeY + scatterY;
            }
        }
    }

    return mask;
}

void MatchReferee::notifyPlayerSwap(Player* p1, Player* p2) {
    if (m_setPieceTaker == p1) m_setPieceTaker = p2;
    else if (m_setPieceTaker == p2) m_setPieceTaker = p1;

    if (m_fouledPlayer == p1) m_fouledPlayer = p2;
    else if (m_fouledPlayer == p2) m_fouledPlayer = p1;

    if (m_prevBallOwner == p1) m_prevBallOwner = p2;
    else if (m_prevBallOwner == p2) m_prevBallOwner = p1;

    for (size_t i = 0; i < m_offsideSnapshot.flaggedPlayers.size(); ++i) {
        if (m_offsideSnapshot.flaggedPlayers[i].player == p1)
            m_offsideSnapshot.flaggedPlayers[i].player = p2;
        else if (m_offsideSnapshot.flaggedPlayers[i].player == p2)
            m_offsideSnapshot.flaggedPlayers[i].player = p1;
    }
}

void MatchReferee::prepareRestart(MatchState state, Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager) {
    updateMatchContexts();

    m_matchState = state;
    m_whistleTimer = 2.0f;

    m_lastPossessor = nullptr;
    m_assistCandidate = nullptr;

    if (state == MatchState::GoalScored || state == MatchState::HalfTime ||
        state == MatchState::FullTime || state == MatchState::OutOfBoundsDelay) {
        return;
    }

    ball.release();
    ball.setSetPiece(true);
    ball.setPosition(m_restartPos);
    ball.setVelocity({ 0.f, 0.f });
    ball.z = 0.f;
    ball.vz = 0.f;

    float closestDist = 99999.f;
    m_setPieceTaker = nullptr;

    for (Player* p : players) {
        if (p->isSentOff() || p->getState() == PlayerState::Injured) continue;

        p->setVelocity({ 0.f, 0.f });
        if (p->getState() == PlayerState::Tackling || p->getState() == PlayerState::Diving || p->getState() == PlayerState::FallOver) {
            p->setState(PlayerState::Normal);
            p->setRotation(90.f); // Force them upright if the play was reset mid-dive!
        }

        if (p->getTeam() == m_awardedTo) {
            if (state == MatchState::KickOff) {
                if (p->getPositionRole() == PositionRole::Striker || p->getPositionRole() == PositionRole::CenterForward) {
                    m_setPieceTaker = p;
                }
            }
            else if (state == MatchState::GoalKick) {
                if (p->getPositionRole() == PositionRole::Goalkeeper) {
                    m_setPieceTaker = p;
                }
            }
            else if (state == MatchState::Penalty || state == MatchState::FreeKick) {
                if (m_fouledPlayer && m_fouledPlayer == p) {
                    m_setPieceTaker = m_fouledPlayer;
                }
            }
            else if (state == MatchState::ThrowIn || state == MatchState::Corner) {
                if (p->getPositionRole() != PositionRole::Goalkeeper) {
                    float d = std::sqrt(std::pow(p->getPosition().x - m_restartPos.x, 2) + std::pow(p->getPosition().y - m_restartPos.y, 2));
                    if (d < closestDist) {
                        closestDist = d;
                        m_setPieceTaker = p;
                    }
                }
            }
        }
    }

    if (m_setPieceTaker == nullptr) {
        float mostForwardX = (m_awardedTo == Team::Home) ? -9999.f : 99999.f;

        for (Player* p : players) {
            if (p->getTeam() == m_awardedTo && p->getPositionRole() != PositionRole::Goalkeeper && !p->isSentOff() && p->getState() != PlayerState::Injured) {

                float xPos = p->getHomePosition().x;
                bool isMoreForward = (m_awardedTo == Team::Home) ? (xPos > mostForwardX) : (xPos < mostForwardX);

                if (isMoreForward) {
                    mostForwardX = xPos;
                    m_setPieceTaker = p;
                }
            }
        }
    }

    // ==========================================
    // --- THE FIX: EXACT SPAWN ALIGNMENTS ---
    // ==========================================
    if (m_setPieceTaker) {
        bool isHome = (m_awardedTo == Team::Home);
        float xOffset = 0.f;
        float yOffset = 0.f;

        if (state == MatchState::KickOff) {
            // Stand behind the ball relative to your own half
            xOffset = isHome ? -60.f : 60.f;
        }
        else if (state == MatchState::Corner) {
            // Stand diagonally OUTSIDE the pitch lines
            xOffset = (m_restartPos.x > pitch.totalWidth / 2.f) ? 120.f : -120.f;
            yOffset = (m_restartPos.y > pitch.totalHeight / 2.f) ? 120.f : -120.f;
        }
        else if (state == MatchState::GoalKick || state == MatchState::FreeKick) {
            // Stand directly behind the ball
            xOffset = isHome ? -150.f : 150.f;
        }
        else if (state == MatchState::Penalty) {
            xOffset = isHome ? -200.f : 200.f;
        }
        else if (state == MatchState::ThrowIn) {
            // Stand just outside the touchline
            yOffset = (m_restartPos.y < pitch.totalHeight / 2.f) ? -60.f : 60.f;
        }

        m_setPieceTaker->setPosition(m_restartPos + sf::Vector2f(xOffset, yOffset));
        m_setPieceTaker->setVelocity({ 0.f, 0.f });
        ball.setPosition(m_restartPos);
    }

    for (Player* p : players) {
        if (p == m_setPieceTaker || p->isSentOff() || p->getState() == PlayerState::Injured) continue;

        if (state == MatchState::KickOff) {
            p->setPosition(p->getHomePosition());
            p->setVelocity({ 0.f, 0.f });
        }
        else if (state == MatchState::Corner) {
            PositioningMask mask = getPositioningMask(p, pitch);
            if (mask.useManualTarget) {
                p->setPosition(mask.manualTarget);
                p->setVelocity({ 0.f, 0.f });
            }
        }

        if (state == MatchState::Penalty) {
            if (p->getPositionRole() == PositionRole::Goalkeeper && p->getTeam() != m_awardedTo) {
                p->setPosition(m_awardedTo == Team::Home ? pitch.awayGoalCenter : pitch.homeGoalCenter);
            }
            else {
                PositioningMask mask = getPositioningMask(p, pitch);
                if (mask.useManualTarget) {
                    p->setPosition(mask.manualTarget);
                    p->setVelocity({ 0.f, 0.f });
                }
            }
        }
    }
    if (state == MatchState::ThrowIn || state == MatchState::GoalKick || state == MatchState::Corner) {
        m_exemptNextPass = true;
    }
    else {
        m_exemptNextPass = false;
    }
}

void MatchReferee::awardFoul(FoulEvent foul, Player* victim, MatchEnvironment& env) {
    if (m_matchState != MatchState::InPlay) return;

    bool isHomeDefending = (foul.offender->getTeam() == Team::Home);
    m_awardedTo = isHomeDefending ? Team::Away : Team::Home;
    m_fouledPlayer = victim;

    if (foul.type == FoulType::Offside) {
        awardOffside(foul.offender, *env.ball, *env.pitch, *env.sound);
        return;
    }

    env.stats->recordFoul(foul.offender->getTeam());
    env.sound->playSound("ref_foul", 100.f);

    // ==========================================
    // --- HOOK 4: INDIVIDUAL FOUL COMMITTED ---
    // ==========================================
    env.info->recordFoul(foul.offender->getId());

    int currentFouls = foul.offender->incrementFouls();
    int currentMinute = static_cast<int>(m_matchMinute);
    std::string teamId = isHomeDefending ? env.info->getHomeTeamId() : env.info->getAwayTeamId();

    if (foul.type == FoulType::Violent) {
        foul.offender->giveRedCard();
        env.info->recordCard(foul.offender->getId(), teamId, currentMinute, true);
        std::cout << "STRAIGHT RED CARD: " << foul.offender->getName() << "\n";
    }
    else if (foul.type == FoulType::Sliding) {
        if (m_matchMinute <= 20.0f && currentFouls == 1) {
            std::cout << "REFEREE WARNING (Early Match): " << foul.offender->getName() << " let off with a warning!\n";
        }
        else {
            foul.offender->giveYellowCard();
            env.info->recordCard(foul.offender->getId(), teamId, currentMinute, foul.offender->isSentOff());
            std::cout << "YELLOW CARD: " << foul.offender->getName() << "\n";
        }
    }
    else if (foul.type == FoulType::Obstruction) {
        if (currentFouls >= 3) {
            foul.offender->giveYellowCard();
            env.info->recordCard(foul.offender->getId(), teamId, currentMinute, foul.offender->isSentOff());
            std::cout << "YELLOW CARD (Persistent Fouling): " << foul.offender->getName() << " has had too many warnings.\n";
        }
        else {
            std::cout << "FOUL CALLED: " << foul.offender->getName() << " (Warning #" << currentFouls << ")\n";
        }
    }

    bool inBox = false;
    if (isHomeDefending && env.pitch->homePenaltyBox.contains(foul.location)) inBox = true;
    if (!isHomeDefending && env.pitch->awayPenaltyBox.contains(foul.location)) inBox = true;

    if (inBox) {
        m_pendingState = MatchState::Penalty;
        m_restartPos = isHomeDefending ? env.pitch->homePenaltySpot : env.pitch->awayPenaltySpot;
    }
    else {
        m_pendingState = MatchState::FreeKick;
        m_restartPos = foul.location;
    }

    m_foulDelayTimer = 2.0f;
    m_matchState = MatchState::FoulDelay;

    env.ball->setVelocity({ 0.f, 0.f });
    updateMatchContexts();
}

void MatchReferee::applyForfeitScore(bool homeForfeited) {
    if (homeForfeited) {
        m_homeScore = 0;
        m_awayScore = 3;
    }
    else {
        m_homeScore = 3;
        m_awayScore = 0;
    }
    m_matchState = MatchState::FullTime;
}

void MatchReferee::setupReplayTeleports(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager)
{
    for (Player* p : players) {
        if (p->isSentOff() && p->getPosition().x > 0.f) {
            p->setPosition({ -5000.f, -5000.f });
            p->setVelocity({ 0.f, 0.f });
        }
    }

    m_matchState = MatchState::ReplayPlaying;
    m_whistleTimer = 5.0f;
}

void MatchReferee::resumeFromReplay(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager)
{
    prepareRestart(m_pendingState, ball, pitch, players, soundManager);
}

void MatchReferee::startMatch(Ball& ball, const Pitch& pitch, const std::vector<Player*>& players, SoundManager& soundManager)
{
    m_timeScale = 90.0f / static_cast<float>(GlobalSettings::matchLengthMinutes);
    m_half = 1;
    m_matchMinute = 0.0f;
    m_awardedTo = Team::Home;
    m_restartPos = sf::Vector2f(pitch.totalWidth / 2.f, pitch.totalHeight / 2.f);
    prepareRestart(MatchState::KickOff, ball, pitch, players, soundManager);
}

void MatchReferee::checkOffsideLogic(float homeOffsideLine, float awayOffsideLine, MatchEnvironment& env) {
    Player* currentOwner = env.ball->getOwner();

    // Ball was just passed
    if (m_prevBallOwner != nullptr && currentOwner == nullptr) {

        // --- THE FIX 4: CHECK THE EXEMPTION MEMORY ---
        if (!m_exemptNextPass) {
            Team attackingTeam = m_prevBallOwner->getTeam();

            float deepestX = (attackingTeam == Team::Home) ? 0.f : env.pitch->totalWidth;
            float secondDeepestX = deepestX;

            for (Player* p : *env.allPlayers) {
                if (p->getTeam() == attackingTeam || p->isSentOff()) continue;

                float px = p->getPosition().x;
                if (attackingTeam == Team::Home) {
                    if (px > deepestX) { secondDeepestX = deepestX; deepestX = px; }
                    else if (px > secondDeepestX) { secondDeepestX = px; }
                }
                else {
                    if (px < deepestX) { secondDeepestX = deepestX; deepestX = px; }
                    else if (px < secondDeepestX) { secondDeepestX = px; }
                }
            }

            float halfwayX = env.pitch->totalWidth / 2.f;

            float strictLine = (attackingTeam == Team::Home) ?
                std::max({ secondDeepestX, m_prevBallOwner->getPosition().x, halfwayX }) :
                std::min({ secondDeepestX, m_prevBallOwner->getPosition().x, halfwayX });

            m_offsideSnapshot.flaggedPlayers.clear();
            m_offsideSnapshot.attackingTeam = attackingTeam;
            m_offsideSnapshot.isActive = true;
            m_frozenDefensiveLineX = strictLine;

            for (Player* p : *env.allPlayers) {
                if (p->getTeam() != attackingTeam || p == m_prevBallOwner) continue;

                bool inOpponentHalf = (attackingTeam == Team::Home) ? (p->getPosition().x > halfwayX) : (p->getPosition().x < halfwayX);

                if (inOpponentHalf) {
                    bool isOffsidePos = (attackingTeam == Team::Home) ? (p->getPosition().x > strictLine) : (p->getPosition().x < strictLine);

                    if (isOffsidePos) {
                        m_offsideSnapshot.flaggedPlayers.push_back({ p, p->getPosition() });
                    }
                }
            }
        }
        else {
            m_offsideSnapshot.isActive = false;
            m_offsideSnapshot.flaggedPlayers.clear();
        }
    }

    // Ball was just received
    if (m_prevBallOwner == nullptr && currentOwner != nullptr) {
        if (m_offsideSnapshot.isActive && currentOwner->getTeam() == m_offsideSnapshot.attackingTeam) {
            for (const auto& flagged : m_offsideSnapshot.flaggedPlayers) {
                if (currentOwner == flagged.player) {
                    m_frozenAttackerPos = flagged.passMomentPos;
                    awardOffside(currentOwner, *env.ball, *env.pitch, *env.sound);
                    break;
                }
            }
        }

        m_offsideSnapshot.isActive = false;
        m_offsideSnapshot.flaggedPlayers.clear();

        // --- THE FIX 5: CLEAR EXEMPTION ON FIRST TOUCH ---
        // The exempt pass has been received. Any subsequent passes in this sequence are subject to normal rules.
        m_exemptNextPass = false;
    }

    m_prevBallOwner = currentOwner;
}

void MatchReferee::awardOffside(Player* offender, Ball& ball, const Pitch& pitch, SoundManager& soundManager) {
    ball.setVelocity({ 0.f, 0.f });
    ball.z = 0.f;

    soundManager.playSound("ref_foul", 100.f);

    m_restartPos = ball.getPosition();
    m_awardedTo = (offender->getTeam() == Team::Home) ? Team::Away : Team::Home;

    m_lastInfraction = FoulType::Offside; // Stamp the foul type

    m_pendingState = MatchState::FreeKick;
    m_matchState = MatchState::FoulDelay;
    m_foulDelayTimer = 2.0f;
}
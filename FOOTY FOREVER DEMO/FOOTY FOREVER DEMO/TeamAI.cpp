#include "TeamAI.h"
#include "Player.h"
#include "Ball.h"
#include "Pitch.h"
#include "MatchReferee.h"
#include <algorithm>

TeamAI::TeamAI(bool isHomeTeam, const TeamTactics& tactics)
    : m_isHomeTeam(isHomeTeam), m_tactics(tactics), m_offsideLineX(0.f), m_ballProgress(0.5f), m_opposingBlockPush(0.5f), m_phaseTimer(0.f)
{
    m_managerCommand = ManagerCommand::Neutral;
    m_currentState = { MatchPhase::Neutral, TacticalSubState::Normal };
    m_lastPhase = MatchPhase::Neutral;
}

void TeamAI::update(const std::vector<Player*>& opposition, const Ball& ball, const Pitch& pitch, float dt, const MatchReferee& referee) {
    float ballX = ball.getPosition().x;
    m_ballProgress = m_isHomeTeam ? (ballX / pitch.totalWidth) : (1.0f - (ballX / pitch.totalWidth));
    m_ballProgress = std::clamp(m_ballProgress, 0.0f, 1.0f);

    float deepestOpponentX = m_isHomeTeam ? 0.0f : pitch.totalWidth;
    m_highestAttackerX = m_isHomeTeam ? pitch.totalWidth : 0.0f;

    for (Player* opp : opposition) {
        if (opp->getPositionRole() == PositionRole::Goalkeeper) continue;
        float oppX = opp->getPosition().x;

        if (m_isHomeTeam && oppX > deepestOpponentX) deepestOpponentX = oppX;
        else if (!m_isHomeTeam && oppX < deepestOpponentX) deepestOpponentX = oppX;

        if (m_isHomeTeam && oppX < m_highestAttackerX) m_highestAttackerX = oppX;
        else if (!m_isHomeTeam && oppX > m_highestAttackerX) m_highestAttackerX = oppX;
    }

    float halfwayX = pitch.totalWidth / 2.f;
    if (m_isHomeTeam) m_offsideLineX = std::max({ deepestOpponentX, ballX, halfwayX });
    else m_offsideLineX = std::min({ deepestOpponentX, ballX, halfwayX });

    float distToTheirGoal = m_isHomeTeam ? (pitch.totalWidth - deepestOpponentX) : deepestOpponentX;
    m_opposingBlockPush = std::clamp((distToTheirGoal - 1500.f) / 3000.f, 0.0f, 1.0f);

    Player* owner = ball.getOwner();
    if (!owner) owner = ball.getLastOwner();

    MatchPhase currentPhase = MatchPhase::Defending;
    if (owner) {
        bool weHaveBall = (owner->getTeam() == (m_isHomeTeam ? Team::Home : Team::Away));
        currentPhase = weHaveBall ? MatchPhase::Attacking : MatchPhase::Defending;
    }

    if (currentPhase != m_lastPhase) {
        m_phaseTimer = 0.f;
        m_lastPhase = currentPhase;
    }
    else {
        m_phaseTimer += dt;
    }

    // ==========================================
    // --- 1. THE MANAGER'S BRAIN (Commands) ---
    // ==========================================
    float matchMin = referee.getMatchMinute();
    int myScore = m_isHomeTeam ? referee.getHomeScore() : referee.getAwayScore();
    int oppScore = m_isHomeTeam ? referee.getAwayScore() : referee.getHomeScore();
    int scoreDiff = myScore - oppScore;

    m_managerCommand = ManagerCommand::Neutral;

    // Right before halftime: Push for a goal if tied, lock it down if winning
    if (matchMin >= 35.0f && matchMin <= 45.0f) {
        if (scoreDiff == 0) m_managerCommand = ManagerCommand::Aggressive;
        else if (scoreDiff > 0) m_managerCommand = ManagerCommand::Cautious;
    }
    // Second Half Tactics
    else if (matchMin > 45.0f && matchMin < 80.0f) {
        if (scoreDiff > 0) m_managerCommand = ManagerCommand::Cautious; // Protect the lead gently
        else if (scoreDiff <= -2) m_managerCommand = ManagerCommand::Aggressive; // Down by 2+, time to push
    }
    // Late Game Desperation / Management
    else if (matchMin >= 80.0f) {
        if (scoreDiff < 0) m_managerCommand = ManagerCommand::VeryAggressive; // Throw the kitchen sink
        else if (scoreDiff > 0) m_managerCommand = ManagerCommand::VeryCautious; // Park the bus
    }


    // ==========================================
    // --- 2. TACTICAL SUBSTATE INJECTION ---
    // ==========================================
    TacticalSubState subState = TacticalSubState::Normal;

    if (m_managerCommand == ManagerCommand::VeryAggressive) {
        subState = TacticalSubState::AllOut;
    }
    else if (m_managerCommand == ManagerCommand::VeryCautious) {
        // If we are Very Cautious, we completely kill the game
        subState = (currentPhase == MatchPhase::Attacking) ? TacticalSubState::TimeWasting : TacticalSubState::KeepPossession;
    }
    else {
        // Use dynamically scaled attacking speed from the getter!
        float transitionTime = 2.0f + (getAttackingSpeedPref() * 2.5f);

        if (m_phaseTimer < transitionTime) {
            subState = TacticalSubState::Transition;
        }
        else if (currentPhase == MatchPhase::Attacking) {
            // A cautious manager naturally wants to keep possession rather than force attacks
            if (m_managerCommand == ManagerCommand::Cautious || getAttackingSpeedPref() < 0.4f) {
                subState = TacticalSubState::KeepPossession;
            }
        }
    }

    m_currentState = { currentPhase, subState };
}

float TeamAI::getDefensiveLineOffset(bool isDefender) const {
    float shiftMagnitude = 0.f;
    float progressDiff = m_ballProgress - 0.5f;

    // Pull from the dynamic getter so the Manager Command natively influences the drop!
    float depthNorm = getDefensiveDepthPref();

    if (progressDiff > 0.f) {
        shiftMagnitude = progressDiff * (2000.f + ((1.0f - depthNorm) * 1500.f));
    }
    else {
        float baseDrop = progressDiff * (3000.f + (depthNorm * 5000.f));

        if (isDefender) {
            shiftMagnitude = baseDrop * 1.10f;
        }
        else {
            shiftMagnitude = baseDrop * 1.30f;
        }
    }

    float reactiveShift = (0.5f - m_opposingBlockPush) * 1500.f;
    float depthShift = (depthNorm * 100.f - 50.0f) * 20.0f;

    float totalOffset = shiftMagnitude - depthShift + reactiveShift;

    float minAnchor = -1200.f + ((1.0f - depthNorm) * 1000.f);
    float maxAnchor = 200.f + ((1.0f - depthNorm) * 600.f);

    totalOffset = std::clamp(totalOffset, minAnchor, maxAnchor);

    return m_isHomeTeam ? totalOffset : -totalOffset;
}

TacticalZone TeamAI::getEffectiveTacticalZone(const Playstyle& playstyle) const {
    TacticalZone effectiveZone = playstyle.zoneMod;

    // ==========================================
    // --- THE FIX: DYNAMIC INJECTION ---
    // ==========================================
    // ALL of these now pull from the Getters, meaning they are fully subjected 
    // to the ManagerCommand modifiers!
    float widthMod = getAttackingWidthPref() * 2.0f;
    float pressMod = getPressingIntensityPref() * 2.0f;
    float roamMod = getPositionalFreedomPref() * 2.0f;
    float lengthShift = (getPassingLengthPref() * 2.0f) - 1.0f;
    float attackSpeedShift = (getAttackingSpeedPref() * 2.0f) - 1.0f;

    if (m_opposingBlockPush < 0.4f) {
        float lowBlockFactor = 1.0f - (m_opposingBlockPush / 0.4f);
        lengthShift = (lengthShift * (1.0f - lowBlockFactor)) + (1.0f * lowBlockFactor);
    }
    else if (m_opposingBlockPush > 0.6f) {
        float highLineFactor = (m_opposingBlockPush - 0.6f) / 0.4f;
        lengthShift = (lengthShift * (1.0f - highLineFactor)) + (-1.0f * highLineFactor);
    }

    effectiveZone.lateralLeash *= widthMod;
    if (std::abs(effectiveZone.widthPreference) > 0.1f) {
        effectiveZone.widthPreference = std::clamp(effectiveZone.widthPreference * widthMod, -1.0f, 1.0f);
    }

    effectiveZone.pressingTrigger *= pressMod;
    effectiveZone.markingRange *= (0.5f + (pressMod * 0.5f));
    effectiveZone.roamingFreedom = std::clamp(effectiveZone.roamingFreedom * roamMod, 0.0f, 1.0f);

    effectiveZone.supportDepth -= (lengthShift * 0.3f);
    effectiveZone.supportDepth += (attackSpeedShift * 0.5f);

    if (m_currentState.phase == MatchPhase::Defending) {
        float depthNorm = getDefensiveDepthPref();
        float compressionFactor = 0.4f + (depthNorm * 0.4f);

        effectiveZone.forwardLeash *= compressionFactor;
        effectiveZone.backwardLeash *= compressionFactor;
    }
    else {
        if (attackSpeedShift > 0.0f) {
            effectiveZone.forwardLeash *= (1.0f + (attackSpeedShift * 1.2f));
            effectiveZone.backwardLeash *= (1.0f - (attackSpeedShift * 0.3f));
        }
        else {
            effectiveZone.forwardLeash *= (1.0f + (attackSpeedShift * 0.5f));
            effectiveZone.backwardLeash *= (1.0f + std::abs(attackSpeedShift) * 0.8f);
        }
    }

    return effectiveZone;
}
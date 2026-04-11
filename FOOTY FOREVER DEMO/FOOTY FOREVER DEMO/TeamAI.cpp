#include "TeamAI.h"
#include "Player.h"
#include "Ball.h"
#include "Pitch.h"
#include <algorithm>

TeamAI::TeamAI(bool isHomeTeam, const TeamTactics& tactics)
    : m_isHomeTeam(isHomeTeam), m_tactics(tactics), m_offsideLineX(0.f), m_ballProgress(0.5f), m_opposingBlockPush(0.5f) {
}

void TeamAI::update(const std::vector<Player*>& opposition, const Ball& ball, const Pitch& pitch) {
    float ballX = ball.getPosition().x;
    m_ballProgress = m_isHomeTeam ? (ballX / pitch.totalWidth) : (1.0f - (ballX / pitch.totalWidth));
    m_ballProgress = std::clamp(m_ballProgress, 0.0f, 1.0f);

    float deepestOpponentX = m_isHomeTeam ? 0.0f : pitch.totalWidth;

    // --- NEW: Initialize highest attacker at the opposite end of the pitch
    m_highestAttackerX = m_isHomeTeam ? pitch.totalWidth : 0.0f;

    for (Player* opp : opposition) {
        if (opp->getPositionRole() == PositionRole::Goalkeeper) continue;
        float oppX = opp->getPosition().x;

        // Calculate Offside Line (Deepest Defender)
        if (m_isHomeTeam && oppX > deepestOpponentX) deepestOpponentX = oppX;
        else if (!m_isHomeTeam && oppX < deepestOpponentX) deepestOpponentX = oppX;

        // --- NEW: Calculate Highest Attacker (Closest to our goal) ---
        // Home defends X=0, so the smallest X is the highest threat.
        if (m_isHomeTeam && oppX < m_highestAttackerX) m_highestAttackerX = oppX;
        else if (!m_isHomeTeam && oppX > m_highestAttackerX) m_highestAttackerX = oppX;
    }

    float halfwayX = pitch.totalWidth / 2.f;
    if (m_isHomeTeam) m_offsideLineX = std::max({ deepestOpponentX, ballX, halfwayX });
    else m_offsideLineX = std::min({ deepestOpponentX, ballX, halfwayX });

    float distToTheirGoal = m_isHomeTeam ? (pitch.totalWidth - deepestOpponentX) : deepestOpponentX;
    m_opposingBlockPush = std::clamp((distToTheirGoal - 1500.f) / 3000.f, 0.0f, 1.0f);
}

float TeamAI::getDefensiveLineOffset() const {
    float shiftMagnitude = 0.f;
    float progressDiff = m_ballProgress - 0.5f;

    // Convert depth to a 0.0 (High Line) to 1.0 (Low Block) normalized value
    float depthNorm = m_tactics.defensiveDepth / 100.0f;

    // ==========================================
    // --- 1. TACTICAL ANCHOR SHIFT ---
    // ==========================================
    if (progressDiff > 0.f) {
        // --- NERFED: ATTACKING PUSH ---
        // Teams no longer flood the opponent's half blindly. 
        // We cut the base multiplier from 5000 down to 2000.
        shiftMagnitude = progressDiff * (2000.f + ((1.0f - depthNorm) * 1500.f));
    }
    else {
        // --- BUFFED: DROPPING BACK QUICKER ---
        // High Line teams drop back with urgency (3000x multiplier).
        // Low Block teams absolutely sprint backwards (up to 8000x multiplier).
        shiftMagnitude = progressDiff * (3000.f + (depthNorm * 5000.f));
    }

    // ==========================================
    // --- 2. REACTIVE SQUEEZE ---
    // ==========================================
    float reactiveShift = (0.5f - m_opposingBlockPush) * 1500.f;
    float depthShift = (m_tactics.defensiveDepth - 50.0f) * 20.0f;

    float totalOffset = shiftMagnitude - depthShift + reactiveShift;

    // ==========================================
    // --- 3. DYNAMIC ANCHOR LIMITS (The Wall) ---
    // ==========================================
    float minAnchor = -1200.f + ((1.0f - depthNorm) * 1000.f);

    // THE FIX: Stop the anchor from pushing the defense into the opponent's half!
    // A High Line (depthNorm = 0.0) caps out at +800.f (holding right at the center circle).
    // A Low Block (depthNorm = 1.0) caps out at +200.f (barely stepping out of their own third).
    float maxAnchor = 200.f + ((1.0f - depthNorm) * 600.f);

    totalOffset = std::clamp(totalOffset, minAnchor, maxAnchor);

    return m_isHomeTeam ? totalOffset : -totalOffset;
}

TacticalZone TeamAI::getEffectiveTacticalZone(const Playstyle& playstyle) const {
    TacticalZone effectiveZone = playstyle.zoneMod;

    // Convert 0-100 multipliers (50 = 1.0x)
    float widthMod = m_tactics.attackingWidth / 50.0f;
    float pressMod = m_tactics.pressingIntensity / 50.0f;
    float roamMod = m_tactics.positionalFreedom / 50.0f;

    // Convert to -1.0 to +1.0 directional shifts
    float lengthShift = (m_tactics.passingLength - 50.0f) / 50.0f; // -1 (Long) to +1 (Tiki Taka)
    float speedShift = (m_tactics.passingSpeed - 50.0f) / 50.0f;   // -1 (Slow) to +1 (Fast Counter)

    // ==========================================
    // --- REACTIVE PASSING ADAPTATION ---
    // ==========================================
    // If the opponent plays a Low Block, the space behind them vanishes.
    // We automatically override the manager to force players to show to feet (Short Passing).
    if (m_opposingBlockPush < 0.4f) {
        float lowBlockFactor = 1.0f - (m_opposingBlockPush / 0.4f); // 0.0 to 1.0 intensity
        lengthShift = (lengthShift * (1.0f - lowBlockFactor)) + (1.0f * lowBlockFactor);
    }
    // Conversely, if they play a High Line, we automatically trigger players 
    // to make penetrating runs in behind (Long Passing) to exploit the space!
    else if (m_opposingBlockPush > 0.6f) {
        float highLineFactor = (m_opposingBlockPush - 0.6f) / 0.4f;
        lengthShift = (lengthShift * (1.0f - highLineFactor)) + (-1.0f * highLineFactor);
    }

    // --- 1. ATTACKING WIDTH ---
    effectiveZone.lateralLeash *= widthMod;
    if (std::abs(effectiveZone.widthPreference) > 0.1f) {
        effectiveZone.widthPreference = std::clamp(effectiveZone.widthPreference * widthMod, -1.0f, 1.0f);
    }

    // --- 2. PRESSING INTENSITY ---
    effectiveZone.pressingTrigger *= pressMod;
    effectiveZone.markingRange *= (0.5f + (pressMod * 0.5f));

    // --- 3. POSITIONAL FREEDOM ---
    effectiveZone.roamingFreedom = std::clamp(effectiveZone.roamingFreedom * roamMod, 0.0f, 1.0f);

    // --- 4. PASSING LENGTH (Support Depth) ---
    effectiveZone.supportDepth -= (lengthShift * 0.5f);

    // --- 5. PASSING SPEED (Forward Leash Stretching) ---
    if (speedShift > 0.0f) {
        effectiveZone.forwardLeash *= (1.0f + (speedShift * 0.4f));
    }
    else {
        effectiveZone.forwardLeash *= (1.0f + (speedShift * 0.2f));
    }

    return effectiveZone;
}
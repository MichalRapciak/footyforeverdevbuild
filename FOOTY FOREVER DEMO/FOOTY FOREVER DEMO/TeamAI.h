#pragma once
#include <vector>
#include <string>
#include <algorithm> // For std::clamp
#include <SFML/Graphics.hpp>
#include "Playstyle.h"
#include "GameDatabase.h" 
#include "Team.h"

class Player;
class Ball;
struct Pitch;
class MatchReferee;

// ==========================================
// --- NEW: MANAGER COMMANDS ---
// ==========================================
enum class ManagerCommand {
    VeryCautious,
    Cautious,
    Neutral,
    Aggressive,
    VeryAggressive
};

class TeamAI {
public:
    TeamAI(bool isHomeTeam, const TeamTactics& tactics);

    void update(const std::vector<Player*>& opposition, const Ball& ball, const Pitch& pitch, float dt, const MatchReferee& referee);

    float getOffsideLineX() const { return m_offsideLineX; }
    float getBallProgress() const { return m_ballProgress; }
    bool isHome() const { return m_isHomeTeam; }
    const TeamTactics& getTactics() const { return m_tactics; }

    TeamState getCurrentState() const { return m_currentState; }
    ManagerCommand getManagerCommand() const { return m_managerCommand; }

    // ==========================================
    // --- THE FIX: FRUSTRATION MECHANIC ---
    // ==========================================
    float getFrustration() const {
        if (m_currentState.phase != MatchPhase::Defending) return 0.0f;

        // In game time, chasing the ball for 15 straight seconds is exhausting.
        // By 35 seconds of pure opponent possession, the team completely snaps.
        float scale = (m_phaseTimer - 15.0f) / 20.0f;
        return std::clamp(scale, 0.0f, 1.0f);
    }

    // ==========================================
    // --- DYNAMIC TACTICAL GETTERS ---
    // ==========================================

    float getPassingLengthPref() const {
        float base = m_tactics.passingLength / 100.0f;
        if (m_managerCommand == ManagerCommand::VeryCautious) return std::max(0.0f, base - 0.3f);
        if (m_managerCommand == ManagerCommand::VeryAggressive) return std::min(1.0f, base + 0.4f);
        return base;
    }

    float getPassingSpeedPref() const {
        float base = m_tactics.passingSpeed / 100.0f;
        if (m_managerCommand == ManagerCommand::VeryCautious) return std::max(0.0f, base - 0.4f);
        if (m_managerCommand == ManagerCommand::Cautious) return std::max(0.0f, base - 0.2f);
        if (m_managerCommand == ManagerCommand::Aggressive) return std::min(1.0f, base + 0.2f);
        if (m_managerCommand == ManagerCommand::VeryAggressive) return std::min(1.0f, base + 0.4f);
        return base;
    }

    float getAttackingWidthPref() const {
        float base = m_tactics.attackingWidth / 100.0f;
        if (m_managerCommand == ManagerCommand::VeryCautious) return std::max(0.0f, base - 0.3f);
        if (m_managerCommand == ManagerCommand::Aggressive || m_managerCommand == ManagerCommand::VeryAggressive) return std::min(1.0f, base + 0.3f);
        return base;
    }

    float getDefensiveDepthPref() const {
        float base = m_tactics.defensiveDepth / 100.0f;
        if (m_managerCommand == ManagerCommand::VeryCautious) base = 0.0f;
        else if (m_managerCommand == ManagerCommand::Cautious) base = std::max(0.0f, base - 0.3f);
        else if (m_managerCommand == ManagerCommand::Aggressive) base = std::min(1.0f, base + 0.2f);
        else if (m_managerCommand == ManagerCommand::VeryAggressive) base = 1.0f;

        // FRUSTRATION: If starved of the ball, push the defensive line up to suffocate the space!
        float frustration = getFrustration();
        if (frustration > 0.0f) {
            base = std::min(1.0f, base + (frustration * 0.6f));
        }
        return base;
    }

    float getPressingIntensityPref() const {
        float base = m_tactics.pressingIntensity / 100.0f;
        if (m_managerCommand == ManagerCommand::VeryCautious) base = 0.0f;
        else if (m_managerCommand == ManagerCommand::Cautious) base = std::max(0.0f, base - 0.3f);
        else if (m_managerCommand == ManagerCommand::Aggressive) base = std::min(1.0f, base + 0.3f);
        else if (m_managerCommand == ManagerCommand::VeryAggressive) base = 1.0f;

        // FRUSTRATION: If starved of the ball, start hunting in packs regardless of manager instructions!
        float frustration = getFrustration();
        if (frustration > 0.0f) {
            base = std::min(1.0f, base + (frustration * 0.9f));
        }
        return base;
    }

    float getPositionalFreedomPref() const {
        float base = m_tactics.positionalFreedom / 100.0f;
        if (m_managerCommand == ManagerCommand::VeryCautious) return 0.0f;
        if (m_managerCommand == ManagerCommand::VeryAggressive) return std::min(1.0f, base + 0.4f);

        float frustration = getFrustration();
        if (frustration > 0.0f) {
            base = std::min(1.0f, base + (frustration * 0.7f));
        }

        return base;
    }

    float getAttackingSpeedPref() const {
        float base = m_tactics.attackingSpeed / 100.0f;
        if (m_managerCommand == ManagerCommand::VeryCautious) return 0.0f;
        if (m_managerCommand == ManagerCommand::Cautious) return std::max(0.0f, base - 0.3f);
        if (m_managerCommand == ManagerCommand::Aggressive) return std::min(1.0f, base + 0.3f);
        if (m_managerCommand == ManagerCommand::VeryAggressive) return 1.0f;
        return base;
    }

    // ==========================================
    // --- NEW: ATTACKING IMPATIENCE ---
    // ==========================================
    float getPenetrationUrgency() const {
        if (m_currentState.phase != MatchPhase::Attacking) return 0.0f;
        // If we are actively trying to waste time, we never get impatient.
        if (m_currentState.subState == TacticalSubState::TimeWasting) return 0.0f;

        // After 15 seconds of pure possession, players start actively looking for a line-breaking pass.
        // By 35 seconds, they will force it forward to break the monotony.
        float scale = (m_phaseTimer - 15.0f) / 20.0f;
        return std::clamp(scale, 0.0f, 1.0f);
    }
    float getPhaseTimer() const { return m_phaseTimer; }
    const std::string& getFormationName() const { return m_tactics.formationName; }
    const std::string& getCaptainId() const { return m_tactics.captainId; }
    const std::string& getPenaltyTakerId() const { return m_tactics.penaltyTakerId; }
    const std::string& getFreeKickTakerId() const { return m_tactics.freeKickTakerId; }
    const std::string& getLeftCornerTakerId() const { return m_tactics.leftCornerTakerId; }
    const std::string& getRightCornerTakerId() const { return m_tactics.rightCornerTakerId; }

    float getDefensiveLineOffset(bool isDefender) const;
    TacticalZone getEffectiveTacticalZone(const Playstyle& playstyle) const;
    float getHighestAttackerX() const { return m_highestAttackerX; }

private:
    float m_highestAttackerX;
    bool m_isHomeTeam;
    TeamTactics m_tactics;

    float m_offsideLineX;
    float m_opposingBlockPush;
    float m_ballProgress;

    // --- State Tracking ---
    ManagerCommand m_managerCommand;
    TeamState m_currentState;
    MatchPhase m_lastPhase;
    float m_phaseTimer;
};
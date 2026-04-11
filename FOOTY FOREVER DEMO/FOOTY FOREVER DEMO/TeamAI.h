#pragma once
#include <vector>
#include <string>
#include <SFML/Graphics.hpp>
#include "Playstyle.h"
#include "GameDatabase.h" 

class Player;
class Ball;
struct Pitch;

class TeamAI {
public:
    // Initialize the manager by passing in the specific tactics loaded from TeamData
    TeamAI(bool isHomeTeam, const TeamTactics& tactics);

    // Run this ONCE per frame per team in GamePlay::update
    void update(const std::vector<Player*>& opposition, const Ball& ball, const Pitch& pitch);

    // ==========================================
    // --- CACHED MATCH DATA ---
    // ==========================================
    float getOffsideLineX() const { return m_offsideLineX; }
    float getBallProgress() const { return m_ballProgress; }
    bool isHome() const { return m_isHomeTeam; }
    const TeamTactics& getTactics() const { return m_tactics; }

    // ==========================================
    // --- THE MANAGER'S PLAYBOOK ---
    // ==========================================

    // TACTICAL SLIDERS (Normalized 0.0f to 1.0f for easy math)
    float getPassingLengthPref() const { return m_tactics.passingLength / 100.0f; }     // 0.0 = Long, 1.0 = Tiki Taka
    float getPassingSpeedPref() const { return m_tactics.passingSpeed / 100.0f; }      // 0.0 = Slow, 1.0 = Fast Counter
    float getAttackingWidthPref() const { return m_tactics.attackingWidth / 100.0f; }    // 0.0 = Narrow, 1.0 = Wide
    float getDefensiveDepthPref() const { return m_tactics.defensiveDepth / 100.0f; }    // 0.0 = High Line, 1.0 = Low Block
    float getPressingIntensityPref() const { return m_tactics.pressingIntensity / 100.0f; } // 0.0 = Drop Off, 1.0 = Gegenpress
    float getPositionalFreedomPref() const { return m_tactics.positionalFreedom / 100.0f; } // 0.0 = Rigid, 1.0 = Fluid

    // TEAM SETUP
    const std::string& getFormationName() const { return m_tactics.formationName; }

    // SET PIECE ASSIGNMENTS
    const std::string& getCaptainId() const { return m_tactics.captainId; }
    const std::string& getPenaltyTakerId() const { return m_tactics.penaltyTakerId; }
    const std::string& getFreeKickTakerId() const { return m_tactics.freeKickTakerId; }
    const std::string& getLeftCornerTakerId() const { return m_tactics.leftCornerTakerId; }
    const std::string& getRightCornerTakerId() const { return m_tactics.rightCornerTakerId; }

    // ==========================================
    // --- TACTICAL ENGINE MATH ---
    // ==========================================
    // Calculates the macro defensive line shift based on the "Defensive Depth" slider
    float getDefensiveLineOffset() const;

    // Takes a player's personal Playstyle and warps it to fit the Manager's sliders
    TacticalZone getEffectiveTacticalZone(const Playstyle& playstyle) const;

    float getHighestAttackerX() const { return m_highestAttackerX; }

private:
    float m_highestAttackerX; // Tracks the opponent closest to our goal
    bool m_isHomeTeam;
    TeamTactics m_tactics; // Stores a local copy of the tactics for this match

    float m_offsideLineX;
    float m_opposingBlockPush; // 0.0 = Deep Low Block, 1.0 = High Line
    float m_ballProgress;
};
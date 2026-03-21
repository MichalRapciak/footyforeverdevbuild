#pragma once
#include <SFML/Graphics.hpp>
#include "MatchState.h" // Assuming your enum is here
#include "Team.h"

// --- THE BRAIN FILTER ---
// Controls WHAT the player is allowed to do physically
struct TacticalContext {
    MatchState state = MatchState::InPlay; // <-- Add the default here!
    bool isTaker = false;
    float maxSpeedLimit = 1000.f; 
    float ballInfluence = 1.0f;   
    bool canTackle = true;
    bool canPossess = true;
    float awarenessMod = 1.0f;   
};

// --- THE PITCH WARPER ---
// Controls WHERE the player wants to be
struct PositioningMask {
    sf::Vector2f homeOffset = { 0.f, 0.f };
    float lateralSqueeze = 0.0f; // How much to shadow ball Y-axis
    float forwardLeashMod = 1.0f;
    float backwardLeashMod = 1.0f;
    sf::Vector2f manualTarget = { 0.f, 0.f }; // For strict "stand here" spots
    bool useManualTarget = false;
};

// --- THE REFEREE'S ORDERS ---
// Specific assignments handed out during a whistle blow
struct SetPieceAssignment {
    class Player* taker = nullptr;
    class Player* primaryTarget = nullptr;
    sf::Vector2f restartSpot = { 0.f, 0.f };
    Team awardedTo = Team::None;
};
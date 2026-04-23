#pragma once
#include <string>
#include <vector>
#include <map>
#include <SFML/Graphics.hpp>
#include "PlayerStats.h"
#include "PositionRole.h"
#include "Playstyle.h"
#include "InjuryData.h"
#include "KitLayer.h"

// --- SUB-STRUCTURES ---

// --- GEOGRAPHY ---
struct Country {
    std::string code; // The 3-letter ID (e.g., "ENG", "FRA", "BRA")
    std::string name; // The full name (e.g., "England", "France", "Brazil")

    // Future-proofing: You can easily add confederations later!
    // std::string confederation; // e.g., "UEFA", "CONMEBOL"
};

struct KitData {
    sf::Color primaryColor;
    std::string badgeId;
    std::string manufacturerId;
    std::string sponsorId; // Empty for shorts/socks
    std::vector<KitLayer> designLayers;
};

struct PlayerGraphicsData {
    sf::Color skinColor;
    std::string faceType;
    std::string hairType;
    sf::Color hairColor;
    std::string beardType;
    sf::Color beardColor;
    std::string bootType;
    sf::Color bootColor;
    std::vector<std::string> accessories; // e.g., "AnkleTape_White", "Undershirt_Black"
};

struct TeamTactics {
    // --- 1. FORMATION ---
    std::string formationName = "4-3-3"; // e.g., "4-4-2", "3-5-2"

    // --- 2. MATCHDAY SQUAD ---
    // Maps a specific spot on the pitch to a Player ID
    std::map<int, std::string> startingXI; // <--- CHANGED HERE


    // The players sitting on the bench available for substitution
    std::vector<std::string> benchIds;

    // --- 3. ASSIGNED ROLES ---
    std::string captainId = "";
    std::string penaltyTakerId = "";
    std::string leftCornerTakerId = "";
    std::string rightCornerTakerId = "";
    std::string freeKickTakerId = "";

    // --- 4. TACTICAL SLIDERS (0 - 100) ---
    // These can feed directly into your PositioningMasks later!
    int defensiveDepth = 50;  // 0 = High Line, 100 = Low Block
    int passingLength = 50;     // 0 = Long Passes Forward, 100 = Tiki Taka
    int attackingWidth = 50;  // 0 = Narrow, 100 = Hug touchlines
    int pressingIntensity = 50; // 0 = Little to no Pressing, 100 = Gegenpressing
    int positionalFreedom = 50; // 0 = Stay in shape, 100 = Full Fluidity and Rotations
    int passingSpeed = 50; // 0 = slow, possession based, 100 = fast, counter attacks
};

// --- CORE ENTITIES ---

struct PlayerData {
    std::string id;
    std::string name;
    std::string nationality; // Stores the 3-letter Country Code
    int squadNumber;
    int age;
    int heightCm;
    int weightKg;
    std::string preferredFoot;

    std::string teamId; // Will be "" (empty) if they are a Free Agent

    int sharpness;
    int loyalty;

    bool isInjured = false;
    std::string currentInjury = "";
    int injuryDaysRemaining = 0;
    InjurySeverity currentInjurySeverity = InjurySeverity::Knock;

    std::vector<std::string> traits;
    PositionRole positionRole;
    std::map<PositionRole, int> positionFamiliarity;
    Playstyle playstyle;
    PlayerStats stats;

    float tacticalFamiliarity = 100.f;

    PlayerGraphicsData graphics;
};

struct TeamData {
    std::string id;
    std::string countryCode; // The 3-letter ID of the league they play in (e.g., "ENG")
    std::string fullName;
    std::string shortName; 
    std::string badgeId;
    std::string stadiumName;
    std::string managerName;

    sf::Color uiColor; 

    // Kits
    KitData shirt;
    KitData shorts;
    KitData socks;
    std::vector<KitLayer> socksLayers;
    std::vector<KitLayer> shortsLayers;
    std::vector<KitLayer> shirtLayers;

    // --- ROSTER (The Entire Club) ---
    std::vector<std::string> rosterPlayerIds; 

    // --- THE PLAYBOOK ---
    // This is the default setup the team uses when a match starts
    TeamTactics defaultTactics; 
    std::vector<PlayerData> startingXI; 
    std::vector<PlayerData> bench;

    float teamChemistry = 100.f;
};

// --- THE DATABASE MANAGER ---

class GameDatabase {
public:
    std::map<std::string, PlayerData> players;
    std::map<std::string, TeamData> teams;
    std::map<std::string, Country> countries;

    // --- NEW: Directory-based Loading & Saving ---
    void loadDatabase(const std::string& baseDir);
    void saveDatabase(const std::string& baseDir);
    
    void savePlayer(const std::string& id, const std::string& baseDir);
    void saveTeam(const std::string& id, const std::string& baseDir);
    void deletePlayerFile(const std::string& id, const std::string& baseDir, const std::string& oldTeamId);
    void initializeDefaultCountries();

    // Helper getters
    PlayerData* getPlayer(const std::string& id);
    TeamData* getTeam(const std::string& id);
    Country* getCountry(const std::string& code); // <-- NEW Helper
};

std::string roleToString(PositionRole role);
std::vector<std::vector<std::pair<int, PositionRole>>> getFormationLayout(const std::string& formationName);
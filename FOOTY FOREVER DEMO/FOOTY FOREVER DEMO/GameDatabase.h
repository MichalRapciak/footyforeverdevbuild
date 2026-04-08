#pragma once
#include <string>
#include <vector>
#include <map>
#include <SFML/Graphics.hpp>
#include "PlayerStats.h"
#include "PositionRole.h"

// --- SUB-STRUCTURES ---

struct KitLayer {
    std::string textureId; // e.g., "Stripes_01"
    sf::Color color;       // Tint for this specific layer
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
    std::map<PositionRole, std::string> startingXI;

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
    int defensiveDepth = 50;  // 0 = Park the bus, 100 = High press
    int buildUpPlay = 50;     // 0 = Short Tiki-Taka, 100 = Long Ball Counter
    int attackingWidth = 50;  // 0 = Narrow, 100 = Hug touchlines
};

// --- CORE ENTITIES ---

struct PlayerData {
    std::string id;
    std::string name;
    int squadNumber;
    int age;
    int heightCm;
    int weightKg;
    std::string preferredFoot;

    std::string teamId; // Will be "" (empty) if they are a Free Agent

    int sharpness;
    int loyalty;

    std::vector<std::string> traits;
    PositionRole positionRole; // <-- CHANGED to your Enum!
    PlayerStats stats;        // <-- CHANGED to your Struct!

    PlayerGraphicsData graphics;
};

struct TeamData {
    std::string id;
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

    // --- ROSTER (The Entire Club) ---
    std::vector<std::string> rosterPlayerIds; 

    // --- THE PLAYBOOK ---
    // This is the default setup the team uses when a match starts
    TeamTactics defaultTactics; 
};

// --- THE DATABASE MANAGER ---

class GameDatabase {
public:
    std::map<std::string, PlayerData> players;
    std::map<std::string, TeamData> teams;

    // --- NEW: Directory-based Loading & Saving ---
    void loadDatabase(const std::string& baseDir);
    void saveDatabase(const std::string& baseDir);
    
    void savePlayer(const std::string& id, const std::string& baseDir);
    void saveTeam(const std::string& id, const std::string& baseDir);
    void deletePlayerFile(const std::string& id, const std::string& baseDir, const std::string& oldTeamId);

    // --- TEMPORARY MIGRATION TOOL ---
    //void migrateLegacyDatabase(const std::string& oldPlayersFile, const std::string& oldTeamsFile, const std::string& newBaseDir);

    // Helper getters
    PlayerData* getPlayer(const std::string& id);
    TeamData* getTeam(const std::string& id);
};

std::string roleToString(PositionRole role);
std::vector<std::vector<PositionRole>> getFormationLayout(const std::string& formationName);
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

// --- CORE ENTITIES ---

struct PlayerData {
    std::string id;
    std::string name;
    int squadNumber;
    int age;
    int heightCm;
    int weightKg;
    std::string preferredFoot;

    std::vector<std::string> traits;
    PositionRole positionRole; // <-- CHANGED to your Enum!
    PlayerStats stats;        // <-- CHANGED to your Struct!

    PlayerGraphicsData graphics;
};

struct TeamData {
    std::string id;
    std::string fullName;
    std::string shortName; // e.g., "LIV", "RMA"
    std::string badgeId;
    std::string stadiumName;
    std::string managerName;
    std::string defaultFormation; // e.g., "4-3-3"

    sf::Color uiColor; // The main color for menus/scoreboards

    // Kits
    KitData shirt;
    KitData shorts;
    KitData socks;

    // Roster
    std::string captainPlayerId;
    std::vector<std::string> rosterPlayerIds; // List of all player IDs in this team
};

// --- THE DATABASE MANAGER ---

class GameDatabase {
public:
    std::map<std::string, PlayerData> players;
    std::map<std::string, TeamData> teams;

    // Functions to populate the maps from JSON files
    void loadPlayersFromFile(const std::string& filepath);
    void loadTeamsFromFile(const std::string& filepath);
    void savePlayersToFile(const std::string& filepath);
    void saveTeamsToFile(const std::string& filepath);

    // Helper getters
    PlayerData* getPlayer(const std::string& id);
    TeamData* getTeam(const std::string& id);
};
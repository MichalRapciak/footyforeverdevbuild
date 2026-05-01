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
    std::string hairType;
    sf::Color hairColor;
    std::string beardType;
    sf::Color beardColor;

    // --- BOOTS & LOGOS ---
    std::string bootType;
    sf::Color bootColor;
    std::string bootLogo1Type;
    sf::Color bootLogo1Color;
    std::string bootLogo2Type;
    sf::Color bootLogo2Color;

    std::vector<std::string> accessories; // e.g., "AnkleTape_White"
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
    int passingSpeed = 50; // 0 = slow ball carrying heavy, 100 = fast ball moving constantly
    int attackingSpeed = 50; // 0 = slow, build up slowly moving up the pitch, 100 = lightning fast counter attacks
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

// ==========================================
// --- COMPETITION & TOURNAMENT STRUCTS ---
// ==========================================

struct PlayerCompStats {
    int goals = 0;
    int assists = 0;
    int yellowCards = 0;
    int redCards = 0;
    int appearances = 0;
    int minutesPlayed = 0;
};

struct Fixture {
    std::string matchId;
    std::string homeTeamId;
    std::string awayTeamId;
    int homeScore = 0;
    int awayScore = 0;
    bool isPlayed = false;
    int roundNumber = 0; // e.g., 16 = Round of 16, 8 = Quarter Final, 1 = Final
};

struct CompetitionData {
    std::string id;
    std::string name;
    std::string type; // "CUP" or "LEAGUE"
    std::vector<std::string> participantTeamIds;
    std::vector<Fixture> fixtures;
    Country country;

    // Tracks stats specifically for this competition (Golden Boot, etc.)
    std::map<std::string, PlayerCompStats> playerStats;
};

struct CupTournamentData {
    std::string id;
    std::string name;
    std::string competitionId; // Links to the CompetitionData
    std::string userTeamId;    // The team the player is controlling
    int currentRound = 0;
    bool isComplete = false;
};

// ==========================================
// --- THE DATABASE MANAGER ---
// ==========================================
class MatchInfo; // Forward declaration so we can pass it in

class GameDatabase {
public:
    std::map<std::string, PlayerData> players;
    std::map<std::string, TeamData> teams;
    std::map<std::string, Country> countries;

    std::map<std::string, CompetitionData> competitions;
    std::map<std::string, CupTournamentData> cupTournaments;

    // --- NEW: Directory-based Loading & Saving ---
    void loadDatabase(const std::string& baseDir);
    void saveDatabase(const std::string& baseDir);
    
    void savePlayer(const std::string& id, const std::string& baseDir);
    void saveTeam(const std::string& id, const std::string& baseDir);
    void deletePlayerFile(const std::string& id, const std::string& baseDir, const std::string& oldTeamId);
    void initializeDefaultCountries();
    void processMatchResult(const MatchInfo& info, const std::string& compId = "");

    // Helper getters
    PlayerData* getPlayer(const std::string& id);
    TeamData* getTeam(const std::string& id);
    Country* getCountry(const std::string& code);
    CompetitionData* getCompetition(const std::string& id);
    CupTournamentData* getCupTournament(const std::string& id);
};

std::string roleToString(PositionRole role);
std::vector<std::vector<std::pair<int, PositionRole>>> getFormationLayout(const std::string& formationName);
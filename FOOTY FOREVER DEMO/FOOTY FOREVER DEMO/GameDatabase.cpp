#include "GameDatabase.h"
#include "PlaystyleDatabase.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;

using json = nlohmann::json;

// Maps a formation string into visual "Lines" (GK, DEF, MID, ATT).
// Now includes the strict 0-10 Slot ID!
std::vector<std::vector<std::pair<int, PositionRole>>> getFormationLayout(const std::string& formation) {
    if (formation == "4-4-2") {
        return {
            { {0, PositionRole::Goalkeeper} },
            { {1, PositionRole::RightBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack}, {4, PositionRole::LeftBack} },
            { {5, PositionRole::RightMid}, {6, PositionRole::CenterMid}, {7, PositionRole::DefensiveMid}, {8, PositionRole::LeftMid} },
            { {9, PositionRole::Striker}, {10, PositionRole::CenterForward} }
        };
    }
    else if (formation == "4-2-4") {
        return {
            { {0, PositionRole::Goalkeeper} },
            { {1, PositionRole::RightBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack}, {4, PositionRole::LeftBack} },
            { {5, PositionRole::CenterMid}, {6, PositionRole::DefensiveMid} },
            { {7, PositionRole::RightWing}, {8, PositionRole::Striker}, {9, PositionRole::AttackingMid}, {10, PositionRole::LeftWing} }
        };
    }
    else if (formation == "5-3-2") {
        return {
            { {0, PositionRole::Goalkeeper} },
            { {1, PositionRole::RightWingBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack}, {4, PositionRole::CenterBack}, {5, PositionRole::LeftWingBack} },
            { {6, PositionRole::RightMid}, {7, PositionRole::CenterMid}, {8, PositionRole::LeftMid} },
            { {9, PositionRole::Striker}, {10, PositionRole::CenterForward} }
        };
    }
    else if (formation == "5-2-3") {
        return {
            { {0, PositionRole::Goalkeeper} },
            { {1, PositionRole::RightWingBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack}, {4, PositionRole::CenterBack}, {5, PositionRole::LeftWingBack} },
            { {6, PositionRole::AttackingMid}, {7, PositionRole::CenterMid} },
            { {8, PositionRole::RightWing}, {9, PositionRole::Striker}, {10, PositionRole::LeftWing} }
        };
    }
    else if (formation == "5-4-1") {
        return {
            { {0, PositionRole::Goalkeeper} },
            { {1, PositionRole::RightWingBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack}, {4, PositionRole::CenterBack}, {5, PositionRole::LeftWingBack} },
            { {6, PositionRole::RightMid}, {7, PositionRole::AttackingMid}, {8, PositionRole::CenterMid}, {9, PositionRole::LeftMid} },
            { {10, PositionRole::Striker} }
        };
    }

    // Default to standard 4-3-3
    return {
        { {0, PositionRole::Goalkeeper} },
        { {1, PositionRole::RightBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack}, {4, PositionRole::LeftBack} },
        { {5, PositionRole::AttackingMid}, {6, PositionRole::CenterMid}, {7, PositionRole::DefensiveMid} },
        { {8, PositionRole::RightWing}, {9, PositionRole::Striker}, {10, PositionRole::LeftWing} }
    };
}

PositionRole stringToRole(const std::string& str)
{
    if (str == "Goalkeeper") return PositionRole::Goalkeeper;
    if (str == "LeftBack") return PositionRole::LeftBack;
    if (str == "CenterBack") return PositionRole::CenterBack;
    if (str == "RightBack") return PositionRole::RightBack;
    if (str == "LeftWingBack") return PositionRole::LeftWingBack;
    if (str == "RightWingBack") return PositionRole::RightWingBack;
    if (str == "DefensiveMid") return PositionRole::DefensiveMid;
    if (str == "CenterMid") return PositionRole::CenterMid;
    if (str == "LeftMid") return PositionRole::LeftMid;
    if (str == "RightMid") return PositionRole::RightMid;
    if (str == "AttackingMid") return PositionRole::AttackingMid;
    if (str == "LeftWing") return PositionRole::LeftWing;
    if (str == "RightWing") return PositionRole::RightWing;
    if (str == "CenterForward") return PositionRole::CenterForward;
    if (str == "Striker") return PositionRole::Striker;
    return PositionRole::CenterMid; // Default
}

std::string roleToString(PositionRole role)
{
    switch (role)
    {
    case PositionRole::Goalkeeper: return "Goalkeeper";
    case PositionRole::LeftBack: return "LeftBack";
    case PositionRole::CenterBack: return "CenterBack";
    case PositionRole::RightBack: return "RightBack";
    case PositionRole::LeftWingBack: return "LeftWingBack";
    case PositionRole::RightWingBack: return "RightWingBack";
    case PositionRole::DefensiveMid: return "DefensiveMid";
    case PositionRole::CenterMid: return "CenterMid";
    case PositionRole::LeftMid: return "LeftMid";
    case PositionRole::RightMid: return "RightMid";
    case PositionRole::AttackingMid: return "AttackingMid";
    case PositionRole::LeftWing: return "LeftWing";
    case PositionRole::RightWing: return "RightWing";
    case PositionRole::CenterForward: return "CenterForward";
    case PositionRole::Striker: return "Striker";
    default: return "CenterMid";
    }
}

PlaystyleType stringToPlaystyle(const std::string& str)
{
    // Goalkeepers
    if (str == "SweeperKeeper") return PlaystyleType::SweeperKeeper;
    if (str == "OnTheLine") return PlaystyleType::OnTheLine;
    if (str == "Distributor") return PlaystyleType::Distributor;
    // Defenders
    if (str == "Sweeper") return PlaystyleType::Sweeper;
    if (str == "TheWall") return PlaystyleType::TheWall;
    if (str == "TheKiller") return PlaystyleType::TheKiller;
    if (str == "CalmAndCollected") return PlaystyleType::CalmAndCollected;
    // Fullbacks
    if (str == "DefensiveFB") return PlaystyleType::DefensiveFB;
    if (str == "UpAndDown") return PlaystyleType::UpAndDown;
    if (str == "TheRoamerFB") return PlaystyleType::TheRoamerFB;
    if (str == "TheCrosser") return PlaystyleType::TheCrosser;
    // DMs
    if (str == "OrchestratorDM") return PlaystyleType::OrchestratorDM;
    if (str == "TheKillerDM") return PlaystyleType::TheKillerDM;
    if (str == "ThreeLungDM") return PlaystyleType::ThreeLungDM;
    if (str == "DefensiveRoamer") return PlaystyleType::DefensiveRoamer;
    if (str == "BacklineBrawler") return PlaystyleType::BacklineBrawler;
    // CMs
    if (str == "OrchestratorCM") return PlaystyleType::OrchestratorCM;
    if (str == "BoxToBox") return PlaystyleType::BoxToBox;
    if (str == "PlaymakerCM") return PlaystyleType::PlaymakerCM;
    if (str == "ThreeLungCM") return PlaystyleType::ThreeLungCM;
    if (str == "QuickPasser") return PlaystyleType::QuickPasser;
    if (str == "RoamerCM") return PlaystyleType::RoamerCM;
    // AMs
    if (str == "PlaymakerAM") return PlaystyleType::PlaymakerAM;
    if (str == "HardcorePress") return PlaystyleType::HardcorePress;
    if (str == "TricksterAM") return PlaystyleType::TricksterAM;
    if (str == "FinisherAM") return PlaystyleType::FinisherAM;
    // Wide
    if (str == "WideWinger") return PlaystyleType::WideWinger;
    if (str == "FalseWinger") return PlaystyleType::FalseWinger;
    if (str == "RoamerWinger") return PlaystyleType::RoamerWinger;
    if (str == "ClassicWideMid") return PlaystyleType::ClassicWideMid;
    if (str == "DefensiveWinger") return PlaystyleType::DefensiveWinger;
    if (str == "InvertedWideMid") return PlaystyleType::InvertedWideMid;
    // Strikers
    if (str == "Finisher") return PlaystyleType::Finisher;
    if (str == "TheTarget") return PlaystyleType::TheTarget;
    if (str == "False9") return PlaystyleType::False9;
    if (str == "SecondStriker") return PlaystyleType::SecondStriker;
    if (str == "ShadowStriker") return PlaystyleType::ShadowStriker;

    return PlaystyleType::BoxToBox; // Default Fallback
}

std::string playstyleToString(PlaystyleType type)
{
    switch (type) {
    case PlaystyleType::SweeperKeeper: return "SweeperKeeper";
    case PlaystyleType::OnTheLine: return "OnTheLine";
    case PlaystyleType::Distributor: return "Distributor";
    case PlaystyleType::Sweeper: return "Sweeper";
    case PlaystyleType::TheWall: return "TheWall";
    case PlaystyleType::TheKiller: return "TheKiller";
    case PlaystyleType::CalmAndCollected: return "CalmAndCollected";
    case PlaystyleType::DefensiveFB: return "DefensiveFB";
    case PlaystyleType::UpAndDown: return "UpAndDown";
    case PlaystyleType::TheRoamerFB: return "TheRoamerFB";
    case PlaystyleType::TheCrosser: return "TheCrosser";
    case PlaystyleType::OrchestratorDM: return "OrchestratorDM";
    case PlaystyleType::TheKillerDM: return "TheKillerDM";
    case PlaystyleType::ThreeLungDM: return "ThreeLungDM";
    case PlaystyleType::DefensiveRoamer: return "DefensiveRoamer";
    case PlaystyleType::BacklineBrawler: return "BacklineBrawler";
    case PlaystyleType::OrchestratorCM: return "OrchestratorCM";
    case PlaystyleType::BoxToBox: return "BoxToBox";
    case PlaystyleType::PlaymakerCM: return "PlaymakerCM";
    case PlaystyleType::ThreeLungCM: return "ThreeLungCM";
    case PlaystyleType::QuickPasser: return "QuickPasser";
    case PlaystyleType::RoamerCM: return "RoamerCM";
    case PlaystyleType::PlaymakerAM: return "PlaymakerAM";
    case PlaystyleType::HardcorePress: return "HardcorePress";
    case PlaystyleType::TricksterAM: return "TricksterAM";
    case PlaystyleType::FinisherAM: return "FinisherAM";
    case PlaystyleType::WideWinger: return "WideWinger";
    case PlaystyleType::FalseWinger: return "FalseWinger";
    case PlaystyleType::RoamerWinger: return "RoamerWinger";
    case PlaystyleType::ClassicWideMid: return "ClassicWideMid";
    case PlaystyleType::DefensiveWinger: return "DefensiveWinger";
    case PlaystyleType::InvertedWideMid: return "InvertedWideMid";
    case PlaystyleType::Finisher: return "Finisher";
    case PlaystyleType::TheTarget: return "TheTarget";
    case PlaystyleType::False9: return "False9";
    case PlaystyleType::SecondStriker: return "SecondStriker";
    case PlaystyleType::ShadowStriker: return "ShadowStriker";
    default: return "BoxToBox";
    }
}

// Helper to convert SFML Color back to "#RRGGBB" or "#RRGGBBAA"
std::string colorToHex(const sf::Color& color)
{
    std::stringstream ss;
    ss << "#" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(color.r)
        << std::setw(2) << static_cast<int>(color.g) << std::setw(2) << static_cast<int>(color.b);

    if (color.a < 255) {
        ss << std::setw(2) << static_cast<int>(color.a);
    }
    return ss.str();
}

// Helper to convert "#RRGGBB" or "#RRGGBBAA" into an SFML Color
sf::Color hexToColor(const std::string& hexStr) {
    if (hexStr.length() < 7 || hexStr[0] != '#') return sf::Color::White; // Fallback

    int r = std::stoi(hexStr.substr(1, 2), nullptr, 16);
    int g = std::stoi(hexStr.substr(3, 2), nullptr, 16);
    int b = std::stoi(hexStr.substr(5, 2), nullptr, 16);
    int a = 255;

    if (hexStr.length() == 9) {
        a = std::stoi(hexStr.substr(7, 2), nullptr, 16);
    }
    return sf::Color(r, g, b, a);
}


PlayerData* GameDatabase::getPlayer(const std::string& id) {
    auto it = players.find(id);
    if (it != players.end()) {
        return &(it->second);
    }
    return nullptr;
}

// --- HELPER: Parse KitData ---
KitData parseKitData(const json& kitJson)
{
    KitData kit;
    kit.primaryColor   = hexToColor(kitJson.value("primary_color", "#FFFFFF"));
    kit.badgeId        = kitJson.value("badge_id", "");
    kit.manufacturerId = kitJson.value("manufacturer_id", "");
    kit.sponsorId      = kitJson.value("sponsor_id", "");

    if (kitJson.contains("design_layers")) {
        for (const auto& layerJson : kitJson["design_layers"]) {
            KitLayer layer;
            layer.textureId = layerJson.value("texture_id", "");
            layer.color     = hexToColor(layerJson.value("color", "#FFFFFF"));
            kit.designLayers.push_back(layer);
        }
    }
    return kit;
}

TeamData* GameDatabase::getTeam(const std::string& id) {
    auto it = teams.find(id);
    if (it != teams.end()) {
        return &(it->second);
    }
    return nullptr;
}

json serializeKitData(const KitData& kit)
{
    json j;
    j["primary_color"] = colorToHex(kit.primaryColor);
    if (!kit.badgeId.empty()) j["badge_id"] = kit.badgeId;
    if (!kit.manufacturerId.empty()) j["manufacturer_id"] = kit.manufacturerId;
    if (!kit.sponsorId.empty()) j["sponsor_id"] = kit.sponsorId;

    j["design_layers"] = json::array(); 
    for (const auto& layer : kit.designLayers) {
        json layerJson;
        layerJson["texture_id"] = layer.textureId;
        layerJson["color"]      = colorToHex(layer.color);
        j["design_layers"].push_back(layerJson);
    }
    return j;
}

void GameDatabase::loadDatabase(const std::string& baseDir) {
    players.clear();
    teams.clear();

    if (!fs::exists(baseDir)) {
        std::cerr << "Database directory does not exist: " << baseDir << "\n";
        return;
    }

    // Recursively scan every folder in the base directory
    for (const auto& entry : fs::recursive_directory_iterator(baseDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::ifstream file(entry.path());
            if (!file.is_open()) continue;

            json j;
            try { file >> j; }
            catch (...) { continue; }

            // Is it a Team file or a Player file?
            if (entry.path().filename() == "TeamData.json") {
                // --- PARSE TEAM ---
                for (auto& [id, teamData] : j.items()) {
                    TeamData t;
                    t.id = id;
                    t.fullName = teamData.value("full_name", "Unknown Team");
                    t.shortName = teamData.value("short_name", "UNK");
                    t.badgeId = teamData.value("badge_id", "Badge_Default");
                    t.stadiumName = teamData.value("stadium_name", "Generic Stadium");
                    t.managerName = teamData.value("manager_name", "Unknown Manager");
                    t.uiColor = hexToColor(teamData.value("ui_color", "#FFFFFF"));

                    if (teamData.contains("kits")) {
                        auto& kits = teamData["kits"];
                        if (kits.contains("shirt"))  t.shirt = parseKitData(kits["shirt"]);
                        if (kits.contains("shorts")) t.shorts = parseKitData(kits["shorts"]);
                        if (kits.contains("socks"))  t.socks = parseKitData(kits["socks"]);
                    }
                    if (teamData.contains("roster")) {
                        for (const auto& playerId : teamData["roster"]) {
                            t.rosterPlayerIds.push_back(playerId.get<std::string>());
                        }
                    }
                    if (teamData.contains("tactics")) {
                        auto& tac = teamData["tactics"];
                        t.defaultTactics.formationName = tac.value("formation", "4-3-3");
                        t.defaultTactics.defensiveDepth = tac.value("defensive_depth", 50);
                        t.defaultTactics.passingLength = tac.value("passing_length", 50);
                        t.defaultTactics.attackingWidth = tac.value("attacking_width", 50);
                        t.defaultTactics.pressingIntensity = tac.value("pressing_intensity", 50);
                        t.defaultTactics.positionalFreedom = tac.value("positional_freedom", 50);
                        t.defaultTactics.passingSpeed = tac.value("passing_speed", 50);
                        t.defaultTactics.captainId = tac.value("captain_id", "");
                        t.defaultTactics.penaltyTakerId = tac.value("penalty_taker_id", "");
                        t.defaultTactics.leftCornerTakerId = tac.value("left_corner_taker_id", "");
                        t.defaultTactics.rightCornerTakerId = tac.value("right_corner_taker_id", "");
                        t.defaultTactics.freeKickTakerId = tac.value("free_kick_taker_id", "");

                        if (tac.contains("starting_xi")) {
                            for (auto& [slotStr, pId] : tac["starting_xi"].items()) {
                                try {
                                    // Convert the JSON string key ("0", "1", "2") back to an integer
                                    int slotId = std::stoi(slotStr);
                                    t.defaultTactics.startingXI[slotId] = pId.get<std::string>();
                                }
                                catch (const std::invalid_argument&) {
                                    // Catch old save files that still say "Goalkeeper" or "CenterMid"
                                    std::cout << "Skipping legacy tactic key: " << slotStr << " for team " << t.id << "\n";
                                }
                            }
                        }
                    }
                    teams[id] = t;
                }
            }
            else {
                // --- PARSE PLAYER ---
                for (auto& [id, playerData] : j.items()) {
                    PlayerData p;
                    p.id = id;
                    p.name = playerData.value("name", "Unknown Player");
                    p.teamId = playerData.value("team_id", "");
                    p.squadNumber = playerData.value("number", 99);
                    p.age = playerData.value("age", 25);
                    p.heightCm = playerData.value("height_cm", 180);
                    p.weightKg = playerData.value("weight_kg", 75);
                    p.preferredFoot = playerData.value("preferred_foot", "Right");
                    p.positionRole = stringToRole(playerData.value("position", "CenterMid"));
                    std::string psString = playerData.value("playstyle", "BoxToBox");
                    p.playstyle = PlaystyleDatabase::getPlaystyle(stringToPlaystyle(psString));
                    p.sharpness = playerData.value("sharpness", 50);
                    p.loyalty = playerData.value("loyalty", 50);

                    if (playerData.contains("traits")) {
                        for (const auto& trait : playerData["traits"]) {
                            p.traits.push_back(trait.get<std::string>());
                        }
                    }

                    if (playerData.contains("stats")) {
                        auto& s = playerData["stats"];
                        p.stats.naturalFitness = s.value("fitness", 50.f);
                        p.stats.weakFootAccuracy = s.value("weak_foot_accuracy", 2);
                        p.stats.finishing = s.value("finishing", 50.0f);
                        p.stats.heading = s.value("heading", 50.0f);
                        p.stats.ballControl = s.value("ball_control", 50.0f);
                        p.stats.balancing = s.value("balancing", 50.0f);
                        p.stats.curl = s.value("curl", 50.0f);
                        p.stats.deadBall = s.value("dead_ball", 50.0f);
                        p.stats.shortPassing = s.value("short_passing", 50.0f);
                        p.stats.longPassing = s.value("long_passing", 50.0f);
                        p.stats.topSpeed = s.value("top_speed", 50.0f);
                        p.stats.acceleration = s.value("acceleration", 50.0f);
                        p.stats.agility = s.value("agility", 50.0f);
                        p.stats.bodyStrength = s.value("body_strength", 50.0f);
                        p.stats.kickPower = s.value("kick_power", 50.0f);
                        p.stats.jumpingStrength = s.value("jumping_strength", 50.0f);
                        p.stats.awareness = s.value("awareness", 50.0f);
                        p.stats.aggression = s.value("aggression", 50.0f);
                        p.stats.blocking = s.value("blocking", 50.0f);
                        p.stats.gkCoverage = s.value("gk_coverage", 10.0f);
                        p.stats.gkReactions = s.value("gk_reactions", 10.0f);
                        p.stats.gkCatching = s.value("gk_catching", 10.0f);
                        p.stats.gkThrowing = s.value("gk_throwing", 10.0f);
                        p.stats.gkAwareness = s.value("gk_awareness", 10.0f);
                        p.stats.gkBlocking = s.value("gk_blocking", 10.0f);
                    }
                    else {
                        p.stats = PlayerStats::createFromRole(p.positionRole);
                    }

                    if (playerData.contains("graphics")) {
                        auto& gfx = playerData["graphics"];
                        p.graphics.skinColor = hexToColor(gfx.value("skin_color", "#FFFFFF"));
                        p.graphics.faceType = gfx.value("face_type", "Face_01");
                        p.graphics.hairType = gfx.value("hair_type", "Hair_Bald");
                        p.graphics.hairColor = hexToColor(gfx.value("hair_color", "#000000"));
                        p.graphics.beardType = gfx.value("beard_type", "Beard_None");
                        p.graphics.beardColor = hexToColor(gfx.value("beard_color", "#000000"));
                        p.graphics.bootType = gfx.value("boot_type", "Boots_Basic");
                        p.graphics.bootColor = hexToColor(gfx.value("boot_color", "#000000"));

                        if (gfx.contains("accessories")) {
                            for (const auto& acc : gfx["accessories"]) {
                                p.graphics.accessories.push_back(acc.get<std::string>());
                            }
                        }
                    }
                    players[id] = p;
                }
            }
        }
    }
    std::cout << "Loaded " << teams.size() << " teams and " << players.size() << " players.\n";
}

void GameDatabase::deletePlayerFile(const std::string& id, const std::string& baseDir, const std::string& oldTeamId) {
    std::string folder = baseDir + (oldTeamId.empty() ? "/FreeAgents/" : "/Teams/" + oldTeamId + "/Players/");
    std::string path = folder + id + ".json";
    if (fs::exists(path)) fs::remove(path);
}

void GameDatabase::saveDatabase(const std::string& baseDir) {
    for (const auto& [id, t] : teams) saveTeam(id, baseDir);
    for (const auto& [id, p] : players) savePlayer(id, baseDir);
    std::cout << "Successfully saved full database to " << baseDir << "\n";
}

void GameDatabase::savePlayer(const std::string& id, const std::string& baseDir) {
    PlayerData* p = getPlayer(id);
    if (!p) return;

    std::string folder = baseDir + (p->teamId.empty() ? "/FreeAgents/" : "/Teams/" + p->teamId + "/Players/");
    fs::create_directories(folder);

    json j;
    j[id]["name"] = p->name;
    j[id]["team_id"] = p->teamId;
    j[id]["number"] = p->squadNumber;
    j[id]["age"] = p->age;
    j[id]["height_cm"] = p->heightCm;
    j[id]["weight_kg"] = p->weightKg;
    j[id]["preferred_foot"] = p->preferredFoot;
    j[id]["position"] = roleToString(p->positionRole);
    j[id]["playstyle"] = playstyleToString(p->playstyle.type);
    j[id]["sharpness"] = p->sharpness;
    j[id]["loyalty"] = p->loyalty;
    j[id]["traits"] = p->traits;

    auto& s = j[id]["stats"];
    s["fitness"] = p->stats.naturalFitness;
    s["weak_foot_accuracy"] = p->stats.weakFootAccuracy;
    s["finishing"] = p->stats.finishing;
    s["heading"] = p->stats.heading;
    s["ball_control"] = p->stats.ballControl;
    s["balancing"] = p->stats.balancing;
    s["curl"] = p->stats.curl;
    s["dead_ball"] = p->stats.deadBall;
    s["short_passing"] = p->stats.shortPassing;
    s["long_passing"] = p->stats.longPassing;
    s["top_speed"] = p->stats.topSpeed;
    s["acceleration"] = p->stats.acceleration;
    s["agility"] = p->stats.agility;
    s["body_strength"] = p->stats.bodyStrength;
    s["kick_power"] = p->stats.kickPower;
    s["jumping_strength"] = p->stats.jumpingStrength;
    s["awareness"] = p->stats.awareness;
    s["aggression"] = p->stats.aggression;
    s["blocking"] = p->stats.blocking;
    s["gk_coverage"] = p->stats.gkCoverage;
    s["gk_blocking"] = p->stats.gkBlocking;
    s["gk_catching"] = p->stats.gkCatching;
    s["gk_throwing"] = p->stats.gkThrowing;
    s["gk_reactions"] = p->stats.gkReactions;
    s["gk_awareness"] = p->stats.gkAwareness;

    auto& gfx = j[id]["graphics"];
    gfx["skin_color"] = colorToHex(p->graphics.skinColor);
    gfx["face_type"] = p->graphics.faceType;
    gfx["hair_type"] = p->graphics.hairType;
    gfx["hair_color"] = colorToHex(p->graphics.hairColor);
    gfx["beard_type"] = p->graphics.beardType;
    gfx["beard_color"] = colorToHex(p->graphics.beardColor);
    gfx["boot_type"] = p->graphics.bootType;
    gfx["boot_color"] = colorToHex(p->graphics.bootColor);
    gfx["accessories"] = p->graphics.accessories;

    std::ofstream file(folder + id + ".json");
    if (file.is_open()) file << j.dump(4);
}

void GameDatabase::saveTeam(const std::string& id, const std::string& baseDir) {
    TeamData* t = getTeam(id);
    if (!t) return;

    std::string folder = baseDir + "/Teams/" + id + "/";
    fs::create_directories(folder);

    json j;
    j[id]["full_name"] = t->fullName;
    j[id]["short_name"] = t->shortName;
    j[id]["badge_id"] = t->badgeId;
    j[id]["stadium_name"] = t->stadiumName;
    j[id]["manager_name"] = t->managerName;
    j[id]["ui_color"] = colorToHex(t->uiColor);

    j[id]["kits"]["shirt"] = serializeKitData(t->shirt);
    j[id]["kits"]["shorts"] = serializeKitData(t->shorts);
    j[id]["kits"]["socks"] = serializeKitData(t->socks);

    j[id]["roster"] = t->rosterPlayerIds;

    auto& tac = j[id]["tactics"];
    tac["formation"] = t->defaultTactics.formationName;
    tac["defensive_depth"] = t->defaultTactics.defensiveDepth;
    tac["passing_length"] = t->defaultTactics.passingLength;
    tac["attacking_width"] = t->defaultTactics.attackingWidth;
    tac["pressing_intensity"] = t->defaultTactics.pressingIntensity;
    tac["positional_freedom"] = t->defaultTactics.positionalFreedom;
    tac["passing_speed"] = t->defaultTactics.passingSpeed;

    tac["captain_id"] = t->defaultTactics.captainId;
    tac["penalty_taker_id"] = t->defaultTactics.penaltyTakerId;
    tac["free_kick_taker_id"] = t->defaultTactics.freeKickTakerId;
    tac["left_corner_taker_id"] = t->defaultTactics.leftCornerTakerId;
    tac["right_corner_taker_id"] = t->defaultTactics.rightCornerTakerId;

    tac["starting_xi"] = json::object();
    for (const auto& [slotId, pId] : t->defaultTactics.startingXI) {
        if (!pId.empty()) {
            // Convert the integer slot ID to a string for the JSON key
            tac["starting_xi"][std::to_string(slotId)] = pId;
        }
    }

    std::ofstream file(folder + "TeamData.json");
    if (file.is_open()) file << j.dump(4);
}
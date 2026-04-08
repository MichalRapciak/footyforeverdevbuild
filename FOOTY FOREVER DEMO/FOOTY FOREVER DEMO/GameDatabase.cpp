#include "GameDatabase.h"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;

using json = nlohmann::json;

// Maps a formation string into visual "Lines" (GK, DEF, MID, ATT) using the 11 available enums.
// Since the GK is at the top attacking down, Right is on the Left of the screen, and Left is on the Right!
std::vector<std::vector<PositionRole>> getFormationLayout(const std::string& formation) {
    if (formation == "4-4-2") {
        return {
            {PositionRole::Goalkeeper},
            {PositionRole::RightBack, PositionRole::RCenterBack, PositionRole::LCenterBack, PositionRole::LeftBack},
            {PositionRole::RightWing, PositionRole::CenterMid, PositionRole::DefensiveMid, PositionRole::LeftWing},
            {PositionRole::Striker, PositionRole::AttackingMid} // AM acts as Second Striker
        };
    }
    else if (formation == "4-2-4") {
        return {
            {PositionRole::Goalkeeper},
            {PositionRole::RightBack, PositionRole::RCenterBack, PositionRole::LCenterBack, PositionRole::LeftBack},
            {PositionRole::CenterMid, PositionRole::DefensiveMid},
            {PositionRole::RightWing, PositionRole::Striker, PositionRole::AttackingMid, PositionRole::LeftWing}
        };
    }
    else if (formation == "5-3-2") {
        return {
            {PositionRole::Goalkeeper},
            // DM drops to become the Central Center-Back (Sweeper)
            {PositionRole::RightBack, PositionRole::RCenterBack, PositionRole::DefensiveMid, PositionRole::LCenterBack, PositionRole::LeftBack},
            // Wingers drop to act as wide midfielders
            {PositionRole::RightWing, PositionRole::CenterMid, PositionRole::LeftWing},
            {PositionRole::Striker, PositionRole::AttackingMid}
        };
    }
    else if (formation == "5-2-3") {
        return {
            {PositionRole::Goalkeeper},
            {PositionRole::RightBack, PositionRole::RCenterBack, PositionRole::DefensiveMid, PositionRole::LCenterBack, PositionRole::LeftBack},
            {PositionRole::AttackingMid, PositionRole::CenterMid},
            {PositionRole::RightWing, PositionRole::Striker, PositionRole::LeftWing}
        };
    }
    else if (formation == "5-4-1") {
        return {
            {PositionRole::Goalkeeper},
            {PositionRole::RightBack, PositionRole::RCenterBack, PositionRole::DefensiveMid, PositionRole::LCenterBack, PositionRole::LeftBack},
            {PositionRole::RightWing, PositionRole::AttackingMid, PositionRole::CenterMid, PositionRole::LeftWing},
            {PositionRole::Striker}
        };
    }

    // Default to standard 4-3-3
    return {
        {PositionRole::Goalkeeper},
        {PositionRole::RightBack, PositionRole::RCenterBack, PositionRole::LCenterBack, PositionRole::LeftBack},
        {PositionRole::AttackingMid, PositionRole::CenterMid, PositionRole::DefensiveMid},
        {PositionRole::RightWing, PositionRole::Striker, PositionRole::LeftWing}
    };
}

PositionRole stringToRole(const std::string& str)
{
    if (str == "Goalkeeper") return PositionRole::Goalkeeper;
    if (str == "LeftBack") return PositionRole::LeftBack;
    if (str == "LCenterBack") return PositionRole::LCenterBack;
    if (str == "RCenterBack") return PositionRole::RCenterBack;
    if (str == "RightBack") return PositionRole::RightBack;
    if (str == "DefensiveMid") return PositionRole::DefensiveMid;
    if (str == "CenterMid") return PositionRole::CenterMid;
    if (str == "AttackingMid") return PositionRole::AttackingMid;
    if (str == "LeftWing") return PositionRole::LeftWing;
    if (str == "RightWing") return PositionRole::RightWing;
    if (str == "Striker") return PositionRole::Striker;
    return PositionRole::CenterMid; // Default
}

std::string roleToString(PositionRole role)
{
    switch (role)
    {
    case PositionRole::Goalkeeper: return "Goalkeeper";
    case PositionRole::LeftBack: return "LeftBack";
    case PositionRole::LCenterBack: return "LCenterBack";
    case PositionRole::RCenterBack: return "RCenterBack";
    case PositionRole::RightBack: return "RightBack";
    case PositionRole::DefensiveMid: return "DefensiveMid";
    case PositionRole::CenterMid: return "CenterMid";
    case PositionRole::AttackingMid: return "AttackingMid";
    case PositionRole::LeftWing: return "LeftWing";
    case PositionRole::RightWing: return "RightWing";
    case PositionRole::Striker: return "Striker";
    default: return "CenterMid";
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
                        t.defaultTactics.buildUpPlay = tac.value("build_up_play", 50);
                        t.defaultTactics.attackingWidth = tac.value("attacking_width", 50);
                        t.defaultTactics.captainId = tac.value("captain_id", "");
                        t.defaultTactics.penaltyTakerId = tac.value("penalty_taker_id", "");
                        t.defaultTactics.leftCornerTakerId = tac.value("left_corner_taker_id", "");
                        t.defaultTactics.rightCornerTakerId = tac.value("right_corner_taker_id", "");
                        t.defaultTactics.freeKickTakerId = tac.value("free_kick_taker_id", "");

                        if (tac.contains("starting_xi")) {
                            for (auto& [roleStr, pId] : tac["starting_xi"].items()) {
                                t.defaultTactics.startingXI[stringToRole(roleStr)] = pId.get<std::string>();
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
    tac["build_up_play"] = t->defaultTactics.buildUpPlay;
    tac["attacking_width"] = t->defaultTactics.attackingWidth;

    tac["captain_id"] = t->defaultTactics.captainId;
    tac["penalty_taker_id"] = t->defaultTactics.penaltyTakerId;
    tac["free_kick_taker_id"] = t->defaultTactics.freeKickTakerId;
    tac["left_corner_taker_id"] = t->defaultTactics.leftCornerTakerId;
    tac["right_corner_taker_id"] = t->defaultTactics.rightCornerTakerId;

    tac["starting_xi"] = json::object();
    for (const auto& [role, pId] : t->defaultTactics.startingXI) {
        if (!pId.empty()) {
            tac["starting_xi"][roleToString(role)] = pId;
        }
    }

    std::ofstream file(folder + "TeamData.json");
    if (file.is_open()) file << j.dump(4);
}
#include "GameDatabase.h"
#include "json.hpp"
#include <fstream>
#include <iostream>



using json = nlohmann::json;

#include <iomanip>
#include <sstream>

PositionRole stringToRole(const std::string& str)
{
    if (str == "Goalkeeper")
        return PositionRole::Goalkeeper;
    if (str == "LeftBack")
        return PositionRole::LeftBack;
    if (str == "LCenterBack")
        return PositionRole::LCenterBack;
    if (str == "RCenterBack")
        return PositionRole::RCenterBack;
    if (str == "RightBack")
        return PositionRole::RightBack;
    if (str == "DefensiveMid")
        return PositionRole::DefensiveMid;
    if (str == "CenterMid")
        return PositionRole::CenterMid;
    if (str == "AttackingMid")
        return PositionRole::AttackingMid;
    if (str == "LeftWing")
        return PositionRole::LeftWing;
    if (str == "RightWing")
        return PositionRole::RightWing;
    if (str == "Striker")
        return PositionRole::Striker;
    return PositionRole::CenterMid; // Default
}

std::string roleToString(PositionRole role)
{
    switch (role)
    {
    case PositionRole::Goalkeeper:
        return "Goalkeeper";
    case PositionRole::LeftBack:
        return "LeftBack";
    case PositionRole::LCenterBack:
        return "LCenterBack";
    case PositionRole::RCenterBack:
        return "RCenterBack";
    case PositionRole::RightBack:
        return "RightBack";
    case PositionRole::DefensiveMid:
        return "DefensiveMid";
    case PositionRole::CenterMid:
        return "CenterMid";
    case PositionRole::AttackingMid:
        return "AttackingMid";
    case PositionRole::LeftWing:
        return "LeftWing";
    case PositionRole::RightWing:
        return "RightWing";
    case PositionRole::Striker:
        return "Striker";
    default:
        return "CenterMid";
    }
}

// Helper to convert SFML Color back to "#RRGGBB" or "#RRGGBBAA"
std::string colorToHex(const sf::Color& color)
{
    std::stringstream ss;
    ss << "#" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(color.r)
        << std::setw(2) << static_cast<int>(color.g) << std::setw(2) << static_cast<int>(color.b);

    // Only add alpha channel if it's not fully opaque
    if (color.a < 255)
    {
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

void GameDatabase::loadPlayersFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open database file: " << filepath << "\n";
        return;
    }

    json j;
    file >> j; // This magically parses the entire file!

    // Loop through every player ID in the JSON (e.g., "PLY_1001")
    for (auto& [id, playerData] : j.items()) {
        PlayerData p;
        p.id = id;

        // --- Core Info ---
        // Use .value() to provide safe fallbacks if a field is missing in the JSON
        p.name = playerData.value("name", "Unknown Player");
        p.squadNumber = playerData.value("number", 99);
        p.age = playerData.value("age", 25);
        p.heightCm = playerData.value("height_cm", 180);
        p.weightKg = playerData.value("weight_kg", 75);
        p.preferredFoot = playerData.value("preferred_foot", "Right");
        p.positionRole  = stringToRole(playerData.value("position", "CenterMid"));

        if (playerData.contains("stats"))
        {
            auto& s                 = playerData["stats"];
            p.stats.finishing       = s.value("finishing", 50.f);
            p.stats.heading         = s.value("heading", 50.f);
            p.stats.ballControl     = s.value("ball_control", 50.f);
            p.stats.balancing       = s.value("balancing", 50.f);
            p.stats.curl            = s.value("curl", 50.f);
            p.stats.deadBall        = s.value("dead_ball", 50.f);
            p.stats.shortPassing    = s.value("short_passing", 50.f);
            p.stats.longPassing     = s.value("long_passing", 50.f);
            p.stats.topSpeed        = s.value("top_speed", 50.f);
            p.stats.acceleration    = s.value("acceleration", 50.f);
            p.stats.agility         = s.value("agility", 50.f);
            p.stats.bodyStrength    = s.value("body_strength", 50.f);
            p.stats.kickPower       = s.value("kick_power", 50.f);
            p.stats.jumpingStrength = s.value("jumping_strength", 50.f);
            p.stats.awareness       = s.value("awareness", 50.f);
            p.stats.aggression      = s.value("aggression", 50.f);
            p.stats.blocking        = s.value("blocking", 50.f);
            p.stats.gkCoverage = s.value("gk_coverage", 10.0f);
            p.stats.gkReactions = s.value("gk_reactions", 10.0f);
            p.stats.gkCatching = s.value("gk_catching", 10.0f);
            p.stats.gkThrowing = s.value("gk_throwing", 10.0f);
            p.stats.gkAwareness = s.value("gk_awareness", 10.0f);
            p.stats.gkBlocking = s.value("gk_blocking", 10.0f);
        }
        else
        {
            // Auto-generate template stats if none exist!
            p.stats = PlayerStats::createFromRole(p.positionRole);
        }
        // --- Arrays & Maps ---
        if (playerData.contains("traits")) {
            for (const auto& trait : playerData["traits"]) {
                p.traits.push_back(trait.get<std::string>());
            }
        }

if (playerData.contains("stats"))
        {
            auto& s = playerData["stats"];

            // SHOOTING
            p.stats.finishing = s.value("finishing", 50.0f);
            p.stats.heading   = s.value("heading", 50.0f);

            // DRIBBLING
            p.stats.ballControl = s.value("ball_control", 50.0f);
            p.stats.balancing   = s.value("balancing", 50.0f);

            // TECHNIQUE
            p.stats.curl     = s.value("curl", 50.0f);
            p.stats.deadBall = s.value("dead_ball", 50.0f);

            // PASSING
            p.stats.shortPassing = s.value("short_passing", 50.0f);
            p.stats.longPassing  = s.value("long_passing", 50.0f);

            // SPEED
            p.stats.topSpeed     = s.value("top_speed", 50.0f);
            p.stats.acceleration = s.value("acceleration", 50.0f);
            p.stats.agility      = s.value("agility", 50.0f);

            // PHYSICAL
            p.stats.bodyStrength    = s.value("body_strength", 50.0f);
            p.stats.kickPower       = s.value("kick_power", 50.0f);
            p.stats.jumpingStrength = s.value("jumping_strength", 50.0f);

            // MENTAL
            p.stats.awareness  = s.value("awareness", 50.0f);
            p.stats.aggression = s.value("aggression", 50.0f);
            p.stats.blocking   = s.value("blocking", 50.0f);

            // GOALKEEPING
            p.stats.gkCoverage  = s.value("gk_coverage", 10.0f);
            p.stats.gkReactions = s.value("gk_reactions", 10.0f);
            p.stats.gkCatching  = s.value("gk_catching", 10.0f);
            p.stats.gkThrowing  = s.value("gk_throwing", 10.0f);
            p.stats.gkAwareness = s.value("gk_awareness", 10.0f);
            p.stats.gkBlocking  = s.value("gk_blocking", 10.0f);
        }
        else
        {
            // Fallback: If no stats are found in the JSON, auto-generate them based on their role!
            p.stats = PlayerStats::createFromRole(p.positionRole);
        }

        // --- Graphics ---
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

        // Save the completed player into our database map!
        players[id] = p;
    }

    std::cout << "Loaded " << players.size() << " players from database.\n";
}

PlayerData* GameDatabase::getPlayer(const std::string& id) {
    auto it = players.find(id);
    if (it != players.end()) {
        return &(it->second);
    }
    return nullptr;
}

// --- HELPER: Parse a single Kit component (Shirt, Shorts, or Socks) ---
KitData parseKitData(const json& kitJson)
{
    KitData kit;
    // Use our existing hexToColor helper!
    kit.primaryColor   = hexToColor(kitJson.value("primary_color", "#FFFFFF"));
    kit.badgeId        = kitJson.value("badge_id", "");
    kit.manufacturerId = kitJson.value("manufacturer_id", "");
    kit.sponsorId      = kitJson.value("sponsor_id", "");

    // Parse the multiple design layers (e.g., Stripes, Sleeves, Trims)
    if (kitJson.contains("design_layers"))
    {
        for (const auto& layerJson : kitJson["design_layers"])
        {
            KitLayer layer;
            layer.textureId = layerJson.value("texture_id", "");
            layer.color     = hexToColor(layerJson.value("color", "#FFFFFF"));
            kit.designLayers.push_back(layer);
        }
    }
    return kit;
}

// --- MAIN TEAM LOADER ---
void GameDatabase::loadTeamsFromFile(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        std::cerr << "ERROR: Could not open database file: " << filepath << "\n";
        return;
    }

    json j;
    file >> j;

    // Loop through every team ID in the JSON (e.g., "TEAM_LIV")
    for (auto& [id, teamData] : j.items())
    {
        TeamData t;
        t.id = id;

        // --- Core Info ---
        t.fullName         = teamData.value("full_name", "Unknown Team");
        t.shortName        = teamData.value("short_name", "UNK");
        t.badgeId          = teamData.value("badge_id", "Badge_Default");
        t.stadiumName      = teamData.value("stadium_name", "Generic Stadium");
        t.managerName      = teamData.value("manager_name", "Unknown Manager");
        t.defaultFormation = teamData.value("default_formation", "4-4-2");
        t.uiColor          = hexToColor(teamData.value("ui_color", "#FFFFFF"));

        // --- Kits ---
        if (teamData.contains("kits"))
        {
            auto& kits = teamData["kits"];
            if (kits.contains("shirt"))
                t.shirt = parseKitData(kits["shirt"]);
            if (kits.contains("shorts"))
                t.shorts = parseKitData(kits["shorts"]);
            if (kits.contains("socks"))
                t.socks = parseKitData(kits["socks"]);
        }

        // --- Roster ---
        t.captainPlayerId = teamData.value("captain_id", "");

        if (teamData.contains("roster"))
        {
            for (const auto& playerId : teamData["roster"])
            {
                t.rosterPlayerIds.push_back(playerId.get<std::string>());
            }
        }

        // Save the completed team into our database map
        teams[id] = t;
    }

    std::cout << "Loaded " << teams.size() << " teams from database.\n";
}

TeamData* GameDatabase::getTeam(const std::string& id)
{
    auto it = teams.find(id);
    if (it != teams.end())
    {
        return &(it->second);
    }
    return nullptr;
}

// --- HELPER: Serialize KitData back to JSON ---
json serializeKitData(const KitData& kit)
{
    json j;
    j["primary_color"] = colorToHex(kit.primaryColor);
    if (!kit.badgeId.empty())
        j["badge_id"] = kit.badgeId;
    if (!kit.manufacturerId.empty())
        j["manufacturer_id"] = kit.manufacturerId;
    if (!kit.sponsorId.empty())
        j["sponsor_id"] = kit.sponsorId;

    j["design_layers"] = json::array(); // Ensure it's an array even if empty
    for (const auto& layer : kit.designLayers)
    {
        json layerJson;
        layerJson["texture_id"] = layer.textureId;
        layerJson["color"]      = colorToHex(layer.color);
        j["design_layers"].push_back(layerJson);
    }
    return j;
}

// --- SAVE PLAYERS ---
void GameDatabase::savePlayersToFile(const std::string& filepath)
{
    json j;

    for (const auto& [id, p] : players)
    {
        j[id]["name"]           = p.name;
        j[id]["number"]         = p.squadNumber;
        j[id]["age"]            = p.age;
        j[id]["height_cm"]      = p.heightCm;
        j[id]["weight_kg"]      = p.weightKg;
        j[id]["preferred_foot"] = p.preferredFoot;
        j[id]["position"]       = p.positionRole;

        // The JSON library auto-converts vectors and maps perfectly!
        j[id]["traits"] = p.traits;
        j[id]["position"] = roleToString(p.positionRole);

        auto& s               = j[id]["stats"];
        s["finishing"]        = p.stats.finishing;
        s["heading"]          = p.stats.heading;
        s["ball_control"]     = p.stats.ballControl;
        s["balancing"]        = p.stats.balancing;
        s["curl"]             = p.stats.curl;
        s["dead_ball"]        = p.stats.deadBall;
        s["short_passing"]    = p.stats.shortPassing;
        s["long_passing"]     = p.stats.longPassing;
        s["top_speed"]        = p.stats.topSpeed;
        s["acceleration"]     = p.stats.acceleration;
        s["agility"]          = p.stats.agility;
        s["body_strength"]    = p.stats.bodyStrength;
        s["kick_power"]       = p.stats.kickPower;
        s["jumping_strength"] = p.stats.jumpingStrength;
        s["awareness"]        = p.stats.awareness;
        s["aggression"]       = p.stats.aggression;
        s["blocking"]         = p.stats.blocking;
        s["gk_coverage"]      = p.stats.gkCoverage;
        s["gk_blocking"]      = p.stats.gkBlocking;
        s["gk_catching"]      = p.stats.gkCatching;
        s["gk_throwing"]      = p.stats.gkThrowing;
        s["gk_reactions"]     = p.stats.gkReactions;
        s["gk_awareness"]     = p.stats.gkAwareness;

        // Graphics
        auto& gfx          = j[id]["graphics"];
        gfx["skin_color"]  = colorToHex(p.graphics.skinColor);
        gfx["face_type"]   = p.graphics.faceType;
        gfx["hair_type"]   = p.graphics.hairType;
        gfx["hair_color"]  = colorToHex(p.graphics.hairColor);
        gfx["beard_type"]  = p.graphics.beardType;
        gfx["beard_color"] = colorToHex(p.graphics.beardColor);
        gfx["boot_type"]   = p.graphics.bootType;
        gfx["boot_color"]  = colorToHex(p.graphics.bootColor);
        gfx["accessories"] = p.graphics.accessories;
    }

    std::ofstream file(filepath);
    if (file.is_open())
    {
        file << j.dump(4); // The '4' adds beautiful 4-space indentation!
        file.close();
        std::cout << "Successfully saved players to " << filepath << "\n";
    }
}

// --- SAVE TEAMS ---
void GameDatabase::saveTeamsToFile(const std::string& filepath)
{
    json j;

    for (const auto& [id, t] : teams)
    {
        j[id]["full_name"]         = t.fullName;
        j[id]["short_name"]        = t.shortName;
        j[id]["badge_id"]          = t.badgeId;
        j[id]["stadium_name"]      = t.stadiumName;
        j[id]["manager_name"]      = t.managerName;
        j[id]["default_formation"] = t.defaultFormation;
        j[id]["ui_color"]          = colorToHex(t.uiColor);

        j[id]["kits"]["shirt"]  = serializeKitData(t.shirt);
        j[id]["kits"]["shorts"] = serializeKitData(t.shorts);
        j[id]["kits"]["socks"]  = serializeKitData(t.socks);

        j[id]["captain_id"] = t.captainPlayerId;
        j[id]["roster"]     = t.rosterPlayerIds;
    }

    std::ofstream file(filepath);
    if (file.is_open())
    {
        file << j.dump(4);
        file.close();
        std::cout << "Successfully saved teams to " << filepath << "\n";
    }
}

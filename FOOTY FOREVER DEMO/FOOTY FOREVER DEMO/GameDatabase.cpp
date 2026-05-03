    #include "GameDatabase.h"
    #include "PlaystyleDatabase.h"
    #include "json.hpp"
    #include <fstream>
    #include <iostream>
    #include <iomanip>
    #include <sstream>
    #include <vector>
    #include <filesystem>
    #include "MatchInfo.h"

    namespace fs = std::filesystem;

    using json = nlohmann::json;

    // Maps a formation string into visual "Lines" (GK, DEF, MID, ATT).
    // Now includes the strict 0-10 Slot ID!
    std::vector<std::vector<std::pair<int, PositionRole>>> getFormationLayout(const std::string& formation) {
        if (formation == "4-4-2") {
            return {
                { {0, PositionRole::Goalkeeper} },
                { {1, PositionRole::RightBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack}, {4, PositionRole::LeftBack} },
                { {5, PositionRole::RightMid}, {6, PositionRole::CenterMid}, {7, PositionRole::CenterMid}, {8, PositionRole::LeftMid} },
                { {9, PositionRole::Striker}, {10, PositionRole::Striker} }
            };
        }
        else if (formation == "4-2-3-1") {
            return {
                { {0, PositionRole::Goalkeeper} },
                { {1, PositionRole::RightBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack}, {4, PositionRole::LeftBack} },
                { {5, PositionRole::DefensiveMid}, {6, PositionRole::DefensiveMid} },
                { {7, PositionRole::RightMid}, {8, PositionRole::AttackingMid}, {9, PositionRole::LeftMid} },
                { {10, PositionRole::Striker} }
            };
        }
        else if (formation == "4-1-4-1") {
            return {
                { {0, PositionRole::Goalkeeper} },
                { {1, PositionRole::RightBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack}, {4, PositionRole::LeftBack} },
                { {5, PositionRole::DefensiveMid} },
                { {6, PositionRole::RightMid}, {7, PositionRole::CenterMid}, {8, PositionRole::CenterMid}, {9, PositionRole::LeftMid} },
                { {10, PositionRole::Striker} }
            };
        }
        else if (formation == "4-1-2-1-2") { // Narrow Diamond
            return {
                { {0, PositionRole::Goalkeeper} },
                { {1, PositionRole::RightBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack}, {4, PositionRole::LeftBack} },
                { {5, PositionRole::DefensiveMid} },
                { {6, PositionRole::CenterMid}, {7, PositionRole::CenterMid} },
                { {8, PositionRole::AttackingMid} },
                { {9, PositionRole::Striker}, {10, PositionRole::Striker} }
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
        else if (formation == "3-4-3") {
            return {
                { {0, PositionRole::Goalkeeper} },
                { {1, PositionRole::CenterBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack} },
                { {4, PositionRole::RightMid}, {5, PositionRole::CenterMid}, {6, PositionRole::CenterMid}, {7, PositionRole::LeftMid} },
                { {8, PositionRole::RightWing}, {9, PositionRole::Striker}, {10, PositionRole::LeftWing} }
            };
        }
        else if (formation == "3-5-2") {
            return {
                { {0, PositionRole::Goalkeeper} },
                { {1, PositionRole::CenterBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack} },
                { {4, PositionRole::RightWingBack}, {5, PositionRole::CenterMid}, {6, PositionRole::DefensiveMid}, {7, PositionRole::CenterMid}, {8, PositionRole::LeftWingBack} },
                { {9, PositionRole::Striker}, {10, PositionRole::Striker} }
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
                { {6, PositionRole::CenterMid}, {7, PositionRole::CenterMid} },
                { {8, PositionRole::RightWing}, {9, PositionRole::Striker}, {10, PositionRole::LeftWing} }
            };
        }
        else if (formation == "5-4-1") {
            return {
                { {0, PositionRole::Goalkeeper} },
                { {1, PositionRole::RightWingBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack}, {4, PositionRole::CenterBack}, {5, PositionRole::LeftWingBack} },
                { {6, PositionRole::RightMid}, {7, PositionRole::CenterMid}, {8, PositionRole::CenterMid}, {9, PositionRole::LeftMid} },
                { {10, PositionRole::Striker} }
            };
        }

        // Default to standard 4-3-3
        return {
            { {0, PositionRole::Goalkeeper} },
            { {1, PositionRole::RightBack}, {2, PositionRole::CenterBack}, {3, PositionRole::CenterBack}, {4, PositionRole::LeftBack} },
            { {5, PositionRole::CenterMid}, {6, PositionRole::DefensiveMid}, {7, PositionRole::CenterMid} },
            { {8, PositionRole::RightWing}, {9, PositionRole::Striker}, {10, PositionRole::LeftWing} }
        };
    }

    PositionRole stringToRole(const std::string& str)
    {
        if (str == "Goalkeeper") return PositionRole::Goalkeeper;
        if (str == "Left Back" || str == "LeftBack") return PositionRole::LeftBack;
        if (str == "Center Back" || str == "CenterBack") return PositionRole::CenterBack;
        if (str == "Right Back" || str == "RightBack") return PositionRole::RightBack;
        if (str == "Left Wing Back" || str == "LeftWingBack") return PositionRole::LeftWingBack;
        if (str == "Right Wing Back" || str == "RightWingBack") return PositionRole::RightWingBack;
        if (str == "Defensive Mid" || str == "DefensiveMid") return PositionRole::DefensiveMid;
        if (str == "Center Mid" || str == "CenterMid") return PositionRole::CenterMid;
        if (str == "Left Mid" || str == "LeftMid") return PositionRole::LeftMid;
        if (str == "Right Mid" || str == "RightMid") return PositionRole::RightMid;
        if (str == "Attacking Mid" || str == "AttackingMid") return PositionRole::AttackingMid;
        if (str == "Left Wing" || str == "LeftWing") return PositionRole::LeftWing;
        if (str == "Right Wing" || str == "RightWing") return PositionRole::RightWing;
        if (str == "Center Forward" || str == "CenterForward") return PositionRole::CenterForward;
        if (str == "Striker") return PositionRole::Striker;

        return PositionRole::CenterMid; // Default
    }

    std::string roleToString(PositionRole role)
    {
        switch (role)
        {
        case PositionRole::Goalkeeper: return "Goalkeeper";
        case PositionRole::LeftBack: return "Left Back";
        case PositionRole::CenterBack: return "Center Back";
        case PositionRole::RightBack: return "Right Back";
        case PositionRole::LeftWingBack: return "Left Wing Back";
        case PositionRole::RightWingBack: return "Right Wing Back";
        case PositionRole::DefensiveMid: return "Defensive Mid";
        case PositionRole::CenterMid: return "Center Mid";
        case PositionRole::LeftMid: return "Left Mid";
        case PositionRole::RightMid: return "Right Mid";
        case PositionRole::AttackingMid: return "Attacking Mid";
        case PositionRole::LeftWing: return "Left Wing";
        case PositionRole::RightWing: return "Right Wing";
        case PositionRole::CenterForward: return "Center Forward";
        case PositionRole::Striker: return "Striker";
        default: return "Center Mid"; // Fixed missing space on fallback!
        }
    }

    PlaystyleType stringToPlaystyle(const std::string& str)
    {
        // Goalkeepers
        if (str == "Sweeper Keeper" || str == "SweeperKeeper") return PlaystyleType::SweeperKeeper;
        if (str == "On The Line" || str == "OnTheLine") return PlaystyleType::OnTheLine;
        if (str == "Distributor") return PlaystyleType::Distributor;
        // Defenders
        if (str == "Sweeper") return PlaystyleType::Sweeper;
        if (str == "The Wall" || str == "TheWall") return PlaystyleType::TheWall;
        if (str == "The Killer" || str == "TheKiller") return PlaystyleType::TheKiller;
        if (str == "Calm And Collected" || str == "CalmAndCollected") return PlaystyleType::CalmAndCollected;
        // Fullbacks
        if (str == "Defensive FB" || str == "DefensiveFB") return PlaystyleType::DefensiveFB;
        if (str == "Up And Down" || str == "UpAndDown") return PlaystyleType::UpAndDown;
        if (str == "The Roamer FB" || str == "TheRoamerFB") return PlaystyleType::TheRoamerFB;
        if (str == "The Crosser" || str == "TheCrosser") return PlaystyleType::TheCrosser;
        // DMs
        if (str == "Orchestrator DM" || str == "OrchestratorDM") return PlaystyleType::OrchestratorDM;
        if (str == "The Killer DM" || str == "TheKillerDM") return PlaystyleType::TheKillerDM;
        if (str == "Three Lung DM" || str == "ThreeLungDM") return PlaystyleType::ThreeLungDM;
        if (str == "Defensive Roamer" || str == "DefensiveRoamer") return PlaystyleType::DefensiveRoamer;
        if (str == "Backline Brawler" || str == "BacklineBrawler") return PlaystyleType::BacklineBrawler;
        // CMs
        if (str == "Orchestrator CM" || str == "OrchestratorCM") return PlaystyleType::OrchestratorCM;
        if (str == "Box To Box" || str == "BoxToBox") return PlaystyleType::BoxToBox;
        if (str == "Playmaker CM" || str == "PlaymakerCM") return PlaystyleType::PlaymakerCM;
        if (str == "Three Lung CM" || str == "ThreeLungCM") return PlaystyleType::ThreeLungCM; // Fixed spacing
        if (str == "Quick Passer" || str == "QuickPasser") return PlaystyleType::QuickPasser;
        if (str == "Roamer CM" || str == "RoamerCM") return PlaystyleType::RoamerCM;
        // AMs
        if (str == "Playmaker AM" || str == "PlaymakerAM") return PlaystyleType::PlaymakerAM;
        if (str == "Hardcore Press" || str == "HardcorePress") return PlaystyleType::HardcorePress;
        if (str == "Trickster AM" || str == "TricksterAM") return PlaystyleType::TricksterAM;
        if (str == "Finisher AM" || str == "FinisherAM") return PlaystyleType::FinisherAM;
        // Wide
        if (str == "Wide Winger" || str == "WideWinger") return PlaystyleType::WideWinger;
        if (str == "False Winger" || str == "FalseWinger") return PlaystyleType::FalseWinger;
        if (str == "Roamer Winger" || str == "RoamerWinger") return PlaystyleType::RoamerWinger;
        if (str == "Classic Wide Mid" || str == "ClassicWideMid") return PlaystyleType::ClassicWideMid;
        if (str == "Defensive Winger" || str == "DefensiveWinger") return PlaystyleType::DefensiveWinger;
        if (str == "Inverted Wide Mid" || str == "InvertedWideMid") return PlaystyleType::InvertedWideMid;
        if (str == "Joga Bonito" || str == "JogaBonito") return PlaystyleType::JogaBonito;
        // Strikers
        if (str == "Finisher") return PlaystyleType::Finisher;
        if (str == "The Target" || str == "TheTarget") return PlaystyleType::TheTarget;
        if (str == "False 9" || str == "False9") return PlaystyleType::False9;
        if (str == "Second Striker" || str == "SecondStriker") return PlaystyleType::SecondStriker;
        if (str == "Shadow Striker" || str == "ShadowStriker") return PlaystyleType::ShadowStriker;

        return PlaystyleType::BoxToBox; // Default Fallback
    }

    std::string playstyleToString(PlaystyleType type)
    {
        switch (type) {
        case PlaystyleType::SweeperKeeper: return "Sweeper Keeper";
        case PlaystyleType::OnTheLine: return "On The Line";
        case PlaystyleType::Distributor: return "Distributor";
        case PlaystyleType::Sweeper: return "Sweeper";
        case PlaystyleType::TheWall: return "The Wall";
        case PlaystyleType::TheKiller: return "The Killer";
        case PlaystyleType::CalmAndCollected: return "Calm And Collected";
        case PlaystyleType::DefensiveFB: return "Defensive FB";
        case PlaystyleType::UpAndDown: return "Up And Down";
        case PlaystyleType::TheRoamerFB: return "The Roamer FB";
        case PlaystyleType::TheCrosser: return "The Crosser";
        case PlaystyleType::OrchestratorDM: return "Orchestrator DM";
        case PlaystyleType::TheKillerDM: return "The Killer DM";
        case PlaystyleType::ThreeLungDM: return "Three Lung DM";
        case PlaystyleType::DefensiveRoamer: return "Defensive Roamer";
        case PlaystyleType::BacklineBrawler: return "Backline Brawler";
        case PlaystyleType::OrchestratorCM: return "Orchestrator CM";
        case PlaystyleType::BoxToBox: return "Box To Box";
        case PlaystyleType::PlaymakerCM: return "Playmaker CM";
        case PlaystyleType::ThreeLungCM: return "Three Lung CM"; // Fixed spacing
        case PlaystyleType::QuickPasser: return "Quick Passer";
        case PlaystyleType::RoamerCM: return "Roamer CM";
        case PlaystyleType::PlaymakerAM: return "Playmaker AM";
        case PlaystyleType::HardcorePress: return "Hardcore Press";
        case PlaystyleType::TricksterAM: return "Trickster AM";
        case PlaystyleType::FinisherAM: return "Finisher AM";
        case PlaystyleType::WideWinger: return "Wide Winger";
        case PlaystyleType::FalseWinger: return "False Winger";
        case PlaystyleType::RoamerWinger: return "Roamer Winger";
        case PlaystyleType::ClassicWideMid: return "Classic Wide Mid";
        case PlaystyleType::DefensiveWinger: return "Defensive Winger";
        case PlaystyleType::InvertedWideMid: return "Inverted Wide Mid";
        case PlaystyleType::JogaBonito: return "Joga Bonito";
        case PlaystyleType::Finisher: return "Finisher";
        case PlaystyleType::TheTarget: return "The Target";
        case PlaystyleType::False9: return "False 9";
        case PlaystyleType::SecondStriker: return "Second Striker";
        case PlaystyleType::ShadowStriker: return "Shadow Striker";
        default: return "Box To Box";
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

    // ==========================================
    // --- NEW: PARSE & SERIALIZE ARRAYS ---
    // ==========================================
    std::vector<KitLayer> parseKitLayers(const json& jArray)
    {
        std::vector<KitLayer> layers;
        if (jArray.is_array()) {
            for (const auto& layerJson : jArray) {
                KitLayer layer;
                layer.textureId = layerJson.value("texture_id", "unknown");
                layer.color = hexToColor(layerJson.value("color", "#FFFFFF"));
                layers.push_back(layer);
            }
        }
        return layers;
    }

    json serializeKitLayers(const std::vector<KitLayer>& layers)
    {
        json jArray = json::array();
        for (const auto& layer : layers) {
            json layerJson;
            layerJson["texture_id"] = layer.textureId;
            layerJson["color"] = colorToHex(layer.color);
            jArray.push_back(layerJson);
        }
        return jArray;
    }


    PlayerData* GameDatabase::getPlayer(const std::string& id) {
        auto it = players.find(id);
        if (it != players.end()) {
            return &(it->second);
        }
        return nullptr;
    }

    Country* GameDatabase::getCountry(const std::string& code) {
        auto it = countries.find(code);
        if (it != countries.end()) {
            return &(it->second);
        }
        return nullptr;
    }

    void GameDatabase::initializeDefaultCountries() {
        // Clear any existing data just in case
        countries.clear();

        // Helper lambda to make adding countries clean and fast
        auto add = [&](const std::string& code, const std::string& name) {
            countries[code] = { code, name };
            };

        // ==========================================
        // --- ALL 55 UEFA MEMBERS (Europe) ---
        // ==========================================
        add("ALB", "Albania");
        add("AND", "Andorra");
        add("ARM", "Armenia");
        add("AUT", "Austria");
        add("AZE", "Azerbaijan");
        add("BLR", "Belarus");
        add("BEL", "Belgium");
        add("BIH", "Bosnia and Herzegovina");
        add("BUL", "Bulgaria");
        add("CRO", "Croatia");
        add("CYP", "Cyprus");
        add("CZE", "Czech Republic");
        add("DEN", "Denmark");
        add("ENG", "England");
        add("EST", "Estonia");
        add("FRO", "Faroe Islands");
        add("FIN", "Finland");
        add("FRA", "France");
        add("GEO", "Georgia");
        add("GER", "Germany");
        add("GIB", "Gibraltar");
        add("GRE", "Greece");
        add("HUN", "Hungary");
        add("ISL", "Iceland");
        add("ISR", "Israel");
        add("ITA", "Italy");
        add("KAZ", "Kazakhstan");
        add("KOS", "Kosovo");
        add("LVA", "Latvia");
        add("LIE", "Liechtenstein");
        add("LTU", "Lithuania");
        add("LUX", "Luxembourg");
        add("MKD", "North Macedonia");
        add("MLT", "Malta");
        add("MDA", "Moldova");
        add("MNE", "Montenegro");
        add("NED", "Netherlands");
        add("NIR", "Northern Ireland");
        add("NOR", "Norway");
        add("POL", "Poland");
        add("POR", "Portugal");
        add("IRL", "Republic of Ireland");
        add("ROU", "Romania");
        add("RUS", "Russia");
        add("SMR", "San Marino");
        add("SCO", "Scotland");
        add("SRB", "Serbia");
        add("SVK", "Slovakia");
        add("SVN", "Slovenia");
        add("ESP", "Spain");
        add("SWE", "Sweden");
        add("SUI", "Switzerland");
        add("TUR", "Turkey");
        add("UKR", "Ukraine");
        add("WAL", "Wales");

        // ==========================================
        // --- CONMEBOL HEAVYWEIGHTS (South America) ---
        // ==========================================
        add("ARG", "Argentina");
        add("BRA", "Brazil");
        add("URU", "Uruguay");
        add("COL", "Colombia");
        add("CHI", "Chile");
        add("PER", "Peru");
        add("ECU", "Ecuador");
        add("VEN", "Venezuela");
        add("PAR", "Paraguay");
        add("BOL", "Bolivia");

        // ==========================================
        // --- CONCACAF GIANTS (North America) ---
        // ==========================================
        add("USA", "United States");
        add("MEX", "Mexico");
        add("CAN", "Canada");
        add("CRC", "Costa Rica");
        add("JAM", "Jamaica");
        add("PAN", "Panama");

        // ==========================================
        // --- CAF POWERHOUSES (Africa) ---
        // ==========================================
        add("SEN", "Senegal");
        add("MAR", "Morocco");
        add("NGA", "Nigeria");
        add("EGY", "Egypt");
        add("CIV", "Ivory Coast");
        add("CMR", "Cameroon");
        add("ALG", "Algeria");
        add("GHA", "Ghana");
        add("MLI", "Mali");

        // ==========================================
        // --- AFC LEADERS (Asia & Australia) ---
        // ==========================================
        add("JPN", "Japan");
        add("IRN", "Iran");
        add("KOR", "South Korea");
        add("AUS", "Australia");
        add("KSA", "Saudi Arabia");
        add("QAT", "Qatar");

        std::cout << "Successfully initialized " << countries.size() << " default countries!\n";
    }

    LeagueData* GameDatabase::getLeague(const std::string& id) {
        auto it = leagues.find(id);
        if (it != leagues.end()) return &(it->second);
        return nullptr;
    }

    // ==========================================
    // --- UPDATED LOADING LOGIC ---
    // ==========================================
    void GameDatabase::loadDatabase(const std::string& baseDir) {
        players.clear();
        teams.clear();
        leagues.clear();
        initializeDefaultCountries();
        if (!fs::exists(baseDir)) return;

        for (const auto& entry : fs::recursive_directory_iterator(baseDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::ifstream file(entry.path());
                if (!file.is_open()) continue;

                json j;
                try { file >> j; }
                catch (...) { continue; }

                // --- PARSE LEAGUES ---
                if (entry.path().filename() == "LeagueData.json") {
                    for (auto& [id, lgData] : j.items()) {
                        LeagueData l;
                        l.id = id;
                        l.name = lgData.value("name", "Unknown League");
                        l.countryCode = lgData.value("country_code", "UNK");
                        l.tier = lgData.value("tier", 1);
                        if (lgData.contains("teams")) {
                            for (const auto& tId : lgData["teams"]) l.teamIds.push_back(tId.get<std::string>());
                        }
                        leagues[id] = l;
                    }
                }
                // --- PARSE TEAMS ---
                else if (entry.path().filename() == "TeamData.json") {
                    for (auto& [id, teamData] : j.items()) {
                        TeamData t;
                        t.id = id;
                        t.countryCode = teamData.value("country_code", "ENG");
                        t.leagueId = teamData.value("league_id", ""); // Load League ID!
                        t.fullName = teamData.value("full_name", "Unknown Team");
                        t.shortName = teamData.value("short_name", "UNK");
                        t.teamChemistry = teamData.value("team_chemistry", 100.f);
                        t.badgeId = teamData.value("badge_id", "Badge_Default");
                        t.stadiumName = teamData.value("stadium_name", "Generic Stadium");
                        t.managerName = teamData.value("manager_name", "Unknown Manager");
                        t.uiColor = hexToColor(teamData.value("ui_color", "#FFFFFF"));

                        if (teamData.contains("kits")) {
                            auto& kits = teamData["kits"];
                            if (kits.contains("shirt"))  t.shirtLayers = parseKitLayers(kits["shirt"]);
                            if (kits.contains("shorts")) t.shortsLayers = parseKitLayers(kits["shorts"]);
                            if (kits.contains("socks"))  t.socksLayers = parseKitLayers(kits["socks"]);
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
                            t.defaultTactics.attackingSpeed = tac.value("attacking_speed", 50);

                            t.defaultTactics.captainId = tac.value("captain_id", "");
                            t.defaultTactics.penaltyTakerId = tac.value("penalty_taker_id", "");
                            t.defaultTactics.leftCornerTakerId = tac.value("left_corner_taker_id", "");
                            t.defaultTactics.rightCornerTakerId = tac.value("right_corner_taker_id", "");
                            t.defaultTactics.freeKickTakerId = tac.value("free_kick_taker_id", "");

                            if (tac.contains("starting_xi")) {
                                for (auto& [slotStr, pId] : tac["starting_xi"].items()) {
                                    try {
                                        int slotId = std::stoi(slotStr);
                                        t.defaultTactics.startingXI[slotId] = pId.get<std::string>();
                                    }
                                    catch (const std::invalid_argument&) {
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
                        p.nationality = playerData.value("nationality", "ENG");
                        p.teamId = playerData.value("team_id", "");
                        p.tacticalFamiliarity = playerData.value("tactical_familiarity", 100.f);
                        p.squadNumber = playerData.value("number", 99);
                        p.age = playerData.value("age", 25);
                        p.heightCm = playerData.value("height_cm", 180);
                        p.weightKg = playerData.value("weight_kg", 75);
                        p.preferredFoot = playerData.value("preferred_foot", "Right");
                        p.positionRole = stringToRole(playerData.value("position", "CenterMid"));
                        p.positionFamiliarity[p.positionRole] = 4;
                        if (playerData.contains("familiarity")) {
                            for (auto& [roleStr, level] : playerData["familiarity"].items()) {
                                int prof = std::clamp(level.get<int>(), 1, 4);
                                p.positionFamiliarity[stringToRole(roleStr)] = prof;
                            }
                        }
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
                            p.stats.injuryResistance = s.value("injury_resistance", 2);
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
                            p.graphics.hairType = gfx.value("hair_type", "Hair_Bald");
                            p.graphics.hairColor = hexToColor(gfx.value("hair_color", "#000000"));
                            p.graphics.beardType = gfx.value("beard_type", "Beard_None");
                            p.graphics.beardColor = hexToColor(gfx.value("beard_color", "#000000"));
                            // --- BOOTS & LOGOS ---
                            p.graphics.bootType = gfx.value("boot_type", "player_boots_run_ing");
                            p.graphics.bootColor = hexToColor(gfx.value("boot_color", "#1E1E1E"));

                            p.graphics.bootLogo1Type = gfx.value("boot_logo1_type", "None");
                            p.graphics.bootLogo1Color = hexToColor(gfx.value("boot_logo1_color", "#FFFFFF"));

                            p.graphics.bootLogo2Type = gfx.value("boot_logo2_type", "None");
                            p.graphics.bootLogo2Color = hexToColor(gfx.value("boot_logo2_color", "#FFFFFF"));

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
        for (auto& [tId, t] : teams) {
            if (!t.leagueId.empty()) {
                LeagueData* l = getLeague(t.leagueId);
                if (l) {
                    // If the team isn't in the league's roster yet, add it
                    if (std::find(l->teamIds.begin(), l->teamIds.end(), tId) == l->teamIds.end()) {
                        l->teamIds.push_back(tId);
                    }
                }
                else {
                    // League doesn't exist, clear the ID so it becomes an Unassigned Team
                    t.leagueId = "";
                }
            }
        }

        std::cout << "Loaded " << leagues.size() << " leagues, " << teams.size() << " teams, and " << players.size() << " players.\n";
}

    void GameDatabase::deletePlayerFile(const std::string& id, const std::string& baseDir, const std::string& oldTeamId) {
        std::string folder;
        if (oldTeamId.empty()) {
            folder = baseDir + "/FreeAgents/";
        }
        else {
            TeamData* t = getTeam(oldTeamId);
            std::string cCode = (t && !t->countryCode.empty()) ? t->countryCode : "UNK";
            folder = baseDir + "/" + cCode + "/Teams/" + oldTeamId + "/Players/";
        }

        std::string path = folder + id + ".json";
        if (fs::exists(path)) fs::remove(path);
    }

    void GameDatabase::saveDatabase(const std::string& baseDir) {
        if (fs::exists(baseDir)) fs::remove_all(baseDir);
        fs::create_directories(baseDir);

        for (const auto& [id, l] : leagues) saveLeague(id, baseDir);
        for (const auto& [id, t] : teams) saveTeam(id, baseDir);
        for (const auto& [id, p] : players) savePlayer(id, baseDir);
    }

    void GameDatabase::saveLeague(const std::string& id, const std::string& baseDir) {
        LeagueData* l = getLeague(id);
        if (!l) return;

        std::string cCode = l->countryCode.empty() ? "UNK" : l->countryCode;
        std::string folder = baseDir + "/" + cCode + "/Leagues/" + id + "/";
        fs::create_directories(folder);

        json j;
        j[id]["name"] = l->name;
        j[id]["country_code"] = l->countryCode;
        j[id]["tier"] = l->tier;
        j[id]["teams"] = l->teamIds;

        std::ofstream file(folder + "LeagueData.json");
        if (file.is_open()) file << j.dump(4);
    }

    void GameDatabase::savePlayer(const std::string& id, const std::string& baseDir) {
        PlayerData* p = getPlayer(id);
        if (!p) return;

        std::string folder;
        if (p->teamId.empty()) {
            folder = baseDir + "/FreeAgents/";
        }
        else {
            TeamData* t = getTeam(p->teamId);
            std::string cCode = (t && !t->countryCode.empty()) ? t->countryCode : "UNK";
            std::string lCode = (t && !t->leagueId.empty()) ? t->leagueId : "NoLeague";

            // NEW PATH TIERING
            folder = baseDir + "/" + cCode + "/Leagues/" + lCode + "/Teams/" + p->teamId + "/Players/";
        }

        fs::create_directories(folder);

        json j;
        j[id]["name"] = p->name;
        j[id]["nationality"] = p->nationality;
        j[id]["team_id"] = p->teamId;
        j[id]["tactical_familiarity"] = p->tacticalFamiliarity;
        j[id]["number"] = p->squadNumber;
        j[id]["age"] = p->age;
        j[id]["height_cm"] = p->heightCm;
        j[id]["weight_kg"] = p->weightKg;
        j[id]["preferred_foot"] = p->preferredFoot;
        j[id]["position"] = roleToString(p->positionRole);
        j[id]["familiarity"] = json::object();
        for (const auto& [role, level] : p->positionFamiliarity) {
            if (role != p->positionRole) {
                j[id]["familiarity"][roleToString(role)] = level;
            }
        }
        j[id]["playstyle"] = playstyleToString(p->playstyle.type);
        j[id]["sharpness"] = p->sharpness;
        j[id]["loyalty"] = p->loyalty;
        j[id]["traits"] = p->traits;

        auto& s = j[id]["stats"];
        s["fitness"] = p->stats.naturalFitness;
        s["weak_foot_accuracy"] = p->stats.weakFootAccuracy;
        s["injury_resistance"] = p->stats.injuryResistance;
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
        gfx["hair_type"] = p->graphics.hairType;
        gfx["hair_color"] = colorToHex(p->graphics.hairColor);
        gfx["beard_type"] = p->graphics.beardType;
        gfx["beard_color"] = colorToHex(p->graphics.beardColor);

        // Save the boot base and the two independent logos
        gfx["boot_type"] = p->graphics.bootType;
        gfx["boot_color"] = colorToHex(p->graphics.bootColor);
        gfx["boot_logo1_type"] = p->graphics.bootLogo1Type;
        gfx["boot_logo1_color"] = colorToHex(p->graphics.bootLogo1Color);
        gfx["boot_logo2_type"] = p->graphics.bootLogo2Type;
        gfx["boot_logo2_color"] = colorToHex(p->graphics.bootLogo2Color);

        gfx["accessories"] = p->graphics.accessories;

        std::ofstream file(folder + id + ".json");
        if (file.is_open()) file << j.dump(4);
    }

    void GameDatabase::saveTeam(const std::string& id, const std::string& baseDir) {
        TeamData* t = getTeam(id);
        if (!t) return;

        std::string cCode = t->countryCode.empty() ? "UNK" : t->countryCode;
        std::string lCode = t->leagueId.empty() ? "NoLeague" : t->leagueId;

        // NEW PATH TIERING
        std::string folder = baseDir + "/" + cCode + "/Leagues/" + lCode + "/Teams/" + id + "/";
        fs::create_directories(folder);

        json j;
        j[id]["league_id"] = t->leagueId;
        j[id]["full_name"] = t->fullName;
        j[id]["short_name"] = t->shortName;
        j[id]["country_code"] = t->countryCode;
        j[id]["team_chemistry"] = t->teamChemistry;
        j[id]["badge_id"] = t->badgeId;
        j[id]["stadium_name"] = t->stadiumName;
        j[id]["manager_name"] = t->managerName;
        j[id]["ui_color"] = colorToHex(t->uiColor);

        // ==========================================
        // --- THE FIX: SERIALIZE THE ARRAYS ---
        // ==========================================
        j[id]["kits"]["shirt"] = serializeKitLayers(t->shirtLayers);
        j[id]["kits"]["shorts"] = serializeKitLayers(t->shortsLayers);
        j[id]["kits"]["socks"] = serializeKitLayers(t->socksLayers);

        j[id]["roster"] = t->rosterPlayerIds;

        auto& tac = j[id]["tactics"];
        tac["formation"] = t->defaultTactics.formationName;
        tac["defensive_depth"] = t->defaultTactics.defensiveDepth;
        tac["passing_length"] = t->defaultTactics.passingLength;
        tac["attacking_width"] = t->defaultTactics.attackingWidth;
        tac["pressing_intensity"] = t->defaultTactics.pressingIntensity;
        tac["positional_freedom"] = t->defaultTactics.positionalFreedom;
        tac["passing_speed"] = t->defaultTactics.passingSpeed;
        tac["attacking_speed"] = t->defaultTactics.attackingSpeed;

        tac["captain_id"] = t->defaultTactics.captainId;
        tac["penalty_taker_id"] = t->defaultTactics.penaltyTakerId;
        tac["free_kick_taker_id"] = t->defaultTactics.freeKickTakerId;
        tac["left_corner_taker_id"] = t->defaultTactics.leftCornerTakerId;
        tac["right_corner_taker_id"] = t->defaultTactics.rightCornerTakerId;

        tac["starting_xi"] = json::object();
        for (const auto& [slotId, pId] : t->defaultTactics.startingXI) {
            if (!pId.empty()) {
                tac["starting_xi"][std::to_string(slotId)] = pId;
            }
        }

        std::ofstream file(folder + "TeamData.json");
        if (file.is_open()) file << j.dump(4);
    }

    // ==========================================
    // --- THE MISSING FUNCTION: PUT THIS BACK! ---
    // ==========================================
    TeamData* GameDatabase::getTeam(const std::string& id) {
        auto it = teams.find(id);
        if (it != teams.end()) {
            return &(it->second);
        }
        return nullptr;
    }

    CompetitionData* GameDatabase::getCompetition(const std::string& id) {
        auto it = competitions.find(id);
        if (it != competitions.end()) return &(it->second);
        return nullptr;
    }

    CupTournamentData* GameDatabase::getCupTournament(const std::string& id) {
        auto it = cupTournaments.find(id);
        if (it != cupTournaments.end()) return &(it->second);
        return nullptr;
    }

    void GameDatabase::processMatchResult(const MatchInfo& info, const std::string& compId) {

        CompetitionData* comp = getCompetition(compId);



        // 1. Process all Goals & Assists
        for (const auto& goal : info.getGoals()) {
            // We don't credit own goals to a player's total tally
            if (!goal.isOwnGoal && !goal.scorerPlayerId.empty()) {
                if (comp) comp->playerStats[goal.scorerPlayerId].goals++;
            }

            if (!goal.assistPlayerId.empty()) {
                if (comp) comp->playerStats[goal.assistPlayerId].assists++;
            }
        }

        // 2. Process all Cards
        for (const auto& card : info.getCards()) {
            if (comp) {
                if (card.isRedCard) {
                    comp->playerStats[card.playerId].redCards++;
                }
                else {
                    comp->playerStats[card.playerId].yellowCards++;
                }
            }
        }

        // 3. Process Appearances & Minutes Played
        for (const auto& app : info.getAppearances()) {
            int minutesPlayed = app.minuteOff - app.minuteOn;
            if (minutesPlayed > 0 && comp) {
                comp->playerStats[app.playerId].appearances++;
                comp->playerStats[app.playerId].minutesPlayed += minutesPlayed;
            }
        }

        // ==========================================
        // --- THE FIX: PROCESS ADVANCED STATS & RATINGS ---
        // ==========================================
        if (comp) {
            for (const auto& [pId, pStats] : info.getPlayerStats()) {

                // This auto-creates the stat block if it's their first match!
                PlayerCompStats& compStats = comp->playerStats[pId];

                compStats.passesAttempted += pStats.passesAttempted;
                compStats.passesCompleted += pStats.passesCompleted;
                compStats.tacklesWon += pStats.tacklesWon;
                compStats.interceptions += pStats.interceptions;
                compStats.saves += pStats.saves;

                // Only add to the rating average if they played enough to get one
                if (pStats.matchRating > 1.0f) { // Assuming 0.0 means unrated
                    compStats.totalMatchRating += pStats.matchRating;
                    compStats.matchesRated++;
                }
            }
        }

        for (const auto& inj : info.getInjuries()) {
            PlayerData* p = getPlayer(inj.playerId);
            if (p) {
                p->isInjured = true;
                p->currentInjurySeverity = inj.severity;

                // THE FIX: Directly apply the exact injury from the simulation!
                p->currentInjury = inj.injuryName;
                p->injuryDaysRemaining = inj.durationDays;
            }
        }

        // 4. Update the Fixture Record
        if (comp) {
            // Find the specific fixture that matches these two teams and mark it played
            for (auto& fixture : comp->fixtures) {
                if (!fixture.isPlayed &&
                    ((fixture.homeTeamId == info.getHomeTeamId() && fixture.awayTeamId == info.getAwayTeamId()) ||
                        (fixture.homeTeamId == info.getAwayTeamId() && fixture.awayTeamId == info.getHomeTeamId())))
                {
                    fixture.homeScore = info.getHomeScore();
                    fixture.awayScore = info.getAwayScore();

                    // If it was played at the away stadium, swap the scores for the record
                    if (fixture.homeTeamId == info.getAwayTeamId()) {
                        fixture.homeScore = info.getAwayScore();
                        fixture.awayScore = info.getHomeScore();
                    }

                    fixture.isPlayed = true;
                    break; // Found and updated!
                }
            }
        }

        std::cout << "Successfully processed match result into database.\n";
    }
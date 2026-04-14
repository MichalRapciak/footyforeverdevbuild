#include "EditorScreen.h"
#include "imgui-1.92.6/imgui.h"
#include "PlaystyleDatabase.h"
#include "Game.h"
#include <cstdint>
#include <algorithm>

bool ColorEdit4SFML(const char* label, sf::Color& color)
{
    float col[4] = { color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f };
    if (ImGui::ColorEdit4(label, col))
    {
        color.r = static_cast<std::uint8_t>(col[0] * 255.f);
        color.g = static_cast<std::uint8_t>(col[1] * 255.f);
        color.b = static_cast<std::uint8_t>(col[2] * 255.f);
        color.a = static_cast<std::uint8_t>(col[3] * 255.f);
        return true;
    }
    return false;
}

EditorScreen::EditorScreen() : m_db(nullptr), bg_s(bg_txt), m_selectedPlayerId("") {}
EditorScreen::~EditorScreen() {}

void EditorScreen::init(sf::Font& font, GameDatabase& database)
{
    m_font = font;
    m_db = &database;
    if (!bg_txt.loadFromFile("ASSETS/IMAGES/help.png"))
    {
        std::cout << "couldn't load splash screen background\n";
    }
    bg_s.setTexture(bg_txt);
    bg_s.setTextureRect(sf::IntRect({ 0,0 }, { 1920,1080 }));
    bg_s.setPosition({ 0,0 });
}

void EditorScreen::update(sf::Time dt, sf::RenderWindow& window)
{
    ImVec2 fullScreenSize = ImVec2(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(fullScreenSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGui::Begin("Database Editor", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    float availableHeight = ImGui::GetContentRegionAvail().y - 50.0f;

    if (ImGui::BeginTabBar("EditorTabs"))
    {
        if (ImGui::BeginTabItem("Players")) {
            drawPlayerTab(availableHeight);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Teams")) {
            drawTeamTab(availableHeight);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    drawFooter();
    ImGui::End(); // End of the Main ImGui Window
}

// ==========================================
// --- MAIN TABS ---
// ==========================================

void EditorScreen::drawPlayerTab(float availableHeight)
{
    ImGui::Columns(2, "PlayerColumns", true);
    ImGui::SetColumnWidth(0, 200.0f);

    // ==========================================
    // --- LEFT COLUMN: Player List (Tree View) ---
    // ==========================================
    ImGui::BeginChild("PlayerList", ImVec2(0, availableHeight - 40.0f), true);

    // 1. Teams
    for (auto& [teamId, team] : m_db->teams) {
        if (ImGui::TreeNode(team.fullName.c_str())) {
            for (auto& [pId, player] : m_db->players) {
                if (player.teamId == teamId) {
                    bool isSelected = (m_selectedPlayerId == pId);
                    if (ImGui::Selectable(player.name.c_str(), isSelected)) m_selectedPlayerId = pId;
                }
            }
            ImGui::TreePop();
        }
    }

    // 2. Free Agents
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    if (ImGui::TreeNode("Free Agents")) {
        for (auto& [pId, player] : m_db->players) {
            if (player.teamId.empty()) {
                bool isSelected = (m_selectedPlayerId == pId);
                if (ImGui::Selectable(player.name.c_str(), isSelected)) m_selectedPlayerId = pId;
            }
        }
        ImGui::TreePop();
    }
    ImGui::PopStyleColor();

    ImGui::EndChild();

    if (ImGui::Button("+ Create New Player", ImVec2(-FLT_MIN, 30.0f))) {
        int counter = m_db->players.size() + 1000;
        std::string newId = "PLY_" + std::to_string(counter);
        while (m_db->players.find(newId) != m_db->players.end()) {
            counter++; newId = "PLY_" + std::to_string(counter);
        }

        PlayerData newPlayer;
        newPlayer.id = newId;
        newPlayer.name = "New Player";
        newPlayer.teamId = ""; // Free Agent
        newPlayer.squadNumber = 99;
        newPlayer.age = 18;
        newPlayer.heightCm = 180;
        newPlayer.weightKg = 75;
        newPlayer.preferredFoot = "Right";
        newPlayer.positionRole = PositionRole::CenterMid;
        newPlayer.graphics.skinColor = sf::Color(255, 224, 189);
        newPlayer.graphics.hairColor = sf::Color(40, 40, 40);
        newPlayer.graphics.beardColor = sf::Color(40, 40, 40);
        newPlayer.graphics.bootColor = sf::Color::Black;
        newPlayer.graphics.faceType = "Face_01";
        newPlayer.graphics.hairType = "Hair_Short";
        newPlayer.graphics.beardType = "Beard_None";
        newPlayer.graphics.bootType = "Boots_Basic";
        newPlayer.stats = PlayerStats::createFromRole(newPlayer.positionRole);

        m_db->players[newId] = newPlayer;
        m_selectedPlayerId = newId;

        // Auto-save the new player so the file exists immediately!
        m_db->savePlayer(newId, "ASSETS/DATA");
    }

    // ==========================================
    // --- THIS IS THE MAGIC LINE YOU MISSED ---
    // ==========================================
    ImGui::NextColumn();

    // ==========================================
    // --- RIGHT COLUMN: Player Editor ---
    // ==========================================
    ImGui::BeginChild("PlayerEditorPane", ImVec2(0, 0), false);

    if (!m_selectedPlayerId.empty()) {
        PlayerData* p = m_db->getPlayer(m_selectedPlayerId);
        if (p) 
        {
            ImGui::Text("Editing Player: %s", p->id.c_str());

            // --- DYNAMIC TEAM ASSIGNMENT & FILE CLEANUP ---
            std::string currentTeamName = "Free Agent";
            if (!p->teamId.empty()) {
                TeamData* t = m_db->getTeam(p->teamId);
                if (t) currentTeamName = t->fullName;
            }

            if (ImGui::BeginCombo("Team", currentTeamName.c_str())) {

                // Helper lambda to cleanly handle all the memory and file logic of a transfer
                auto handleTransfer = [&](const std::string& newTeamId) {
                    if (p->teamId == newTeamId) return;

                    // 1. Remove from old team's roster in memory
                    if (!p->teamId.empty()) {
                        TeamData* oldT = m_db->getTeam(p->teamId);
                        if (oldT) {
                            auto it = std::find(oldT->rosterPlayerIds.begin(), oldT->rosterPlayerIds.end(), p->id);
                            if (it != oldT->rosterPlayerIds.end()) oldT->rosterPlayerIds.erase(it);

                            // Safety Check: Remove them from the Starting XI and Captaincy if they leave!
                            if (oldT->defaultTactics.captainId == p->id) oldT->defaultTactics.captainId = "";
                            for (auto xiIt = oldT->defaultTactics.startingXI.begin(); xiIt != oldT->defaultTactics.startingXI.end(); ) {
                                if (xiIt->second == p->id) {
                                    xiIt = oldT->defaultTactics.startingXI.erase(xiIt);
                                }
                                else {
                                    ++xiIt;
                                }
                            }
                        }
                    }

                    // 2. Add to new team's roster in memory
                    if (!newTeamId.empty()) {
                        TeamData* newT = m_db->getTeam(newTeamId);
                        if (newT) {
                            // Ensure no duplicates just in case
                            if (std::find(newT->rosterPlayerIds.begin(), newT->rosterPlayerIds.end(), p->id) == newT->rosterPlayerIds.end()) {
                                newT->rosterPlayerIds.push_back(p->id);
                            }
                        }
                    }

                    // 3. Move the physical files on the hard drive
                    m_db->deletePlayerFile(p->id, "ASSETS/DATA", p->teamId);
                    p->teamId = newTeamId;
                    m_db->savePlayer(p->id, "ASSETS/DATA");
                    };

                // Option 1: Free Agent
                if (ImGui::Selectable("Free Agent", p->teamId.empty())) {
                    handleTransfer("");
                }
                ImGui::Separator();

                // Option 2: Actual Teams
                for (auto& [tId, team] : m_db->teams) {
                    if (ImGui::Selectable(team.fullName.c_str(), p->teamId == tId)) {
                        handleTransfer(tId);
                    }
                }
                ImGui::EndCombo();
            }
        }
            ImGui::Separator();

            // Basic Info
            char nameBuffer[128] = "";
            strcpy_s(nameBuffer, p->name.c_str());
            if (ImGui::InputText("Name", nameBuffer, IM_ARRAYSIZE(nameBuffer))) p->name = nameBuffer;

            ImGui::SliderInt("Squad Number", &p->squadNumber, 1, 99);
            ImGui::SliderInt("Age", &p->age, 15, 45);
            ImGui::SliderInt("Height (cm)", &p->heightCm, 150, 220);
            ImGui::SliderInt("Weight (kg)", &p->weightKg, 50, 110);

            const char* feet[] = { "Right", "Left", "Both" };
            int footIdx = (p->preferredFoot == "Left") ? 1 : (p->preferredFoot == "Both" ? 2 : 0);
            if (ImGui::Combo("Preferred Foot", &footIdx, feet, IM_ARRAYSIZE(feet))) p->preferredFoot = feet[footIdx];

            // --- UPDATED POSITION ROLES ---
            const char* roles[] = {
                "Goalkeeper", "Left Back", "Center Back", "Right Back",
                "Left Wing Back", "Right Wing Back", "Defensive Mid", "Center Mid",
                "Left Mid", "Right Mid", "Attacking Mid", "Left Wing", "Right Wing",
                "Center Forward", "Striker"
            };
            int roleIdx = static_cast<int>(p->positionRole);
            if (ImGui::Combo("Position Role", &roleIdx, roles, IM_ARRAYSIZE(roles))) {
                p->positionRole = static_cast<PositionRole>(roleIdx);
            }

            // --- NEW: FILTERED PLAYSTYLE SELECTOR ---
            std::vector<const char*> availableNames;
            std::vector<PlaystyleType> availableTypes;

            // 1. Filter the lists based on the current Position Role
            switch (p->positionRole) {
            case PositionRole::Goalkeeper:
                availableNames = { "Sweeper Keeper", "On The Line", "Distributor" };
                availableTypes = { PlaystyleType::SweeperKeeper, PlaystyleType::OnTheLine, PlaystyleType::Distributor };
                break;
            case PositionRole::CenterBack:
                availableNames = { "Sweeper", "The Wall", "The Killer", "Calm And Collected" };
                availableTypes = { PlaystyleType::Sweeper, PlaystyleType::TheWall, PlaystyleType::TheKiller, PlaystyleType::CalmAndCollected };
                break;
            case PositionRole::LeftBack:
            case PositionRole::RightBack:
            case PositionRole::LeftWingBack:
            case PositionRole::RightWingBack:
                availableNames = { "Defensive FB", "Up And Down", "The Roamer FB", "The Crosser" };
                availableTypes = { PlaystyleType::DefensiveFB, PlaystyleType::UpAndDown, PlaystyleType::TheRoamerFB, PlaystyleType::TheCrosser };
                break;
            case PositionRole::DefensiveMid:
                availableNames = { "Orchestrator DM", "The Killer DM", "Three Lung DM", "Defensive Roamer", "Backline Brawler" };
                availableTypes = { PlaystyleType::OrchestratorDM, PlaystyleType::TheKillerDM, PlaystyleType::ThreeLungDM, PlaystyleType::DefensiveRoamer, PlaystyleType::BacklineBrawler };
                break;
            case PositionRole::CenterMid:
                availableNames = { "Orchestrator CM", "Box To Box", "Playmaker CM", "Three Lung CM", "Quick Passer", "Roamer CM" };
                availableTypes = { PlaystyleType::OrchestratorCM, PlaystyleType::BoxToBox, PlaystyleType::PlaymakerCM, PlaystyleType::ThreeLungCM, PlaystyleType::QuickPasser, PlaystyleType::RoamerCM };
                break;
            case PositionRole::AttackingMid:
                availableNames = { "Playmaker AM", "Hardcore Press", "Trickster AM", "Finisher AM" };
                availableTypes = { PlaystyleType::PlaymakerAM, PlaystyleType::HardcorePress, PlaystyleType::TricksterAM, PlaystyleType::FinisherAM };
                break;
            case PositionRole::LeftMid:
            case PositionRole::RightMid:
            case PositionRole::LeftWing:
            case PositionRole::RightWing:
                availableNames = { "Wide Winger", "False Winger", "Roamer Winger", "Classic Wide Mid", "Defensive Winger", "Inverted Wide Mid" , "Joga Bonito"};
                availableTypes = { PlaystyleType::WideWinger, PlaystyleType::FalseWinger, PlaystyleType::RoamerWinger, PlaystyleType::ClassicWideMid, PlaystyleType::DefensiveWinger, PlaystyleType::InvertedWideMid, PlaystyleType::JogaBonito };
                break;
            case PositionRole::CenterForward:
            case PositionRole::Striker:
                availableNames = { "Finisher", "The Target", "False 9", "Second Striker", "Shadow Striker" };
                availableTypes = { PlaystyleType::Finisher, PlaystyleType::TheTarget, PlaystyleType::False9, PlaystyleType::SecondStriker, PlaystyleType::ShadowStriker };
                break;
            default:
                availableNames = { "Box To Box" };
                availableTypes = { PlaystyleType::BoxToBox };
                break;
            }

            // 2. Find the current selected index relative to the FILTERED list
            int playstyleIdx = 0;
            for (size_t i = 0; i < availableTypes.size(); ++i) {
                if (p->playstyle.type == availableTypes[i]) {
                    playstyleIdx = static_cast<int>(i);
                    break;
                }
            }

            // 3. Render the Combo Box using the dynamically filtered arrays
            if (ImGui::Combo("Playstyle", &playstyleIdx, availableNames.data(), static_cast<int>(availableNames.size()))) {
                // Safely assign the correct enum using the mapped array
                p->playstyle = PlaystyleDatabase::getPlaystyle(availableTypes[playstyleIdx]);
            }

            // Safety Fallback: If you change a player from Striker to CB, their active 
            // "Finisher" playstyle will suddenly become invalid for their new role. 
            // This forces them to default to the first valid playstyle in their new list!
            if (playstyleIdx == 0 && p->playstyle.type != availableTypes[0]) {
                p->playstyle = PlaystyleDatabase::getPlaystyle(availableTypes[0]);
            }

            // Stats
            ImGui::Separator();

            // Force an update to the overall rating so the UI reflects immediate slider changes
            p->stats.calculateOverallRating(p->positionRole);

            // ==========================================
            // --- LEFT COLUMN: STAT SLIDERS ---
            // ==========================================
            // Give the sliders 55% of the available width, and a fixed height to scroll within
            ImGui::BeginChild("StatsSlidersPane", ImVec2(ImGui::GetContentRegionAvail().x * 0.55f, 520.0f), false);

            ImGui::Text("Player Statistics");
            ImGui::SliderFloat("Natural Fitness", &p->stats.naturalFitness, 1.f, 99.f, "%.0f");
            ImGui::SliderInt("Weak Foot Accuracy", &p->stats.weakFootAccuracy, 1, 5);
            ImGui::SliderInt("Injury Resistance", &p->stats.injuryResistance, 1, 5);
            ImGui::Text("Shooting");
            ImGui::SliderFloat("Finishing", &p->stats.finishing, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Heading", &p->stats.heading, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Kick Power", &p->stats.kickPower, 1.f, 99.f, "%.0f");
            ImGui::Text("Passing");
            ImGui::SliderFloat("Short Passing", &p->stats.shortPassing, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Long Passing", &p->stats.longPassing, 1.f, 99.f, "%.0f");
            ImGui::Text("Technique");
            ImGui::SliderFloat("Dead Ball", &p->stats.deadBall, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Curl", &p->stats.curl, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Ball Control", &p->stats.ballControl, 1.f, 99.f, "%.0f");
            ImGui::Text("Speed");
            ImGui::SliderFloat("Top Speed", &p->stats.topSpeed, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Acceleration", &p->stats.acceleration, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Agility", &p->stats.agility, 1.f, 99.f, "%.0f");
            ImGui::Separator(); ImGui::Text("Physical");
            ImGui::SliderFloat("Body Strength", &p->stats.bodyStrength, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Jumping", &p->stats.jumpingStrength, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Balancing", &p->stats.balancing, 1.f, 99.f, "%.0f");
            ImGui::Separator(); ImGui::Text("Mental");
            ImGui::SliderFloat("Awareness", &p->stats.awareness, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Aggression", &p->stats.aggression, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Blocking", &p->stats.blocking, 1.f, 99.f, "%.0f");

            if (p->positionRole == PositionRole::Goalkeeper) {
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                ImGui::Text("(Goalkeeper Specific Stats)");
                ImGui::PopStyleColor();
                ImGui::SliderFloat("GK Coverage", &p->stats.gkCoverage, 1.f, 99.f, "%.0f");
                ImGui::SliderFloat("GK Reactions", &p->stats.gkReactions, 1.f, 99.f, "%.0f");
                ImGui::SliderFloat("GK Catching", &p->stats.gkCatching, 1.f, 99.f, "%.0f");
                ImGui::SliderFloat("GK Throwing", &p->stats.gkThrowing, 1.f, 99.f, "%.0f");
                ImGui::SliderFloat("GK Awareness", &p->stats.gkAwareness, 1.f, 99.f, "%.0f");
                ImGui::SliderFloat("GK Blocking", &p->stats.gkBlocking, 1.f, 99.f, "%.0f");
            }

            ImGui::Separator(); ImGui::Text("Invisible Stats");
            ImGui::SliderInt("Match Sharpness", &p->sharpness, 1, 99);
            ImGui::SliderInt("Loyalty", &p->loyalty, 1, 99);

            ImGui::EndChild(); // End Left Pane

            // ==========================================
            // --- RIGHT COLUMN: RADAR & OVERALL RATING ---
            // ==========================================
            ImGui::SameLine();
            ImGui::BeginChild("RadarPane", ImVec2(0, 520.0f), false);

            // --- BIG Overall Rating Text ---
            ImGui::SetWindowFontScale(4.0f); // Scale up standard text size
            std::string ovrStr = std::to_string(static_cast<int>(p->stats.overallRating));
            float textWidth = ImGui::CalcTextSize(ovrStr.c_str()).x;

            // Center the numbers
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) * 0.25f);
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.1f, 1.0f), "%s", ovrStr.c_str());
            ImGui::SetWindowFontScale(1.0f); // Reset scale for normal text

            const char* ovrLabel = "OVERALL";
            textWidth = ImGui::CalcTextSize(ovrLabel).x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) * 0.25f);
            ImGui::TextDisabled("%s", ovrLabel);

            ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();

            // --- Hexagon Math & Drawing ---
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
            float availWidth = ImGui::GetContentRegionAvail().x;

            // REDUCED RADIUS: Changed from 0.35f to 0.22f to leave plenty of room for text
            float radius = availWidth * 0.22f;

            // Center point of the radar
            ImVec2 center = ImVec2(cursorScreenPos.x + availWidth * 0.5f, cursorScreenPos.y + radius + 35.0f);

            // --- DYNAMIC DATA: Check if Goalkeeper ---
            float nodeStats[6];
            const char* nodeLabels[6];

            if (p->positionRole == PositionRole::Goalkeeper) {
                // Goalkeeper Specific Hexagon Data
                nodeStats[0] = p->stats.getGkCoverage();
                nodeStats[1] = p->stats.getGkReactions();
                nodeStats[2] = p->stats.getGkCatching();
                nodeStats[3] = p->stats.getGkThrowing();
                nodeStats[4] = p->stats.getGkAwareness();
                nodeStats[5] = p->stats.getGkBlocking();

                nodeLabels[0] = "COV"; // Coverage
                nodeLabels[1] = "REA"; // Reactions
                nodeLabels[2] = "CAT"; // Catching
                nodeLabels[3] = "THR"; // Throwing
                nodeLabels[4] = "AWA"; // GK Awareness
                nodeLabels[5] = "BLK"; // GK Blocking
            }
            else {
                // Outfield Player Hexagon Data
                nodeStats[0] = p->stats.getShootingAverage();
                nodeStats[1] = p->stats.getPassingAverage();
                nodeStats[2] = p->stats.getTechniqueAverage();
                nodeStats[3] = p->stats.getSpeedAverage();
                nodeStats[4] = p->stats.getPhysicalAverage();
                nodeStats[5] = p->stats.getMentalAverage();

                nodeLabels[0] = "SHOT";
                nodeLabels[1] = "PASS";
                nodeLabels[2] = "TECH";
                nodeLabels[3] = "PACE";
                nodeLabels[4] = "PHYS";
                nodeLabels[5] = "MENT";
            }

            // 1. Draw Background Web (Concentric circles/hexagons)
            const int numSegments = 6;
            for (int ring = 1; ring <= 5; ++ring) {
                float ringRadius = radius * (ring / 5.0f);
                ImVec2 points[6];
                for (int i = 0; i < numSegments; ++i) {
                    // Start at top (-90 degrees / -PI/2) and go clockwise
                    float angle = (i * (3.14159265f / 3.0f)) - (3.14159265f / 2.0f);
                    points[i] = ImVec2(center.x + cos(angle) * ringRadius, center.y + sin(angle) * ringRadius);
                }
                // Grey semi-transparent lines
                drawList->AddPolyline(points, 6, IM_COL32(100, 100, 100, 100), ImDrawFlags_Closed, 1.0f);
            }

            // 2. Draw the Player's Stat Polygon
            ImVec2 statPoints[6];
            for (int i = 0; i < numSegments; ++i) {
                float angle = (i * (3.14159265f / 3.0f)) - (3.14159265f / 2.0f);
                float statRadius = radius * (nodeStats[i] / 100.0f); // Scale distance by stat (0-100)
                statPoints[i] = ImVec2(center.x + cos(angle) * statRadius, center.y + sin(angle) * statRadius);
            }
            // Fill with a transparent green, outline with solid green
            drawList->AddConvexPolyFilled(statPoints, 6, IM_COL32(0, 255, 150, 100));
            drawList->AddPolyline(statPoints, 6, IM_COL32(0, 255, 150, 255), ImDrawFlags_Closed, 2.0f);

            // 3. Draw Labels and Numbers at the edges
            for (int i = 0; i < numSegments; ++i) {
                float angle = (i * (3.14159265f / 3.0f)) - (3.14159265f / 2.0f);
                float labelRadius = radius * 1.35f; // Push text outside the outer hexagon boundary
                ImVec2 labelCenter = ImVec2(center.x + cos(angle) * labelRadius, center.y + sin(angle) * labelRadius);

                char labelBuf[32];
                snprintf(labelBuf, sizeof(labelBuf), "%s\n%.0f", nodeLabels[i], nodeStats[i]);

                ImVec2 textSize = ImGui::CalcTextSize(labelBuf);
                ImVec2 textDrawPos = ImVec2(labelCenter.x - textSize.x * 0.5f, labelCenter.y - textSize.y * 0.5f);

                drawList->AddText(textDrawPos, IM_COL32(255, 255, 255, 255), labelBuf);
            }

            ImGui::EndChild(); // End Right Pane

            // Graphics
            ImGui::Separator(); ImGui::Text("Player Graphics");
            char faceBuf[64] = "", hairBuf[64] = "", beardBuf[64] = "", bootBuf[64] = "";
            strcpy_s(faceBuf, p->graphics.faceType.c_str());
            strcpy_s(hairBuf, p->graphics.hairType.c_str());
            strcpy_s(beardBuf, p->graphics.beardType.c_str());
            strcpy_s(bootBuf, p->graphics.bootType.c_str());

            if (ImGui::InputText("Face Texture ID", faceBuf, IM_ARRAYSIZE(faceBuf))) p->graphics.faceType = faceBuf;
            if (ImGui::InputText("Hair Texture ID", hairBuf, IM_ARRAYSIZE(hairBuf))) p->graphics.hairType = hairBuf;
            ColorEdit4SFML("Hair Color", p->graphics.hairColor);
            if (ImGui::InputText("Beard Texture ID", beardBuf, IM_ARRAYSIZE(beardBuf))) p->graphics.beardType = beardBuf;
            ColorEdit4SFML("Beard Color", p->graphics.beardColor);
            if (ImGui::InputText("Boot Texture ID", bootBuf, IM_ARRAYSIZE(bootBuf))) p->graphics.bootType = bootBuf;
            ColorEdit4SFML("Boot Color", p->graphics.bootColor);
            ColorEdit4SFML("Skin Color", p->graphics.skinColor);
    }
    else {
        ImGui::Text("Select a player to edit.");
    }

    ImGui::EndChild(); // <-- Ends the right pane scroll box
    ImGui::Columns(1); // <-- Ends the ImGui columns
}

void EditorScreen::drawTeamTab(float availableHeight)
{
    ImGui::Columns(2, "TeamColumns", true);
    ImGui::SetColumnWidth(0, 200.0f);

    // --- LEFT COLUMN: Team List ---
    ImGui::BeginChild("TeamList", ImVec2(0, availableHeight - 40.0f), true);
    for (auto& [id, team] : m_db->teams) {
        bool isSelected = (m_selectedTeamId == id);
        if (ImGui::Selectable(team.fullName.c_str(), isSelected)) m_selectedTeamId = id;
    }
    ImGui::EndChild();

    if (ImGui::Button("+ Create New Team", ImVec2(-FLT_MIN, 30.0f))) {
        int counter = m_db->teams.size() + 100;
        std::string newId = "TEAM_" + std::to_string(counter);
        while (m_db->teams.find(newId) != m_db->teams.end()) {
            counter++; newId = "TEAM_" + std::to_string(counter);
        }

        TeamData newTeam;
        newTeam.id = newId;
        newTeam.fullName = "New FC";
        newTeam.shortName = "NEW";
        newTeam.badgeId = "Badge_Default";
        newTeam.stadiumName = "Local Park";
        newTeam.managerName = "Coach";
        newTeam.uiColor = sf::Color(100, 100, 100);
        newTeam.shirt.primaryColor = sf::Color::White;
        newTeam.shorts.primaryColor = sf::Color::White;
        newTeam.socks.primaryColor = sf::Color::White;
        newTeam.defaultTactics.formationName = "4-3-3";
        newTeam.defaultTactics.defensiveDepth = 50;
        newTeam.defaultTactics.passingLength = 50;
        newTeam.defaultTactics.attackingWidth = 50;

        m_db->teams[newId] = newTeam;
        m_selectedTeamId = newId;
    }

    ImGui::NextColumn();

    // --- RIGHT COLUMN: Team Sub-Tabs ---
    if (!m_selectedTeamId.empty()) {
        TeamData* t = m_db->getTeam(m_selectedTeamId);
        if (t) {
            ImGui::Text("Editing Team: %s", t->id.c_str());
            ImGui::Separator();

            if (ImGui::BeginTabBar("TeamSubTabs")) {
                if (ImGui::BeginTabItem("General")) {
                    drawTeamGeneralTab(t);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Kits")) {
                    drawTeamKitsTab(t);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Club Roster")) {
                    drawTeamRosterTab(t);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Tactics & XI")) {
                    drawTeamTacticsTab(t);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
    }
    else {
        ImGui::Text("Select a team to edit.");
    }
    ImGui::Columns(1);
}

// ==========================================
// --- TEAM SUB-TABS ---
// ==========================================

void EditorScreen::drawTeamGeneralTab(TeamData* t)
{
    char nameBuffer[128] = "", shortNameBuffer[16] = "", stadiumBuffer[128] = "", managerBuffer[128] = "", badgeBuffer[128] = "";
    strcpy_s(nameBuffer, t->fullName.c_str());
    if (ImGui::InputText("Full Name", nameBuffer, IM_ARRAYSIZE(nameBuffer))) t->fullName = nameBuffer;

    strcpy_s(shortNameBuffer, t->shortName.c_str());
    if (ImGui::InputText("Short Name", shortNameBuffer, IM_ARRAYSIZE(shortNameBuffer))) t->shortName = shortNameBuffer;

    strcpy_s(stadiumBuffer, t->stadiumName.c_str());
    if (ImGui::InputText("Stadium", stadiumBuffer, IM_ARRAYSIZE(stadiumBuffer))) t->stadiumName = stadiumBuffer;

    strcpy_s(managerBuffer, t->managerName.c_str());
    if (ImGui::InputText("Manager Name", managerBuffer, IM_ARRAYSIZE(managerBuffer))) t->managerName = managerBuffer;

    strcpy_s(badgeBuffer, t->badgeId.c_str());
    if (ImGui::InputText("Main Badge ID", badgeBuffer, IM_ARRAYSIZE(badgeBuffer))) t->badgeId = badgeBuffer;

    ImGui::Spacing(); ImGui::Separator(); ImGui::Text("Team Branding");
    ColorEdit4SFML("UI Main Color", t->uiColor);
}

void EditorScreen::drawTeamKitsTab(TeamData* t)
{
    char sBadge[64] = "", sManuf[64] = "", sSpon[64] = "";

    if (ImGui::TreeNode("Shirt Details")) {
        ColorEdit4SFML("Shirt Primary Color", t->shirt.primaryColor);
        strcpy_s(sBadge, t->shirt.badgeId.c_str());
        strcpy_s(sManuf, t->shirt.manufacturerId.c_str());
        strcpy_s(sSpon, t->shirt.sponsorId.c_str());
        if (ImGui::InputText("Shirt Badge ID", sBadge, IM_ARRAYSIZE(sBadge))) t->shirt.badgeId = sBadge;
        if (ImGui::InputText("Shirt Manufacturer ID", sManuf, IM_ARRAYSIZE(sManuf))) t->shirt.manufacturerId = sManuf;
        if (ImGui::InputText("Shirt Sponsor ID", sSpon, IM_ARRAYSIZE(sSpon))) t->shirt.sponsorId = sSpon;
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Shorts Details")) {
        ColorEdit4SFML("Shorts Primary Color", t->shorts.primaryColor);
        strcpy_s(sBadge, t->shorts.badgeId.c_str());
        strcpy_s(sManuf, t->shorts.manufacturerId.c_str());
        if (ImGui::InputText("Shorts Badge ID", sBadge, IM_ARRAYSIZE(sBadge))) t->shorts.badgeId = sBadge;
        if (ImGui::InputText("Shorts Manufacturer ID", sManuf, IM_ARRAYSIZE(sManuf))) t->shorts.manufacturerId = sManuf;
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Socks Details")) {
        ColorEdit4SFML("Socks Primary Color", t->socks.primaryColor);
        strcpy_s(sManuf, t->socks.manufacturerId.c_str());
        if (ImGui::InputText("Socks Manufacturer ID", sManuf, IM_ARRAYSIZE(sManuf))) t->socks.manufacturerId = sManuf;
        ImGui::TreePop();
    }
}

void EditorScreen::drawTeamRosterTab(TeamData* t)
{
    float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f;

    ImGui::BeginChild("TeamRoster", ImVec2(halfWidth, 0), true);
    ImGui::Text("Current Squad");
    ImGui::Separator();

    for (auto it = t->rosterPlayerIds.begin(); it != t->rosterPlayerIds.end();) {
        std::string pId = *it;
        PlayerData* p = m_db->getPlayer(pId);
        if (p) {
            if (ImGui::Button(("X##" + pId).c_str())) {
                it = t->rosterPlayerIds.erase(it);
                if (t->defaultTactics.captainId == pId) t->defaultTactics.captainId = "";
                p->teamId = ""; // Make Free Agent
                continue;
            }
            ImGui::SameLine();

            bool isCaptain = (t->defaultTactics.captainId == pId);
            if (isCaptain) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.7f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.8f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.6f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            }
            if (ImGui::Button(("C##" + pId).c_str())) {
                t->defaultTactics.captainId = isCaptain ? "" : pId;
            }
            if (isCaptain) ImGui::PopStyleColor(4);

            ImGui::SameLine();
            if (isCaptain) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.84f, 0.0f, 1.0f));
            ImGui::Selectable((std::to_string(p->squadNumber) + ". " + p->name).c_str());
            if (isCaptain) ImGui::PopStyleColor();
        }
        ++it;
    }
    ImGui::EndChild();

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PLAYER_ID")) {
            std::string droppedId = static_cast<const char*>(payload->Data);
            if (std::find(t->rosterPlayerIds.begin(), t->rosterPlayerIds.end(), droppedId) == t->rosterPlayerIds.end()) {
                t->rosterPlayerIds.push_back(droppedId);
                PlayerData* droppedPlayer = m_db->getPlayer(droppedId);
                if (droppedPlayer) droppedPlayer->teamId = t->id;
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::SameLine();

    ImGui::BeginChild("AvailablePlayers", ImVec2(0, 0), true);
    ImGui::Text("Free Agents");
    ImGui::Separator();
    for (auto& [id, player] : m_db->players) {
        if (player.teamId.empty()) {
            ImGui::Selectable(player.name.c_str());
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("PLAYER_ID", id.c_str(), id.size() + 1);
                ImGui::Text("Sign %s to %s", player.name.c_str(), t->shortName.c_str());
                ImGui::EndDragDropSource();
            }
        }
    }
    ImGui::EndChild();
}

void EditorScreen::drawTeamTacticsTab(TeamData* t)
{
    float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f;

    // ==========================================
    // --- LEFT SIDE: TACTICAL SLIDERS & SET PIECES ---
    // ==========================================
    ImGui::BeginChild("TacticsLeftPane", ImVec2(halfWidth, 0), false);

    ImGui::Text("Team Style");
    ImGui::Separator();

    // --- FORMATION COMBO BOX ---
    const char* formations[] = { "4-3-3", "4-4-2", "4-2-4", "5-3-2", "5-2-3", "5-4-1" };
    int formIdx = 0;
    for (int i = 0; i < 6; ++i) {
        if (t->defaultTactics.formationName == formations[i]) {
            formIdx = i;
            break;
        }
    }
    if (ImGui::Combo("Formation", &formIdx, formations, IM_ARRAYSIZE(formations))) {
        t->defaultTactics.formationName = formations[formIdx];
    }

    // --- TACTICAL SLIDERS ---
    ImGui::SliderInt("Defensive Depth", &t->defaultTactics.defensiveDepth, 0, 100);
    ImGui::SliderInt("Passing Length", &t->defaultTactics.passingLength, 0, 100);
    ImGui::SliderInt("Attacking Width", &t->defaultTactics.attackingWidth, 0, 100);
    ImGui::SliderInt("Pressing Intensity", &t->defaultTactics.pressingIntensity, 0, 100);
    ImGui::SliderInt("Positional Freedom", &t->defaultTactics.positionalFreedom, 0, 100);
    ImGui::SliderInt("Passing Speed", &t->defaultTactics.passingSpeed, 0, 100);

    ImGui::Spacing();
    ImGui::Text("Set Piece Takers");
    ImGui::Separator();

    auto playerCombo = [&](const char* label, std::string& currentId) {
        std::string currentName = "None";
        if (!currentId.empty()) {
            PlayerData* p = m_db->getPlayer(currentId);
            if (p) currentName = p->name;
        }
        if (ImGui::BeginCombo(label, currentName.c_str())) {
            if (ImGui::Selectable("None", currentId.empty())) currentId = "";
            for (const auto& pId : t->rosterPlayerIds) {
                PlayerData* p = m_db->getPlayer(pId);
                if (p && ImGui::Selectable(p->name.c_str(), currentId == pId)) currentId = pId;
            }
            ImGui::EndCombo();
        }
        };

    playerCombo("Captain", t->defaultTactics.captainId);
    playerCombo("Penalties", t->defaultTactics.penaltyTakerId);
    playerCombo("Free Kicks", t->defaultTactics.freeKickTakerId);
    playerCombo("L. Corner", t->defaultTactics.leftCornerTakerId);
    playerCombo("R. Corner", t->defaultTactics.rightCornerTakerId);

    ImGui::EndChild(); // End Left Pane

    ImGui::SameLine();

    // ==========================================
    // --- RIGHT SIDE: STARTING XI ASSIGNMENT ---
    // ==========================================
    ImGui::BeginChild("TacticsRightPane", ImVec2(0, 0), false);

    ImGui::Text("Starting XI Builder (%s)", t->defaultTactics.formationName.c_str());
    ImGui::Separator();

    // Fetch the structural layout based on the current formation!
    auto formationLayout = getFormationLayout(t->defaultTactics.formationName);

    float paneWidth = ImGui::GetContentRegionAvail().x;

    // Draw the UI line by line (GK, DEF, MID, ATT)
    for (size_t i = 0; i < formationLayout.size(); ++i) {
        const auto& line = formationLayout[i];

        // Center the buttons dynamically based on how many players are in this line
        float buttonWidth = 90.0f;
        float totalLineWidth = (buttonWidth * line.size()) + (ImGui::GetStyle().ItemSpacing.x * (line.size() - 1));
        float startPosX = (paneWidth - totalLineWidth) * 0.5f;

        ImGui::SetCursorPosX(startPosX);

        for (size_t j = 0; j < line.size(); ++j) {
            int slotId = line[j].first;            // <--- NEW: Grab the Slot ID (0-10)
            PositionRole role = line[j].second;    // <--- The tactical role for the pitch
            std::string roleName = roleToString(role);

            // Format the display string: "Role\nPlayerName"
            std::string currPlayerName = "Empty";
            if (t->defaultTactics.startingXI.count(slotId)) {   // <--- Check map by slotId
                PlayerData* p = m_db->getPlayer(t->defaultTactics.startingXI[slotId]);
                if (p) {
                    size_t spacePos = p->name.find_last_of(' ');
                    currPlayerName = (spacePos != std::string::npos) ? p->name.substr(spacePos + 1) : p->name;
                }
            }

            // Hidden ID ensures ImGui knows which button is which, even if names are identical
            std::string buttonText = roleName + "\n" + currPlayerName;
            std::string buttonId = "##Slot_" + std::to_string(slotId);

            if (currPlayerName == "Empty") {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            }

            if (ImGui::Button((buttonText + buttonId).c_str(), ImVec2(buttonWidth, 40))) {
                if (currPlayerName != "Empty") {
                    t->defaultTactics.startingXI.erase(slotId); // <--- Erase by slotId
                }
            }

            if (currPlayerName == "Empty") {
                ImGui::PopStyleColor();
            }
            else if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Click to remove %s from XI", currPlayerName.c_str());
            }

            // Drag and Drop Logic
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ROSTER_PLAYER")) {
                    std::string droppedId = static_cast<const char*>(payload->Data);
                    t->defaultTactics.startingXI[slotId] = droppedId; // <--- Assign by slotId
                }
                ImGui::EndDragDropTarget();
            }

            if (j < line.size() - 1) ImGui::SameLine();
        }
        ImGui::Spacing(); // Add a gap between lines (e.g. between Def and Mid)
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Drag Source: Squad Roster");

    ImGui::BeginChild("TacticsRosterSource", ImVec2(0, 0), true);
    for (auto& pId : t->rosterPlayerIds) {
        PlayerData* p = m_db->getPlayer(pId);
        if (p) {
            ImGui::Selectable(p->name.c_str());
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("ROSTER_PLAYER", pId.c_str(), pId.size() + 1);
                ImGui::Text("Assign %s to XI", p->name.c_str());
                ImGui::EndDragDropSource();
            }
        }
    }
    ImGui::EndChild();

    ImGui::EndChild();
}

// ==========================================
// --- GLOBAL FOOTER ---
// ==========================================

void EditorScreen::drawFooter()
{
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));

    if (ImGui::Button("Save All Changes to JSON", ImVec2(200, 30))) {
        // --- NEW: Single call to save the whole directory structure ---
        m_db->saveDatabase("ASSETS/DATA");
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));

    if (ImGui::Button("Return to Main Menu", ImVec2(200, 30))) {
        Game::currentState = GameState::MainMenu;
    }
    ImGui::PopStyleColor(3);
}

void EditorScreen::render(sf::RenderWindow& window)
{
    window.draw(bg_s);
}

void EditorScreen::processEvents(sf::Event& event)
{
}
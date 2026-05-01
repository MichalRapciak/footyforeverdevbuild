#include "EditorScreen.h"
#include "imgui-1.92.6/imgui.h"
#include "imgui-1.92.6/imgui-sfml.h"
#include "PlaystyleDatabase.h"
#include "AnimationServer.h"
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

EditorScreen::EditorScreen() : m_db(nullptr), bg_s(bg_txt), m_selectedPlayerId(""), m_selectedTeamId("") {}
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

    // ==========================================
    // --- INITIALIZE RENDER PIPELINE ---
    // ==========================================
    AnimationServer::init();

    // THE FIX: SFML 3.0 Fragment Enum!
    if (!m_kitShader.loadFromFile("ASSETS/SHADERS/kit_mixer.frag", sf::Shader::Type::Fragment)) {
        std::cout << "Failed to load kit shader in Editor!\n";
    }

    m_playerPreviewTexture.resize({ 250, 250 });
    m_teamPreviewTexture.resize({ 250, 250 });
}

// ==========================================
// --- SHADER SNAPSHOT GENERATOR ---
// ==========================================
// THE FIX: Accept the dynamic vector stack!
void EditorScreen::updatePreviewTexture(sf::RenderTexture& target, sf::Color skin, const std::vector<KitLayer>& kitLayers, float heightCm, float weightKg)
{
    target.clear(sf::Color(30, 30, 40, 255));

    sf::Sprite s(AnimationServer::getSkinTexture());
    const Animation& anim = AnimationServer::getRunningAnimation(Direction::Down);
    s.setTextureRect(anim.frames[0]);
    s.setOrigin({ 250.f, 250.f });

    float scaleX = (weightKg / 70.0f) * 0.45f;
    float scaleY = (heightCm / 180.0f) * 0.45f;
    s.setScale({ scaleX, scaleY });

    s.setPosition({ target.getSize().x / 2.f, target.getSize().y / 2.f });
    s.setRotation(sf::degrees(180.f));

    auto toGlslColor = [](sf::Color c) {
        return sf::Glsl::Vec4(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
        };

    m_kitShader.setUniform("skinColor", toGlslColor(skin));
    m_kitShader.setUniform("skinTex", sf::Shader::CurrentTexture);

    // ==========================================
    // --- THE FIX: EXPAND TO 15 LAYER MAXIMUM ---
    // ==========================================
    static const std::string uUse[15] = {
        "use0", "use1", "use2", "use3", "use4", "use5", "use6", "use7",
        "use8", "use9", "use10", "use11", "use12", "use13", "use14"
    };
    static const std::string uTex[15] = {
        "tex0", "tex1", "tex2", "tex3", "tex4", "tex5", "tex6", "tex7",
        "tex8", "tex9", "tex10", "tex11", "tex12", "tex13", "tex14"
    };
    static const std::string uCol[15] = {
        "col0", "col1", "col2", "col3", "col4", "col5", "col6", "col7",
        "col8", "col9", "col10", "col11", "col12", "col13", "col14"
    };

    for (int i = 0; i < 15; ++i) {
        if (i < kitLayers.size()) {
            sf::Texture* tex = AnimationServer::getKitTexture(kitLayers[i].textureId);
            if (tex) {
                m_kitShader.setUniform(uUse[i], true);
                m_kitShader.setUniform(uTex[i], *tex);
                m_kitShader.setUniform(uCol[i], toGlslColor(kitLayers[i].color));
            }
            else {
                m_kitShader.setUniform(uUse[i], false);
            }
        }
        else {
            m_kitShader.setUniform(uUse[i], false);
        }
    }

    target.draw(s, &m_kitShader);
    target.display();
}

void EditorScreen::update(sf::Time dt, sf::RenderWindow& window)
{
    ImVec2 fullScreenSize = ImVec2(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(fullScreenSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGui::Begin("Database Editor", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

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
    ImGui::End();
}

// ==========================================
// --- MAIN TABS ---
// ==========================================

void EditorScreen::drawPlayerTab(float availableHeight)
{
    // 1. LEFT PANEL (Player List)
    ImGui::BeginChild("PlayerListPane", ImVec2(250.0f, availableHeight), true);

    ImGui::BeginChild("PlayerTree", ImVec2(0, availableHeight - 40.0f), false);
    for (auto& [code, country] : m_db->countries) {
        bool hasTeams = false;
        for (auto& [tId, team] : m_db->teams) {
            if (team.countryCode == code) { hasTeams = true; break; }
        }

        if (hasTeams) {
            if (ImGui::TreeNode(country.name.c_str())) {
                for (auto& [tId, team] : m_db->teams) {
                    if (team.countryCode == code) {
                        if (ImGui::TreeNode(team.fullName.c_str())) {
                            for (auto& [pId, player] : m_db->players) {
                                if (player.teamId == tId) {
                                    bool isSelected = (m_selectedPlayerId == pId);
                                    if (ImGui::Selectable(player.name.c_str(), isSelected)) m_selectedPlayerId = pId;
                                }
                            }
                            ImGui::TreePop();
                        }
                    }
                }
                ImGui::TreePop();
            }
        }
    }

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
    ImGui::EndChild(); // End PlayerTree

    if (ImGui::Button("+ Create New Player", ImVec2(-FLT_MIN, 30.0f))) {
        int counter = m_db->players.size() + 1000;
        std::string newId = "PLY_" + std::to_string(counter);
        while (m_db->players.find(newId) != m_db->players.end()) {
            counter++; newId = "PLY_" + std::to_string(counter);
        }

        PlayerData newPlayer;
        newPlayer.id = newId;
        newPlayer.nationality = "ENG";
        newPlayer.name = "New Player";
        newPlayer.teamId = "";
        newPlayer.tacticalFamiliarity = 100.f;
        newPlayer.squadNumber = 99;
        newPlayer.age = 18;
        newPlayer.heightCm = 180;
        newPlayer.weightKg = 75;
        newPlayer.preferredFoot = "Right";
        newPlayer.positionRole = PositionRole::CenterMid;
        newPlayer.positionFamiliarity[PositionRole::CenterMid] = 4;
        newPlayer.graphics.skinColor = sf::Color(255, 224, 189);
        newPlayer.graphics.hairColor = sf::Color(40, 40, 40);
        newPlayer.graphics.beardColor = sf::Color(40, 40, 40);
        newPlayer.graphics.hairType = "Hair_Short";
        newPlayer.graphics.beardType = "Beard_None";
        newPlayer.graphics.bootType = "player_boots_run_ing";
        newPlayer.graphics.bootColor = sf::Color(30, 30, 30); // Default dark grey boot

        newPlayer.graphics.bootLogo1Type = "boots_logo1_ing";
        newPlayer.graphics.bootLogo1Color = sf::Color::White; // Default white logo

        newPlayer.graphics.bootLogo2Type = "None"; // Second logo off by default
        newPlayer.graphics.bootLogo2Color = sf::Color::White;
        newPlayer.stats = PlayerStats::createFromRole(newPlayer.positionRole);

        m_db->players[newId] = newPlayer;
        m_selectedPlayerId = newId;
        m_db->savePlayer(newId, "ASSETS/DATA");
    }
    ImGui::EndChild(); // End PlayerListPane

    ImGui::SameLine();

    // 2. RIGHT PANEL (Editor Workspace)
    ImGui::BeginChild("PlayerEditorPane", ImVec2(0, availableHeight), true);

    if (!m_selectedPlayerId.empty()) {
        PlayerData* p = m_db->getPlayer(m_selectedPlayerId);
        if (p)
        {
            ImGui::Text("Editing Player: %s", p->id.c_str());

            std::string currentTeamName = "Free Agent";
            TeamData* t = nullptr;
            if (!p->teamId.empty()) {
                t = m_db->getTeam(p->teamId);
                if (t) currentTeamName = t->fullName;
            }

            if (ImGui::BeginCombo("Team", currentTeamName.c_str())) {
                auto handleTransfer = [&](const std::string& newTeamId) {
                    if (p->teamId == newTeamId) return;

                    if (!p->teamId.empty()) {
                        TeamData* oldT = m_db->getTeam(p->teamId);
                        if (oldT) {
                            auto it = std::find(oldT->rosterPlayerIds.begin(), oldT->rosterPlayerIds.end(), p->id);
                            if (it != oldT->rosterPlayerIds.end()) oldT->rosterPlayerIds.erase(it);

                            if (oldT->defaultTactics.captainId == p->id) oldT->defaultTactics.captainId = "";
                            for (auto xiIt = oldT->defaultTactics.startingXI.begin(); xiIt != oldT->defaultTactics.startingXI.end(); ) {
                                if (xiIt->second == p->id) xiIt = oldT->defaultTactics.startingXI.erase(xiIt);
                                else ++xiIt;
                            }
                        }
                    }

                    if (!newTeamId.empty()) {
                        TeamData* newT = m_db->getTeam(newTeamId);
                        if (newT) {
                            if (std::find(newT->rosterPlayerIds.begin(), newT->rosterPlayerIds.end(), p->id) == newT->rosterPlayerIds.end()) {
                                newT->rosterPlayerIds.push_back(p->id);
                            }
                        }
                    }

                    m_db->deletePlayerFile(p->id, "ASSETS/DATA", p->teamId);
                    p->teamId = newTeamId;
                    m_db->savePlayer(p->id, "ASSETS/DATA");
                    };

                if (ImGui::Selectable("Free Agent", p->teamId.empty())) handleTransfer("");
                ImGui::Separator();

                for (auto& [tId, team] : m_db->teams) {
                    if (ImGui::Selectable(team.fullName.c_str(), p->teamId == tId)) handleTransfer(tId);
                }
                ImGui::EndCombo();
            }
            ImGui::Separator();

            // --- SPLIT VISUALS AND STATS PANE ---
            ImGui::BeginChild("PlayerVisualsPane", ImVec2(0, 265.0f), false);

            ImGui::BeginChild("PlayerPreviewBox", ImVec2(265, 265), true);
            ImGui::TextDisabled("Live Engine Preview");

            // ==========================================
            // --- THE FIX 1: EXACT LAYER ORDERING ---
            // ==========================================
            std::vector<KitLayer> previewStack;

            // 2. BEARD (On top of face)
            if (!p->graphics.beardType.empty() && p->graphics.beardType != "None") {
                previewStack.push_back({ p->graphics.beardType, p->graphics.beardColor });
            }

            // 3. HAIR (On top of beard)
            if (!p->graphics.hairType.empty() && p->graphics.hairType != "None") {
                previewStack.push_back({ p->graphics.hairType, p->graphics.hairColor });
            }

            // ==========================================
            // 4. KITS & BOOTS (On top of the body/head)
            // ==========================================
            if (t) {
                previewStack.insert(previewStack.end(), t->socksLayers.begin(), t->socksLayers.end());

                // --- ADD BOOTS OVER SOCKS ---
                if (!p->graphics.bootType.empty() && p->graphics.bootType != "None") {
                    previewStack.push_back({ p->graphics.bootType, p->graphics.bootColor });
                }
                if (!p->graphics.bootLogo1Type.empty() && p->graphics.bootLogo1Type != "None") {
                    previewStack.push_back({ p->graphics.bootLogo1Type, p->graphics.bootLogo1Color });
                }
                if (!p->graphics.bootLogo2Type.empty() && p->graphics.bootLogo2Type != "None") {
                    previewStack.push_back({ p->graphics.bootLogo2Type, p->graphics.bootLogo2Color });
                }

                previewStack.insert(previewStack.end(), t->shortsLayers.begin(), t->shortsLayers.end());
                for (const auto& layer : t->shirtLayers) {
                    KitLayer shirtLayer = layer;
                    if (p->positionRole == PositionRole::Goalkeeper) {
                        shirtLayer.color = sf::Color(50, 200, 50);
                    }
                    previewStack.push_back(shirtLayer);
                }
            }
            else {
                // Free Agent Default Kits
                previewStack.push_back({ "socks_base", sf::Color::White });

                // --- ADD BOOTS OVER SOCKS ---
                if (!p->graphics.bootType.empty() && p->graphics.bootType != "None") {
                    previewStack.push_back({ p->graphics.bootType, p->graphics.bootColor });
                }
                if (!p->graphics.bootLogo1Type.empty() && p->graphics.bootLogo1Type != "None") {
                    previewStack.push_back({ p->graphics.bootLogo1Type, p->graphics.bootLogo1Color });
                }
                if (!p->graphics.bootLogo2Type.empty() && p->graphics.bootLogo2Type != "None") {
                    previewStack.push_back({ p->graphics.bootLogo2Type, p->graphics.bootLogo2Color });
                }

                previewStack.push_back({ "shorts_base", sf::Color::White });
                previewStack.push_back({ "shirt_base", (p->positionRole == PositionRole::Goalkeeper) ? sf::Color(50, 200, 50) : sf::Color::White });
            }

            // ==========================================
            // 5. GLOBAL SHADING OVERLAY
            // ==========================================
            // Drawn absolutely last. This casts shadows and highlights uniformly 
            // over the skin, face, hair, boots, and all kit elements below it!
            previewStack.push_back({ "player_shading", sf::Color::White });

            updatePreviewTexture(m_playerPreviewTexture, p->graphics.skinColor, previewStack, p->heightCm, p->weightKg);

            ImGui::SetCursorPos(ImVec2(7.5f, 25.0f));
            ImGui::Image(m_playerPreviewTexture.getTexture());
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("RadarPane", ImVec2(265.0f, 265.0f), true);

            ImGui::SetWindowFontScale(3.0f);
            std::string ovrStr = std::to_string(static_cast<int>(p->stats.overallRating));
            float textWidth = ImGui::CalcTextSize(ovrStr.c_str()).x;

            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) * 0.5f);
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.1f, 1.0f), "%s", ovrStr.c_str());
            ImGui::SetWindowFontScale(1.0f);

            const char* ovrLabel = "OVERALL";
            textWidth = ImGui::CalcTextSize(ovrLabel).x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) * 0.5f);
            ImGui::TextDisabled("%s", ovrLabel);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
            float availWidthRadar = ImGui::GetContentRegionAvail().x;

            float radius = availWidthRadar * 0.18f;
            ImVec2 center = ImVec2(cursorScreenPos.x + availWidthRadar * 0.5f, cursorScreenPos.y + radius + 25.0f);

            float nodeStats[6];
            const char* nodeLabels[6];

            if (p->positionRole == PositionRole::Goalkeeper) {
                nodeStats[0] = p->stats.getGkCoverage(); nodeStats[1] = p->stats.getGkReactions(); nodeStats[2] = p->stats.getGkCatching();
                nodeStats[3] = p->stats.getGkThrowing(); nodeStats[4] = p->stats.getGkAwareness(); nodeStats[5] = p->stats.getGkBlocking();
                nodeLabels[0] = "COV"; nodeLabels[1] = "REA"; nodeLabels[2] = "CAT";
                nodeLabels[3] = "THR"; nodeLabels[4] = "AWA"; nodeLabels[5] = "BLK";
            }
            else {
                nodeStats[0] = p->stats.getShootingAverage(); nodeStats[1] = p->stats.getPassingAverage(); nodeStats[2] = p->stats.getTechniqueAverage();
                nodeStats[3] = p->stats.getSpeedAverage(); nodeStats[4] = p->stats.getPhysicalAverage(); nodeStats[5] = p->stats.getMentalAverage();
                nodeLabels[0] = "SHOT"; nodeLabels[1] = "PASS"; nodeLabels[2] = "TECH";
                nodeLabels[3] = "PACE"; nodeLabels[4] = "PHYS"; nodeLabels[5] = "MENT";
            }

            const int numSegments = 6;
            for (int ring = 1; ring <= 5; ++ring) {
                float ringRadius = radius * (ring / 5.0f);
                ImVec2 points[6];
                for (int i = 0; i < numSegments; ++i) {
                    float angle = (i * (3.14159265f / 3.0f)) - (3.14159265f / 2.0f);
                    points[i] = ImVec2(center.x + cos(angle) * ringRadius, center.y + sin(angle) * ringRadius);
                }
                drawList->AddPolyline(points, 6, IM_COL32(100, 100, 100, 100), ImDrawFlags_Closed, 1.0f);
            }

            ImVec2 statPoints[6];
            for (int i = 0; i < numSegments; ++i) {
                float angle = (i * (3.14159265f / 3.0f)) - (3.14159265f / 2.0f);
                float statRadius = radius * (nodeStats[i] / 100.0f);
                statPoints[i] = ImVec2(center.x + cos(angle) * statRadius, center.y + sin(angle) * statRadius);
            }
            drawList->AddConvexPolyFilled(statPoints, 6, IM_COL32(0, 255, 150, 100));
            drawList->AddPolyline(statPoints, 6, IM_COL32(0, 255, 150, 255), ImDrawFlags_Closed, 2.0f);

            for (int i = 0; i < numSegments; ++i) {
                float angle = (i * (3.14159265f / 3.0f)) - (3.14159265f / 2.0f);
                float labelRadius = radius * 1.45f;
                ImVec2 labelCenter = ImVec2(center.x + cos(angle) * labelRadius, center.y + sin(angle) * labelRadius);
                char labelBuf[32]; snprintf(labelBuf, sizeof(labelBuf), "%s\n%.0f", nodeLabels[i], nodeStats[i]);
                ImVec2 textSize = ImGui::CalcTextSize(labelBuf);
                ImVec2 textDrawPos = ImVec2(labelCenter.x - textSize.x * 0.5f, labelCenter.y - textSize.y * 0.5f);
                drawList->AddText(textDrawPos, IM_COL32(255, 255, 255, 255), labelBuf);
            }
            ImGui::EndChild(); // End RadarPane

            ImGui::EndChild(); // End PlayerVisualsPane

            // --- BASIC INFO & EDITING ---
            ImGui::BeginChild("PlayerStatsPane", ImVec2(0, 0), true);

            ImGui::Spacing();
            char nameBuffer[128] = "";
            strcpy_s(nameBuffer, p->name.c_str());
            if (ImGui::InputText("Name", nameBuffer, IM_ARRAYSIZE(nameBuffer))) p->name = nameBuffer;

            std::string currentNatName = "Unknown";
            Country* c = m_db->getCountry(p->nationality);
            if (c) currentNatName = c->name;

            if (ImGui::BeginCombo("Nationality", currentNatName.c_str())) {
                for (auto& [code, country] : m_db->countries) {
                    if (ImGui::Selectable(country.name.c_str(), p->nationality == code)) {
                        p->nationality = code;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SliderInt("Squad Number", &p->squadNumber, 1, 99);
            ImGui::SliderInt("Age", &p->age, 15, 45);
            ImGui::SliderInt("Height (cm)", &p->heightCm, 150, 220);
            ImGui::SliderInt("Weight (kg)", &p->weightKg, 50, 110);

            const char* feet[] = { "Right", "Left", "Both" };
            int footIdx = (p->preferredFoot == "Left") ? 1 : (p->preferredFoot == "Both" ? 2 : 0);
            if (ImGui::Combo("Preferred Foot", &footIdx, feet, IM_ARRAYSIZE(feet))) p->preferredFoot = feet[footIdx];

            const char* roles[] = {
                "Goalkeeper", "Left Back", "Center Back", "Right Back",
                "Left Wing Back", "Right Wing Back", "Defensive Mid", "Center Mid",
                "Left Mid", "Right Mid", "Attacking Mid", "Left Wing", "Right Wing",
                "Center Forward", "Striker"
            };
            int roleIdx = static_cast<int>(p->positionRole);
            if (ImGui::Combo("Main Position", &roleIdx, roles, IM_ARRAYSIZE(roles))) {
                p->positionRole = static_cast<PositionRole>(roleIdx);
                p->positionFamiliarity[p->positionRole] = 4;
            }

            if (ImGui::TreeNode("Secondary Positions & Familiarity")) {
                ImGui::TextDisabled("1: Unable | 2: Rough | 3: Competent | 4: Mastered");
                ImGui::Spacing();

                for (int i = 0; i < IM_ARRAYSIZE(roles); ++i) {
                    PositionRole currentRoleLoop = static_cast<PositionRole>(i);

                    if (p->positionFamiliarity.find(currentRoleLoop) == p->positionFamiliarity.end()) {
                        p->positionFamiliarity[currentRoleLoop] = 1;
                    }
                    if (currentRoleLoop == p->positionRole) {
                        p->positionFamiliarity[currentRoleLoop] = 4;
                    }

                    int profLevel = p->positionFamiliarity[currentRoleLoop];
                    ImGui::PushID(i);
                    ImGui::Text("%-20s", roles[i]);
                    ImGui::SameLine(ImGui::GetWindowWidth() * 0.4f);

                    if (currentRoleLoop == p->positionRole) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
                        ImGui::Text("Main Position (Mastered)");
                        ImGui::PopStyleColor();
                    }
                    else {
                        ImGui::SetNextItemWidth(100.f);
                        if (ImGui::SliderInt("##prof", &profLevel, 1, 4)) {
                            p->positionFamiliarity[currentRoleLoop] = profLevel;
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }

            std::vector<const char*> availableNames;
            std::vector<PlaystyleType> availableTypes;

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
                availableNames = { "Wide Winger", "False Winger", "Roamer Winger", "Classic Wide Mid", "Defensive Winger", "Inverted Wide Mid" , "Joga Bonito" };
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

            int playstyleIdx = 0;
            for (size_t i = 0; i < availableTypes.size(); ++i) {
                if (p->playstyle.type == availableTypes[i]) {
                    playstyleIdx = static_cast<int>(i);
                    break;
                }
            }

            if (ImGui::Combo("Playstyle", &playstyleIdx, availableNames.data(), static_cast<int>(availableNames.size()))) {
                p->playstyle = PlaystyleDatabase::getPlaystyle(availableTypes[playstyleIdx]);
            }
            if (playstyleIdx == 0 && p->playstyle.type != availableTypes[0]) {
                p->playstyle = PlaystyleDatabase::getPlaystyle(availableTypes[0]);
            }

            ImGui::Separator();
            p->stats.calculateOverallRating(p->positionRole);

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
            ImGui::SliderFloat("Tactical Familiarity", &p->tacticalFamiliarity, 1.f, 100.f, "%.0f");

            ImGui::Separator(); ImGui::Text("Player Graphics");

            // Base Skin
            ColorEdit4SFML("Skin Color", p->graphics.skinColor);
            ImGui::Separator();

            // ==========================================
            // --- THE FIX 2: NEW UI DROPDOWNS ---
            // ==========================================

            // --- BEARD ---
            const char* beardOptions[] = { "None", "player_beard_ing", "player_goatee_ing" };
            int currentBeardIdx = 0;
            for (int i = 0; i < IM_ARRAYSIZE(beardOptions); ++i) {
                if (p->graphics.beardType == beardOptions[i]) { currentBeardIdx = i; break; }
            }
            if (ImGui::Combo("Beard Style", &currentBeardIdx, beardOptions, IM_ARRAYSIZE(beardOptions))) {
                p->graphics.beardType = beardOptions[currentBeardIdx];
            }
            ColorEdit4SFML("Beard Color", p->graphics.beardColor);

            // --- HAIR ---
            const char* hairOptions[] = { "None", "hair_bun", "hair_curlytop", "hair_dreads", "hair_flattop", "hair_short", "hair_skinfade" };
            int currentHairIdx = 0;
            for (int i = 0; i < IM_ARRAYSIZE(hairOptions); ++i) {
                if (p->graphics.hairType == hairOptions[i]) { currentHairIdx = i; break; }
            }
            if (ImGui::Combo("Hair Style", &currentHairIdx, hairOptions, IM_ARRAYSIZE(hairOptions))) {
                p->graphics.hairType = hairOptions[currentHairIdx];
            }
            ColorEdit4SFML("Hair Color", p->graphics.hairColor);

            ImGui::Separator();
            ImGui::Text("Boots & Branding");

            // --- BOOT BASE ---
            const char* bootOptions[] = { "None", "player_boots_run_ing" }; // Expand if you make different boot shapes
            int currentBootIdx = 0;
            for (int i = 0; i < IM_ARRAYSIZE(bootOptions); ++i) {
                if (p->graphics.bootType == bootOptions[i]) { currentBootIdx = i; break; }
            }
            if (ImGui::Combo("Boot Base", &currentBootIdx, bootOptions, IM_ARRAYSIZE(bootOptions))) {
                p->graphics.bootType = bootOptions[currentBootIdx];
            }
            ColorEdit4SFML("Boot Color", p->graphics.bootColor);

            ImGui::Spacing();

            // --- LOGOS ---
            const char* logoOptions[] = { "None", "boots_logo1_ing", "boots_logo2_ing" };

            int currentLogo1Idx = 0;
            for (int i = 0; i < IM_ARRAYSIZE(logoOptions); ++i) {
                if (p->graphics.bootLogo1Type == logoOptions[i]) { currentLogo1Idx = i; break; }
            }
            if (ImGui::Combo("Logo 1", &currentLogo1Idx, logoOptions, IM_ARRAYSIZE(logoOptions))) {
                p->graphics.bootLogo1Type = logoOptions[currentLogo1Idx];
            }
            ColorEdit4SFML("Logo 1 Color", p->graphics.bootLogo1Color);

            int currentLogo2Idx = 0;
            for (int i = 0; i < IM_ARRAYSIZE(logoOptions); ++i) {
                if (p->graphics.bootLogo2Type == logoOptions[i]) { currentLogo2Idx = i; break; }
            }
            if (ImGui::Combo("Logo 2", &currentLogo2Idx, logoOptions, IM_ARRAYSIZE(logoOptions))) {
                p->graphics.bootLogo2Type = logoOptions[currentLogo2Idx];
            }
            ColorEdit4SFML("Logo 2 Color", p->graphics.bootLogo2Color);

            ImGui::EndChild(); // End PlayerStatsPane
        }
    }
    else {
        ImGui::Text("Select a player to edit.");
    }

    ImGui::EndChild(); // Ends PlayerEditorPane
}

void EditorScreen::drawTeamTab(float availableHeight)
{
    // 1. LEFT PANE (Team List)
    ImGui::BeginChild("TeamListPane", ImVec2(250.0f, availableHeight), true);

    ImGui::BeginChild("TeamTree", ImVec2(0, availableHeight - 40.0f), false);
    for (auto& [code, country] : m_db->countries) {
        bool hasTeams = false;
        for (auto& [tId, team] : m_db->teams) {
            if (team.countryCode == code) { hasTeams = true; break; }
        }

        if (hasTeams) {
            if (ImGui::TreeNode(country.name.c_str())) {
                for (auto& [tId, team] : m_db->teams) {
                    if (team.countryCode == code) {
                        bool isSelected = (m_selectedTeamId == tId);
                        if (ImGui::Selectable(team.fullName.c_str(), isSelected)) m_selectedTeamId = tId;
                    }
                }
                ImGui::TreePop();
            }
        }
    }
    ImGui::EndChild(); // End TeamTree

    if (ImGui::Button("+ Create New Team", ImVec2(-FLT_MIN, 30.0f))) {
        int counter = m_db->teams.size() + 100;
        std::string newId = "TEAM_" + std::to_string(counter);
        while (m_db->teams.find(newId) != m_db->teams.end()) {
            counter++; newId = "TEAM_" + std::to_string(counter);
        }

        TeamData newTeam;
        newTeam.id = newId;
        newTeam.countryCode = "ENG";
        newTeam.fullName = "New FC";
        newTeam.shortName = "NEW";
        newTeam.badgeId = "Badge_Default";
        newTeam.teamChemistry = 100.f;
        newTeam.stadiumName = "Local Park";
        newTeam.managerName = "Coach";
        newTeam.uiColor = sf::Color(100, 100, 100);

        // Setup initial default layers
        newTeam.shirtLayers.push_back({ "shirt_base", sf::Color::White });
        newTeam.shortsLayers.push_back({ "shorts_base", sf::Color::White });
        newTeam.socksLayers.push_back({ "socks_base", sf::Color::White });

        newTeam.defaultTactics.formationName = "4-3-3";
        newTeam.defaultTactics.defensiveDepth = 50;
        newTeam.defaultTactics.passingLength = 50;
        newTeam.defaultTactics.attackingWidth = 50;

        m_db->teams[newId] = newTeam;
        m_selectedTeamId = newId;
    }
    ImGui::EndChild(); // End TeamListPane

    ImGui::SameLine();

    // 2. RIGHT PANE (Team Editor)
    ImGui::BeginChild("TeamEditorPane", ImVec2(0, availableHeight), true);

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
    ImGui::EndChild(); // End TeamEditorPane
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

    std::string currentCountryName = "Unknown";
    Country* clubCountry = m_db->getCountry(t->countryCode);
    if (clubCountry) currentCountryName = clubCountry->name;

    if (ImGui::BeginCombo("Country / League", currentCountryName.c_str())) {
        for (auto& [code, country] : m_db->countries) {
            if (ImGui::Selectable(country.name.c_str(), t->countryCode == code)) {
                t->countryCode = code;
            }
        }
        ImGui::EndCombo();
    }

    strcpy_s(stadiumBuffer, t->stadiumName.c_str());
    if (ImGui::InputText("Stadium", stadiumBuffer, IM_ARRAYSIZE(stadiumBuffer))) t->stadiumName = stadiumBuffer;

    strcpy_s(managerBuffer, t->managerName.c_str());
    if (ImGui::InputText("Manager Name", managerBuffer, IM_ARRAYSIZE(managerBuffer))) t->managerName = managerBuffer;

    strcpy_s(badgeBuffer, t->badgeId.c_str());
    if (ImGui::InputText("Main Badge ID", badgeBuffer, IM_ARRAYSIZE(badgeBuffer))) t->badgeId = badgeBuffer;

    ImGui::Spacing(); ImGui::Separator(); ImGui::Text("Team Branding");
    ColorEdit4SFML("UI Main Color", t->uiColor);

    ImGui::Spacing(); ImGui::Separator(); ImGui::Text("Team Attributes");
    ImGui::SliderFloat("Team Chemistry", &t->teamChemistry, 1.f, 100.f, "%.0f");
}

void EditorScreen::drawTeamKitsTab(TeamData* t)
{
    ImGui::BeginChild("KitControlsPane", ImVec2(350.0f, 300.0f), false);

    // THE FIX: Dynamic Array Lambda! Allows for infinite Kit Layer building.
    auto drawKitArrayUI = [](const char* title, std::vector<KitLayer>& layerArray, const char* defaultTex) {
        if (ImGui::TreeNodeEx(title, ImGuiTreeNodeFlags_DefaultOpen)) {

            for (size_t i = 0; i < layerArray.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));

                char texBuf[64];
                strcpy_s(texBuf, layerArray[i].textureId.c_str());
                if (ImGui::InputText("Texture ID", texBuf, IM_ARRAYSIZE(texBuf))) {
                    layerArray[i].textureId = texBuf;
                }

                ColorEdit4SFML("Color", layerArray[i].color);

                if (ImGui::Button("Remove Layer")) {
                    layerArray.erase(layerArray.begin() + i);
                    ImGui::PopID();
                    ImGui::TreePop();
                    return; // Early return to prevent vector out-of-bounds crash!
                }

                ImGui::Separator();
                ImGui::PopID();
            }

            if (ImGui::Button("+ Add Layer")) {
                layerArray.push_back({ defaultTex, sf::Color::White });
            }

            ImGui::TreePop();
        }
        };

    drawKitArrayUI("Shirt Layers", t->shirtLayers, "shirt_base");
    drawKitArrayUI("Shorts Layers", t->shortsLayers, "shorts_base");
    drawKitArrayUI("Socks Layers", t->socksLayers, "socks_base");

    ImGui::EndChild(); // End KitControlsPane

    ImGui::SameLine();

    ImGui::BeginChild("KitPreviewPane", ImVec2(265.0f, 300.0f), true);
    ImGui::TextDisabled("Home Kit Preview");

    // THE FIX: Compile the entire team's layer stack dynamically for the Preview!
    std::vector<KitLayer> teamStack;
    teamStack.insert(teamStack.end(), t->socksLayers.begin(), t->socksLayers.end());
    teamStack.insert(teamStack.end(), t->shortsLayers.begin(), t->shortsLayers.end());
    teamStack.insert(teamStack.end(), t->shirtLayers.begin(), t->shirtLayers.end());

    updatePreviewTexture(m_teamPreviewTexture, sf::Color(255, 224, 189), teamStack, 180.f, 75.f);

    ImGui::SetCursorPos(ImVec2(7.5f, 25.0f));
    ImGui::Image(m_teamPreviewTexture.getTexture());
    ImGui::EndChild(); // End KitPreviewPane
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
                p->teamId = "";
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

    ImGui::BeginChild("TacticsLeftPane", ImVec2(halfWidth, 0), false);

    ImGui::Text("Team Style");
    ImGui::Separator();

    const char* formations[] = { "4-3-3", "4-4-2", "4-2-3-1", "4-1-4-1", "4-1-2-1-2", "4-2-4", "3-4-3", "3-5-2", "5-3-2", "5-2-3", "5-4-1"};
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

    ImGui::SliderInt("Defensive Depth", &t->defaultTactics.defensiveDepth, 0, 100);
    ImGui::SliderInt("Passing Length", &t->defaultTactics.passingLength, 0, 100);
    ImGui::SliderInt("Attacking Width", &t->defaultTactics.attackingWidth, 0, 100);
    ImGui::SliderInt("Pressing Intensity", &t->defaultTactics.pressingIntensity, 0, 100);
    ImGui::SliderInt("Positional Freedom", &t->defaultTactics.positionalFreedom, 0, 100);
    ImGui::SliderInt("Passing Speed", &t->defaultTactics.passingSpeed, 0, 100);
    ImGui::SliderInt("Attacking Speed", &t->defaultTactics.attackingSpeed, 0, 100);

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

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("TacticsRightPane", ImVec2(0, 0), false);

    ImGui::Text("Starting XI Builder (%s)", t->defaultTactics.formationName.c_str());
    ImGui::Separator();

    auto formationLayout = getFormationLayout(t->defaultTactics.formationName);
    float paneWidth = ImGui::GetContentRegionAvail().x;

    for (size_t i = 0; i < formationLayout.size(); ++i) {
        const auto& line = formationLayout[i];

        float buttonWidth = 90.0f;
        float totalLineWidth = (buttonWidth * line.size()) + (ImGui::GetStyle().ItemSpacing.x * (line.size() - 1));
        float startPosX = (paneWidth - totalLineWidth) * 0.5f;

        ImGui::SetCursorPosX(startPosX);

        for (size_t j = 0; j < line.size(); ++j) {
            int slotId = line[j].first;
            PositionRole role = line[j].second;
            std::string roleName = roleToString(role);

            std::string currPlayerName = "Empty";
            if (t->defaultTactics.startingXI.count(slotId)) {
                PlayerData* p = m_db->getPlayer(t->defaultTactics.startingXI[slotId]);
                if (p) {
                    size_t spacePos = p->name.find_last_of(' ');
                    currPlayerName = (spacePos != std::string::npos) ? p->name.substr(spacePos + 1) : p->name;
                }
            }

            std::string buttonText = roleName + "\n" + currPlayerName;
            std::string buttonId = "##Slot_" + std::to_string(slotId);

            if (currPlayerName == "Empty") {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            }

            if (ImGui::Button((buttonText + buttonId).c_str(), ImVec2(buttonWidth, 40))) {
                if (currPlayerName != "Empty") {
                    t->defaultTactics.startingXI.erase(slotId);
                }
            }

            if (currPlayerName == "Empty") {
                ImGui::PopStyleColor();
            }
            else if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Click to remove %s from XI", currPlayerName.c_str());
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ROSTER_PLAYER")) {
                    std::string droppedId = static_cast<const char*>(payload->Data);
                    t->defaultTactics.startingXI[slotId] = droppedId;
                }
                ImGui::EndDragDropTarget();
            }

            if (j < line.size() - 1) ImGui::SameLine();
        }
        ImGui::Spacing();
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
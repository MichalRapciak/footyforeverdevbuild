#include "EditorScreen.h"
#include "imgui-1.92.6/imgui.h"
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

            const char* roles[] = { "Goalkeeper", "LeftBack", "LCenterBack", "RCenterBack", "RightBack", "DefensiveMid", "CenterMid", "AttackingMid", "LeftWing", "RightWing", "Striker" };
            int roleIdx = static_cast<int>(p->positionRole);
            if (ImGui::Combo("Position Role", &roleIdx, roles, IM_ARRAYSIZE(roles))) p->positionRole = static_cast<PositionRole>(roleIdx);

            // Stats
            ImGui::Separator(); ImGui::Text("Physical & Speed");
            ImGui::SliderFloat("Natural Fitness", &p->stats.naturalFitness, 1.f, 99.f, "%.0f");
            ImGui::SliderInt("Weak Foot Accuracy", &p->stats.weakFootAccuracy, 1, 5);
            ImGui::SliderFloat("Top Speed", &p->stats.topSpeed, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Acceleration", &p->stats.acceleration, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Agility", &p->stats.agility, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Body Strength", &p->stats.bodyStrength, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Jumping", &p->stats.jumpingStrength, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Balancing", &p->stats.balancing, 1.f, 99.f, "%.0f");

            ImGui::Separator(); ImGui::Text("Technical");
            ImGui::SliderFloat("Finishing", &p->stats.finishing, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Heading", &p->stats.heading, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Ball Control", &p->stats.ballControl, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Short Passing", &p->stats.shortPassing, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Long Passing", &p->stats.longPassing, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Kick Power", &p->stats.kickPower, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Curl", &p->stats.curl, 1.f, 99.f, "%.0f");
            ImGui::SliderFloat("Dead Ball", &p->stats.deadBall, 1.f, 99.f, "%.0f");

            ImGui::Separator(); ImGui::Text("Mental & Defending");
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
        newTeam.defaultTactics.buildUpPlay = 50;
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

    ImGui::SliderInt("Defensive Depth", &t->defaultTactics.defensiveDepth, 0, 100);
    ImGui::SliderInt("Build-up Play", &t->defaultTactics.buildUpPlay, 0, 100);
    ImGui::SliderInt("Attacking Width", &t->defaultTactics.attackingWidth, 0, 100);

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
            PositionRole role = line[j];
            std::string roleName = roleToString(role);

            // Format the display string: "Role\nPlayerName"
            std::string currPlayerName = "Empty";
            if (t->defaultTactics.startingXI.count(role)) {
                PlayerData* p = m_db->getPlayer(t->defaultTactics.startingXI[role]);
                if (p) {
                    // Extract just the last name for UI brevity if there's a space
                    size_t spacePos = p->name.find_last_of(' ');
                    currPlayerName = (spacePos != std::string::npos) ? p->name.substr(spacePos + 1) : p->name;
                }
            }

            std::string buttonText = roleName + "\n" + currPlayerName;
            std::string buttonId = "##" + roleName; // Hidden ID

            // Highlight empty slots in red so they stand out
            if (currPlayerName == "Empty") {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            }

            // --- NEW: THE CLEAR BUTTON LOGIC ---
            // If the user left-clicks this button, and it has a player in it, erase them!
            if (ImGui::Button((buttonText + buttonId).c_str(), ImVec2(buttonWidth, 40))) {
                if (currPlayerName != "Empty") {
                    t->defaultTactics.startingXI.erase(role);
                }
            }

            if (currPlayerName == "Empty") {
                ImGui::PopStyleColor();
            }
            else if (ImGui::IsItemHovered()) {
                // Add a helpful tooltip so they know clicking it clears the slot
                ImGui::SetTooltip("Click to remove %s from XI", currPlayerName.c_str());
            }

            // Drag and Drop Logic
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ROSTER_PLAYER")) {
                    std::string droppedId = static_cast<const char*>(payload->Data);
                    t->defaultTactics.startingXI[role] = droppedId;
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
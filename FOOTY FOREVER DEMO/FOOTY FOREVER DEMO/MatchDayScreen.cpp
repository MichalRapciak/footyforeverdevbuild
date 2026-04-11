#include "MatchDayScreen.h"
#include "imgui-1.92.6/imgui.h"
#include "Game.h" 

MatchDayScreen::MatchDayScreen() : m_db(nullptr), bg_s(bg_txt) {}
MatchDayScreen::~MatchDayScreen() {}


void MatchDayScreen::init(sf::Font& font, GameDatabase& database) {
    m_font = font;
    m_db = &database;
    if (!bg_txt.loadFromFile("ASSETS/IMAGES/help.png"))
    {
        std::cout << "couldn't load splash screen background\n";
    }
    bg_s.setTexture(bg_txt);
    bg_s.setTextureRect(sf::IntRect({ 0,0 }, { 1920,1080 }));
    bg_s.setPosition({ 0,0 });
    if (m_db->teams.size() >= 2) {
        auto it = m_db->teams.begin();
        m_homeTeamId = it->first;
        std::advance(it, 1);
        m_awayTeamId = it->first;
    }
}

void MatchDayScreen::update(sf::Time dt, sf::RenderWindow& window) {
    ImVec2 fullScreenSize = ImVec2(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(fullScreenSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGui::Begin("MatchDay Setup", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    ImGui::TextDisabled("MATCH SETUP");
    ImGui::Separator();
    ImGui::Spacing();

    // Calculate available space, leaving 80px at the bottom for the footer buttons
    float availableY = ImGui::GetContentRegionAvail().y - 80.0f;
    float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f;
    float rosterListHeight = ImGui::GetTextLineHeightWithSpacing() * 16.0f;

    // ==========================================
    // --- LEFT PANEL: HOME TEAM ---
    // ==========================================
    ImGui::BeginChild("HomePanel", ImVec2(halfWidth, availableY), false);

    ImGui::Text("HOME TEAM");
    float comboWidth = ImGui::GetContentRegionAvail().x - 70.0f;

    ImGui::SetNextItemWidth(comboWidth);
    if (ImGui::BeginCombo("##HomeTeamCombo", m_homeTeamId.empty() ? "Select Home Team" : m_db->getTeam(m_homeTeamId)->fullName.c_str())) {
        for (const auto& [id, team] : m_db->teams) {
            if (id == m_awayTeamId) continue;
            if (ImGui::Selectable(team.fullName.c_str(), m_homeTeamId == id)) {
                m_homeTeamId = id;
                m_userPlayerId = "";
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear##Home", ImVec2(60, 0))) {
        m_homeTeamId = "";
        m_userPlayerId = "";
    }

    if (!m_homeTeamId.empty()) {
        TeamData* t = m_db->getTeam(m_homeTeamId);
        ImVec4 teamColor(t->uiColor.r / 255.f, t->uiColor.g / 255.f, t->uiColor.b / 255.f, 1.0f);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, teamColor);
        ImGui::Text("%s", t->fullName.c_str());
        ImGui::PopStyleColor();

        ImGui::Text("Stadium: %s", t->stadiumName.c_str());
        ImGui::Text("Manager: %s", t->managerName.c_str());
        ImGui::Text("Formation: %s", t->defaultTactics.formationName.c_str());

        // --- INTERACTIVE FORMATION MAP ---
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(teamColor, "PLAYER CONTROL");
        ImGui::TextDisabled("Click a player on the map to control them");

        if (ImGui::BeginChild("HomeXIList", ImVec2(comboWidth, rosterListHeight), true)) {

            // Auto-Select Button at the top
            bool isAuto = m_userPlayerId.empty();
            if (isAuto) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.7f, 0.1f, 1.0f)); // Gold highlight
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            }
            if (ImGui::Button("Auto-Select (Any Outfield)##HomeAuto", ImVec2(comboWidth - 15.f, 30))) {
                m_userPlayerId = "";
            }
            if (isAuto) ImGui::PopStyleColor(2);
            ImGui::Separator();
            ImGui::Spacing();

            // Draw Formation
            auto formationLayout = getFormationLayout(t->defaultTactics.formationName);
            float paneWidth = ImGui::GetContentRegionAvail().x;

            for (size_t i = 0; i < formationLayout.size(); ++i) {
                const auto& line = formationLayout[i];
                float buttonWidth = 80.0f;
                float totalLineWidth = (buttonWidth * line.size()) + (ImGui::GetStyle().ItemSpacing.x * (line.size() - 1));
                float startPosX = std::max(0.f, (paneWidth - totalLineWidth) * 0.5f);

                ImGui::SetCursorPosX(startPosX);

                for (size_t j = 0; j < line.size(); ++j) {

                    // --- NEW: UNPACK THE PAIR AND USE SLOT ID ---
                    int slotId = line[j].first;
                    PositionRole role = line[j].second;
                    std::string roleName = roleToString(role);
                    std::string currPlayerName = "Empty";
                    std::string pId = "";

                    if (t->defaultTactics.startingXI.count(slotId)) {
                        pId = t->defaultTactics.startingXI[slotId];
                        PlayerData* p = m_db->getPlayer(pId);
                        if (p) {
                            size_t spacePos = p->name.find_last_of(' ');
                            currPlayerName = (spacePos != std::string::npos) ? p->name.substr(spacePos + 1) : p->name;
                        }
                    }

                    std::string buttonText = roleName + "\n" + currPlayerName;
                    std::string buttonId = "##H_Slot_" + std::to_string(slotId); // Use Slot ID for unique ImGui ID

                    bool isSelected = (m_userPlayerId == pId && !pId.empty());
                    bool isGK = (role == PositionRole::Goalkeeper);

                    // Style the buttons
                    if (isGK) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f)); // Greyed out GK
                    }
                    else if (isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.7f, 0.1f, 1.0f)); // Gold for selected
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text
                    }
                    else if (currPlayerName == "Empty") {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f)); // Red for missing player
                    }
                    else {
                        // Standard team color for unselected players
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(teamColor.x * 0.8f, teamColor.y * 0.8f, teamColor.z * 0.8f, 0.8f));
                    }

                    if (isGK) ImGui::BeginDisabled();

                    if (ImGui::Button((buttonText + buttonId).c_str(), ImVec2(buttonWidth, 40))) {
                        if (!isGK && currPlayerName != "Empty") {
                            m_userPlayerId = pId;
                        }
                    }

                    if (isGK) ImGui::EndDisabled();

                    // Pop colors
                    if (isGK || currPlayerName == "Empty") {
                        ImGui::PopStyleColor();
                    }
                    else if (isSelected) {
                        ImGui::PopStyleColor(2);
                    }
                    else {
                        ImGui::PopStyleColor();
                    }

                    if (j < line.size() - 1) ImGui::SameLine();
                }
                ImGui::Spacing();
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild(); // End Home Panel

    ImGui::SameLine();

    // ==========================================
    // --- RIGHT PANEL: AWAY TEAM ---
    // ==========================================
    ImGui::BeginChild("AwayPanel", ImVec2(0, availableY), false);

    ImGui::Text("AWAY TEAM");
    comboWidth = ImGui::GetContentRegionAvail().x - 70.0f;

    ImGui::SetNextItemWidth(comboWidth);
    if (ImGui::BeginCombo("##AwayTeamCombo", m_awayTeamId.empty() ? "Select Away Team" : m_db->getTeam(m_awayTeamId)->fullName.c_str())) {
        for (const auto& [id, team] : m_db->teams) {
            if (id == m_homeTeamId) continue;
            if (ImGui::Selectable(team.fullName.c_str(), m_awayTeamId == id)) m_awayTeamId = id;
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear##Away", ImVec2(60, 0))) {
        m_awayTeamId = "";
    }

    if (!m_awayTeamId.empty()) {
        TeamData* t = m_db->getTeam(m_awayTeamId);
        ImVec4 teamColor(t->uiColor.r / 255.f, t->uiColor.g / 255.f, t->uiColor.b / 255.f, 1.0f);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, teamColor);
        ImGui::Text("%s", t->fullName.c_str());
        ImGui::PopStyleColor();

        ImGui::Text("Stadium: %s", t->stadiumName.c_str());
        ImGui::Text("Manager: %s", t->managerName.c_str());
        ImGui::Text("Formation: %s", t->defaultTactics.formationName.c_str());

        // --- READ-ONLY FORMATION MAP ---
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(teamColor, "STARTING XI");
        ImGui::TextDisabled("AI Controlled");

        if (ImGui::BeginChild("AwayXIList", ImVec2(comboWidth, rosterListHeight), true)) {

            // Dummy Auto-Select button space to keep it perfectly aligned with the home side
            ImGui::BeginDisabled();
            ImGui::Button("AI Controlled Roster##AwayAuto", ImVec2(comboWidth - 15.f, 30));
            ImGui::EndDisabled();
            ImGui::Separator();
            ImGui::Spacing();

            // Draw Formation
            auto formationLayout = getFormationLayout(t->defaultTactics.formationName);
            float paneWidth = ImGui::GetContentRegionAvail().x;

            for (size_t i = 0; i < formationLayout.size(); ++i) {
                const auto& line = formationLayout[i];
                float buttonWidth = 80.0f;
                float totalLineWidth = (buttonWidth * line.size()) + (ImGui::GetStyle().ItemSpacing.x * (line.size() - 1));
                float startPosX = std::max(0.f, (paneWidth - totalLineWidth) * 0.5f);

                ImGui::SetCursorPosX(startPosX);

                for (size_t j = 0; j < line.size(); ++j) {

                    // --- NEW: UNPACK THE PAIR AND USE SLOT ID ---
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
                    std::string buttonId = "##A_Slot_" + std::to_string(slotId); // Use Slot ID for unique ImGui ID

                    // Style the buttons (Slightly more transparent for the Away team to show they are inactive)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(teamColor.x * 0.6f, teamColor.y * 0.6f, teamColor.z * 0.6f, 0.6f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(teamColor.x * 0.6f, teamColor.y * 0.6f, teamColor.z * 0.6f, 0.6f)); // Disable hover effect
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(teamColor.x * 0.6f, teamColor.y * 0.6f, teamColor.z * 0.6f, 0.6f));

                    ImGui::Button((buttonText + buttonId).c_str(), ImVec2(buttonWidth, 40));

                    ImGui::PopStyleColor(3);

                    if (j < line.size() - 1) ImGui::SameLine();
                }
                ImGui::Spacing();
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild(); // End Away Panel

    // ==========================================
    // FOOTER BUTTONS
    // ==========================================
    ImGui::SetCursorPosY(fullScreenSize.y - 60.0f);
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));

    bool canPlay = !m_homeTeamId.empty() && !m_awayTeamId.empty();
    if (!canPlay) ImGui::BeginDisabled();

    if (ImGui::Button("START KICK-OFF", ImVec2(200, 40))) {
        Game::currentState = GameState::GamePlay;
    }

    if (!canPlay) ImGui::EndDisabled();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));

    if (ImGui::Button("BACK TO MAIN MENU", ImVec2(200, 40))) {
        Game::currentState = GameState::MainMenu;
    }
    ImGui::PopStyleColor(3);
    ImGui::End();
}

void MatchDayScreen::render(sf::RenderWindow& window) 
{
    window.draw(bg_s);
}
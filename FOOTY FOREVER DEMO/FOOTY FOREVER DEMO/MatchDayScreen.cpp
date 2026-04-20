#include "MatchDayScreen.h"
#include "imgui-1.92.6/imgui.h"
#include "Game.h" 
#include "PlayerStats.h"

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
    // --- STATE MACHINE FOR ROSTER SWAPPING ---
    static std::string pendingHomeSwapId = "";
    static std::string pendingAwaySwapId = "";

    ImVec2 fullScreenSize = ImVec2(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(fullScreenSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGui::Begin("MatchDay Setup", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    ImGui::TextDisabled("MATCH SETUP");
    ImGui::Separator();
    ImGui::Spacing();

    // Calculate available space
    float availableY = ImGui::GetContentRegionAvail().y - 80.0f;
    float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f;

    // Split the vertical space: 55% for the Formation Map, 45% for the Bench
    float formationMapHeight = availableY * 0.55f;

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
                pendingHomeSwapId = ""; // Clear swap state on team change
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear##Home", ImVec2(60, 0))) {
        m_homeTeamId = "";
        m_userPlayerId = "";
        pendingHomeSwapId = "";
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
        ImGui::TextColored(teamColor, "STARTING XI");

        if (!pendingHomeSwapId.empty()) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "SWAP MODE: Select a slot to replace!");
        }
        else {
            ImGui::TextDisabled("Click a player to control them");
        }

        if (ImGui::BeginChild("HomeXIList", ImVec2(comboWidth, formationMapHeight), true)) {

            bool isAuto = m_userPlayerId.empty();
            if (isAuto) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.7f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            }

            if (ImGui::Button("Auto-Select (Any Player)##HomeAuto", ImVec2(comboWidth - 15.f, 35))) { // Slightly taller auto button
                m_userPlayerId = "";
            }
            if (isAuto) ImGui::PopStyleColor(2);
            ImGui::Separator();
            ImGui::Spacing();

            auto formationLayout = getFormationLayout(t->defaultTactics.formationName);
            float paneWidth = ImGui::GetContentRegionAvail().x;

            for (size_t i = 0; i < formationLayout.size(); ++i) {
                const auto& line = formationLayout[i];
                float buttonWidth = 140.0f; // LARGER BUTTON WIDTH
                float totalLineWidth = (buttonWidth * line.size()) + (ImGui::GetStyle().ItemSpacing.x * (line.size() - 1));
                float startPosX = std::max(0.f, (paneWidth - totalLineWidth) * 0.5f);

                ImGui::SetCursorPosX(startPosX);

                for (size_t j = 0; j < line.size(); ++j) {
                    int slotId = line[j].first;
                    PositionRole role = line[j].second;
                    std::string roleName = roleToString(role);
                    std::string currPlayerName = "Empty";
                    std::string pId = "";
                    int ovrRating = 0; // Track rating for the button label

                    if (t->defaultTactics.startingXI.count(slotId)) {
                        pId = t->defaultTactics.startingXI[slotId];
                        PlayerData* p = m_db->getPlayer(pId);
                        ovrRating = static_cast<int>(p->stats.overallRating);
                        if (p) {
                            p->stats.calculateOverallRating(p->positionRole);
                            size_t spacePos = p->name.find_last_of(' ');
                            currPlayerName = (spacePos != std::string::npos) ? p->name.substr(spacePos + 1) : p->name;
                            ovrRating = static_cast<int>(p->stats.overallRating);
                        }
                    }

                    // Format: "CM [84]\nSmith"
                    std::string buttonText = roleName;
                    if (ovrRating > 0) buttonText += " [" + std::to_string(ovrRating) + "]";
                    buttonText += "\n" + currPlayerName;

                    std::string buttonId = "##H_Slot_" + std::to_string(slotId);

                    bool isSelected = (m_userPlayerId == pId && !pId.empty());

                    if (isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.7f, 0.1f, 1.0f)); // Gold for controlled
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
                    }
                    else if (!pendingHomeSwapId.empty()) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 0.5f)); // Green tint for swap targets
                    }
                    else if (currPlayerName == "Empty") {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                    }
                    else {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(teamColor.x * 0.8f, teamColor.y * 0.8f, teamColor.z * 0.8f, 0.8f));
                    }

                    // LARGER BUTTON HEIGHT (55 instead of 40)
                    if (ImGui::Button((buttonText + buttonId).c_str(), ImVec2(buttonWidth, 55))) {
                        // THE FIX: SWAP LOGIC VS CONTROL LOGIC
                        if (!pendingHomeSwapId.empty()) {
                            // Inject the bench player into this XI slot
                            t->defaultTactics.startingXI[slotId] = pendingHomeSwapId;
                            pendingHomeSwapId = ""; // Clear the pending state
                            m_userPlayerId = ""; // Reset user control just to be safe
                        }
                        else if (currPlayerName != "Empty") {
                            // Standard Control Selection
                            m_userPlayerId = pId;
                        }
                    }

                    if (isSelected) ImGui::PopStyleColor(2);
                    else ImGui::PopStyleColor();

                    if (j < line.size() - 1) ImGui::SameLine();
                }
                ImGui::Spacing();
            }
        }
        ImGui::EndChild();

        // ==========================================
        // --- NEW: THE BENCH BUILDER ---
        // ==========================================
        ImGui::Spacing();
        ImGui::TextColored(teamColor, "BENCH / RESERVES");
        ImGui::TextDisabled("Select a player, then click an XI slot above to swap");

        // Fills the remaining vertical space
        if (ImGui::BeginChild("HomeBenchList", ImVec2(comboWidth, 0), true)) {

            // Build a quick list of all players currently starting
            std::vector<std::string> startingIds;
            for (const auto& [sId, pId] : t->defaultTactics.startingXI) startingIds.push_back(pId);

            for (auto& [benchPId, benchPlayer] : m_db->players) {

                if (benchPlayer.teamId != m_homeTeamId) continue;

                // If they are NOT in the starting XI vector, they are on the bench!
                if (std::find(startingIds.begin(), startingIds.end(), benchPId) == startingIds.end()) {

                    benchPlayer.stats.calculateOverallRating(benchPlayer.positionRole);
                    bool isPending = (pendingHomeSwapId == benchPId);

                    if (isPending) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f)); // Bright Green
                    }
                    else {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f)); // Dark Grey
                    }

                    int ovr = static_cast<int>(benchPlayer.stats.overallRating);
                    std::string benchBtnLabel = "[" + std::to_string(ovr) + "] " + benchPlayer.name + "##Bench_" + benchPId;

                    // LARGER BENCH BUTTONS (35 instead of 25)
                    if (ImGui::Button(benchBtnLabel.c_str(), ImVec2(comboWidth - 20.f, 35))) {
                        // Toggle swap state
                        if (isPending) pendingHomeSwapId = "";
                        else pendingHomeSwapId = benchPId;
                    }

                    ImGui::PopStyleColor();
                }
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
            if (ImGui::Selectable(team.fullName.c_str(), m_awayTeamId == id)) {
                m_awayTeamId = id;
                pendingAwaySwapId = ""; // <-- NEW: Clear swap state on team change
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear##Away", ImVec2(60, 0))) {
        m_awayTeamId = "";
        pendingAwaySwapId = ""; // <-- NEW
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

        // --- INTERACTIVE FORMATION MAP ---
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(teamColor, "STARTING XI");

        // THE FIX: Dynamic helper text for the Away Team!
        if (!pendingAwaySwapId.empty()) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "SWAP MODE: Select a slot to replace!");
        }
        else {
            ImGui::TextDisabled("Click a player to control them");
        }

        if (ImGui::BeginChild("AwayXIList", ImVec2(comboWidth, formationMapHeight), true)) {

            bool isAuto = (m_userPlayerId == "AUTO_AWAY");
            if (isAuto) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.7f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            }

            if (ImGui::Button("Auto-Select (Any Player)##AwayAuto", ImVec2(comboWidth - 15.f, 35))) {
                m_userPlayerId = "AUTO_AWAY";
            }
            if (isAuto) ImGui::PopStyleColor(2);
            ImGui::Separator();
            ImGui::Spacing();

            auto formationLayout = getFormationLayout(t->defaultTactics.formationName);
            float paneWidth = ImGui::GetContentRegionAvail().x;

            for (size_t i = 0; i < formationLayout.size(); ++i) {
                const auto& line = formationLayout[i];
                float buttonWidth = 140.0f;
                float totalLineWidth = (buttonWidth * line.size()) + (ImGui::GetStyle().ItemSpacing.x * (line.size() - 1));
                float startPosX = std::max(0.f, (paneWidth - totalLineWidth) * 0.5f);

                ImGui::SetCursorPosX(startPosX);

                for (size_t j = 0; j < line.size(); ++j) {
                    int slotId = line[j].first;
                    PositionRole role = line[j].second;
                    std::string roleName = roleToString(role);
                    std::string currPlayerName = "Empty";
                    std::string pId = "";
                    int ovrRating = 0;

                    if (t->defaultTactics.startingXI.count(slotId)) {
                        pId = t->defaultTactics.startingXI[slotId];
                        PlayerData* p = m_db->getPlayer(pId);
                        if (p) {
                            p->stats.calculateOverallRating(p->positionRole);
                            ovrRating = static_cast<int>(p->stats.overallRating);

                            size_t spacePos = p->name.find_last_of(' ');
                            currPlayerName = (spacePos != std::string::npos) ? p->name.substr(spacePos + 1) : p->name;
                        }
                    }

                    std::string buttonText = roleName;
                    if (ovrRating > 0) buttonText += " [" + std::to_string(ovrRating) + "]";
                    buttonText += "\n" + currPlayerName;

                    std::string buttonId = "##A_Slot_" + std::to_string(slotId);

                    bool isSelected = (m_userPlayerId == pId && !pId.empty());

                    // THE FIX: Color logic to handle the green swap tint
                    if (isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.7f, 0.1f, 1.0f)); // Gold for controlled
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
                    }
                    else if (!pendingAwaySwapId.empty()) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 0.5f)); // Green tint for swap targets
                    }
                    else if (currPlayerName == "Empty") {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                    }
                    else {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(teamColor.x * 0.8f, teamColor.y * 0.8f, teamColor.z * 0.8f, 0.8f));
                    }

                    if (ImGui::Button((buttonText + buttonId).c_str(), ImVec2(buttonWidth, 55))) {
                        // THE FIX: Execute the swap for the Away Team!
                        if (!pendingAwaySwapId.empty()) {
                            t->defaultTactics.startingXI[slotId] = pendingAwaySwapId;
                            pendingAwaySwapId = ""; // Clear the pending state
                            if (m_userPlayerId == pId) m_userPlayerId = ""; // Reset user control just to be safe
                        }
                        else if (currPlayerName != "Empty") {
                            m_userPlayerId = pId;
                        }
                    }

                    if (isSelected) ImGui::PopStyleColor(2);
                    else ImGui::PopStyleColor();

                    if (j < line.size() - 1) ImGui::SameLine();
                }
                ImGui::Spacing();
            }
        }
        ImGui::EndChild();

        // ==========================================
        // --- THE FIX: INTERACTIVE AWAY BENCH ---
        // ==========================================
        ImGui::Spacing();
        ImGui::TextColored(teamColor, "BENCH / RESERVES");
        ImGui::TextDisabled("Select a player, then click an XI slot above to swap");

        if (ImGui::BeginChild("AwayBenchList", ImVec2(comboWidth, 0), true)) {

            std::vector<std::string> startingIds;
            for (const auto& [sId, pId] : t->defaultTactics.startingXI) startingIds.push_back(pId);

            for (auto& [benchPId, benchPlayer] : m_db->players) {
                if (benchPlayer.teamId != m_awayTeamId) continue;

                if (std::find(startingIds.begin(), startingIds.end(), benchPId) == startingIds.end()) {
                    // ImGui::BeginDisabled(); <-- DELETED!

                    benchPlayer.stats.calculateOverallRating(benchPlayer.positionRole);
                    bool isPending = (pendingAwaySwapId == benchPId);

                    // Add color logic for selecting a bench player
                    if (isPending) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f)); // Bright Green
                    }
                    else {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f)); // Dark Grey
                    }

                    int ovr = static_cast<int>(benchPlayer.stats.overallRating);
                    std::string benchBtnLabel = "[" + std::to_string(ovr) + "] " + benchPlayer.name + "##Bench_" + benchPId;

                    if (ImGui::Button(benchBtnLabel.c_str(), ImVec2(comboWidth - 20.f, 35))) {
                        // Toggle swap state
                        if (isPending) pendingAwaySwapId = "";
                        else pendingAwaySwapId = benchPId;
                    }

                    ImGui::PopStyleColor();
                    // ImGui::EndDisabled(); <-- DELETED!
                }
            }
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    // ==========================================
    // FOOTER BUTTONS
    // ==========================================
    ImGui::SetCursorPosY(fullScreenSize.y - 60.0f);
    ImGui::Separator();
    ImGui::Spacing();

    bool canPlay = !m_homeTeamId.empty() && !m_awayTeamId.empty();
    if (!canPlay) ImGui::BeginDisabled();

    // 1. STANDARD KICK-OFF BUTTON
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
    if (ImGui::Button("START KICK-OFF", ImVec2(200, 40))) {
        Game::currentState = GameState::MatchIntro;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();

    // 2. SPECTATOR BUTTON
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.7f, 1.0f));
    if (ImGui::Button("SPECTATE AI MATCH", ImVec2(200, 40))) {
        m_userPlayerId = "SPECTATOR";
        Game::currentState = GameState::MatchIntro;
    }
    ImGui::PopStyleColor(3);

    if (!canPlay) ImGui::EndDisabled();

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
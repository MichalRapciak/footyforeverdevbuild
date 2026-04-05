#include "EditorScreen.h"
#include "imgui-1.92.6/imgui.h"
#include "Game.h"
#include <cstdint>

inline PositionRole stringToRole(const std::string& str)
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

inline std::string roleToString(PositionRole role)
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

bool ColorEdit4SFML(const char* label, sf::Color& color)
{
    // Convert 0-255 to 0.0f-1.0f
    float col[4] = {color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f};

    // Draw the ImGui Color Picker
    if (ImGui::ColorEdit4(label, col))
    {
        // If the user changed the color, convert back to 0-255 and save it
        color.r = static_cast<std::uint8_t>(col[0] * 255.f);
        color.g = static_cast<std::uint8_t>(col[1] * 255.f);
        color.b = static_cast<std::uint8_t>(col[2] * 255.f);
        color.a = static_cast<std::uint8_t>(col[3] * 255.f);
        return true;
    }
    return false;
}

EditorScreen::EditorScreen() : m_db(nullptr), m_selectedPlayerId(""){}
EditorScreen::~EditorScreen() {}

void EditorScreen::init(sf::Font& font, GameDatabase& database)
{
    m_font = font;
    m_db = &database;
}

void EditorScreen::update(sf::Time dt, sf::RenderWindow& window)
{
    // Grab the actual size of your SFML game window
    ImVec2 fullScreenSize = ImVec2(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));

    // Anchor the ImGui window to the top-left corner
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);

    // Force the ImGui window to fill the exact size of the game window
    ImGui::SetNextWindowSize(fullScreenSize, ImGuiCond_Always);

    // Add ImGuiWindowFlags_NoCollapse and ImGuiWindowFlags_NoMove so it acts like a solid menu screen
    ImGui::Begin("Database Editor", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    // We calculate the available height minus ~50 pixels to leave room for the Save button at the bottom
    float availableHeight = ImGui::GetContentRegionAvail().y - 50.0f;

    if (ImGui::BeginTabBar("EditorTabs"))
    {
        // ==========================================
        // TAB 1: PLAYERS
        // ==========================================
        if (ImGui::BeginTabItem("Players"))
        {
            ImGui::Columns(2, "PlayerColumns", true);
            ImGui::SetColumnWidth(0, 200.0f);

            // --- LEFT COLUMN: Player List ---
            ImGui::BeginChild("PlayerList", ImVec2(0, availableHeight - 40.0f), true);
            for (auto& [id, player] : m_db->players)
            {
                bool isSelected = (m_selectedPlayerId == id);
                if (ImGui::Selectable(player.name.c_str(), isSelected))
                {
                    m_selectedPlayerId = id;
                }
            }
            ImGui::EndChild();

            // --- THE "CREATE PLAYER" BUTTON ---
            if (ImGui::Button("+ Create New Player", ImVec2(-FLT_MIN, 30.0f)))
            {
                int counter = m_db->players.size() + 1000;
                std::string newId = "PLY_" + std::to_string(counter);
                while (m_db->players.find(newId) != m_db->players.end()) {
                    counter++;
                    newId = "PLY_" + std::to_string(counter);
                }

                PlayerData newPlayer;
                newPlayer.id = newId;
                newPlayer.name = "New Player";
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

                // Auto-generate starting stats
                newPlayer.stats = PlayerStats::createFromRole(newPlayer.positionRole);

                m_db->players[newId] = newPlayer;
                m_selectedPlayerId = newId;
            }

            ImGui::NextColumn();

            // --- RIGHT COLUMN: Player Editor ---
            if (!m_selectedPlayerId.empty())
            {
                PlayerData* p = m_db->getPlayer(m_selectedPlayerId);
                if (p)
                {
                    ImGui::Text("Editing Player: %s", p->id.c_str());
                    ImGui::Separator();

                    // --- BASIC INFO ---
                    char nameBuffer[128] = "";
                    strcpy_s(nameBuffer, p->name.c_str());
                    if (ImGui::InputText("Name", nameBuffer, IM_ARRAYSIZE(nameBuffer))) p->name = nameBuffer;

                    ImGui::SliderInt("Squad Number", &p->squadNumber, 1, 99);
                    ImGui::SliderInt("Age", &p->age, 15, 45);
                    ImGui::SliderInt("Height (cm)", &p->heightCm, 150, 220);
                    ImGui::SliderInt("Weight (kg)", &p->weightKg, 50, 110);

                    // COMBO BOX: Preferred Foot
                    const char* feet[] = { "Right", "Left", "Both" };
                    int footIdx = (p->preferredFoot == "Left") ? 1 : (p->preferredFoot == "Both" ? 2 : 0);
                    if (ImGui::Combo("Preferred Foot", &footIdx, feet, IM_ARRAYSIZE(feet))) {
                        const char* selectedFoot = feet[footIdx];
                        p->preferredFoot = selectedFoot;
                    }

                    // COMBO BOX: Position Role (Casts your Enum to an int and back!)
                    const char* roles[] = { "Goalkeeper", "LeftBack", "LCenterBack", "RCenterBack", "RightBack", "DefensiveMid", "CenterMid", "AttackingMid", "LeftWing", "RightWing", "Striker" };
                    int roleIdx = static_cast<int>(p->positionRole);
                    if (ImGui::Combo("Position Role", &roleIdx, roles, IM_ARRAYSIZE(roles))) {
                        p->positionRole = static_cast<PositionRole>(roleIdx);
                    }

                    // --- STATS ---
                    ImGui::Separator();
                    ImGui::Text("Physical & Speed");
                    ImGui::SliderFloat("Top Speed", &p->stats.topSpeed, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Acceleration", &p->stats.acceleration, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Agility", &p->stats.agility, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Body Strength", &p->stats.bodyStrength, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Jumping", &p->stats.jumpingStrength, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Balancing", &p->stats.balancing, 1.f, 99.f, "%.0f");

                    ImGui::Separator();
                    ImGui::Text("Technical");
                    ImGui::SliderFloat("Finishing", &p->stats.finishing, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Heading", &p->stats.heading, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Ball Control", &p->stats.ballControl, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Short Passing", &p->stats.shortPassing, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Long Passing", &p->stats.longPassing, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Kick Power", &p->stats.kickPower, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Curl", &p->stats.curl, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Dead Ball", &p->stats.deadBall, 1.f, 99.f, "%.0f");

                    ImGui::Separator();
                    ImGui::Text("Mental & Defending");
                    ImGui::SliderFloat("Awareness", &p->stats.awareness, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Aggression", &p->stats.aggression, 1.f, 99.f, "%.0f");
                    ImGui::SliderFloat("Blocking", &p->stats.blocking, 1.f, 99.f, "%.0f");
                    if (p->positionRole == PositionRole::Goalkeeper)
                    {
                        ImGui::Separator();
                        ImGui::Text("Goalkeeping");

                        // You can optionally wrap this in an 'if' statement so these only show up 
                        // if the player's role is set to Goalkeeper!
                        if (p->positionRole == PositionRole::Goalkeeper)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f)); // Gold text to stand out
                            ImGui::Text("(Goalkeeper Specific Stats)");
                            ImGui::PopStyleColor();
                        }

                        ImGui::SliderFloat("GK Coverage", &p->stats.gkCoverage, 1.f, 99.f, "%.0f");
                        ImGui::SliderFloat("GK Reactions", &p->stats.gkReactions, 1.f, 99.f, "%.0f");
                        ImGui::SliderFloat("GK Catching", &p->stats.gkCatching, 1.f, 99.f, "%.0f");
                        ImGui::SliderFloat("GK Throwing", &p->stats.gkThrowing, 1.f, 99.f, "%.0f");
                        ImGui::SliderFloat("GK Awareness", &p->stats.gkAwareness, 1.f, 99.f, "%.0f");
                        ImGui::SliderFloat("GK Blocking", &p->stats.gkBlocking, 1.f, 99.f, "%.0f");
                    }
                    // --- GRAPHICS ---
                    ImGui::Separator();
                    ImGui::Text("Player Graphics");

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
            }
            else
            {
                ImGui::Text("Select a player to edit.");
            }
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }

        // ==========================================
        // TAB 2: TEAMS
        // ==========================================
        if (ImGui::BeginTabItem("Teams"))
        {
            ImGui::Columns(2, "TeamColumns", true);
            ImGui::SetColumnWidth(0, 200.0f);

            // --- LEFT COLUMN: Team List ---
            // Shrink the list to leave 40px for the Create button
            ImGui::BeginChild("TeamList", ImVec2(0, availableHeight - 40.0f), true);
            for (auto& [id, team] : m_db->teams)
            {
                bool isSelected = (m_selectedTeamId == id);
                if (ImGui::Selectable(team.fullName.c_str(), isSelected))
                {
                    m_selectedTeamId = id;
                }
            }
            ImGui::EndChild();

            // --- THE "CREATE TEAM" BUTTON ---
            if (ImGui::Button("+ Create New Team", ImVec2(-FLT_MIN, 30.0f)))
            {
                // 1. Generate a unique ID (e.g., TEAM_101)
                int         counter = m_db->teams.size() + 100;
                std::string newId   = "TEAM_" + std::to_string(counter);
                while (m_db->teams.find(newId) != m_db->teams.end())
                {
                    counter++;
                    newId = "TEAM_" + std::to_string(counter);
                }

                // 2. Create the default TeamData template
                TeamData newTeam;
                newTeam.id               = newId;
                newTeam.fullName         = "New FC";
                newTeam.shortName        = "NEW";
                newTeam.badgeId          = "Badge_Default";
                newTeam.stadiumName      = "Local Park";
                newTeam.managerName      = "Coach";
                newTeam.defaultFormation = "4-4-2";
                newTeam.uiColor          = sf::Color(100, 100, 100); // Generic grey UI

                // Give them a basic all-white kit to start
                newTeam.shirt.primaryColor  = sf::Color::White;
                newTeam.shorts.primaryColor = sf::Color::White;
                newTeam.socks.primaryColor  = sf::Color::White;

                // 3. Add to Database and Auto-Select!
                m_db->teams[newId] = newTeam;
                m_selectedTeamId   = newId;
            }

            ImGui::NextColumn();

            // --- RIGHT COLUMN: Team Editor ---
            // --- RIGHT COLUMN: Team Editor ---
            if (!m_selectedTeamId.empty())
            {
                TeamData* t = m_db->getTeam(m_selectedTeamId);
                if (t)
                {
                    ImGui::Text("Editing Team: %s", t->id.c_str());
                    ImGui::Separator();

                    // --- BASIC INFO ---
                    char nameBuffer[128] = "", shortNameBuffer[16] = "", stadiumBuffer[128] = "", formationBuffer[32] = "", managerBuffer[128] = "", badgeBuffer[128] = "";

                    strcpy_s(nameBuffer, t->fullName.c_str());
                    if (ImGui::InputText("Full Name", nameBuffer, IM_ARRAYSIZE(nameBuffer))) t->fullName = nameBuffer;

                    strcpy_s(shortNameBuffer, t->shortName.c_str());
                    if (ImGui::InputText("Short Name", shortNameBuffer, IM_ARRAYSIZE(shortNameBuffer))) t->shortName = shortNameBuffer;

                    strcpy_s(stadiumBuffer, t->stadiumName.c_str());
                    if (ImGui::InputText("Stadium", stadiumBuffer, IM_ARRAYSIZE(stadiumBuffer))) t->stadiumName = stadiumBuffer;

                    strcpy_s(managerBuffer, t->managerName.c_str());
                    if (ImGui::InputText("Manager Name", managerBuffer, IM_ARRAYSIZE(managerBuffer))) t->managerName = managerBuffer;

                    strcpy_s(formationBuffer, t->defaultFormation.c_str());
                    if (ImGui::InputText("Default Formation", formationBuffer, IM_ARRAYSIZE(formationBuffer))) t->defaultFormation = formationBuffer;

                    strcpy_s(badgeBuffer, t->badgeId.c_str());
                    if (ImGui::InputText("Main Badge ID", badgeBuffer, IM_ARRAYSIZE(badgeBuffer))) t->badgeId = badgeBuffer;

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Text("Team Branding");
                    ColorEdit4SFML("UI Main Color", t->uiColor);

                    // --- KITS ---
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Text("Kit Customization");

                    // Helper buffers for the Kit strings
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

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Text("Roster Management (Drag & Drop)");

                    // ... [Keep your existing Drag & Drop Roster code here exactly as it is!] ...

                    // We split the remaining space into two side-by-side lists
                    float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f;

                    // ==========================================
                    // BOX 1: CURRENT ROSTER (The Drop Target)
                    // ==========================================
                    ImGui::BeginChild("TeamRoster", ImVec2(halfWidth, 200.0f), true);
                    ImGui::Text("Current Squad");
                    ImGui::Separator();

                    // We use an iterator so we can safely erase players if we click the [X]
                    for (auto it = t->rosterPlayerIds.begin(); it != t->rosterPlayerIds.end();)
                    {
                        std::string pId = *it;
                        PlayerData* p   = m_db->getPlayer(pId);
                        if (p)
                        {
                            // Draw an [X] button to remove the player from the team
                            if (ImGui::Button(("X##" + pId).c_str()))
                            {
                                it = t->rosterPlayerIds.erase(it);
                                // If they were the captain, clear the captain status
                                if (t->captainPlayerId == pId)
                                    t->captainPlayerId = "";
                                continue;
                            }

                            ImGui::SameLine();

                            // Highlight the captain in Gold!
                            if (t->captainPlayerId == pId)
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.84f, 0.0f, 1.0f));
                            ImGui::Selectable((std::to_string(p->squadNumber) + ". " + p->name).c_str());
                            if (t->captainPlayerId == pId)
                                ImGui::PopStyleColor();

                            // RIGHT CLICK to set as Captain
                            if (ImGui::BeginPopupContextItem(("RosterContext##" + pId).c_str()))
                            {
                                if (ImGui::Selectable("Set as Captain"))
                                    t->captainPlayerId = pId;
                                ImGui::EndPopup();
                            }
                        }
                        ++it;
                    }
                    ImGui::EndChild();

                    // --- THE DROP TARGET LOGIC ---
                    if (ImGui::BeginDragDropTarget())
                    {
                        // Check if the payload matches our secret keyword "PLAYER_ID"
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PLAYER_ID"))
                        {
                            // Unpack the string!
                            std::string droppedId = static_cast<const char*>(payload->Data);

                            // Make sure they aren't already on the team before adding them
                            if (std::find(t->rosterPlayerIds.begin(), t->rosterPlayerIds.end(), droppedId) ==
                                t->rosterPlayerIds.end())
                            {
                                t->rosterPlayerIds.push_back(droppedId);
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::SameLine(); // Puts the next box right next to the previous one

                    // ==========================================
                    // BOX 2: AVAILABLE PLAYERS (The Drag Source)
                    // ==========================================
                    ImGui::BeginChild("AvailablePlayers", ImVec2(0, 200.0f), true);
                    ImGui::Text("Available Players");
                    ImGui::Separator();

                    for (auto& [id, player] : m_db->players)
                    {
                        // Only show players who are NOT already on this team
                        if (std::find(t->rosterPlayerIds.begin(), t->rosterPlayerIds.end(), id) == t->rosterPlayerIds.end())
                        {
                            ImGui::Selectable(player.name.c_str());

                            // --- THE DRAG SOURCE LOGIC ---
                            // If the user clicks and drags this Selectable...
                            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                            {
                                // Pack the ID string into the payload! (+1 to include the null terminator)
                                ImGui::SetDragDropPayload("PLAYER_ID", id.c_str(), id.size() + 1);

                                // Draw a tooltip attached to the mouse while dragging
                                ImGui::Text("Assign %s to %s", player.name.c_str(), t->shortName.c_str());

                                ImGui::EndDragDropSource();
                            }
                        }
                    }
                    ImGui::EndChild();
                }
            }
            else
            {
                ImGui::Text("Select a team to edit.");
            }
            ImGui::Columns(1);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // ==========================================
        // GLOBAL FOOTER: Save & Exit Buttons
        // ==========================================
    ImGui::Separator();
    ImGui::Spacing();

    // --- 1. THE GREEN SAVE BUTTON ---
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));

    if (ImGui::Button("Save All Changes to JSON", ImVec2(200, 30)))
    {
        m_db->savePlayersToFile("ASSETS/DATA/players.json");
        m_db->saveTeamsToFile("ASSETS/DATA/teams.json");
    }
    ImGui::PopStyleColor(3);


    ImGui::SameLine(); // Puts the next button on the exact same horizontal line!


    // --- 2. THE RED RETURN BUTTON ---
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));

    if (ImGui::Button("Return to Main Menu", ImVec2(200, 30)))
    {
        // Tell the master game loop to swap out of the Editor state!
        Game::currentState = GameState::MainMenu;
    }
    ImGui::PopStyleColor(3);

    ImGui::End(); // End of the ImGui Window
}

void EditorScreen::render(sf::RenderWindow& window)
{
    // You can clear to a nice editor background color here
    // No need to draw ImGui here, Game.cpp handles ImGui::SFML::Render!
}

void EditorScreen::processEvents(sf::Event& event)
{
    // We don't need to write anything here yet! 
    // ImGui handles its own events in Game.cpp.
}
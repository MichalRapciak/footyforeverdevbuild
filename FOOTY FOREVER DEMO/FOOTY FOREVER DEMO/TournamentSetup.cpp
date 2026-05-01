#include "TournamentSetup.h"
#include "imgui-1.92.6/imgui.h"
#include "imgui-1.92.6/imgui-sfml.h"
#include "Game.h"
#include "GameDatabase.h"
#include <iostream>
#include <random>
#include <algorithm>

TournamentSetup::TournamentSetup() : m_db(nullptr), bg_s(bg_txt) {}

TournamentSetup::~TournamentSetup() {}

void TournamentSetup::init(sf::Font& font, GameDatabase& database) {
	m_font = font;
	m_db = &database;

	if (!bg_txt.loadFromFile("ASSETS/IMAGES/help.png")) {
		std::cout << "couldn't load tournament setup background\n";
	}
	bg_s.setTexture(bg_txt);
	bg_s.setTextureRect(sf::IntRect({ 0,0 }, { 1920,1080 }));
	bg_s.setPosition({ 0,0 });

	// Default to the first team in the database if available
	if (!m_db->teams.empty()) {
		m_userTeamId = m_db->teams.begin()->first;
	}
	m_tournamentSize = 4;
}

void TournamentSetup::update(sf::Time dt, sf::RenderWindow& window) {
	ImVec2 fullScreenSize = ImVec2(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::SetNextWindowSize(fullScreenSize, ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.65f); // Match the MatchDayScreen opacity

	ImGui::Begin("Tournament Setup", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

	ImGui::TextDisabled("CUP TOURNAMENT SETUP");
	ImGui::Separator();
	ImGui::Spacing();

	float availableY = ImGui::GetContentRegionAvail().y - 80.0f;
	float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f;

	// ==========================================
	// --- LEFT PANEL: CONFIGURATION ---
	// ==========================================
	ImGui::BeginChild("ConfigPanel", ImVec2(halfWidth, availableY), false);
	ImGui::Text("TOURNAMENT SETTINGS");
	ImGui::Spacing();
	ImGui::Spacing();

	// 1. Format Selection
	ImGui::Text("Select Tournament Size:");
	if (ImGui::RadioButton("4 Teams (Semi-Finals)", m_tournamentSize == 4)) { m_tournamentSize = 4; }
	ImGui::SameLine();
	if (ImGui::RadioButton("8 Teams (Quarter-Finals)", m_tournamentSize == 8)) { m_tournamentSize = 8; }

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 2. User Team Selection
	ImGui::Text("Select Your Team:");
	float comboWidth = halfWidth - 50.0f;
	ImGui::SetNextItemWidth(comboWidth);

	if (ImGui::BeginCombo("##UserTeamCombo", m_userTeamId.empty() ? "Select Team" : m_db->getTeam(m_userTeamId)->fullName.c_str())) {
		for (const auto& [id, team] : m_db->teams) {
			if (ImGui::Selectable(team.fullName.c_str(), m_userTeamId == id)) {
				m_userTeamId = id;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::EndChild(); // End Config Panel

	ImGui::SameLine();

	// ==========================================
	// --- RIGHT PANEL: TEAM PREVIEW ---
	// ==========================================
	ImGui::BeginChild("PreviewPanel", ImVec2(0, availableY), true);

	if (!m_userTeamId.empty()) {
		TeamData* t = m_db->getTeam(m_userTeamId);
		ImVec4 teamColor(t->uiColor.r / 255.f, t->uiColor.g / 255.f, t->uiColor.b / 255.f, 1.0f);

		ImGui::TextDisabled("TEAM PREVIEW");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::PushStyleColor(ImGuiCol_Text, teamColor);
		ImGui::SetWindowFontScale(1.5f);
		ImGui::Text("%s", t->fullName.c_str());
		ImGui::SetWindowFontScale(1.0f);
		ImGui::PopStyleColor();

		ImGui::Spacing();
		ImGui::Text("Stadium: %s", t->stadiumName.c_str());
		ImGui::Text("Manager: %s", t->managerName.c_str());
		ImGui::Text("Base Formation: %s", t->defaultTactics.formationName.c_str());

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// Quick preview of their top players (Optional, but looks cool)
		ImGui::Text("Key Players:");

		// 1. Gather all starting players and calculate their ratings
		std::vector<PlayerData*> startingPlayers;
		for (const auto& [slotId, pId] : t->defaultTactics.startingXI) {
			PlayerData* p = m_db->getPlayer(pId);
			if (p) {
				p->stats.calculateOverallRating(p->positionRole);
				startingPlayers.push_back(p);
			}
		}

		// 2. Sort the players by overall rating (highest to lowest)
		std::sort(startingPlayers.begin(), startingPlayers.end(), [](PlayerData* a, PlayerData* b) {
			return a->stats.overallRating > b->stats.overallRating;
			});

		// 3. Display only the top 5
		int displayCount = 0;
		for (PlayerData* p : startingPlayers) {
			if (displayCount >= 5) break;

			ImGui::BulletText("[%d] %s - %s",
				static_cast<int>(p->stats.overallRating),
				roleToString(p->positionRole).c_str(),
				p->name.c_str());

			displayCount++;
		}
	}
	else {
		ImGui::TextDisabled("Select a team to view details.");
	}

	ImGui::EndChild(); // End Preview Panel

	// ==========================================
	// --- FOOTER BUTTONS ---
	// ==========================================
	ImGui::SetCursorPosY(fullScreenSize.y - 60.0f);
	ImGui::Separator();
	ImGui::Spacing();

	bool canPlay = !m_userTeamId.empty() && (m_db->teams.size() >= m_tournamentSize);

	if (!canPlay) ImGui::BeginDisabled();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));

	if (ImGui::Button("GENERATE TOURNAMENT", ImVec2(250, 40))) {

		// 1. Gather all AI teams available
		std::vector<std::string> availableAITeams;
		for (const auto& [id, team] : m_db->teams) {
			if (id != m_userTeamId) {
				availableAITeams.push_back(id);
			}
		}

		// 2. Shuffle the AI teams so we get a random draw
		std::random_device rd;
		std::mt19937 g(rd());
		std::shuffle(availableAITeams.begin(), availableAITeams.end(), g);

		// 3. Fill the bracket
		m_tournamentBracket.clear();
		m_tournamentBracket.push_back(m_userTeamId); // Add the user

		// Add enough AI teams to reach the selected size
		for (int i = 0; i < m_tournamentSize - 1; ++i) {
			m_tournamentBracket.push_back(availableAITeams[i]);
		}

		// 4. Shuffle the final bracket so the user isn't always Team 1 vs Team 2
		std::shuffle(m_tournamentBracket.begin(), m_tournamentBracket.end(), g);

		std::cout << "Bracket Generated! Moving to Hub...\n";
		Game::currentState = GameState::TournamentHub; // Transition to your new screen!
	}
	ImGui::PopStyleColor(3);

	if (!canPlay) ImGui::EndDisabled();

	if (m_db->teams.size() < m_tournamentSize) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "Error: Not enough teams in database to run a %d-team tournament.", m_tournamentSize);
	}

	ImGui::SameLine();
	ImGui::SetCursorPosX(fullScreenSize.x - 220.0f); // Right align the back button

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));

	if (ImGui::Button("BACK TO MODE SELECT", ImVec2(200, 40))) {
		Game::currentState = GameState::GamemodeSelect;
	}
	ImGui::PopStyleColor(3);

	ImGui::End();
}

void TournamentSetup::render(sf::RenderWindow& window) {
	window.draw(bg_s);
}
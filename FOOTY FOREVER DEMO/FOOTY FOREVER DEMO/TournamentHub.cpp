#include "TournamentHub.h"
#include "imgui-1.92.6/imgui.h"
#include "imgui-1.92.6/imgui-sfml.h"
#include "Game.h"
#include "GameDatabase.h"
#include <iostream>

TournamentHub::TournamentHub() : m_db(nullptr), bg_s(bg_txt) {}

TournamentHub::~TournamentHub() {}

void TournamentHub::init(sf::Font& font, GameDatabase& database, const std::vector<std::string>& bracket, const std::string& userTeamId) {
	m_db = &database;
	m_userTeamId = userTeamId;
	m_tournamentSize = bracket.size();

	if (!bg_txt.loadFromFile("ASSETS/IMAGES/mainmenu.png")) {
		std::cout << "couldn't load tournament hub background\n";
	}
	bg_s.setTexture(bg_txt);
	bg_s.setTextureRect(sf::IntRect({ 0,0 }, { 1920,1080 }));
	bg_s.setPosition({ 0,0 });

	// Clear previous data
	m_quarterFinals.clear();
	m_semiFinals.clear();
	m_final.clear();
	m_nextOpponentId = "";
	m_currentHomeId = "";
	m_currentAwayId = "";

	// Populate Initial Round
	if (m_tournamentSize == 8) {
		for (size_t i = 0; i < 8; i += 2) {
			m_quarterFinals.push_back({ bracket[i], bracket[i + 1] });
		}
		m_semiFinals.resize(2);
		m_final.resize(1);
	}
	else if (m_tournamentSize == 4) {
		for (size_t i = 0; i < 4; i += 2) {
			m_semiFinals.push_back({ bracket[i], bracket[i + 1] });
		}
		m_final.resize(1);
	}

	// ==========================================
	// --- THE FIX: FIND THE FIXTURE SIDES ---
	// ==========================================
	const auto& currentRound = (m_tournamentSize == 8) ? m_quarterFinals : m_semiFinals;
	for (const auto& match : currentRound) {
		if (match.team1Id == m_userTeamId || match.team2Id == m_userTeamId) {
			// Lock in exactly who is Home (1) and who is Away (2) for this matchup
			m_currentHomeId = match.team1Id;
			m_currentAwayId = match.team2Id;

			// Figure out which one of them is the opponent
			m_nextOpponentId = (match.team1Id == m_userTeamId) ? match.team2Id : match.team1Id;
		}
	}
}

void TournamentHub::update(sf::Time dt, sf::RenderWindow& window) {
	ImVec2 fullScreenSize = ImVec2(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::SetNextWindowSize(fullScreenSize, ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.75f);

	ImGui::Begin("Tournament Hub", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

	TeamData* userTeam = m_db->getTeam(m_userTeamId);
	TeamData* nextOpp = m_nextOpponentId.empty() ? nullptr : m_db->getTeam(m_nextOpponentId);

	// ==========================================
	// --- TOP HEADER: NEXT FIXTURE ---
	// ==========================================
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 0.8f));
	ImGui::BeginChild("HeaderPanel", ImVec2(0, 120), true);

	ImGui::SetCursorPosY(20.0f);
	ImGui::SetWindowFontScale(2.0f);

	if (nextOpp) {
		// Use the correct Home vs Away order for the header text
		TeamData* homeTeamData = m_db->getTeam(m_currentHomeId);
		TeamData* awayTeamData = m_db->getTeam(m_currentAwayId);

		std::string fixtureText = "NEXT MATCH: " + homeTeamData->fullName + " vs " + awayTeamData->fullName;

		float textWidth = ImGui::CalcTextSize(fixtureText.c_str()).x;
		ImGui::SetCursorPosX((fullScreenSize.x - textWidth) / 2.0f);
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", fixtureText.c_str());
	}
	else {
		std::string statusText = "TOURNAMENT COMPLETE";
		float textWidth = ImGui::CalcTextSize(statusText.c_str()).x;
		ImGui::SetCursorPosX((fullScreenSize.x - textWidth) / 2.0f);
		ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", statusText.c_str());
	}

	ImGui::SetWindowFontScale(1.0f);
	ImGui::EndChild();
	ImGui::PopStyleColor();

	ImGui::Spacing();

	float availableY = ImGui::GetContentRegionAvail().y - 80.0f;
	float bracketWidth = fullScreenSize.x * 0.70f;

	// ==========================================
	// --- LEFT PANEL: THE BRACKET VISUALIZER ---
	// ==========================================
	ImGui::BeginChild("BracketPanel", ImVec2(bracketWidth, availableY), true);
	ImGui::TextDisabled("ROAD TO THE FINAL");
	ImGui::Separator();

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 winPos = ImGui::GetCursorScreenPos();

	float nodeWidth = 200.f;
	float nodeHeight = 40.f;
	float xSpacing = 250.f;

	// Draw Quarter Finals
	if (m_tournamentSize == 8) {
		float startY = 50.f;
		float ySpacing = 120.f;

		for (size_t i = 0; i < 4; ++i) {
			ImVec2 pos1(winPos.x + 50.f, winPos.y + startY + (i * ySpacing));
			ImVec2 pos2(pos1.x, pos1.y + nodeHeight + 10.f);

			drawBracketNode(m_quarterFinals[i].team1Id, pos1, ImVec2(nodeWidth, nodeHeight), drawList);
			drawBracketNode(m_quarterFinals[i].team2Id, pos2, ImVec2(nodeWidth, nodeHeight), drawList);
		}
	}

	// Draw Semi Finals
	float sfStartX = (m_tournamentSize == 8) ? winPos.x + 50.f + xSpacing : winPos.x + 50.f;
	float sfStartY = (m_tournamentSize == 8) ? 100.f : 150.f;
	float sfSpacing = (m_tournamentSize == 8) ? 240.f : 150.f;

	for (size_t i = 0; i < 2; ++i) {
		ImVec2 pos1(sfStartX, winPos.y + sfStartY + (i * sfSpacing));
		ImVec2 pos2(pos1.x, pos1.y + nodeHeight + 10.f);

		drawBracketNode(m_semiFinals[i].team1Id, pos1, ImVec2(nodeWidth, nodeHeight), drawList);
		drawBracketNode(m_semiFinals[i].team2Id, pos2, ImVec2(nodeWidth, nodeHeight), drawList);
	}

	// Draw Final
	float finStartX = sfStartX + xSpacing;
	float finStartY = (m_tournamentSize == 8) ? 220.f : 225.f;

	ImVec2 finPos1(finStartX, winPos.y + finStartY);
	ImVec2 finPos2(finPos1.x, finPos1.y + nodeHeight + 10.f);

	drawBracketNode(m_final[0].team1Id, finPos1, ImVec2(nodeWidth, nodeHeight), drawList);
	drawBracketNode(m_final[0].team2Id, finPos2, ImVec2(nodeWidth, nodeHeight), drawList);

	// Draw Winner Crown
	ImVec2 winnerPos(finStartX + xSpacing, winPos.y + finStartY + 25.f);
	drawBracketNode(m_final[0].winnerId, winnerPos, ImVec2(nodeWidth, nodeHeight * 1.5f), drawList);

	ImGui::EndChild();

	ImGui::SameLine();

	// ==========================================
	// --- RIGHT PANEL: SQUAD STATUS ---
	// ==========================================
	ImGui::BeginChild("SquadPanel", ImVec2(0, availableY), true);
	ImGui::TextDisabled("SQUAD STATUS");
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Injuries & Suspensions");
	ImGui::Spacing();

	bool hasIssues = false;

	if (userTeam) {
		for (auto& [pId, player] : m_db->players) {
			if (player.teamId != m_userTeamId) continue;
			// Expand injury checks here later
		}
	}

	if (!hasIssues) {
		ImGui::TextDisabled("Squad is fully fit and available.");
	}

	ImGui::EndChild();

	// ==========================================
	// --- FOOTER BUTTONS ---
	// ==========================================
	ImGui::SetCursorPosY(fullScreenSize.y - 60.0f);
	ImGui::Separator();
	ImGui::Spacing();

	// 1. PLAY NEXT MATCH BUTTON
	if (nextOpp) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));

		if (ImGui::Button("PROCEED TO MATCHDAY", ImVec2(250, 40))) {
			Game::currentState = GameState::MatchDay;
		}
		ImGui::PopStyleColor(3);
	}
	else {
		ImGui::BeginDisabled();
		ImGui::Button("TOURNAMENT OVER", ImVec2(250, 40));
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	ImGui::SetCursorPosX(fullScreenSize.x - 220.0f);

	// 2. BACK TO MENU
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));

	if (ImGui::Button("ABANDON TOURNAMENT", ImVec2(200, 40))) {
		Game::currentState = GameState::MainMenu;
	}
	ImGui::PopStyleColor(3);

	ImGui::End();
}

void TournamentHub::render(sf::RenderWindow& window) {
	window.draw(bg_s);
}

void TournamentHub::drawBracketNode(const std::string& teamId, ImVec2 pos, ImVec2 size, ImDrawList* drawList) {
	ImU32 bgColor = IM_COL32(40, 40, 40, 255);
	ImU32 borderColor = IM_COL32(100, 100, 100, 255);
	ImU32 textColor = IM_COL32(200, 200, 200, 255);

	std::string displayTxt = "TBD";

	if (!teamId.empty()) {
		TeamData* t = m_db->getTeam(teamId);
		if (t) {
			displayTxt = t->fullName;
			if (teamId == m_userTeamId) {
				bgColor = IM_COL32(150, 120, 20, 255);
				textColor = IM_COL32(0, 0, 0, 255);
				borderColor = IM_COL32(255, 215, 0, 255);
			}
			else {
				borderColor = IM_COL32(t->uiColor.r, t->uiColor.g, t->uiColor.b, 255);
			}
		}
	}

	drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bgColor, 4.0f);
	drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), borderColor, 4.0f, 0, 2.0f);

	ImVec2 textSize = ImGui::CalcTextSize(displayTxt.c_str());
	ImVec2 textPos(
		pos.x + (size.x - textSize.x) * 0.5f,
		pos.y + (size.y - textSize.y) * 0.5f
	);
	drawList->AddText(textPos, textColor, displayTxt.c_str());
}
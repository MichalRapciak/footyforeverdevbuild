#include "TournamentHub.h"
#include "imgui-1.92.6/imgui.h"
#include "imgui-1.92.6/imgui-sfml.h"
#include "Game.h"
#include <iostream>

TournamentHub::TournamentHub() : m_db(nullptr), bg_s(bg_txt) {}
TournamentHub::~TournamentHub() {}

void TournamentHub::init(sf::Font& font, GameDatabase& db, const std::vector<std::string>& participantIds, const std::string& userTeamId) {
    m_font = font;
    m_db = &db;
    m_userTeamId = userTeamId;
    m_tournamentWinnerId = "";

    m_activeCompId = "TEMP_CUP_" + std::to_string(rand() % 10000);

    CompetitionData newCup;
    newCup.id = m_activeCompId;
    newCup.name = "Custom Knockout Cup";
    newCup.type = "CUP";
    newCup.participantTeamIds = participantIds;

    m_db->competitions[m_activeCompId] = newCup;

    generateBracket(participantIds);
    simulateBackgroundMatches();
    updateNextFixture(); // <-- THE FIX: Grab the first match!
}

void TournamentHub::generateBracket(const std::vector<std::string>& participantIds) {
    m_bracket.clear();
    m_currentRound = 0;
    m_currentMatchIndex = 0;

    int numTeams = participantIds.size();
    int teamsInRound = numTeams;

    while (teamsInRound > 1) {
        int matchesInRound = teamsInRound / 2;
        m_bracket.push_back(std::vector<BracketNode>(matchesInRound));
        teamsInRound /= 2;
    }

    for (size_t i = 0; i < participantIds.size(); i += 2) {
        BracketNode& node = m_bracket[0][i / 2];
        node.homeTeamId = participantIds[i];
        node.awayTeamId = participantIds[i + 1];
    }
}

// ==========================================
// --- THE FIX: FIND THE NEXT MATCH ---
// ==========================================
void TournamentHub::updateNextFixture() {
    m_currentHomeId = "";
    m_currentAwayId = "";
    m_nextOpponentId = "";

    // If the tournament is over, the winner is in the last match of the last round
    if (m_currentRound >= m_bracket.size()) {
        const BracketNode& finalMatch = m_bracket.back()[0];
        m_tournamentWinnerId = (finalMatch.homeScore > finalMatch.awayScore) ? finalMatch.homeTeamId : finalMatch.awayTeamId;
        return;
    }

    // Otherwise, grab the current match
    const BracketNode& activeMatch = m_bracket[m_currentRound][m_currentMatchIndex];
    m_currentHomeId = activeMatch.homeTeamId;
    m_currentAwayId = activeMatch.awayTeamId;

    if (m_currentHomeId == m_userTeamId) m_nextOpponentId = m_currentAwayId;
    else if (m_currentAwayId == m_userTeamId) m_nextOpponentId = m_currentHomeId;
}

void TournamentHub::advanceTournament(const MatchInfo& result) {

    BracketNode& userMatch = m_bracket[m_currentRound][m_currentMatchIndex];
    userMatch.homeScore = result.getHomeScore();
    userMatch.awayScore = result.getAwayScore();
    userMatch.isCompleted = true;

    std::string winnerId = (userMatch.homeScore > userMatch.awayScore) ? userMatch.homeTeamId : userMatch.awayTeamId;

    if (m_currentRound + 1 < m_bracket.size()) {
        int nextMatchIdx = m_currentMatchIndex / 2;
        if (m_currentMatchIndex % 2 == 0) m_bracket[m_currentRound + 1][nextMatchIdx].homeTeamId = winnerId;
        else m_bracket[m_currentRound + 1][nextMatchIdx].awayTeamId = winnerId;
    }

    m_currentMatchIndex++;
    simulateBackgroundMatches();
    updateNextFixture(); // <-- THE FIX: Queue up the next match!
}

void TournamentHub::simulateBackgroundMatches() {
    while (m_currentRound < m_bracket.size() && m_currentMatchIndex < m_bracket[m_currentRound].size()) {

        BracketNode& activeMatch = m_bracket[m_currentRound][m_currentMatchIndex];

        if (activeMatch.homeTeamId == m_userTeamId || activeMatch.awayTeamId == m_userTeamId) {
            return;
        }

        activeMatch.homeScore = rand() % 4;
        activeMatch.awayScore = rand() % 4;
        if (activeMatch.homeScore == activeMatch.awayScore) activeMatch.homeScore++;

        activeMatch.isCompleted = true;
        std::string winnerId = (activeMatch.homeScore > activeMatch.awayScore) ? activeMatch.homeTeamId : activeMatch.awayTeamId;

        if (m_currentRound + 1 < m_bracket.size()) {
            int nextMatchIdx = m_currentMatchIndex / 2;
            if (m_currentMatchIndex % 2 == 0) m_bracket[m_currentRound + 1][nextMatchIdx].homeTeamId = winnerId;
            else m_bracket[m_currentRound + 1][nextMatchIdx].awayTeamId = winnerId;
        }

        m_currentMatchIndex++;
    }

    if (m_currentMatchIndex >= m_bracket[m_currentRound].size()) {
        m_currentRound++;
        m_currentMatchIndex = 0;

        if (m_currentRound < m_bracket.size()) {
            simulateBackgroundMatches();
        }
    }
}

// ==========================================
// --- THE FIX: DYNAMIC BRACKET UI ---
// ==========================================
void TournamentHub::update(sf::Time dt, sf::RenderWindow& window) {
    ImVec2 fullScreenSize = ImVec2(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y));
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(fullScreenSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGui::Begin("Tournament Hub", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);

    TeamData* userTeam = m_db->getTeam(m_userTeamId);
    TeamData* nextOpp = m_nextOpponentId.empty() ? nullptr : m_db->getTeam(m_nextOpponentId);

    // --- TOP HEADER ---
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.05f, 0.9f));
    ImGui::BeginChild("HeaderPanel", ImVec2(0, 120), true);
    ImGui::SetCursorPosY(20.0f);
    ImGui::SetWindowFontScale(2.0f);

    if (nextOpp) {
        TeamData* homeTeamData = m_db->getTeam(m_currentHomeId);
        TeamData* awayTeamData = m_db->getTeam(m_currentAwayId);

        std::string fixtureText = "NEXT MATCH: " + homeTeamData->fullName + " vs " + awayTeamData->fullName;
        float textWidth = ImGui::CalcTextSize(fixtureText.c_str()).x;
        ImGui::SetCursorPosX((fullScreenSize.x - textWidth) / 2.0f);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", fixtureText.c_str());
    }
    else {
        TeamData* winner = m_db->getTeam(m_tournamentWinnerId);
        std::string statusText = winner ? "TOURNAMENT WINNERS: " + winner->fullName : "TOURNAMENT COMPLETE";
        float textWidth = ImGui::CalcTextSize(statusText.c_str()).x;
        ImGui::SetCursorPosX((fullScreenSize.x - textWidth) / 2.0f);
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", statusText.c_str());
    }

    ImGui::SetWindowFontScale(1.0f);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    float availableY = ImGui::GetContentRegionAvail().y - 80.0f;
    float bracketWidth = fullScreenSize.x * 0.70f;

    // --- LEFT PANEL: BRACKET ---
    ImGui::BeginChild("BracketPanel", ImVec2(bracketWidth, availableY), true);
    ImGui::TextDisabled("ROAD TO THE FINAL");
    ImGui::Separator();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetCursorScreenPos();

    float nodeWidth = 200.f;
    float nodeHeight = 40.f;
    float xSpacing = 280.f;

    // Dynamically draw the rounds!
    for (size_t r = 0; r < m_bracket.size(); ++r) {
        float startX = winPos.x + 50.f + (r * xSpacing);

        // As rounds go on, the vertical space between matches doubles
        float ySpacing = 90.f * std::pow(2, r);
        float startY = winPos.y + 50.f + ((std::pow(2, r) - 1) * 45.f);

        for (size_t m = 0; m < m_bracket[r].size(); ++m) {
            const BracketNode& node = m_bracket[r][m];

            ImVec2 pos1(startX, startY + (m * ySpacing));
            ImVec2 pos2(pos1.x, pos1.y + nodeHeight + 4.f);

            drawBracketNode(node.homeTeamId, pos1, ImVec2(nodeWidth, nodeHeight), drawList);
            drawBracketNode(node.awayTeamId, pos2, ImVec2(nodeWidth, nodeHeight), drawList);

            // Draw connection lines to the next round
            if (r + 1 < m_bracket.size()) {
                float nextX = startX + xSpacing;
                float nextY = winPos.y + 50.f + ((std::pow(2, r + 1) - 1) * 45.f) + ((m / 2) * (90.f * std::pow(2, r + 1)));

                // Connector out from the middle of this match
                float midY = pos1.y + nodeHeight;
                drawList->AddLine(ImVec2(pos1.x + nodeWidth, midY), ImVec2(pos1.x + nodeWidth + 30.f, midY), IM_COL32(100, 100, 100, 255), 2.0f);

                // Vertical line down (or up) to the next match
                // (Only draw the vertical line if it's the TOP match of the pair, extending down to the BOTTOM match)
                if (m % 2 == 0) {
                    float bottomMidY = startY + ((m + 1) * ySpacing) + nodeHeight;
                    drawList->AddLine(ImVec2(pos1.x + nodeWidth + 30.f, midY), ImVec2(pos1.x + nodeWidth + 30.f, bottomMidY), IM_COL32(100, 100, 100, 255), 2.0f);

                    // Branch into the next round
                    drawList->AddLine(ImVec2(pos1.x + nodeWidth + 30.f, nextY + nodeHeight), ImVec2(nextX, nextY + nodeHeight), IM_COL32(100, 100, 100, 255), 2.0f);
                }
            }
        }
    }

    // Draw Winner Crown
    if (!m_tournamentWinnerId.empty()) {
        size_t finalRoundIdx = m_bracket.size() - 1;
        float winnerX = winPos.x + 50.f + ((finalRoundIdx + 1) * xSpacing);
        float winnerY = winPos.y + 50.f + ((std::pow(2, finalRoundIdx) - 1) * 45.f) + 20.f;

        drawBracketNode(m_tournamentWinnerId, ImVec2(winnerX, winnerY), ImVec2(nodeWidth, nodeHeight * 1.5f), drawList);
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // --- RIGHT PANEL: SQUAD STATUS ---
    ImGui::BeginChild("SquadPanel", ImVec2(0, availableY), true);
    ImGui::TextDisabled("SQUAD STATUS");
    ImGui::Separator();
    ImGui::Spacing();

    // (You can expand this later to list actual injuries!)
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Injuries & Suspensions");
    ImGui::TextDisabled("Squad is fully fit and available.");

    ImGui::EndChild();

    // --- FOOTER BUTTONS ---
    ImGui::SetCursorPosY(fullScreenSize.y - 60.0f);
    ImGui::Separator();
    ImGui::Spacing();

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
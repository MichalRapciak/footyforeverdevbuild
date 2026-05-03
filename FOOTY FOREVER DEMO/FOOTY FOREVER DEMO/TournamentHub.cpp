#include "TournamentHub.h"
#include "imgui-1.92.6/imgui.h"
#include "imgui-1.92.6/imgui-sfml.h"
#include "QuickSimEngine.h"
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

        // ==========================================
        // --- 1. RUN THE QUICK SIM ENGINE ---
        // ==========================================
        // Note: m_db is a pointer in TournamentHub, so we dereference it with *m_db
        MatchInfo result = QuickSimEngine::simulateMatch(*m_db, activeMatch.homeTeamId, activeMatch.awayTeamId);

        activeMatch.homeScore = result.getHomeScore();
        activeMatch.awayScore = result.getAwayScore();

        // ==========================================
        // --- 2. KNOCKOUT TIEBREAKER ---
        // ==========================================
        // If the 90 minutes end in a draw, simulate a quick "Penalty Shootout" win
        // by bumping one score by 1. 
        if (activeMatch.homeScore == activeMatch.awayScore) {
            if (rand() % 2 == 0) {
                activeMatch.homeScore++;
                // Optional: You could log a fake "Penalty shootout winner" event here later
            }
            else {
                activeMatch.awayScore++;
            }
        }

        activeMatch.isCompleted = true;
        std::string winnerId = (activeMatch.homeScore > activeMatch.awayScore) ? activeMatch.homeTeamId : activeMatch.awayTeamId;

        // ==========================================
        // --- 3. LOG AI STATS TO THE DATABASE ---
        // ==========================================
        // This makes the AI teams populate the top scorers/assisters leaderboards!
        m_db->processMatchResult(result, m_activeCompId);

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

            // THE FIX: Pass the score and completion status!
            drawBracketNode(node.homeTeamId, node.homeScore, node.isCompleted, pos1, ImVec2(nodeWidth, nodeHeight), drawList);
            drawBracketNode(node.awayTeamId, node.awayScore, node.isCompleted, pos2, ImVec2(nodeWidth, nodeHeight), drawList);

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

        // The winner crown doesn't need a score, so pass 0 and false
        drawBracketNode(m_tournamentWinnerId, 0, false, ImVec2(winnerX, winnerY), ImVec2(nodeWidth, nodeHeight * 1.5f), drawList);
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // ==========================================
        // --- RIGHT PANEL: SQUAD STATUS & STATS ---
        // ==========================================
    ImGui::BeginChild("SquadPanel", ImVec2(0, availableY), true);

    ImGui::TextDisabled("SQUAD STATUS");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Injuries & Suspensions");
    ImGui::Spacing();

    bool hasIssues = false;

    // THE FIX: Grab the active competition first!
    CompetitionData* activeComp = m_db->getCompetition(m_activeCompId);

    if (userTeam && activeComp) {
        for (auto& [pId, player] : m_db->players) {
            if (player.teamId != m_userTeamId) continue;

            // 1. Check for Injuries (Uses Core PlayerData)
            if (player.isInjured) {
                std::string injText = "- " + player.name + " (" + player.currentInjury + ", " + std::to_string(player.injuryDaysRemaining) + " days)";
                ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "%s", injText.c_str());
                hasIssues = true;
            }

            // 2. THE FIX: Check for Suspensions (Uses PlayerCompStats!)
            // Make sure the player actually has a stat block in this tournament first
            if (activeComp->playerStats.find(pId) != activeComp->playerStats.end()) {
                if (activeComp->playerStats[pId].redCards > 0) {
                    std::string suspText = "- " + player.name + " (Suspended: Red Card)";
                    ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "%s", suspText.c_str());
                    hasIssues = true;
                }
            }
        }
    }

    if (!hasIssues) {
        ImGui::TextDisabled("Squad is fully fit and available.");
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    // ==========================================
    // --- THE FIX: TOP PERFORMERS LEADERBOARD ---
    // ==========================================
    ImGui::TextDisabled("TEAM TOP PERFORMERS");
    ImGui::Separator();
    ImGui::Spacing();

    if (activeComp && userTeam) {

        // 1. Gather all user players who have played at least 1 match
        std::vector<std::pair<std::string, PlayerCompStats>> performers;

        for (const auto& [pId, pStats] : activeComp->playerStats) {
            PlayerData* p = m_db->getPlayer(pId);
            if (p && p->teamId == m_userTeamId && pStats.matchesRated > 0) {
                performers.push_back({ p->name, pStats });
            }
        }

        // 2. Sort them by Average Match Rating (Highest first)
        std::sort(performers.begin(), performers.end(), [](const auto& a, const auto& b) {
            return a.second.getAverageRating() > b.second.getAverageRating();
            });

        // 3. Display the Top 5
        if (performers.empty()) {
            ImGui::TextDisabled("No match data available yet.");
        }
        else {
            ImGui::Columns(4, "statsColumns", false);
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Player"); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Avg Rtg"); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Gls"); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Asts"); ImGui::NextColumn();
            ImGui::Separator();

            int displayCount = std::min(static_cast<int>(performers.size()), 5);
            for (int i = 0; i < displayCount; ++i) {

                // Color code the rating! (Green = Great, Yellow = Okay, Red = Bad)
                float avgRtg = performers[i].second.getAverageRating();
                ImVec4 rtgColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                if (avgRtg >= 7.5f) rtgColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
                else if (avgRtg <= 6.0f) rtgColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

                ImGui::Text("%s", performers[i].first.c_str()); ImGui::NextColumn();
                ImGui::TextColored(rtgColor, "%.1f", avgRtg); ImGui::NextColumn();
                ImGui::Text("%d", performers[i].second.goals); ImGui::NextColumn();
                ImGui::Text("%d", performers[i].second.assists); ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }
    }

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

void TournamentHub::drawBracketNode(const std::string& teamId, int score, bool isCompleted, ImVec2 pos, ImVec2 size, ImDrawList* drawList) {
    ImU32 bgColor = IM_COL32(40, 40, 40, 255);
    ImU32 borderColor = IM_COL32(100, 100, 100, 255);
    ImU32 textColor = IM_COL32(200, 200, 200, 255);

    std::string displayTxt = "TBD";

    if (!teamId.empty()) {
        TeamData* t = m_db->getTeam(teamId);
        if (t) {
            // THE FIX: Append the score if the match is completed!
            if (isCompleted) {
                displayTxt = t->shortName + "  " + std::to_string(score);
            }
            else {
                displayTxt = t->fullName;
            }

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
#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "GameDatabase.h"

struct BracketNode {
    std::string homeTeamId;
    std::string awayTeamId;
    int homeScore = 0;
    int awayScore = 0;
    bool isCompleted = false;
};

struct ImVec2;
struct ImDrawList;

class TournamentHub {
public:
    TournamentHub();
    ~TournamentHub();

    void init(sf::Font& font, GameDatabase& db, const std::vector<std::string>& participantIds, const std::string& userTeamId);

    void advanceTournament(const MatchInfo& result);

    // THE FIX: Provide the getters for Game.cpp!
    std::string getCurrentMatchHomeId() const { return m_currentHomeId; }
    std::string getCurrentMatchAwayId() const { return m_currentAwayId; }
    std::string getUserTeamId() const { return m_userTeamId; }
    std::string getActiveCompId() const { return m_activeCompId; }

    void update(sf::Time dt, sf::RenderWindow& window);
    void render(sf::RenderWindow& window);

private:
    GameDatabase* m_db = nullptr;
    sf::Font m_font;

    std::string m_userTeamId;
    std::string m_activeCompId;

    // The Tree: m_bracket[RoundIndex][MatchIndex]
    std::vector<std::vector<BracketNode>> m_bracket;

    int m_currentRound = 0;
    int m_currentMatchIndex = 0;

    // Tracker variables for the UI and the MatchDay transition
    std::string m_nextOpponentId = "";
    std::string m_currentHomeId = "";
    std::string m_currentAwayId = "";
    std::string m_tournamentWinnerId = "";

    sf::Texture bg_txt;
    sf::Sprite bg_s;

    void generateBracket(const std::vector<std::string>& participantIds);
    void simulateBackgroundMatches();
    void updateNextFixture(); // NEW: Extracts the next match info

    void drawBracketNode(const std::string& teamId, ImVec2 pos, ImVec2 size, ImDrawList* drawList);
};
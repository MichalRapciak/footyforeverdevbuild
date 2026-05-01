#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

class GameDatabase;
struct ImVec2;
struct ImDrawList;

// Simple struct to hold match data for the bracket
struct TournamentMatch {
	std::string team1Id; // Home Team
	std::string team2Id; // Away Team
	std::string winnerId = "";
	int score1 = 0;
	int score2 = 0;
	bool isPlayed = false;
};

class TournamentHub
{
public:
	TournamentHub();
	~TournamentHub();

	void init(sf::Font& font, GameDatabase& database, const std::vector<std::string>& bracket, const std::string& userTeamId);
	void update(sf::Time dt, sf::RenderWindow& window);
	void render(sf::RenderWindow& window);

	const std::string& getUserTeamId() const { return m_userTeamId; }
	const std::string& getNextOpponentId() const { return m_nextOpponentId; }

	// NEW: Getters to pass directly into MatchDayScreen::init()
	const std::string& getCurrentMatchHomeId() const { return m_currentHomeId; }
	const std::string& getCurrentMatchAwayId() const { return m_currentAwayId; }

protected:
	GameDatabase* m_db{ nullptr };
	sf::Sprite bg_s;
	sf::Texture bg_txt;

	// State Variables
	std::string m_userTeamId;
	int m_tournamentSize;
	std::string m_nextOpponentId;

	// NEW: Store the explicit sides for the upcoming match
	std::string m_currentHomeId;
	std::string m_currentAwayId;

	// Rounds
	std::vector<TournamentMatch> m_quarterFinals;
	std::vector<TournamentMatch> m_semiFinals;
	std::vector<TournamentMatch> m_final;

	// Helper to draw the bracket boxes
	void drawBracketNode(const std::string& teamId, ImVec2 pos, ImVec2 size, ImDrawList* drawList);
};
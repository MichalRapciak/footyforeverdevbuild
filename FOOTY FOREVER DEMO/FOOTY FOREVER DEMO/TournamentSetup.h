#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

// Forward declarations
class GameDatabase;

class TournamentSetup
{
public:
	TournamentSetup();
	~TournamentSetup();

	void init(sf::Font& font, GameDatabase& database);
	void update(sf::Time dt, sf::RenderWindow& window);
	void render(sf::RenderWindow& window);

	// Retrieve the generated bracket once setup is complete
	const std::vector<std::string>& getGeneratedBracket() const { return m_tournamentBracket; }
	const std::string& getUserTeamId() const { return m_userTeamId; }

protected:
	GameDatabase* m_db{ nullptr };
	sf::Font m_font;
	sf::Sprite bg_s;
	sf::Texture bg_txt;

	// State Variables
	int m_tournamentSize = 4;
	std::string m_userTeamId = "";

	// The final generated array of Team IDs that will be passed to TournamentHub
	std::vector<std::string> m_tournamentBracket;
};
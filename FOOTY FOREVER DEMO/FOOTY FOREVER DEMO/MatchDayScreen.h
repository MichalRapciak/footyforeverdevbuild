#pragma once
#include <SFML/Graphics.hpp>
#include "GameDatabase.h"
#include <string>

class MatchDayScreen {
public:
    MatchDayScreen();
    ~MatchDayScreen();

    // UPDATED: Added userTeamId so we know which side to unlock!
    void init(sf::Font& font, GameDatabase& database, std::string homeId = "", std::string awayId = "", std::string userTeamId = "", bool isTournament = false);
    void update(sf::Time dt, sf::RenderWindow& window);
    void render(sf::RenderWindow& window);

    // Getters for the Game loop to pull the selected teams when starting the match
    std::string getHomeTeamId() const { return m_homeTeamId; }
    std::string getAwayTeamId() const { return m_awayTeamId; }
    std::string getUserPlayerId() const { return m_userPlayerId; }

private:
    sf::Font m_font;
    GameDatabase* m_db;
    sf::Sprite bg_s;
    sf::Texture bg_txt;

    std::string m_homeTeamId;
    std::string m_awayTeamId;
    std::string m_userPlayerId;

    // TOURNAMENT LOCK STATES
    bool m_isTournamentMode = false;
    std::string m_tournamentUserTeamId = "";
};
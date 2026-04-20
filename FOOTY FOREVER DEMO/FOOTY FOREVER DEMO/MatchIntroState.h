#pragma once
#include <SFML/Graphics.hpp>
#include "GameDatabase.h"
#include <string>

class MatchIntroState {
public:
    MatchIntroState();

    void init(sf::Font& font, GameDatabase& database, const std::string& homeId, const std::string& awayId, const std::string& userId);

    // Accepts the progress from Game.cpp
    void update(sf::Time dt, sf::RenderWindow& window, float progress);
    void render(sf::RenderWindow& window);

private:
    sf::Font m_font;
    GameDatabase* m_db;

    std::string m_homeTeamId;
    std::string m_awayTeamId;
    std::string m_userPlayerId;

    sf::RectangleShape m_bg;
    sf::RectangleShape m_barBg;
    sf::RectangleShape m_barFill;

    TeamData m_homeData;
    TeamData m_awayData;

    // --- NEW: Stored Progress ---
    float m_currentProgress = 0.0f;
};
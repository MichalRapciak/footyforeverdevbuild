#pragma once
#include <SFML/Graphics.hpp>

class SettingsState {
public:
    void init(sf::RenderWindow& window);
    void update(sf::RenderWindow& window);
    void render(sf::RenderWindow& window);

private:
    sf::RectangleShape m_background;
};
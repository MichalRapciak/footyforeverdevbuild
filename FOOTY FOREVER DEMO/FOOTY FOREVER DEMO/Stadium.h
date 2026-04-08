#pragma once
#include "SFML/Graphics.hpp"
#include <math.h>

class GamePlay;

/// <summary>
/// Class in charge of the Level, Tilemap etc.
/// </summary>
class Stadium
{
public:
	Stadium();
	~Stadium();

	void initialiseStadium();
	void update(GamePlay& game);

	void draw(sf::RenderWindow& window) const;

private:
	// Layer 1: Grass
	sf::Texture m_grassTXT;
	sf::Sprite  m_grassBG;

	// Layer 2: Pitch Lines
	sf::Texture m_linesTXT;
	sf::Sprite  m_linesBG;
	short randomNo = 0;
};
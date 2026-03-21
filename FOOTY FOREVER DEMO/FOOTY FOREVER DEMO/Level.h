#pragma once
#include "SFML/Graphics.hpp"
#include <math.h>

class GamePlay;

enum class Tile {
	Grass,
	Wet,
	Snow,
	Wall
};

/// <summary>
/// Class in charge of the Level, Tilemap etc.
/// </summary>
class Level
{
public:
	Level();
	~Level();

	void initialiseArrays();
	void initialiseMap();
	void update(GamePlay& game);
	int returnTile(sf::Vector2f t_Pos);
	bool isSolid(sf::Vector2i t_pos);
	bool isGameWon() { return gameWon; }


	sf::Sprite getLevelBG() { return m_levelBG; }

private:
	sf::Sprite m_levelBG;
	sf::Texture m_levelTXT;
	static const int X_BOXES = 70;
	static const int Y_BOXES = 100;
	int levelGrid1[Y_BOXES][X_BOXES];
	int tile;
	short randomNo = 0;
	bool gameWon = false;
};
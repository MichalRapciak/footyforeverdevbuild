#include "Level.h"
#include <iostream>
#include <fstream>
#include "GamePlay.h"
#include <random>

Level::Level() : m_levelBG(m_levelTXT)
{
	initialiseArrays();
	initialiseMap();
	srand(time(nullptr)); // sets up random seed
}

Level::~Level()
{
}

/// <summary>
/// This function initializes the sprite visible in the background
/// </summary>
void Level::initialiseMap()
{
	if (!m_levelTXT.loadFromFile("ASSETS/LEVEL/level1bg.png"))
	{
		std::cout << "Can't load background" << std::endl;
	}
	m_levelTXT.setRepeated(false);
	m_levelBG.setTexture(m_levelTXT);
	m_levelBG.setPosition({ 0, 0 });
	m_levelBG.setTextureRect(sf::IntRect({ 0,0 }, { 1000,700 }));
	m_levelBG.setScale({ 10.0f,10.f });
}

void Level::update(GamePlay& game)
{
}

/// <summary>
/// This function loads the tilemap (stored in a .txt file) into the system
/// </summary>
void Level::initialiseArrays()
{
}

/// <summary>
/// This function is used to get the current tile entity is in
/// </summary>
/// <param name="t_pos"></param>
/// <returns></returns>
int Level::returnTile(sf::Vector2f t_pos)
{
	int xTile = static_cast<int>(t_pos.x / 100); // Current position is divided by 100 as 1 tile = 100 pixels
	int yTile = static_cast<int>(t_pos.y / 100);
	tile = levelGrid1[yTile][xTile];
	return tile;
}

/// <summary>
/// This function checks if tile is a wall
/// </summary>
/// <param name="t_pos"></param>
/// <returns></returns>
bool Level::isSolid(sf::Vector2i t_pos)
{
	tile = levelGrid1[t_pos.y][t_pos.x];
	if (tile == 3)
	{
		return true;
	}
	else
	{
		return false;
	}
}
#pragma once
#include <SFML/Graphics.hpp>
#include <iostream>
#include "Player.h"
#include "PlayerStats.h"
#include "PlayerState.h"

class GamePlay;

class UserPlayer : public Player
{
public:
	UserPlayer();
	~UserPlayer();

	sf::Sprite getSprite() override { return m_playerSprite; }
	void setPlayerTexture(sf::Texture t_texture) { m_playerTexture = t_texture; }
	sf::Vector2f getPlayerAim() { return m_playerAim; }
	sf::Vector2f getAimDirection() const override { sf::Vector2f direction; float radians = m_playerSprite.getRotation().asRadians(); direction.x = std::cos(radians); direction.y = std::sin(radians); return direction; }
	void move(sf::Vector2f t_move) override { m_position += t_move; }

	void setPlayerAim(sf::Vector2f t_mousePos) { m_playerAim = t_mousePos; }
	void updateAim(sf::Vector2i t_mousePos, float facingDir);
	void update(float dt) override;
	void shooting(float dt, GamePlay& game);

private:

	sf::Vector2f m_tackleDirection;

	sf::Sprite m_playerSprite;
	sf::Texture m_playerTexture;
	sf::Vector2f m_playerAim{ 0, 0 }; // coordinates where player is aiming

};
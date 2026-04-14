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
	UserPlayer(const sf::Texture& texture);
	~UserPlayer();

	sf::Vector2f getPlayerAim() { return m_playerAim; }
	sf::Vector2f getAimDirection() const override {
		sf::Vector2f direction = m_playerAim - m_position;
		float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
		if (length != 0) {
			direction.x /= length;
			direction.y /= length;
		}
		return direction;
	}
	void move(sf::Vector2f t_move) override { m_position += t_move; }

	void setPlayerAim(sf::Vector2f t_mousePos) { m_playerAim = t_mousePos; }
	void updateAim(sf::Vector2f t_mouseWorldPos);
	void update(float dt, AnimationServer& animServer) override;

private:
	sf::Vector2f m_tackleDirection;

	sf::Vector2f m_playerAim{ 0, 0 }; // coordinates where player is aiming

};
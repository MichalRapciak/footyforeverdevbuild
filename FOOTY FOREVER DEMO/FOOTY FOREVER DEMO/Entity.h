#pragma once
#include <SFML/Graphics.hpp>

class Entity
{
public:
	Entity() = default;
	virtual ~Entity() = default;

	virtual sf::FloatRect getBoundingBox() const = 0;
	virtual sf::Sprite getSprite() = 0;
	virtual sf::Vector2f getPosition() const = 0;

	float z = 0.0f;
	float vz = 0.0f;

private:


};
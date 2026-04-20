#pragma once
#include <SFML/Graphics.hpp>
#include "Entity.h"
#include "Player.h"

class UserPlayer;

class Ball : public Entity
{
    friend class PhysicsEngine;
public:
    Ball();

    void update(float dt);
    void updateFreePhysics(float dt);
    void draw(sf::RenderWindow& window);

    void possess(Player* player);
    void release();
    void shoot(const sf::Vector2f& direction, float power, float kickSpin, float v0z, float backspin);

    bool hasOwner() const;
    Player* getOwner() const;
    Player* getLastOwner() const { return lastOwner; }

    void applyImpulse(sf::Vector2f force);
    sf::Vector2f reflect(const sf::Vector2f& velocity, const sf::Vector2f& normal);

    sf::FloatRect getBoundingBox() const override { return sf::FloatRect({ shape.getPosition().x - 30,shape.getPosition().y - 30}, {30,30}); }
    sf::Vector2f getPosition() const override;
    sf::Vector2f getVelocity() { return velocity; }
    void setVelocity(sf::Vector2f t_velocity) { velocity = t_velocity; }
    void setPosition(const sf::Vector2f& pos);
    sf::Sprite getSprite() override { return sprite; }
    sf::CircleShape getShadow() { return shadow; }

    bool isSetPiece() const { return m_isSetPiece; }
    void setSetPiece(bool state) { m_isSetPiece = state; }

    Player* lastTouch = nullptr;
    float bs = 0.0f;          // Backspin
    float spin = 0.f;         // spin
    float friction = 800.f;
    float gravity = 980.f;   // pixels per second
    sf::Vector2f velocity;
    sf::CircleShape shape;
    bool passCompletedEvent = false;
private:
    sf::Sprite sprite;
    sf::Texture texture;

    Player* owner = nullptr;
    Player* lastOwner = nullptr;
    bool m_isSetPiece = false;


    float maxSpeed = 3400.f;
    float footTimer = 0.f;
    const float footSwitchTime = 0.15f;

    float spinFriction = 0.f;

    float minScale = 1.f;
    float maxScale = 1.6f;



    sf::CircleShape shadow;   // shadow on the ground

    void updateDribbling(float dt);

};
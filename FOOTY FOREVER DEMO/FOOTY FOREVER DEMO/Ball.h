#pragma once
#include <SFML/Graphics.hpp>
#include "Entity.h"
#include "Player.h"

struct MatchEnvironment;
class UserPlayer;

class Ball : public Entity
{
    friend class PhysicsEngine;
public:
    Ball();

    void update(float dt);
    void updateFreePhysics(float dt);
    void draw(sf::RenderWindow& window);

    void possess(Player* player, MatchEnvironment& env);
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

    bool isLooseControl() const { return m_isLooseControl; }

    void notifyPlayerSwap(Player* p1, Player* p2);

    Player* lastTouch = nullptr;
    float bs = 0.0f;          // Backspin
    float spin = 0.f;         // spin
    float friction = 800.f;
    float gravity = 980.f;   // pixels per second
    sf::Vector2f velocity;
    sf::CircleShape shape;
    bool passCompletedEvent = false;

    Player* lastShooter = nullptr;
    Player* assistCandidate = nullptr;
    bool isPassIntent = false;
    Player* lastShooterAssister = nullptr;
    bool lastShotWasOnTarget = false;

private:

    sf::Sprite sprite;
    sf::Texture texture;

    bool m_isLooseControl = false;

    Player* owner = nullptr;
    Player* lastOwner = nullptr;
    bool m_isSetPiece = false;

    float maxSpeed = 3400.f;

    // --- NEW: Replaced footTimer with Frame Tracker ---
    int m_lastDribbleFrame = -1;

    // --- NEW: Advanced Touch Mechanics ---
    bool m_isFirstTouch = false;
    sf::Vector2f m_lastDribbleDir = { 0.f, 0.f };

    const float footSwitchTime = 0.15f;
    float spinFriction = 0.f;

    float minScale = 1.f;
    float maxScale = 1.6f;

    sf::CircleShape shadow;
    void updateDribbling(float dt);

};
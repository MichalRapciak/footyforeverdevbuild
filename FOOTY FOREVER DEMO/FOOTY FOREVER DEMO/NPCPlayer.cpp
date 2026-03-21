#include "NPCPlayer.h"
#include <cmath>

NPCPlayer::NPCPlayer() : m_sprite(m_texture)
{
    // Load your placeholder or a different color kit for teammates
    if (!m_texture.loadFromFile("Assets/Player/playerplaceholder.png", false, sf::IntRect({ 0,0 }, { 256, 256 })))
    {
        std::cout << "Teammate texture not loaded." << std::endl;
    }

    m_position = { 5000.f, 1000.f }; // Default start
    m_sprite.setTexture(m_texture);
    m_sprite.setTextureRect(sf::IntRect({ 0,0 }, { 256,256 }));
    m_sprite.setOrigin({ 128.f, 128.f });
    m_sprite.setScale({ 0.40f, 0.40f });
    m_sprite.setPosition(m_position);

    // Give them consistent stats
    m_stats = { 70.0f, 70.0f, 70.0f, 70.0f, 70.0f, 70.0f, 70.0f, 77.0f, 77.0f, 77.0f, 77.0f, 70.0f, 70.f, 70.f, 70.f, 70.f, 70.f, 70.f, 70.0f, 70.0f, 70.0f };	// FINISHING, CURL, BALL CONTROL, BALANCING, SHORT PASSING, LONG PASSING, THROUGH PASSING, TOP SPEED, ACCELERATION, AGILITY, BODY STRENGTH, KICK POWER, AWARENESS, AGGRESSION, BLOCKING, GK COVER, GK REACT, GK CATCH, GK THROW, GK AWARE, GK BLOCK
}
    NPCPlayer::~NPCPlayer()
    {
    }

void NPCPlayer::update(float dt)
{
    Player::update(dt);
    if (m_team != Team::Home)
    {
        m_sprite.setColor(sf::Color::Blue);
    }
    if (m_positionRole == PositionRole::Goalkeeper)
    {
        m_sprite.setColor(sf::Color::Green);
    }

    // 2. APPLY PHYSICS TO POSITION
    // This uses the velocity calculated by your NPCController
    m_sprite.setPosition(m_position);
}

void NPCPlayer::setRotationToward(sf::Vector2f targetPos)
{
    // NPCs don't have a mouse, so we calculate the angle to a target (like the ball)
    sf::Vector2f direction = targetPos - m_position;

    // atan2 returns the angle in radians
    float radians = std::atan2(direction.y, direction.x);
    float degrees = radians * (180.f / 3.14159265f);

    // +90 assuming your sprite faces right by default
    m_sprite.setRotation(sf::degrees(degrees));
}



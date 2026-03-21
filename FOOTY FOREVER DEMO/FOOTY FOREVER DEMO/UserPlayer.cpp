#include "UserPlayer.h"

/// <summary>
/// Setting up player sprite when the player function is created.
/// </summary>
UserPlayer::UserPlayer() : m_playerSprite(m_playerTexture)
{
	if (!m_playerTexture.loadFromFile("Assets/Player/playerplaceholder.png", false, sf::IntRect({ 0,0 }, { 256, 256 }))) // if texture doesnt load, output text
	{
		std::cout << "Texture not loaded." << std::endl;
	}
	m_position = { 4000,3500 };
	m_playerSprite.setTexture(m_playerTexture);
	m_playerSprite.setTextureRect(sf::IntRect({ 0,0 }, { 256,256 }));
	m_playerSprite.setOrigin({ 128,128 });
	m_playerSprite.setScale({ 0.40f,0.40f });
	m_playerSprite.setPosition(m_position);
	m_stats = { 80.0f, 80.0f, 80.0f, 88.0f, 80.0f, 80.0f, 80.0f, 88.0f, 88.0f, 88.0f, 80.0f, 80.0f, 80.f, 80.f, 80.f, 80.f, 80.f, 80.f, 80.0f, 80.0f, 80.0f }; 	// FINISHING, CURL, BALL CONTROL, BALANCING, SHORT PASSING, LONG PASSING, THROUGH PASSING, TOP SPEED, ACCELERATION, AGILITY, BODY STRENGTH, KICK POWER, AWARENESS, AGGRESSION, BLOCKING, GK COVER, GK REACT, GK CATCH, GK THROW< GK AWARE, GK BLOCK
	m_team = Team::Home;
}

UserPlayer::~UserPlayer()
{
}

/// <summary>
/// Function used to update player position
/// </summary>
/// <param name="dt"></param>
void UserPlayer::update(float dt)
{
	Player::update(dt);
    if (m_team != Team::Home)
    {
        m_playerSprite.setColor(sf::Color::Blue);
    }
    if (m_positionRole == PositionRole::Goalkeeper)
    {
        m_playerSprite.setColor(sf::Color::Green);
    }
	m_playerSprite.setPosition(m_position);
}

/// <summary>
/// Function used to update player aiming
/// </summary>
/// <param name="t_mousePos"></param>
/// <param name="facingDir"></param>
void UserPlayer::updateAim(sf::Vector2i t_mousePos, float facingDir)
{
	m_playerAim.x = t_mousePos.x;
	m_playerAim.y = t_mousePos.y;
	m_playerSprite.setRotation(sf::degrees(facingDir + 90));
}

/// <summary>
/// This function is in charge of shooting
/// </summary>
/// <param name="dt"></param>
void UserPlayer::shooting(float dt, GamePlay& game)
{
}

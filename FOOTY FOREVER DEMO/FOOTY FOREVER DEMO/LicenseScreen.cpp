#include "LicenseScreen.h"
#include "Game.h"

LicenseScreen::LicenseScreen() : m_text(m_font)
{
}

LicenseScreen::~LicenseScreen()
{
}

void LicenseScreen::initialise(sf::Font& t_font)
{
	m_font = t_font;
	m_text.setCharacterSize(60U);
	m_text.setFillColor(sf::Color::White);
	m_text.setString("All names are purely fictional.\n any resemblance to real names\nis purely coincidental.");

	sf::FloatRect textSize = m_text.getGlobalBounds();
	float xPosition = 1920 / 2.0f - textSize.size.x / 2.0f; //middle of text put on middle of screen
	float yPosisiton = 1080 * 0.4f;
	m_text.setPosition({ xPosition, yPosisiton });
	m_licenseTime = sf::seconds(0);
}

void LicenseScreen::render(sf::RenderWindow& t_window)
{
	t_window.draw(m_text);
}

void LicenseScreen::update(sf::Time& t_deltaTime)
{
	m_licenseTime += t_deltaTime;
	if (m_licenseTime.asSeconds() > 1.5)
	{
		Game::currentState = GameState::Splash;
	}
}

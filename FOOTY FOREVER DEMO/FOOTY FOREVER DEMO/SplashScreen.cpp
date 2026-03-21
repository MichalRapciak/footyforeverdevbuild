#include <iostream>
#include "SplashScreen.h"
#include "Game.h"

SplashScreen::SplashScreen() : m_splashText(m_font)
{
}

SplashScreen::~SplashScreen()
{
}

void SplashScreen::initialise(sf::Font& t_font)
{
	m_font = t_font;
	m_splashText.setFont(m_font); // Text seen on the screen
	m_splashText.setString("Press Space To Continue");
	m_splashText.setCharacterSize(36);
	m_splashText.setFillColor(sf::Color::Red);
	m_splashText.setStyle(sf::Text::Bold);

	sf::FloatRect textSize = m_splashText.getGlobalBounds(); // will be used to put the text in the middle
	float xpos = (1920 / 2) - (textSize.size.x / 2);
	m_splashText.setPosition({ xpos, 1080 / 2 - (textSize.size.y / 2) });

	m_anyKeyPressed = false;
}

void SplashScreen::update(sf::Time& t_deltaTime)
{
	if (m_anyKeyPressed)
	{
		Game::currentState = GameState::MainMenu;
	}
}

void SplashScreen::processInput(sf::Event& t_event)
{
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
	{
		m_anyKeyPressed = true;
	}
}

void SplashScreen::render(sf::RenderWindow& t_window)
{
	t_window.draw(m_splashText);
}

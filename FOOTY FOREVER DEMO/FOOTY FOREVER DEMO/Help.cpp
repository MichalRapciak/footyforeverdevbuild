#include "Help.h"
#include "Game.h"

Help::Help() : m_helpText(m_font)
{
}

Help::~Help()
{
}

void Help::initialise(sf::Font& t_font)
{
	m_font = t_font;

	m_helpText.setFont(m_font);
	m_helpText.setString("Press Esc to Pause The Game\n(or to return to the Main Menu now)");
	m_helpText.setCharacterSize(40);
	m_helpText.setFillColor(sf::Color::White);

	sf::FloatRect textSize = m_helpText.getGlobalBounds();
	float xpos = 1920 / 2 - textSize.size.x / 2;
	m_helpText.setPosition({ xpos, 1080 * 0.20f });
	m_pressedExit = false;
}

void Help::processInput(sf::Event& t_event)
{
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape))
	{
		m_pressedExit = true;
	}
}

void Help::update(sf::Time& t_deltatime)
{
	if (m_pressedExit == true)
	{
		Game::currentState = GameState::MainMenu;
	}
	m_pressedExit = false;
}

void Help::render(sf::RenderWindow& t_window)
{
	t_window.draw(m_helpText);
}

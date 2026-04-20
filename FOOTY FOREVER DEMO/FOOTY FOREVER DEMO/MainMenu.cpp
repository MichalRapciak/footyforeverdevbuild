#include "MainMenu.h"
#include "Game.h"
#include "GamePlay.h"
#include <iostream>

MainMenu::MainMenu() : m_mainMenuView(sf::FloatRect({ 0,0 }, { 1920,1080 })), bg_s(bg_txt)
{

}

MainMenu::~MainMenu()
{
}

void MainMenu::initialise(sf::Font& t_font)
{
	m_buttonWidth = 400;
	m_buttonHeight = 100;
	m_yOffset = 400;
	m_xOffset = (m_mainMenuView.getSize().x / 2) - m_buttonWidth / 2;
	m_buttonSpacing = 120;
	int textDropOffset = 15;
	sf::String m_Texts[] = { "Go To Match Day", "Database Editor", "Settings", "Exit Game" };

	m_font = t_font;

	if (!m_buttonTxt.loadFromFile("ASSETS/IMAGES/button.png"))
	{
		std::cout << "Can't load button texture";
	}
	sf::IntRect txtRect;
	txtRect.position = { 0,0 };
	txtRect.size = { static_cast<int>(m_buttonTxt.getSize().x) , static_cast<int>(m_buttonTxt.getSize().y) };
	for (int i = 0; i < m_buttonCount; i++)
	{
		auto& sprite = m_buttonSprite.emplace_back(m_buttonTxt);
		sprite.setTexture(m_buttonTxt);
		sprite.setTextureRect(txtRect);
		sprite.setPosition({ m_xOffset, m_buttonSpacing * i + m_yOffset });
		sf::Vector2u txtSize = m_buttonTxt.getSize();
		sprite.setScale({ m_buttonWidth / txtSize.x, m_buttonHeight / txtSize.y });

		auto& text = m_text.emplace_back(m_font);
		text.setFont(m_font);
		text.setString(m_Texts[i]);
		text.setFillColor(sf::Color::Black);
		text.setCharacterSize(40);
		sf::FloatRect textSize = text.getGlobalBounds();
		float textOffset = (m_buttonWidth - textSize.size.x) / (2);
		text.setPosition({ m_xOffset + textOffset, m_buttonSpacing * i + m_yOffset + textDropOffset });
	}
	if (!bg_txt.loadFromFile("ASSETS/IMAGES/mainmenu.png"))
	{
		std::cout << "couldn't load main menu background\n";
	}
	bg_s.setTexture(bg_txt);
	bg_s.setTextureRect(sf::IntRect({ 0,0 }, { 1920,1080 }));
	bg_s.setPosition({ 0,0 });
}

void MainMenu::processInput(sf::Event& t_event, sf::RenderWindow& t_window)
{
	if (const auto resized = t_event.getIf<sf::Event::Resized>()) //debugging to see if window resizing works
	{
		sf::Vector2f visibleArea(sf::Vector2f(resized->size));
		m_mainMenuView.setSize(visibleArea);
		m_xOffset = (m_mainMenuView.getSize().x / 2) - m_buttonWidth / 2;
		m_yOffset = 200;
		t_window.setView(m_mainMenuView);
	}

}

void MainMenu::update(sf::Time& t_deltaTime, sf::RenderWindow& t_window)
{
	sf::Vector2f mouseLocation;
	mouseLocation = t_window.mapPixelToCoords(sf::Mouse::getPosition(t_window));
	for (int i = 0; i < m_buttonCount; i++)
	{
		m_text[i].setFillColor(sf::Color::White);
		m_buttonSprite[i].setColor(sf::Color{0,100,0,255});
	}
	if (mouseLocation.x > m_xOffset && mouseLocation.x < m_xOffset + m_buttonWidth)
	{
		if (mouseLocation.y > m_yOffset && mouseLocation.y < m_yOffset + m_buttonHeight)
		{
			m_buttonSprite[0].setColor(sf::Color{ 0,50,0,255 });
			//m_text[0].setFillColor(sf::Color{ 75,75,75,255 });
			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
			{
				Game::currentState = GameState::MatchDay;
			}
		}
		if (mouseLocation.y > m_yOffset + m_buttonSpacing && mouseLocation.y < m_yOffset + m_buttonHeight + m_buttonSpacing)
		{
			m_buttonSprite[1].setColor(sf::Color{ 0,50,0,255 });
			//m_text[1].setFillColor(sf::Color{ 75,75,75,255 });
			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
			{
				Game::currentState = GameState::Editor;
			}
		}
		if (mouseLocation.y > m_yOffset + (m_buttonSpacing * 2) && mouseLocation.y < m_yOffset + m_buttonHeight + (m_buttonSpacing * 2))
		{
			m_buttonSprite[2].setColor(sf::Color{ 0,50,0,255 });
			//m_text[2].setFillColor(sf::Color{ 75,75,75,255 });
			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
			{
				Game::currentState = GameState::Settings;
			}
		}
		if (mouseLocation.y > m_yOffset + (m_buttonSpacing * 3) && mouseLocation.y < m_yOffset + m_buttonHeight + (m_buttonSpacing * 3))
		{
			m_buttonSprite[3].setColor(sf::Color{ 0,50,0,255 });
			//m_text[2].setFillColor(sf::Color{ 75,75,75,255 });
			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
			{
				t_window.close();
			}
		}
	}
}

void MainMenu::render(sf::RenderWindow& t_window)
{
	t_window.setView(m_mainMenuView);
	t_window.draw(bg_s);
	for (int i = 0; i < m_buttonCount; i++)
	{
		t_window.draw(m_buttonSprite[i]);
		t_window.draw(m_text[i]);
	}
}

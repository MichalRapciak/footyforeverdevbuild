#include "GamemodeSelect.h"
#include "Game.h"
#include <iostream>

GamemodeSelect::GamemodeSelect() : m_menuView(sf::FloatRect({ 0,0 }, { 1920,1080 })), bg_s(bg_txt), m_titleText(m_font)
{
}

GamemodeSelect::~GamemodeSelect()
{
}

void GamemodeSelect::initialise(sf::Font& t_font)
{
	m_font = t_font;
	m_buttonWidth = 400;
	m_buttonHeight = 100;
	m_yOffset = 400;
	m_xOffset = (m_menuView.getSize().x / 2) - m_buttonWidth / 2;
	m_buttonSpacing = 120;
	int textDropOffset = 15;

	// Title Text Setup
	m_titleText.setFont(m_font);
	m_titleText.setString("SELECT GAME MODE");
	m_titleText.setFillColor(sf::Color::White);
	m_titleText.setCharacterSize(80);
	sf::FloatRect titleSize = m_titleText.getGlobalBounds();
	m_titleText.setPosition({ (m_menuView.getSize().x / 2.f) - (titleSize.size.x / 2.f), 150.f });

	sf::String m_Texts[] = { "Exhibition Match", "Cup Tournament", "Back to Menu" };

	if (!m_buttonTxt.loadFromFile("ASSETS/IMAGES/button.png"))
	{
		std::cout << "Can't load button texture\n";
	}

	sf::IntRect txtRect;
	txtRect.position = { 0,0 };
	txtRect.size = { static_cast<int>(m_buttonTxt.getSize().x) , static_cast<int>(m_buttonTxt.getSize().y) };

	m_buttonSprite.clear();
	m_text.clear();

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

	// Reuse the main menu background, or load a distinct one!
	if (!bg_txt.loadFromFile("ASSETS/IMAGES/help.png"))
	{
		std::cout << "couldn't load background\n";
	}
	bg_s.setTexture(bg_txt);
	bg_s.setTextureRect(sf::IntRect({ 0,0 }, { 1920,1080 }));
	bg_s.setPosition({ 0,0 });
}

void GamemodeSelect::processInput(sf::Event& t_event, sf::RenderWindow& t_window)
{
	if (const auto resized = t_event.getIf<sf::Event::Resized>())
	{
		sf::Vector2f visibleArea(sf::Vector2f(resized->size));
		m_menuView.setSize(visibleArea);
		m_xOffset = (m_menuView.getSize().x / 2) - m_buttonWidth / 2;
		m_yOffset = 400; // Resetting standard offset on resize

		// Re-center title
		sf::FloatRect titleSize = m_titleText.getGlobalBounds();
		m_titleText.setPosition({ (m_menuView.getSize().x / 2.f) - (titleSize.size.x / 2.f), 150.f });

		// Re-position buttons
		for (int i = 0; i < m_buttonCount; i++) {
			m_buttonSprite[i].setPosition({ m_xOffset, m_buttonSpacing * i + m_yOffset });
			sf::FloatRect textSize = m_text[i].getGlobalBounds();
			float textOffset = (m_buttonWidth - textSize.size.x) / (2);
			m_text[i].setPosition({ m_xOffset + textOffset, m_buttonSpacing * i + m_yOffset + 15 });
		}

		t_window.setView(m_menuView);
	}
}

void GamemodeSelect::update(sf::Time& t_deltaTime, sf::RenderWindow& t_window)
{
	if (m_clickCooldown > 0.f) m_clickCooldown -= t_deltaTime.asSeconds();

	sf::Vector2f mouseLocation;
	mouseLocation = t_window.mapPixelToCoords(sf::Mouse::getPosition(t_window), m_menuView);

	for (int i = 0; i < m_buttonCount; i++)
	{
		m_text[i].setFillColor(sf::Color::White);
		m_buttonSprite[i].setColor(sf::Color{ 0,100,0,255 });
	}

	if (mouseLocation.x > m_xOffset && mouseLocation.x < m_xOffset + m_buttonWidth)
	{
		// 1. EXHIBITION MATCH (MatchDay)
		if (mouseLocation.y > m_yOffset && mouseLocation.y < m_yOffset + m_buttonHeight)
		{
			m_buttonSprite[0].setColor(sf::Color{ 0,50,0,255 });
			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && m_clickCooldown <= 0.f)
			{
				m_clickCooldown = 0.2f;
				// Go straight to team select or match generation
				Game::currentState = GameState::MatchDay;
			}
		}
		// 2. CUP TOURNAMENT (Future Tournament Setup Menu)
		else if (mouseLocation.y > m_yOffset + m_buttonSpacing && mouseLocation.y < m_yOffset + m_buttonHeight + m_buttonSpacing)
		{
			m_buttonSprite[1].setColor(sf::Color{ 0,50,0,255 });
			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && m_clickCooldown <= 0.f)
			{
				m_clickCooldown = 0.2f;
				Game::currentState = GameState::TournamentSetup;
			}
		}
		// 3. BACK TO MAIN MENU
		else if (mouseLocation.y > m_yOffset + (m_buttonSpacing * 2) && mouseLocation.y < m_yOffset + m_buttonHeight + (m_buttonSpacing * 2))
		{
			m_buttonSprite[2].setColor(sf::Color{ 0,50,0,255 });
			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && m_clickCooldown <= 0.f)
			{
				m_clickCooldown = 0.2f;
				Game::currentState = GameState::MainMenu;
			}
		}
	}
}

void GamemodeSelect::render(sf::RenderWindow& t_window)
{
	t_window.setView(m_menuView);
	t_window.draw(bg_s);
	t_window.draw(m_titleText);

	for (int i = 0; i < m_buttonCount; i++)
	{
		t_window.draw(m_buttonSprite[i]);
		t_window.draw(m_text[i]);
	}
}
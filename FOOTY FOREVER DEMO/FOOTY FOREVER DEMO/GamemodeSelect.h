#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

class GamemodeSelect
{
public:
	GamemodeSelect();
	~GamemodeSelect();

	void initialise(sf::Font& t_font);
	void processInput(sf::Event& t_event, sf::RenderWindow& t_window);
	void update(sf::Time& t_deltaTime, sf::RenderWindow& t_window);
	void render(sf::RenderWindow& t_window);

protected:
	sf::View m_menuView;
	static const int m_buttonCount = 3; // Exhibition, Tournament, Back

	sf::Texture m_buttonTxt;
	std::vector<sf::Sprite> m_buttonSprite;
	std::vector<sf::Text> m_text;

	sf::Font m_font;
	sf::Sprite bg_s;
	sf::Texture bg_txt;

	// Menu title
	sf::Text m_titleText;

	float m_yOffset{ 0.0f };
	float m_xOffset{ 0.0f };
	float m_buttonSpacing{ 0.0f };
	float m_buttonWidth{ 0.0f };
	float m_buttonHeight{ 0.0f };

	// Simple cooldown to prevent double-clicking through menus instantly
	float m_clickCooldown{ 0.2f };
};
#pragma once
#include <SFML/Graphics.hpp>

class Help
{
public:
	Help();
	~Help();

	void initialise(sf::Font& t_font);
	void processInput(sf::Event& t_event);
	void update(sf::Time& t_deltatime);
	void render(sf::RenderWindow& t_window);

protected:
	sf::Text m_helpText;
	sf::Font m_font;
	bool m_pressedExit{ false };
};


#pragma once
#include <SFML/Graphics.hpp>
class SplashScreen
{
public:
	SplashScreen();
	~SplashScreen();

	void initialise(sf::Font& t_font);
	void update(sf::Time& t_deltaTime);
	void processInput(sf::Event& t_event);
	void render(sf::RenderWindow& t_window);

protected:
	sf::Font m_font;
	sf::Text m_splashText;

	bool m_anyKeyPressed{ false }; //boolean for splash screen to close

};



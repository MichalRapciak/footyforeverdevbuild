#include "Game.h"
#include <iostream>

GameState Game::currentState = GameState::License;

/// <summary>
/// default constructor
/// setup the window properties
/// load and setup the text 
/// load and setup the image
/// </summary>
Game::Game() :
	m_window{ sf::VideoMode({ 1920U, 1080U }), "FOOTY FOREVER DEMO"}, // , sf::State::Fullscreen
	m_exitGame{ false } //when true game will exit

{
	m_window.setVerticalSyncEnabled(true);
	initialiseStates();
}

/// <summary>
/// default destructor we didn't dynamically allocate anything
/// so we don't need to free it, but method needs to be here
/// </summary>
Game::~Game()
{
}

/// <summary>
/// main game loop
/// update 75 times per second,
/// process update as often as possible and at least 75 times per second
/// draw as often as possible but only updates are on time
/// if updates run slow then don't render frames
/// </summary>
void Game::run()
{
	sf::Clock gameClock;
	sf::Time timeSinceLastUpdate = sf::Time::Zero;
	const float fps{ 75.0f };
	sf::Time timePerFrame = sf::seconds(1.0f / fps); // 75 fps
	while (m_window.isOpen())
	{
		processEvents(); // as many as possible
		timeSinceLastUpdate += gameClock.restart();
		while (timeSinceLastUpdate > timePerFrame)
		{
			timeSinceLastUpdate -= timePerFrame;
			processEvents(); // at least 75 fps
			update(timePerFrame); //75 fps
		}
		render(); // as many as possible
	}
}
/// <summary>
/// Handles the keyboard events in current game state
/// </summary>
void Game::processEvents()
{
	while (auto event = m_window.pollEvent())
	{
		if (event->is<sf::Event::Closed>())
			m_window.close();
		if (const auto resized = event->getIf<sf::Event::Resized>()) //debugging to see if window resizing works
		{
			sf::FloatRect visibleArea({ 0.f,0.f }, sf::Vector2f(resized->size));
			m_window.setView(sf::View(visibleArea));
		}
		switch (currentState)
		{
		case GameState::None:
			break;
		case GameState::License:
			//no process events in license
			break;
		case GameState::Splash:
			m_splashScreen.processInput(*event);
			break;
		case GameState::MainMenu:
			m_mainMenuScreen.processInput(*event, m_window);
			break;
		case GameState::Help:
			m_helpScreen.processInput(*event);
			break;
		case GameState::GamePlay:
			m_gamingScreen->processEvents(*event, m_window);
			break;
		default:
			break;
		}
	}

}


/// <summary>
/// deal with key presses from the user
/// </summary>
/// <param name="t_event">key press event</param>
void Game::processKeys(sf::Event t_event)
{
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LAlt))
	{
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F4))
		{
			m_exitGame = true;
		}
	}
}

/// <summary>
/// Update the game world in current game state
/// </summary>
/// <param name="t_deltaTime">time interval per frame</param>
void Game::update(sf::Time t_deltaTime)
{
	if (m_exitGame)
	{
		m_window.close();
	}

	switch (currentState)
	{
	case GameState::None:
		break;
	case GameState::License:
		m_licenseScreen.update(t_deltaTime);
		break;
	case GameState::Splash:
		m_splashScreen.update(t_deltaTime);
		break;
	case GameState::MainMenu:
		m_mainMenuScreen.update(t_deltaTime, m_window);
		if (m_gamingScreen->getGameOver())
		{
			m_gamingScreen.reset();
			m_gamingScreen = std::make_unique<GamePlay>();
			m_gamingScreen->initialise(m_font);
		}
		if (m_gamingScreen->getGameWon())
		{
			m_gamingScreen.reset();
			m_gamingScreen = std::make_unique<GamePlay>();
			m_gamingScreen->initialise(m_font);
		}
		m_gamingScreen.reset();
		m_gamingScreen = std::make_unique<GamePlay>();
		m_gamingScreen->initialise(m_font);
		break;
	case GameState::Help:
		m_helpScreen.update(t_deltaTime);
		break;
	case GameState::GamePlay:
		m_gamingScreen->update(t_deltaTime, m_window);
		break;
	default:
		break;
	}
}

/// <summary>
/// draw the frame and then switch buffers in current game state
/// </summary>
void Game::render()
{
	m_window.clear(sf::Color::Black);
	switch (currentState)
	{
	case GameState::None:
		break;
	case GameState::License:
		m_licenseScreen.render(m_window);
		break;
	case GameState::Splash:
		m_splashScreen.render(m_window);
		break;
	case GameState::MainMenu:
		m_mainMenuScreen.render(m_window);
		break;
	case GameState::Help:
		m_helpScreen.render(m_window);
		break;
	case GameState::GamePlay:
		m_gamingScreen->render(m_window);
		break;
	default:
		break;
	}
	m_window.display();
}

/// <summary>
/// initialise the font in all game states
/// </summary>
void Game::initialiseStates()
{
	if (!m_font.openFromFile("ASSETS\\FONTS\\ariblk.ttf"))
	{
		std::cout << "problem loading arial black font" << std::endl;
	}
	m_licenseScreen.initialise(m_font);
	m_splashScreen.initialise(m_font);
	m_mainMenuScreen.initialise(m_font);
	m_helpScreen.initialise(m_font);
	m_gamingScreen = std::make_unique<GamePlay>();
	m_gamingScreen->initialise(m_font);
}


#ifndef GAME_HPP
#define GAME_HPP
#include <SFML/Graphics.hpp>
#include "GamePlay.h"
#include "LicenseScreen.h"
#include "SplashScreen.h"
#include "MainMenu.h"
#include "Help.h"
#include "EditorScreen.h" // <-- NEW
#include "GameDatabase.h"
#include "MatchDayScreen.h"
#include "SettingsScreen.h"
#include "MatchIntroState.h"

enum class
	GameState
{
	None,
	License,
	Splash,
	MainMenu,
	Help,
	GamePlay,
	Editor,
	MatchDay,
	Settings,
	MatchIntro
};

class Game
{

public:
	Game();
	~Game();
	/// <summary>
	/// main method for game
	/// </summary>
	void run();
	static GameState currentState;

private:
	GameDatabase m_database;

	// ==========================================
	// --- NEW: ASYNC LOADING LOCKS ---
	// ==========================================
	float m_introProgress = 0.0f;
	bool m_introReadyToLoad = true;

	void processEvents();
	void processKeys(sf::Event t_event);
	void update(sf::Time t_deltaTime);
	void render();

	void initialiseStates();
	void ApplyBrazilTheme();

	sf::Font m_font;
	sf::RenderWindow m_window; // main SFML window

	LicenseScreen m_licenseScreen;
	SplashScreen m_splashScreen;
	MainMenu m_mainMenuScreen;
	Help m_helpScreen;
	EditorScreen m_editorScreen;
	MatchDayScreen m_matchDayScreen;
	SettingsState m_settingsScreen;
	MatchIntroState m_matchIntroScreen;
	std::unique_ptr<GamePlay> m_gamingScreen;

	bool m_exitGame; // control exiting game

};

#endif // !GAME_HPP
#ifndef GAME_HPP
#define GAME_HPP
#include <SFML/Graphics.hpp>
#include "MatchEngine.h"
#include "LicenseScreen.h"
#include "SplashScreen.h"
#include "MainMenu.h"
#include "EditorScreen.h" // <-- NEW
#include "GameDatabase.h"
#include "MatchDayScreen.h"
#include "SettingsScreen.h"
#include "MatchIntroState.h"
#include "GamemodeSelect.h"
#include "TournamentSetup.h"
#include "TournamentHub.h"

enum class
	GameState
{
	None,
	License,
	Splash,
	MainMenu,
	Editor,
	Settings,
	GamemodeSelect,
	TournamentSetup,
	TournamentHub,
	MatchDay,
	MatchIntro,
	MatchEngine
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
	GamemodeSelect m_gamemodeSelectScreen;
	EditorScreen m_editorScreen;
	MatchDayScreen m_matchDayScreen;
	SettingsState m_settingsScreen;
	MatchIntroState m_matchIntroScreen;
	TournamentSetup m_tourSetupScreen;
	TournamentHub m_tourHubScreen;
	std::unique_ptr<MatchEngine> m_matchEngineScreen;

	bool m_exitGame; // control exiting game

};

#endif // !GAME_HPP
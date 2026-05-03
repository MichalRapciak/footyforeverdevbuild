#include "Game.h"
#include "imgui-1.92.6/imgui.h"
#include "imgui-1.92.6/imgui-sfml.h"
#include <iostream>
#include "GlobalSettings.h"

GameState Game::currentState = GameState::License;

/// <summary>
/// default constructor
/// setup the window properties
/// load and setup the text 
/// load and setup the image
/// </summary>
// 1. Remove m_window from the initializer list!
Game::Game() :
	m_exitGame{ false }
{
	// 2. Load the settings from disk BEFORE we create the window
	GlobalSettings::load();

	// 3. Create the window dynamically based on the saved settings (SFML 3 syntax)
	auto windowState = GlobalSettings::isFullscreen ? sf::State::Fullscreen : sf::State::Windowed;

	m_window.create(
		sf::VideoMode({ static_cast<unsigned int>(GlobalSettings::windowWidth),
						static_cast<unsigned int>(GlobalSettings::windowHeight) }),
		"FOOTY FOREVER DEV BUILD",
		windowState
	);

	m_window.setVerticalSyncEnabled(true);
	initialiseStates();
}

/// <summary>
/// default destructor we didn't dynamically allocate anything
/// so we don't need to free it, but method needs to be here
/// </summary>
Game::~Game()
{
    ImGui::SFML::Shutdown();
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

	while (m_window.isOpen())
	{
		float fps = static_cast<float>(GlobalSettings::targetFPS);
		sf::Time timePerFrame = sf::seconds(1.0f / fps);

		processEvents(); 
		timeSinceLastUpdate += gameClock.restart();

		// ==========================================
		// --- THE FIX: ANTI SPIRAL-OF-DEATH CLAMP ---
		// ==========================================
		// If a frame takes longer than 250ms (like baking a giant texture), 
		// throw away the accumulated time so the engine doesn't panic and 
		// try to run update() 20 times in a row!
		if (timeSinceLastUpdate > sf::seconds(0.25f)) {
			timeSinceLastUpdate = timePerFrame; 
		}

		while (timeSinceLastUpdate > timePerFrame)
		{
			timeSinceLastUpdate -= timePerFrame;
			processEvents();
			update(timePerFrame);
		}
		render();
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
		case GameState::GamemodeSelect:
			m_gamemodeSelectScreen.processInput(*event, m_window);
			break;
		case GameState::TournamentSetup:
			ImGui::SFML::ProcessEvent(m_window, *event);
			break;
		case GameState::TournamentHub:
			ImGui::SFML::ProcessEvent(m_window, *event);
			break;
		case GameState::MatchEngine:
			ImGui::SFML::ProcessEvent(m_window, *event);
			m_matchEngineScreen->processEvents(*event, m_window);
			break;
		case GameState::Editor:
			ImGui::SFML::ProcessEvent(m_window, *event);
			m_editorScreen.processEvents(*event);
			break;
		case GameState::MatchDay:
			ImGui::SFML::ProcessEvent(m_window, *event);
			break;
		case GameState::MatchIntro:
			break;
		case GameState::Settings:
			ImGui::SFML::ProcessEvent(m_window, *event);
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
		break;
	case GameState::GamemodeSelect:
		m_gamemodeSelectScreen.update(t_deltaTime, m_window);
		if (currentState == GameState::MainMenu)
		{
			m_mainMenuScreen.setClickCooldown(1.5f);
		}
		if (currentState == GameState::MatchDay)
		{
			m_matchDayScreen.init(m_font, m_database); // Drops them in with default values, unlocked
		}
		break;
	case GameState::TournamentSetup:
		ImGui::SFML::Update(m_window, t_deltaTime);
		m_tourSetupScreen.update(t_deltaTime, m_window);
		if (currentState == GameState::TournamentHub)
		{
			m_tourHubScreen.init(m_font, m_database, m_tourSetupScreen.getGeneratedBracket(), m_tourSetupScreen.getUserTeamId());
		}
		break;
	case GameState::TournamentHub:
		ImGui::SFML::Update(m_window, t_deltaTime);
		m_tourHubScreen.update(t_deltaTime, m_window);
		if (currentState == GameState::MatchDay)
		{
			m_matchDayScreen.init(
				m_font,
				m_database,
				m_tourHubScreen.getCurrentMatchHomeId(),  // The exact Home team ID from the bracket
				m_tourHubScreen.getCurrentMatchAwayId(),  // The exact Away team ID from the bracket
				m_tourHubScreen.getUserTeamId(),          // The ID of the team the user picked
				true                                      // Yes, this is a tournament!
			);
		}
		break;
	case GameState::MatchEngine:
		ImGui::SFML::Update(m_window, t_deltaTime);
		m_matchEngineScreen->update(t_deltaTime, m_window);

		if (m_matchEngineScreen->isExitRequested())
		{
			if (m_matchEngineScreen->isMatchFinished())
			{
				MatchInfo result = m_matchEngineScreen->getMatchInfo();

				if (m_matchDayScreen.isTournamentMode()) {
					// 1. Log the stats to the transient tournament DB
					m_database.processMatchResult(result, m_tourHubScreen.getActiveCompId());

					// 2. Advance the bracket and auto-simulate AI games
					m_tourHubScreen.advanceTournament(result);

					currentState = GameState::TournamentHub;
				}
				else {
					// Standard matchday, don't pass a comp ID
					m_database.processMatchResult(result, "");
					currentState = GameState::MainMenu;
				}
			}
			else
			{
				// Match Aborted
				if (m_matchDayScreen.isTournamentMode()) currentState = GameState::TournamentHub;
				else currentState = GameState::MatchDay;
			}
		}
		break;
	case GameState::Editor:
		if (currentState == GameState::MainMenu)
		{
			m_mainMenuScreen.setClickCooldown(1.5f);
		}
		ImGui::SFML::Update(m_window, t_deltaTime);
		m_editorScreen.update(t_deltaTime, m_window);
		break;
	case GameState::MatchDay:
		if (currentState == GameState::MainMenu)
		{
			m_mainMenuScreen.setClickCooldown(1.5f);
		}
		ImGui::SFML::Update(m_window, t_deltaTime);
		m_matchDayScreen.update(t_deltaTime, m_window);

		if (currentState == GameState::MatchIntro)
		{
			m_matchEngineScreen.reset();
			m_matchEngineScreen = std::make_unique<MatchEngine>();
			m_matchEngineScreen->initialise(m_font);

			m_matchEngineScreen->beginMatchSetup(m_database, m_matchDayScreen.getHomeTeamId(), m_matchDayScreen.getAwayTeamId(), m_matchDayScreen.getUserPlayerId());
			m_matchIntroScreen.init(m_font, m_database, m_matchDayScreen.getHomeTeamId(), m_matchDayScreen.getAwayTeamId(), m_matchDayScreen.getUserPlayerId());

			// Reset our locks for the new match!
			m_introProgress = 0.0f;
			m_introReadyToLoad = true;
		}
		break;

	case GameState::MatchIntro:
	{
		// 1. Only process a heavy load if the Render loop gave us permission!
		if (m_introReadyToLoad) {
			m_introProgress = m_matchEngineScreen->loadNextPlayer();

			// Lock the door! No more players can be loaded until the screen draws.
			m_introReadyToLoad = false;
		}

		// 2. We still update the visual UI bar every tick
		m_matchIntroScreen.update(t_deltaTime, m_window, m_introProgress);

		// 3. Transition when complete
		if (m_introProgress >= 1.0f)
		{
			m_matchEngineScreen->finalizeMatchSetup();
			currentState = GameState::MatchEngine;
		}
		break;
	}
	case GameState::Settings:
		if (currentState == GameState::MainMenu)
		{
			m_mainMenuScreen.setClickCooldown(1.5f);
		}
		ImGui::SFML::Update(m_window, t_deltaTime);
		m_settingsScreen.update(m_window);
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
	case GameState::GamemodeSelect:
		m_gamemodeSelectScreen.render(m_window);
		break;
	case GameState::TournamentSetup:
		m_tourSetupScreen.render(m_window);
		ImGui::SFML::Render(m_window);
		break;
	case GameState::TournamentHub:
		m_tourHubScreen.render(m_window);
		ImGui::SFML::Render(m_window);
		break;
	case GameState::MatchEngine:
		m_matchEngineScreen->render(m_window);
		ImGui::SFML::Render(m_window);
		break;
	case GameState::Editor:
		m_editorScreen.render(m_window);
		ImGui::SFML::Render(m_window);
		break;
	case GameState::MatchDay:
		m_matchDayScreen.render(m_window);
		ImGui::SFML::Render(m_window);
		break;
	case GameState::MatchIntro:
		m_matchIntroScreen.render(m_window);

		// ==========================================
		// --- THE FIX: UNLOCK THE NEXT PLAYER ---
		// ==========================================
		// The frame has officially been painted to the monitor. 
		// It is now safe to bake the next texture!
		m_introReadyToLoad = true;
		break;
	case GameState::Settings:
		m_settingsScreen.render(m_window);
		ImGui::SFML::Render(m_window);
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
    ImGui::SFML::Init(m_window);
	ApplyBrazilTheme();
	m_database.loadDatabase("ASSETS/DATA");
	if (!m_font.openFromFile("ASSETS\\FONTS\\agencyr.ttf"))
	{
		std::cout << "problem loading agency r  font" << std::endl;
	}
	m_editorScreen.init(m_font, m_database);
	m_licenseScreen.initialise(m_font);
	m_splashScreen.initialise(m_font);
	m_mainMenuScreen.initialise(m_font);
	m_tourSetupScreen.init(m_font, m_database);
	m_gamemodeSelectScreen.initialise(m_font);
	m_settingsScreen.init(m_window);
}

void Game::ApplyBrazilTheme()
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	// --- FRAMES (Input boxes, Dropdowns, Slider tracks) ---
	// Faint Deep Blue
	colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.15f, 0.25f, 0.5f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.1f, 0.25f, 0.35f, 0.7f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.35f, 0.45f, 0.8f);

	// --- BUTTONS ---
	// Muted Forest Green -> Gold when clicked
	colors[ImGuiCol_Button] = ImVec4(0.1f, 0.35f, 0.2f, 0.7f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.15f, 0.45f, 0.25f, 0.8f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.6f, 0.5f, 0.1f, 0.9f);

	// --- HEADERS (Selectable items in your lists, TreeNodes) ---
	// Same as buttons: Green transitioning to Gold
	colors[ImGuiCol_Header] = ImVec4(0.1f, 0.35f, 0.2f, 0.5f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.15f, 0.45f, 0.25f, 0.7f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.6f, 0.5f, 0.1f, 0.8f);

	// --- TABS ---
	colors[ImGuiCol_Tab] = ImVec4(0.05f, 0.15f, 0.25f, 0.7f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.1f, 0.4f, 0.2f, 0.8f);
	colors[ImGuiCol_TabActive] = ImVec4(0.6f, 0.5f, 0.1f, 0.9f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.05f, 0.15f, 0.25f, 0.7f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.4f, 0.35f, 0.1f, 0.8f);

	// --- SLIDER GRABS (The little handle you drag) ---
	// Soft Gold/Yellow so it pops against the dark background
	colors[ImGuiCol_SliderGrab] = ImVec4(0.6f, 0.5f, 0.1f, 0.8f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.8f, 0.7f, 0.1f, 0.9f);

	// --- TITLE BARS (If you ever enable window dragging/titles) ---
	colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.15f, 0.25f, 0.8f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.1f, 0.35f, 0.2f, 0.8f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.15f, 0.25f, 0.5f);
}


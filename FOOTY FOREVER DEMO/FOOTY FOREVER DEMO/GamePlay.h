#pragma once
#include <SFML/Graphics.hpp>
#include "Entity.h"
#include "UserPlayer.h"
#include "UserController.h"
#include "Stadium.h"
#include "Ball.h"
#include "NPCPlayer.h"
#include "NPCController.h"
#include "Pitch.h"
#include "MatchReferee.h"
#include <memory>
#include "AnimationServer.h"
#include "GameDatabase.h"
#include "ReplayEngine.h"
#include "PhysicsEngine.h"
#include "TeamAI.h"
#include "SoundManager.h"
#include "SpatialGrid.h"
#include "MatchStatistics.h"
#include "MatchInfo.h"
#include <algorithm>

class GamePlay
{
	public:
		GamePlay(); // main game function
		~GamePlay();

		void processEvents(sf::Event& t_event, sf::RenderWindow& t_window);
		void processKeys(sf::Event t_event);
		void update(sf::Time& t_deltaTime, sf::RenderWindow& t_window);
		void render(sf::RenderWindow& t_window);
		void initialise(sf::Font& t_font);
		void powerBarUpdate();
		void powerBarDraw(sf::RenderWindow& t_window);
		void updateCamera(sf::RenderWindow& t_window);
		void drawUI(sf::RenderWindow& t_window);

		bool getGameOver() { return m_gameOver; }
		bool getGameWon() { return m_gameWon; }
		float distance(sf::Vector2f a, sf::Vector2f b);
		sf::Vector2f normalize(sf::Vector2f source);

		void runStandardSystems(float dt, sf::RenderWindow& t_window);
		Player* findFirstResponder(const std::vector<Player*>& t_team);
		UserPlayer* getUserPlayer() { return m_userPlayer.get(); }

		void executePlayerSwitch(Player* targetNPC);

		void beginMatchSetup(GameDatabase& db, const std::string& homeTeamId, const std::string& awayTeamId, const std::string& userPlayerId);
		float loadNextPlayer(); // Returns progress from 0.0 to 1.0
		void finalizeMatchSetup();

		std::unique_ptr<Ball> m_ball;
		Pitch m_pitch;
		Goal m_homeGoal;
		Goal m_awayGoal;
		MatchReferee m_referee;
		SoundManager m_soundManager;
		std::unique_ptr<TeamAI> m_homeTeamAI;
		std::unique_ptr<TeamAI> m_awayTeamAI;
		MatchInfo m_matchInfo;

		// --- The Bodies ---
		// We use a vector of unique_ptrs so they are cleaned up automatically
		std::vector<std::unique_ptr<NPCPlayer>> m_homeside;
		std::vector<std::unique_ptr<NPCPlayer>> m_awayside;

		std::vector<Player*> m_homeTeam; // <-- NEW
		std::vector<Player*> m_awayTeam; // <-- NEW

		MatchStatistics m_matchStats;

	protected:
		sf::Font m_font;
		sf::Text m_pauseText;
		sf::Text m_gameOverText;
		sf::Text m_gameWonText;
		SpatialGrid m_spatialGrid;

	private:
		sf::Shader m_kitShader;

		int m_loadingPhase = 0; // 0 = Idle, 1 = Home Team, 2 = Away Team, 3 = Done
		size_t m_loadingIndex = 0;
		std::string m_setupUserPlayerId;
		bool m_userAssigned = false;
		GameDatabase* m_db;
		TeamData m_homeTeamData;
		TeamData m_awayTeamData;

		bool m_pause = false;
		bool m_gameOver = false;
		bool m_gameWon = false;

		sf::Vector2f mouseWorld = { 0,0 };
		sf::View m_playerCam; // Player-centered camera

		int m_homeSubsUsed = 0;
		int m_awaySubsUsed = 0;
		const int MAX_SUBS = 5;
		std::vector<std::string> m_homeSubbedOutIds;
		std::vector<std::string> m_awaySubbedOutIds;

		void performSubstitution(Team team, int pitchIndex, int benchIndex);
		void handleAISubstitutions();

		struct SubEvent {
			Team team; // <-- NEW: Used for sorting!
			float timer = 0.f;
			std::string teamName;
			sf::Color teamColor;
			std::string playerOff;
			std::string playerOn;
			int numOff;
			int numOn;
		};

		// THE FIX: Now it's a list that can hold multiple events at once!
		std::vector<SubEvent> m_activeSubEvents;

		struct PendingSub {
			Team team;
			int pitchIndex;
			int benchIndex;
		};
		std::vector<PendingSub> m_pendingSubsQueue;

		void queueSubstitution(Team team, int pitchIndex, int benchIndex);

	sf::RectangleShape barBackground;
	sf::RectangleShape barFill;
	sf::Vector2f barSize = { 200.f, 20.f };

	Stadium m_stadium1;
	std::unique_ptr<UserPlayer> m_userPlayer;
	std::unique_ptr<UserController> m_userController;

	// --- Pause Menu Buttons ---
	sf::Texture m_buttonTxt;
	std::vector<sf::Sprite> m_pauseButtons;
	std::vector<sf::Text> m_pauseTexts;

	float m_btnWidth = 400.f;
	float m_btnHeight = 100.f;
	float m_btnSpacing = 120.f;

	// --- Game Plan State ---
	bool m_showGamePlan = false;

	// Helper functions
	void initPauseMenuButtons();
	void updatePauseMenu(sf::RenderWindow& t_window);
	void drawGamePlan(sf::RenderWindow& t_window);

	void drawMatchStats(sf::RenderWindow& t_window);

	std::unique_ptr<NPCController> m_npcController;

	std::vector<Entity*> m_entities;

	ReplayEngine m_replayEngine;
	std::vector<Player*> m_allActivePlayers; // Helper list to feed the recorder
	void renderPlayerEntity(sf::RenderWindow& t_window, Entity* entity);

	void triggerForfeit(bool isHomeForfeit);

	// Pitch Constants (100px = 1m)
	const float m_pitchWidth = 10000.f;
	const float m_pitchHeight = 7000.f;
	const float m_goalCenterY = 3500.f;
	const float m_margin = 500.f; // Buffer from the screen edge
	float minScale = 1.f;
	float maxScale = 1.8f;

	void drawDebugOffsideLines(sf::RenderWindow& window);
	void drawDebugNames(sf::RenderWindow& window, const sf::Font& font);
	void drawDebugHitboxes(sf::RenderWindow& window);
	void drawPassDebug(sf::RenderWindow& t_window);

};
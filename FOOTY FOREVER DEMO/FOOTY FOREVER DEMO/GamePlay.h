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
		void handlePlayerCollisions(std::vector<Player*>& players, const Pitch& pitch);
		void handleBallPlayerPhysics(std::vector<Player*>& players, Ball& ball);
		void updateCamera(sf::RenderWindow& t_window);
		void drawUI(sf::RenderWindow& t_window);

		bool getGameOver() { return m_gameOver; }
		bool getGameWon() { return m_gameWon; }
		float distance(sf::Vector2f a, sf::Vector2f b);
		sf::Vector2f normalize(sf::Vector2f source);

		void handleGoalPhysics(Goal& goal);
		void resolvePlayerGoalCollision(Player& player, Goal& goal);

		void runStandardSystems(float dt, sf::RenderWindow& t_window);
		Player* findFirstResponder(const std::vector<Player*>& t_team);
		UserPlayer* getUserPlayer() { return m_userPlayer.get(); }

		void setupMatch(GameDatabase& db, const std::string& homeTeamId, const std::string& awayTeamId, const std::string& userPlayerId);

		std::unique_ptr<Ball> m_ball;
		Pitch m_pitch;
		Goal m_homeGoal;
		Goal m_awayGoal;
		MatchReferee m_referee;

		// --- The Bodies ---
// We use a vector of unique_ptrs so they are cleaned up automatically
		std::vector<std::unique_ptr<NPCPlayer>> m_teammates;
		std::vector<std::unique_ptr<NPCPlayer>> m_opponents;

	protected:
		sf::Font m_font;
		sf::Text m_pauseText;
		sf::Text m_gameOverText;
		sf::Text m_gameWonText;
		AnimationServer m_animServer;

	private:
		GameDatabase* m_db;
		TeamData m_homeTeamData;
		TeamData m_awayTeamData;

		bool m_pause = false;
		bool m_gameOver = false;
		bool m_gameWon = false;

		sf::Vector2f mouseWorld = { 0,0 };
		sf::View m_playerCam; // Player-centered camera

	sf::RectangleShape barBackground;
	sf::RectangleShape barFill;
	sf::Vector2f barSize = { 200.f, 20.f };

	Stadium m_stadium1;

	std::unique_ptr<UserPlayer> m_userPlayer;
	std::unique_ptr<UserController> m_userController;



	std::unique_ptr<NPCController> m_npcController;

	std::vector<Entity*> m_entities;

	ReplayEngine m_replayEngine;
	std::vector<Player*> m_allActivePlayers; // Helper list to feed the recorder
	void refreshEntities();
	void spawnTeamDynamic(std::vector<std::unique_ptr<NPCPlayer>>& team, std::vector<Entity*>& entities, TeamData& teamData, bool isHomeSide, const std::string& userPlayerId);
	void renderPlayerEntity(sf::RenderWindow& t_window, Entity* entity);

	void triggerForfeit(bool isHomeForfeit);

	// Pitch Constants (100px = 1m)
	const float m_pitchWidth = 10000.f;
	const float m_pitchHeight = 7000.f;
	const float m_goalCenterY = 3500.f;
	const float m_margin = 500.f; // Buffer from the screen edge
	float minScale = 1.f;
	float maxScale = 1.8f;
};
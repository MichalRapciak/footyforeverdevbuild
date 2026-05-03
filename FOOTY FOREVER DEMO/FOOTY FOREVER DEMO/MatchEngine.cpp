#include "MatchEngine.h"
#include "Game.h"
#include "imgui-1.92.6/imgui.h"
#include "GlobalSettings.h"
// for debug line
#include "PlayerAI.h"
#include "PositioningAI.h"

MatchEngine::MatchEngine() :  m_gameOverText(m_font), m_gameWonText(m_font), m_warningText(m_font)
{
}

MatchEngine::~MatchEngine()
{
}

/// <summary>
/// Initialise text
/// </summary>
/// <param name="t_font"></param>
void MatchEngine::initialise(sf::Font& t_font)
{
	m_font = t_font;
	sf::FloatRect textSize;
	float xpos = (1920 / 2) - (textSize.size.x / 2);

	m_gameOverText.setFont(m_font); // Text seen on the screen
	m_gameOverText.setString("");
	m_gameOverText.setCharacterSize(42);
	m_gameOverText.setFillColor(sf::Color::Red);
	m_gameOverText.setStyle(sf::Text::Bold);
	textSize = m_gameOverText.getGlobalBounds(); // will be used to put the text in the middle
	xpos = (1920 / 2) - (textSize.size.x / 2);
	m_gameOverText.setPosition({ xpos, 1080 * 0.5f });

	m_warningText.setFont(m_font);
	m_warningText.setString("Warning: Match progress will not be saved if you leave now!");
	m_warningText.setCharacterSize(24);
	m_warningText.setFillColor(sf::Color(255, 100, 100)); // Light red

	m_gameWonText.setFont(m_font); // Text seen on the screen
	m_gameWonText.setString("");
	m_gameWonText.setCharacterSize(42);
	m_gameWonText.setFillColor(sf::Color::Red);
	m_gameWonText.setStyle(sf::Text::Bold);
	textSize = m_gameWonText.getGlobalBounds(); // will be used to put the text in the middle
	xpos = (1920 / 2) - (textSize.size.x / 2);
	m_gameWonText.setPosition({ xpos, 1080 * 0.5f });

	initPauseMenuButtons();

	// ==========================================
	// --- NEW: LOAD GRAPHICS TO VRAM ---
	// ==========================================
	if (!m_kitShader.loadFromFile("ASSETS/SHADERS/kit_mixer.frag", sf::Shader::Type::Fragment)) {
		std::cout << "Failed to load kit shader!\n";
	}

	// Boot up the Flyweight Animation Server to load the 5 master templates!
	AnimationServer::init();
}

/// <summary>
/// handle user and system events / inputs
/// get key pressed, mouse moves etc. from OS
/// </summary>
void MatchEngine::processEvents(sf::Event& t_event, sf::RenderWindow& t_window)
{
	if (const auto resized = t_event.getIf<sf::Event::Resized>()) //debugging to see if window resizing works
	{
		sf::Vector2f visibleArea(sf::Vector2f(resized->size));
		m_playerCam.setSize(visibleArea);
	}
	if (const auto keyPressed = t_event.getIf<sf::Event::KeyPressed>()) //user pressed a key
	{
		processKeys(t_event);
		if (!m_pause && m_userPlayer)
			m_userController->inputHandler(t_event);
	}
	if (const auto keyReleased = t_event.getIf<sf::Event::KeyReleased>())
	{
		if (!m_pause && m_userPlayer)
			m_userController->inputHandler(t_event);
	}
	if (const auto buttonPressed = t_event.getIf<sf::Event::MouseButtonPressed>())
	{
		if (!m_pause && m_userPlayer)
			m_userController->inputHandler(t_event);
	}
	if (const auto buttonReleased = t_event.getIf<sf::Event::MouseButtonReleased>())
	{
		if (!m_pause && m_userPlayer)
			m_userController->inputHandler(t_event);
	}

}

/// <summary>
/// This function processes all keyboard presses and performs the correct action
/// </summary>
/// <param name="t_event">key press event</param>
void MatchEngine::processKeys(sf::Event t_event)
{
if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape))
    {
        if (!m_pause && !m_gameOver) m_pause = true;
        else if (m_pause) m_pause = false;
    }

    // THE FIX: Trigger the exit flag instead of directly changing Game::currentState
    if (m_pause || m_gameOver || m_gameWon)
    {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
        {
            m_exitRequested = true;
        }
    }
	if (m_pause)
	{
	}
	if (m_gameOver)
	{
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
		{
			Game::currentState = GameState::MainMenu;
		}
	}
	if (m_gameWon)
	{
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
		{
			Game::currentState = GameState::MainMenu;
		}
	}
	if (m_pause)
	{
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
		{
			Game::currentState = GameState::MainMenu;
		}
	}
}

/// <summary>
/// Updates the Game World
/// </summary>
/// <param name="t_deltaTime">time interval per frame</param>
void MatchEngine::update(sf::Time& t_deltaTime, sf::RenderWindow& t_window)
{
	float dt = t_deltaTime.asSeconds();

	if (m_pause || m_referee.getMatchState() == MatchState::HalfTime || m_referee.getMatchState() == MatchState::FullTime)
	{
		updatePauseMenu(t_window); // This checks for clicks and sets m_showGamePlan = true

		// THE FIX: Construct the ImGui window during the UPDATE tick!
		if (m_showGamePlan) {
			drawGamePlan(t_window);
		}
	}

	// ==========================================
		// 1. THE SMOKE AND MIRRORS REPLAY MEDIATOR
		// ==========================================
	if (m_referee.getMatchState() == MatchState::RequestReplay)
	{
		if (m_referee.getLastInfraction() == FoulType::Offside) {
			float defLine = m_referee.getOffsideDefensiveLineX();
			sf::Vector2f attPos = m_referee.getOffsideAttackerPos();

			m_replayEngine.startOffsideReplay(0.7f, defLine, attPos);

			// THE FIX 3: Clear the infraction so the next Goal doesn't trigger VAR!
			m_referee.clearLastInfraction();
		}
		else {
			m_replayEngine.startReplay(0.7f);
		}

		std::vector<Player*> allPlayers;
		if (m_userPlayer)
		{
			allPlayers.push_back(m_userPlayer.get());
		}
		for (auto& tm : m_homeside) allPlayers.push_back(tm.get());
		for (auto& opp : m_awayside) allPlayers.push_back(opp.get());

		// Instantly teleport everyone, but hold the state in ReplayPlaying
		m_referee.setupReplayTeleports(*m_ball, m_pitch, allPlayers, m_soundManager);
	}

	// ==========================================
	// 2. REPLAY INTERCEPTOR
	// ==========================================
	if (m_replayEngine.isReplaying()) {
		m_replayEngine.update(dt);
		return; // SKIP EVERYTHING ELSE!
	}
	// --- NEW: REPLAY JUST FINISHED ---
	else if (m_referee.getMatchState() == MatchState::ReplayPlaying)
	{
		// THE FIX 3: Gather the players to repair the pitch layout
		std::vector<Player*> allPlayers;
		if (m_userPlayer) allPlayers.push_back(m_userPlayer.get());
		for (auto& tm : m_homeside) allPlayers.push_back(tm.get());
		for (auto& opp : m_awayside) allPlayers.push_back(opp.get());

		// The isReplaying() check just became false this exact frame.
		// Repair the physics objects, arrange the set piece, and resume!
		m_referee.resumeFromReplay(*m_ball, m_pitch, allPlayers, m_soundManager);
	}

	// 1. Gather all players
	std::vector<Player*> homeFriends;
	
	if (m_userPlayer) if (m_userPlayer->getTeam() == Team::Home) homeFriends.push_back(m_userPlayer.get());
	for (auto& npc : m_homeside) homeFriends.push_back(npc.get());

	std::vector<Player*> homeEnemies;
	if (m_userPlayer) if (m_userPlayer->getTeam() == Team::Away) homeEnemies.push_back(m_userPlayer.get());
	for (auto& opp : m_awayside) homeEnemies.push_back(opp.get());

	std::vector<Player*> allPlayers = homeFriends;
	allPlayers.insert(allPlayers.end(), homeEnemies.begin(), homeEnemies.end());

	// ==========================================
	// --- THE FIX: PACK THE ENVIRONMENT ---
	// ==========================================
	MatchEnvironment env;
	env.ball = m_ball.get();
	env.pitch = &m_pitch;
	env.homeGoal = &m_homeGoal;
	env.awayGoal = &m_awayGoal;
	env.sound = &m_soundManager;
	env.stats = &m_matchStats;
	env.info = &m_matchInfo;
	env.referee = &m_referee;
	env.grid = &m_spatialGrid;
	env.allPlayers = &allPlayers;

	// Team AI Updates (Just swap the opposition pointer!)
	env.opposition = &homeEnemies;
	m_homeTeamAI->update(dt, env);

	env.opposition = &homeFriends;
	m_awayTeamAI->update(dt, env);

	if (!m_pause && !m_gameOver)
	{
		if (m_ball->getOwner()) {
			m_matchStats.updatePossession(m_ball->getOwner()->getTeam(), dt);
		}

		if (m_ball->passCompletedEvent && m_ball->getOwner()) {
			m_matchStats.recordPassComplete(m_ball->getOwner()->getTeam());
			m_ball->passCompletedEvent = false;
		}

		// 2. THE REFEREE UPDATES THE CONTEXTS FIRST
		m_referee.update(dt, env); // <--- Beautiful!
		handleAISubstitutions();
		m_referee.checkOffsideLogic(m_homeTeamAI->getOffsideLineX(), m_awayTeamAI->getOffsideLineX(), env); // <--- Beautiful!

		// 3. RUN THE SIMULATION
		runStandardSystems(dt, t_window);

		// 4. Update visuals
		updateCamera(t_window);
		if (m_userPlayer) powerBarUpdate();
	}

	// Inside MatchEngine::update, after the referee updates...
	if (m_referee.getMatchState() == MatchState::FullTime && !m_matchLogged) {

		int finalMinute = static_cast<int>(m_referee.getMatchMinute());

		// Cap off all active players!
		for (Player* p : homeFriends) {
			m_matchInfo.recordAppearanceEnd(p->getId(), finalMinute);
		}
		for (Player* p : homeEnemies) {
			m_matchInfo.recordAppearanceEnd(p->getId(), finalMinute);
		}

		m_matchInfo.calculateFinalRatings(allPlayers);

		m_matchLogged = true;
		std::cout << "MATCH LOGGED! Final Score: " << m_matchInfo.getHomeScore() << " - " << m_matchInfo.getAwayScore() << "\n";
		m_gameOver = true;
	}

	// ==========================================
	// --- NEW: DVR POST-ROLL MANAGER ---
	// ==========================================
	MatchState currentState = m_referee.getMatchState();

	// 1. Lock the DVR if an event just happened. 
	// It will record for exactly 1 more second (60 frames), then freeze completely.
	if (currentState == MatchState::GoalScored ||
		currentState == MatchState::FoulDelay ||
		currentState == MatchState::OutOfBoundsDelay)
	{
		m_replayEngine.lockRecording(60);
	}
	// 2. CRITICAL FAILSAFE: If the random 30% chance fails and we skip the replay, 
	// we MUST unlock the DVR the moment the new play starts so it can record again!
	else if (currentState == MatchState::InPlay ||
		currentState == MatchState::KickOff ||
		currentState == MatchState::FreeKick ||
		currentState == MatchState::Corner ||
		currentState == MatchState::GoalKick ||
		currentState == MatchState::ThrowIn)
	{
		m_replayEngine.unlockRecording();
	}

	m_replayEngine.recordFrame(m_ball.get(), allPlayers);
}

/// <summary>
/// Draw Frames and Switch Buffers
/// </summary>
void MatchEngine::render(sf::RenderWindow& t_window)
{
	// ==========================================
	// 1. SET THE CORRECT WORLD CAMERA
	// ==========================================
	if (m_replayEngine.isReplaying()) {
		m_replayEngine.replayCam(t_window);
	}
	else {
		t_window.setView(m_playerCam);
	}

	// 2. Draw Background 
	m_stadium1.draw(t_window);

	//m_spatialGrid.drawDebug(t_window, m_pitch);

	if (m_userController && !m_replayEngine.isReplaying()) {
		m_userController->draw(t_window);
	}

	// ==========================================
	// --- THE FIX: VIEWPORT CULLING ---
	// ==========================================
	// Grab the active view (works for both live cam and replay cam!)
	sf::View currentView = t_window.getView();
	sf::Vector2f viewCenter = currentView.getCenter();
	sf::Vector2f viewSize = currentView.getSize();

	// Create a Render Bounds box. 
	// We add a 1200px buffer (~12 meters) so players casting long shadows 
	// or making diving saves don't disappear while at the edge of the screen!
	float renderBuffer = 1200.f;
	sf::FloatRect renderBounds({
		viewCenter.x - (viewSize.x / 2.f) - renderBuffer ,
		viewCenter.y - (viewSize.y / 2.f) - renderBuffer },
		{ viewSize.x + (renderBuffer * 2.f),
		viewSize.y + (renderBuffer * 2.f) }
	);

	// Filter out the entities that are actually on screen
	std::vector<Entity*> visibleEntities;
	visibleEntities.reserve(m_entities.size()); // Prevent reallocation

	for (Entity* entity : m_entities) {
		if (entity != nullptr && renderBounds.contains(entity->getPosition())) {
			visibleEntities.push_back(entity);
		}
	}

	// 3. Sort ONLY the Visible Entities by Depth
	std::sort(visibleEntities.begin(), visibleEntities.end(), [](Entity* a, Entity* b) {
		return a->getSortDepth() > b->getSortDepth();
		});

	// ==========================================
	// 4. DRAW ENTITIES (WITH SPLIT GOAL LAYERS!)
	// ==========================================
	if (m_replayEngine.isReplaying()) {
		// Draw static components behind the DVR buffer
		m_homeGoal.drawFloor(t_window); m_awayGoal.drawFloor(t_window);
		m_homeGoal.drawNet(t_window);   m_awayGoal.drawNet(t_window);
		m_homeGoal.drawPosts(t_window); m_awayGoal.drawPosts(t_window);
		m_homeGoal.drawCrossbar(t_window); m_awayGoal.drawCrossbar(t_window);

		m_replayEngine.render(t_window, &m_kitShader);
	}
	else {
		// A. Draw the Floor & Netting FIRST (Always behind the players)
		// (Optional: You could also wrap these in renderBounds checks if your pitch is huge!)
		m_homeGoal.drawFloor(t_window); m_awayGoal.drawFloor(t_window);
		m_homeGoal.drawNet(t_window);   m_awayGoal.drawNet(t_window);

		// Grab the specific Goal Line depths for the Mid Layer
		float homeGoalDepth = m_homeGoal.center.x;
		float awayGoalDepth = m_awayGoal.center.x;

		bool homePostsDrawn = false;
		bool awayPostsDrawn = false;

		// B. Draw Live Game Entities & Interleave the Posts
		// Notice we are looping over visibleEntities now!
		for (Entity* entity : visibleEntities) {

			// --- INTERLEAVE HOME POSTS ---
			if (!homePostsDrawn && entity->getSortDepth() <= homeGoalDepth) {
				m_homeGoal.drawPosts(t_window);
				homePostsDrawn = true;
			}

			// --- INTERLEAVE AWAY POSTS ---
			if (!awayPostsDrawn && entity->getSortDepth() <= awayGoalDepth) {
				m_awayGoal.drawPosts(t_window);
				awayPostsDrawn = true;
			}

			// Draw the actual entity
			if (entity == m_ball.get()) {
				m_ball->draw(t_window);
			}
			else {
				Player* p = dynamic_cast<Player*>(entity);
				if (p && p->isSentOff()) continue;

				renderPlayerEntity(t_window, entity);
			}
		}

		// Catch-all: If the posts were somehow lower depth than ALL visible entities
		// (e.g. all visible players are running away from the goal)
		if (!homePostsDrawn) m_homeGoal.drawPosts(t_window);
		if (!awayPostsDrawn) m_awayGoal.drawPosts(t_window);

		// C. Draw Crossbars LAST (Simulating physical Z-height overhead)
		m_homeGoal.drawCrossbar(t_window);
		m_awayGoal.drawCrossbar(t_window);
	}

	if (!m_replayEngine.isReplaying()) {
		drawDebugNames(t_window, m_font);

		// ==========================================
		// 5. DRAW UI (SCREEN SPACE)
		// ==========================================
		drawUI(t_window);
		if (m_userPlayer)
		{
			powerBarDraw(t_window);
		}
	}

	// Reset view to default before drawing static screens/UI overlays
	t_window.setView(t_window.getDefaultView());

	if (m_pause || m_referee.getMatchState() == MatchState::FullTime || m_referee.getMatchState() == MatchState::HalfTime)
    {
        sf::RectangleShape overlay(sf::Vector2f(t_window.getSize()));
        overlay.setFillColor(sf::Color(20, 20, 20, 200));
        t_window.draw(overlay);

        if (m_showGamePlan) {
            // Do Gameplan render logic here
        }
        else {
            drawMatchStats(t_window);

            // THE FIX: Respect the dynamic hiding of buttons and draw the warning
            for (int i = 0; i < 2; i++) {
                if (m_referee.getMatchState() == MatchState::FullTime && i == 0) continue;
                t_window.draw(m_pauseButtons[i]);
                t_window.draw(m_pauseTexts[i]);
            }
            
            if (m_showWarning && m_referee.getMatchState() != MatchState::FullTime) {
                t_window.draw(m_warningText);
            }
        }
    }
}

void MatchEngine::renderPlayerEntity(sf::RenderWindow& t_window, Entity* entity)
{
	// 1. Get the actual ground position and Z height
	sf::Vector2f groundPos = entity->getPosition();
	float z = entity->z;

	// --- Calculate exact feet position for the shadows ---
	sf::Vector2f spriteScale = entity->getSprite().getScale();
	sf::Vector2f feetPos = groundPos;
	// The sprite center is at the chest. Feet are 300px down (-X axis), scaled dynamically.
	feetPos.x -= 150.f * std::abs(spriteScale.x);

	// Calculate how jump height (Z) affects shadow size and visibility
	float zRatio = std::min(z / 100.f, 1.f);
	float airFade = std::max(0.f, 1.0f - (z / 150.f)); // Fades out completely when very high

	// --- Calculate the dynamic scale so everything shrinks together ---
	float shadowScale = 1.f - (zRatio * 0.5f);
	float currentRadius = 20.f * shadowScale; // The actual current radius of the circle

	// ==========================================
	// 2A. DYNAMIC FLOODLIGHT SHADOWS (TRUE 3D PROJECTION)
	// ==========================================
	if (airFade > 0.01f) // Only draw if they are close enough to the ground to cast a shadow
	{
		sf::Vector2f lights[4] = {
			{-500.f, -500.f},     // Home Left Corner (Top-Left)
			{-500.f, 7500.f},     // Home Right Corner (Bottom-Left)
			{10500.f, -500.f},    // Away Left Corner (Top-Right)
			{10500.f, 7500.f}     // Away Right Corner (Bottom-Right)
		};

		// --- 3D Projection Variables (100px = 1m) ---
		const float lightHeight = 6000.f;  // Lights are 60 meters in the air
		const float playerHeight = 180.f;  // Player is ~1.8 meters tall
		const float maxLightDist = 12500.f;// Max pitch diagonal

		for (int i = 0; i < 4; ++i)
		{
			// Direction & Distance
			sf::Vector2f toPlayer = feetPos - lights[i];
			float distXY = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);

			if (distXY > 0.1f) {
				sf::Vector2f dir = toPlayer / distXY;
				sf::Vector2f normal(-dir.y, dir.x);

				// EXACT GEOMETRIC LENGTH
				float totalHeight = playerHeight + z;
				float length = totalHeight * (distXY / lightHeight);
				length = std::max(15.f, length); // Ensure a tiny physical base

				// EXPONENTIAL FADE (Inverse Square Approximation)
				float normalizedDist = std::min(distXY / maxLightDist, 1.0f);
				float intensity = std::pow(1.0f - normalizedDist, 2.0f);

				// Max opacity is 60 when directly under, fading rapidly to 0 across the pitch
				std::uint8_t alpha = static_cast<std::uint8_t>(60 * intensity * airFade);

				// Optimization: Don't bother drawing the shadow if it's too faint to see
				if (alpha < 2) continue;

				sf::Color baseColor(0, 0, 0, alpha);
				sf::Color tipColor(0, 0, 0, 0);

				// Dynamic Diffusion (Blur/Widen)
				float diffusion = 1.2f + (normalizedDist * 3.0f);
				float width = 12.f * shadowScale;

				// Anchor exactly at the edge of the core circle
				sf::Vector2f start = feetPos + (dir * (currentRadius - 2.f));

				sf::VertexArray floodShadow(sf::PrimitiveType::TriangleStrip, 4);

				// Base vertices
				floodShadow[0].position = start + normal * width;
				floodShadow[0].color = baseColor;
				floodShadow[1].position = start - normal * width;
				floodShadow[1].color = baseColor;

				// Tip vertices 
				floodShadow[2].position = start + (dir * length) + normal * (width * diffusion);
				floodShadow[2].color = tipColor;
				floodShadow[3].position = start + (dir * length) - normal * (width * diffusion);
				floodShadow[3].color = tipColor;

				t_window.draw(floodShadow);
			}
		}

		// ==========================================
		// 2B. CORE CIRCULAR SHADOW
		// ==========================================
		sf::CircleShape core(20.f);
		core.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(100 * airFade)));
		core.setOrigin({ 20.f, 20.f });
		core.setPosition(feetPos);
		core.setScale({ shadowScale, shadowScale });
		t_window.draw(core);
	}

	// ==========================================
	// 3. ELEVATED PLAYER VISUALS
	// ==========================================
	sf::Vector2f visualPos = { groundPos.x + (z / 1.5f), groundPos.y };
	sf::Sprite visualSprite = entity->getSprite();
	visualSprite.setPosition(visualPos);

	float scaleMultiplier = 1.0f + (z / 750.f);
	visualSprite.setScale({ visualSprite.getScale().x * scaleMultiplier, visualSprite.getScale().y * scaleMultiplier });

	// ==========================================
	// --- DYNAMIC SHADER INJECTION ---
	// ==========================================
	Player* p = dynamic_cast<Player*>(entity);

	if (p) {
		auto toGlslColor = [](sf::Color c) {
			return sf::Glsl::Vec4(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
			};

		// 1. Base Layer (Skin)
		m_kitShader.setUniform("skinColor", toGlslColor(p->getSkinColor()));
		m_kitShader.setUniform("skinTex", sf::Shader::CurrentTexture);

		// ==========================================
		// --- THE FIX: EXPAND TO 15 LAYER MAXIMUM ---
		// ==========================================
		static const std::string uUse[15] = {
			"use0", "use1", "use2", "use3", "use4", "use5", "use6", "use7",
			"use8", "use9", "use10", "use11", "use12", "use13", "use14"
		};
		static const std::string uTex[15] = {
			"tex0", "tex1", "tex2", "tex3", "tex4", "tex5", "tex6", "tex7",
			"tex8", "tex9", "tex10", "tex11", "tex12", "tex13", "tex14"
		};
		static const std::string uCol[15] = {
			"col0", "col1", "col2", "col3", "col4", "col5", "col6", "col7",
			"col8", "col9", "col10", "col11", "col12", "col13", "col14"
		};

		const auto& layers = p->getKitLayers();

		// 3. Feed the dynamic stack to the GPU (Loop up to 15)
		for (int i = 0; i < 15; ++i) {
			if (i < layers.size()) {
				sf::Texture* tex = AnimationServer::getKitTexture(layers[i].textureId);
				if (tex) {
					m_kitShader.setUniform(uUse[i], true);
					m_kitShader.setUniform(uTex[i], *tex);
					m_kitShader.setUniform(uCol[i], toGlslColor(layers[i].color));
				}
				else {
					m_kitShader.setUniform(uUse[i], false); // Texture ID was invalid
				}
			}
			else {
				m_kitShader.setUniform(uUse[i], false); // No more layers for this player
			}
		}

		// 4. Draw the sprite USING the shader!
		t_window.draw(visualSprite, &m_kitShader);
	}
	else {
		// Non-players (like the ball) draw normally without the shader
		t_window.draw(visualSprite);
	}
}

void MatchEngine::beginMatchSetup(GameDatabase& db, const std::string& homeTeamId, const std::string& awayTeamId, const std::string& userPlayerId)
{
	m_db = &db;
	TeamData* homeTeam = db.getTeam(homeTeamId);
	TeamData* awayTeam = db.getTeam(awayTeamId);
	if (!homeTeam || !awayTeam) return;

	m_homeTeamData = *homeTeam;
	m_awayTeamData = *awayTeam;

	m_homeTeamAI = std::make_unique<TeamAI>(true, m_homeTeamData.defaultTactics);
	m_awayTeamAI = std::make_unique<TeamAI>(false, m_awayTeamData.defaultTactics);

	m_entities.clear();
	m_homeside.clear();
	m_awayside.clear();
	m_homeTeam.clear();
	m_awayTeam.clear();

	m_setupUserPlayerId = userPlayerId;
	m_userAssigned = false;

	if (userPlayerId != "SPECTATOR") {
		m_userPlayer = std::make_unique<UserPlayer>();
		m_userController = std::make_unique<UserController>(*m_userPlayer);
		m_entities.push_back(m_userPlayer.get());
	}
	else {
		m_userPlayer.reset();
		m_userController.reset();
	}

	m_ball = std::make_unique<Ball>();
	m_entities.push_back(m_ball.get());
	m_npcController = std::make_unique<NPCController>();

	auto loadMatchdayMemory = [&](TeamData& team) {
		team.startingXI.clear();
		team.bench.clear();
		std::vector<std::string> activeStarters;

		for (const auto& [slotId, pId] : team.defaultTactics.startingXI) {
			PlayerData* pData = m_db->getPlayer(pId);
			if (pData) {
				team.startingXI.push_back(*pData);
				activeStarters.push_back(pId);
			}
		}

		if (!team.defaultTactics.benchIds.empty()) {
			for (const std::string& benchId : team.defaultTactics.benchIds) {
				PlayerData* bData = m_db->getPlayer(benchId);
				if (bData) team.bench.push_back(*bData);
			}
		}
		else {
			for (const std::string& rosterId : team.rosterPlayerIds) {
				bool isStarting = std::find(activeStarters.begin(), activeStarters.end(), rosterId) != activeStarters.end();
				if (!isStarting) {
					PlayerData* bData = m_db->getPlayer(rosterId);
					if (bData) team.bench.push_back(*bData);
				}
			}
		}
	};

	m_matchInfo.initMatch(homeTeamId, awayTeamId);

	loadMatchdayMemory(m_homeTeamData);
	loadMatchdayMemory(m_awayTeamData);

	// Set the state machine to start baking the Home Team
	m_loadingPhase = 1;
	m_loadingIndex = 0;
}

float MatchEngine::loadNextPlayer()
{
	if (m_loadingPhase == 0 || m_loadingPhase > 2) return 1.0f; // Finished

	TeamData& teamData = (m_loadingPhase == 1) ? m_homeTeamData : m_awayTeamData;
	auto& teamUnique = (m_loadingPhase == 1) ? m_homeside : m_awayside;
	auto& teamPointers = (m_loadingPhase == 1) ? m_homeTeam : m_awayTeam;
	bool isHomeSide = (m_loadingPhase == 1);

	// If we finished this team, move to the next phase!
	if (m_loadingIndex >= teamData.defaultTactics.startingXI.size()) {
		m_loadingPhase++;
		m_loadingIndex = 0;
		return loadNextPlayer(); // Recursively call to start the away team or finish
	}

	// Grab the exact map iterator for this slot
	auto it = teamData.defaultTactics.startingXI.begin();
	std::advance(it, m_loadingIndex);
	int slotId = it->first;
	std::string playerId = it->second;

	PlayerData& pData = teamData.startingXI[m_loadingIndex];

	auto layout = getFormationLayout(teamData.defaultTactics.formationName);
	PositionRole matchRole = PositionRole::CenterMid;
	for (const auto& line : layout) {
		for (const auto& slot : line) {
			if (slot.first == slotId) {
				matchRole = slot.second;
				break;
			}
		}
	}

	// --- Check User Assignment ---
	bool hasPlayer8 = false;
	if (m_setupUserPlayerId == "AUTO_HOME" || m_setupUserPlayerId == "AUTO_AWAY" || m_setupUserPlayerId.empty()) {
		for (const auto& [sId, pId] : teamData.defaultTactics.startingXI) {
			if (pId == "8") { hasPlayer8 = true; break; }
		}
	}

	bool isTargetUser = false;
	if (m_setupUserPlayerId != "SPECTATOR" && !m_userAssigned) {
		if (m_setupUserPlayerId == "AUTO_HOME" || m_setupUserPlayerId.empty()) {
			if (isHomeSide) {
				if (hasPlayer8) isTargetUser = (playerId == "8");
				else if (matchRole != PositionRole::Goalkeeper) isTargetUser = true;
			}
		}
		else if (m_setupUserPlayerId == "AUTO_AWAY") {
			if (!isHomeSide) {
				if (hasPlayer8) isTargetUser = (playerId == "8");
				else if (matchRole != PositionRole::Goalkeeper) isTargetUser = true;
			}
		}
		else {
			isTargetUser = (playerId == m_setupUserPlayerId);
		}
	}

	// ==========================================
	// --- THIS IS WHERE THE HEAVY BAKING HAPPENS ---
	// ==========================================
	if (isTargetUser && m_userPlayer)
	{
		m_userPlayer->loadFromData(pData, teamData); // <-- BAKES TEXTURE IN RAM
		m_userPlayer->setTeam(isHomeSide ? Team::Home : Team::Away);
		m_userPlayer->setPositionRole(matchRole);
		m_userPlayer->setMatchTimeScale(90.0f / static_cast<float>(GlobalSettings::matchLengthMinutes));
		sf::Vector2f basePos = m_userPlayer->getBaseTacticalCoordinate(isHomeSide, slotId, layout);
		m_userPlayer->setBaseHomePosition(basePos);
		m_userPlayer->setPosition(basePos);
		m_userPlayer->setTeamChemistry(teamData.teamChemistry);

		teamPointers.push_back(m_userPlayer.get());
		m_userAssigned = true;
	}
	else
	{
		auto player = std::make_unique<NPCPlayer>();
		player->loadFromData(pData, teamData); // <-- BAKES TEXTURE IN RAM
		player->setTeam(isHomeSide ? Team::Home : Team::Away);
		player->setPositionRole(matchRole);
		player->setMatchTimeScale(90.0f / static_cast<float>(GlobalSettings::matchLengthMinutes));
		sf::Vector2f basePos = player->getBaseTacticalCoordinate(isHomeSide, slotId, layout);
		player->setBaseHomePosition(basePos);
		if (m_userPlayer)
		{
			if (m_userPlayer->getTeam() == player->getTeam()) player->setIsUserOpponent(false);
			else player->setIsUserOpponent(true);
		}
		player->setPosition(player->getHomePosition());
		player->setTeamChemistry(teamData.teamChemistry);

		m_entities.push_back(player.get());
		teamPointers.push_back(player.get());
		teamUnique.push_back(std::move(player));
	}

	m_loadingIndex++;

	// Calculate overall progress
	float totalPlayers = m_homeTeamData.defaultTactics.startingXI.size() + m_awayTeamData.defaultTactics.startingXI.size();
	float currentLoaded = (m_loadingPhase == 1 ? 0 : m_homeTeamData.defaultTactics.startingXI.size()) + m_loadingIndex;
	return currentLoaded / totalPlayers;
}

void MatchEngine::finalizeMatchSetup()
{
	m_ball->setPosition(m_pitch.centerSpot);

	barBackground.setSize(barSize);
	barBackground.setFillColor(sf::Color(50, 50, 50, 200));
	barBackground.setOutlineThickness(2.f);
	barBackground.setOutlineColor(sf::Color::White);

	barFill.setSize(barSize);
	barFill.setFillColor(sf::Color::Green);

	m_homeGoal.initialize(sf::Vector2f{ m_pitch.margin, 3500.f }, true);
	m_awayGoal.initialize(sf::Vector2f{ m_pitch.totalWidth - m_pitch.margin, 3500.f }, false);

	m_playerCam.setCenter(m_userPlayer ? m_userPlayer->getPosition() : m_pitch.centerSpot);
	m_playerCam.setSize({ 1920.f * 2.4f, 1080.f * 2.4f });
	m_playerCam.zoom(1.0f / GlobalSettings::cameraZoom);

	std::vector<Player*> allPlayers;
	if (m_userPlayer) allPlayers.push_back(m_userPlayer.get());
	for (auto& tm : m_homeside) allPlayers.push_back(tm.get());
	for (auto& opp : m_awayside) allPlayers.push_back(opp.get());

	m_soundManager.loadAllSounds();
	m_soundManager.playCrowd("ASSETS/SOUNDS/CROWD/stadium_noise.ogg", 80.f);

	m_referee.startMatch(*m_ball, m_pitch, allPlayers, m_soundManager);

	for (Player* p : m_homeTeam) {
		m_matchInfo.recordAppearanceStart(p->getId(), m_matchInfo.getHomeTeamId(), 0);
	}
	for (Player* p : m_awayTeam) {
		m_matchInfo.recordAppearanceStart(p->getId(), m_matchInfo.getAwayTeamId(), 0);
	}
}

void MatchEngine::updateCamera(sf::RenderWindow& t_window)
{
	sf::Vector2f ballPos = m_ball->getPosition();
	sf::Vector2f targetCenter;
	float dynamicZoom = 1.0f;

	if (m_userPlayer) {
		// --- PLAYER CAMERA ---
		sf::Vector2f playerPos = m_userPlayer->getPosition();
		sf::Vector2f mouseWorldPos = t_window.mapPixelToCoords(sf::Mouse::getPosition(t_window), m_playerCam);

		sf::Vector2f aimVec = mouseWorldPos - playerPos;
		sf::Vector2f ballVec = ballPos - playerPos;

		float aimDist = std::sqrt(aimVec.x * aimVec.x + aimVec.y * aimVec.y);
		float ballDist = std::sqrt(ballVec.x * ballVec.x + ballVec.y * ballVec.y);

		float maxAimPull = 2000.f;
		if (aimDist > maxAimPull) {
			aimVec = (aimVec / aimDist) * maxAimPull;
			aimDist = maxAimPull;
		}

		// ==========================================
		// --- THE FIX 1: BALL TRACKING BLEND ---
		// ==========================================
		// t = 0.0 (Lock to player), t = 1.0 (Lock to ball)
		float t = GlobalSettings::cameraBallFollow;

		// The more we track the ball, the less the mouse aim influences the screen!
		// Swap 0.35f with GlobalSettings::cameraAimPull
		sf::Vector2f blendedAim = aimVec * (GlobalSettings::cameraAimPull * (1.0f - t));

		// Interpolate between the player and the ball, then add the aiming offset
		targetCenter = playerPos + ((ballPos - playerPos) * t) + blendedAim;

		dynamicZoom = 1.0f + (aimDist * 0.00015f) + (ballDist * 0.0001f);
		dynamicZoom = std::clamp(dynamicZoom, 1.0f, 1.35f);
	}
	else {
		// --- TV BROADCAST CAMERA (Spectator) ---
		sf::Vector2f ballVel = m_ball->getVelocity();
		targetCenter = ballPos + (ballVel * 0.3f);

		float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);
		dynamicZoom = 1.15f + (ballSpeed * 0.0001f);
		dynamicZoom = std::clamp(dynamicZoom, 1.15f, 1.4f);
	}

	float baseSizeX = 1920.f * 2.4f;
	float baseSizeY = 1080.f * 2.4f;

	// ==========================================
	// --- THE FIX 2: GLOBAL ZOOM APPLICATION ---
	// ==========================================
	// A higher GlobalSettings::cameraZoom value (e.g., 2.5) results in a smaller view size.
	// This naturally zooms the camera in on the action!
	float finalZoomMultiplier = dynamicZoom / GlobalSettings::cameraZoom;

	sf::Vector2f targetSize(baseSizeX * finalZoomMultiplier, baseSizeY * finalZoomMultiplier);

	sf::Vector2f currentCenter = m_playerCam.getCenter();
	sf::Vector2f currentSize = m_playerCam.getSize();

	float lerpFactor = 0.035f;
	sf::Vector2f smoothedCenter = currentCenter + (targetCenter - currentCenter) * lerpFactor;
	sf::Vector2f smoothedSize = currentSize + (targetSize - currentSize) * lerpFactor;

	m_playerCam.setSize(smoothedSize);
	m_playerCam.setCenter(smoothedCenter);
	m_playerCam.setRotation(sf::degrees(90));
	t_window.setView(m_playerCam);
}

void MatchEngine::runStandardSystems(float dt, sf::RenderWindow& t_window)
{
	Player* effectiveOwner = m_ball->getOwner();
	if (effectiveOwner == nullptr) effectiveOwner = m_ball->getLastOwner();

	std::vector<Player*> homeTeamActive;
	std::vector<Player*> awayTeamActive;

	for (auto& npc : m_homeside) {
		if (!npc->isSentOff()) homeTeamActive.push_back(npc.get());
	}
	for (auto& opp : m_awayside) {
		if (!opp->isSentOff()) awayTeamActive.push_back(opp.get());
	}

	if (m_userPlayer && !m_userPlayer->isSentOff()) {
		if (m_userPlayer->getTeam() == Team::Home) homeTeamActive.push_back(m_userPlayer.get());
		else awayTeamActive.push_back(m_userPlayer.get());
	}

	if (!m_gameOver) {
		if (homeTeamActive.size() < 7) { triggerForfeit(true); return; }
		else if (awayTeamActive.size() < 7) { triggerForfeit(false); return; }
	}

	Player* homeFirstResponder = findFirstResponder(homeTeamActive);
	Player* awayFirstResponder = findFirstResponder(awayTeamActive);

	std::vector<Player*> allPlayers = homeTeamActive;
	allPlayers.insert(allPlayers.end(), awayTeamActive.begin(), awayTeamActive.end());

	m_spatialGrid.update(homeTeamActive, awayTeamActive, *m_ball, m_pitch);

	// ==========================================
	// --- THE FIX: BUILD THE CONTEXTS ---
	// ==========================================
	MatchEnvironment baseEnv;
	baseEnv.ball = m_ball.get();
	baseEnv.pitch = &m_pitch;
	baseEnv.homeGoal = &m_homeGoal;
	baseEnv.awayGoal = &m_awayGoal;
	baseEnv.sound = &m_soundManager;
	baseEnv.stats = &m_matchStats;
	baseEnv.info = &m_matchInfo;
	baseEnv.referee = &m_referee;
	baseEnv.grid = &m_spatialGrid;
	baseEnv.allPlayers = &allPlayers;

	MatchEnvironment homeEnv = baseEnv;
	homeEnv.teammates = &homeTeamActive;
	homeEnv.opposition = &awayTeamActive;

	MatchEnvironment awayEnv = baseEnv;
	awayEnv.teammates = &awayTeamActive;
	awayEnv.opposition = &homeTeamActive;

	// ==========================================
	// --- 5. UPDATE AI BRAINS ---
	// ==========================================
	if (m_userController) {
		if (m_userPlayer->getTeam() == Team::Home)
		{
			m_userController->update(dt, homeEnv);
		}
		else
		{
			m_userController->update(dt, awayEnv);
		}
		m_userController->mouseAiming(mouseWorld, t_window, m_playerCam);
	}
	if (m_userPlayer) m_userPlayer->update(dt);

	for (auto& npc : m_homeside) {
		if (npc->isSentOff()) continue;
		m_npcController->update(*npc, m_userPlayer.get(), dt, homeFirstResponder, *m_homeTeamAI, homeEnv);
		npc->update(dt);
	}

	for (auto& npc : m_awayside) {
		if (npc->isSentOff()) continue;
		m_npcController->update(*npc, m_userPlayer.get(), dt, awayFirstResponder, *m_awayTeamAI, awayEnv);
		npc->update(dt);
	}

	// ==========================================
	// --- 6. WORLD PHYSICS & COLLISIONS ---
	// ==========================================
	m_ball->update(dt);

	// Look at how much cleaner the Physics Engine becomes!
	PhysicsEngine::resolvePlayerPlayerCollisions(baseEnv);

	for (Player* p : allPlayers) {
		PhysicsEngine::resolvePlayerPitchBoundaries(*p, *baseEnv.pitch);
		PhysicsEngine::resolvePlayerGoalCollisions(*p, *baseEnv.homeGoal);
		PhysicsEngine::resolvePlayerGoalCollisions(*p, *baseEnv.awayGoal);
	}

	PhysicsEngine::resolveBallPitchBoundaries(baseEnv);
	PhysicsEngine::resolveBallGoalCollisions(*baseEnv.ball, *baseEnv.homeGoal, *baseEnv.sound);
	PhysicsEngine::resolveBallGoalCollisions(*baseEnv.ball, *baseEnv.awayGoal, *baseEnv.sound);

	PhysicsEngine::resolveGoalkeeperBallCollisions(baseEnv);
	PhysicsEngine::resolveBallPlayerCollisions(baseEnv);
}

Player* MatchEngine::findFirstResponder(const std::vector<Player*>& t_team) {
	if (t_team.empty()) return nullptr;

	Player* bestResponder = nullptr;
	float minScore = 9999999.f; // Lower score is better

	sf::Vector2f ballPos = m_ball->getPosition();
	sf::Vector2f ballVel = m_ball->getVelocity();

	for (auto* p : t_team) {
		sf::Vector2f pPos = p->getPosition();
		sf::Vector2f pVel = p->getVelocity();

		// 1. Basic distance
		float d = distance(pPos, ballPos);

		// 2. Velocity Weighting (The "Momentum" Factor)
		// We calculate if the player's current movement is toward or away from the ball.
		sf::Vector2f toBall = normalize(ballPos - pPos);
		float velocityDot = (pVel.x * toBall.x + pVel.y * toBall.y);

		// If velocityDot is positive, they are moving toward the ball.
		// We subtract this from the distance score to "reward" players already moving the right way.
		float momentumBonus = velocityDot * 0.5f;

		// 3. Goal-Side Bias (Optional)
		// Defenders get a small bonus to be first responders if the ball is near their own goal.
		float goalBias = 0.f;
		if (p->getPositionRole() == PositionRole::CenterBack || p->getPositionRole() == PositionRole::LeftBack || p->getPositionRole() == PositionRole::RightBack) {
			goalBias = 50.f;
		}

		float finalScore = d - momentumBonus - goalBias;

		if (finalScore < minScore) {
			minScore = finalScore;
			bestResponder = p;
		}
	}

	return bestResponder;
}

void MatchEngine::drawUI(sf::RenderWindow& t_window)
{
	// ==========================================
	// 1. WORLD-SPACE UI (Player Indicator)
	// ==========================================
	// Draw this BEFORE changing the view so it stays attached to the player in the world!
	if (m_userPlayer) {
		// A CircleShape with 3 points is a triangle!
		sf::CircleShape indicator(20.f, 3);
		indicator.setFillColor(sf::Color(255, 255, 0, 130)); // Semi-transparent yellow
		indicator.setOrigin({ 20.f, 20.f });
		indicator.setRotation(sf::degrees(270.f)); // Rotate 180 degrees to point down

		// Position it hovering just above the player's head
		sf::Vector2f playerPos = m_userPlayer->getPosition();
		indicator.setPosition({ playerPos.x + 100.f, playerPos.y });

		t_window.draw(indicator);
	}
	// ==========================================
	// 2. SCREEN-SPACE UI (Minimap & Scoreboard)
	// ==========================================
	sf::View worldView = t_window.getView();
	t_window.setView(t_window.getDefaultView());

	// --- A. MINIMAP BACKGROUND (Rotated 90 Degrees) ---
	float mapWidth = 210.f;  // Swapped proportions
	float mapHeight = 300.f; // 7:10 aspect ratio to match 7000x10000 layout
	float padding = 20.f;

	// Top right corner calculation
	sf::Vector2f mapPos(t_window.getSize().x - mapWidth - padding, padding);

	sf::RectangleShape mapBg(sf::Vector2f(mapWidth, mapHeight));
	mapBg.setPosition(mapPos);
	mapBg.setFillColor(sf::Color(50, 50, 50, 150));
	mapBg.setOutlineThickness(2.f);
	mapBg.setOutlineColor(sf::Color(255, 255, 255, 200));
	t_window.draw(mapBg);

	// --- B. PITCH LINES (Rotated) ---
	// Halfway Line (Now Horizontal)
	sf::RectangleShape halfway(sf::Vector2f(mapWidth, 2.f));
	halfway.setPosition({ mapPos.x, mapPos.y + (mapHeight / 2.f) - 1.f });
	halfway.setFillColor(sf::Color(255, 255, 255, 100));
	t_window.draw(halfway);

	// Penalty Boxes (Proportions Swapped)
	float boxW = mapWidth * 0.575f;
	float boxH = mapHeight * 0.165f;
	float boxX = mapPos.x + (mapWidth - boxW) / 2.f;

	// Away Box (Top of minimap)
	sf::RectangleShape topBox(sf::Vector2f(boxW, boxH));
	topBox.setPosition({ boxX, mapPos.y });
	topBox.setFillColor(sf::Color::Transparent);
	topBox.setOutlineThickness(1.f);
	topBox.setOutlineColor(sf::Color(255, 255, 255, 100));
	t_window.draw(topBox);

	// Home Box (Bottom of minimap)
	sf::RectangleShape bottomBox(sf::Vector2f(boxW, boxH));
	bottomBox.setPosition({ boxX, mapPos.y + mapHeight - boxH });
	bottomBox.setFillColor(sf::Color::Transparent);
	bottomBox.setOutlineThickness(1.f);
	bottomBox.setOutlineColor(sf::Color(255, 255, 255, 100));
	t_window.draw(bottomBox);

	// --- C. PLAYER & BALL DOTS (Rotated Mapping) ---
	auto drawMinimapDot = [&](sf::Vector2f worldPos, sf::Color color, float radius = 3.f) {
		float playWidth = m_pitch.totalWidth - (2.f * m_pitch.margin);
		float playHeight = m_pitch.totalHeight - (2.f * m_pitch.margin);

		// THE 90-DEGREE FIX: 
		// Minimap X (Left/Right) is mapped to Pitch Y
		// Minimap Y (Top/Bottom) is mapped to inverted Pitch X
		float normX = (worldPos.y - m_pitch.margin) / playHeight;
		float normY = 1.0f - ((worldPos.x - m_pitch.margin) / playWidth);

		normX = std::clamp(normX, 0.0f, 1.0f);
		normY = std::clamp(normY, 0.0f, 1.0f);

		sf::CircleShape dot(radius);
		dot.setOrigin({ radius, radius });
		dot.setPosition({ mapPos.x + (normX * mapWidth), mapPos.y + (normY * mapHeight) });
		dot.setFillColor(color);

		dot.setOutlineThickness(1.f);
		dot.setOutlineColor(sf::Color(255, 255, 255, 200));

		t_window.draw(dot);
		};

	// --- DYNAMIC MINIMAP COLORS ---
		// Give the dots a solid alpha of 255 so they pop against the dark minimap
	sf::Color homeDot = m_homeTeamData.uiColor; homeDot.a = 255;
	sf::Color awayDot = m_awayTeamData.uiColor; awayDot.a = 255;

	// FIX: Only draw the dot if they are actually allowed on the pitch!
	for (const auto& tm : m_homeside) {
		if (!tm->isSentOff()) drawMinimapDot(tm->getPosition(), homeDot);
	}

	for (const auto& opp : m_awayside) {
		if (!opp->isSentOff()) drawMinimapDot(opp->getPosition(), awayDot);
	}

	// Make sure the User's dot disappears too if you manage to get yourself sent off!
	if (m_userPlayer && !m_userPlayer->isSentOff()) {
		drawMinimapDot(m_userPlayer->getPosition(), sf::Color::Yellow, 4.f);
	}

	if (m_ball) drawMinimapDot(m_ball->getPosition(), sf::Color(255, 165, 0), 4.f);
	// ==========================================
	// --- D. TV BROADCAST SCOREBOARD ---
	// ==========================================
	float scoreWidth = 260.f;
	float scoreHeight = 65.f;
	sf::Vector2f scorePos(20.f, 25.f); // Pushed down 5px to leave room for the red cards!

	sf::RectangleShape scoreBg(sf::Vector2f(scoreWidth, scoreHeight));
	scoreBg.setPosition(scorePos);
	scoreBg.setFillColor(sf::Color(20, 20, 30, 220)); // Made slightly darker for contrast
	scoreBg.setOutlineThickness(2.f);
	scoreBg.setOutlineColor(sf::Color(255, 255, 255, 150));
	t_window.draw(scoreBg);

	// --- TEAM COLOR ACCENT BARS ---
	sf::Color homeAccentColor = m_homeTeamData.uiColor; homeAccentColor.a = 255;
	sf::Color awayAccentColor = m_awayTeamData.uiColor; awayAccentColor.a = 255;

	// Home Color on the Left
	sf::RectangleShape homeAccent(sf::Vector2f(12.f, scoreHeight));
	homeAccent.setPosition(scorePos);
	homeAccent.setFillColor(homeAccentColor);
	t_window.draw(homeAccent);

	// Away Color on the Right
	sf::RectangleShape awayAccent(sf::Vector2f(12.f, scoreHeight));
	awayAccent.setPosition({ scorePos.x + scoreWidth - 12.f, scorePos.y });
	awayAccent.setFillColor(awayAccentColor);
	t_window.draw(awayAccent);

	// --- NEW: RED CARD INDICATORS ---
	int homeReds = 0;
	int awayReds = 0;

	// Helper to count the cards
	auto countCards = [&](Player* p) {
		if (p && p->isSentOff()) {
			if (p->getTeam() == Team::Home) homeReds++;
			else awayReds++;
		}
		};

	countCards(m_userPlayer.get());
	for (const auto& tm : m_homeside) countCards(tm.get());
	for (const auto& opp : m_awayside) countCards(opp.get());

	// Cap the visual display at 3 cards so the UI doesn't clutter
	int displayHomeReds = std::min(homeReds, 3);
	int displayAwayReds = std::min(awayReds, 3);

	// Draw Home Red Cards (Resting on the top-left edge)
	for (int i = 0; i < displayHomeReds; ++i) {
		sf::RectangleShape redCard(sf::Vector2f(8.f, 12.f));
		redCard.setFillColor(sf::Color(220, 20, 20, 255)); // Bright broadcast red
		redCard.setOutlineThickness(1.f);
		redCard.setOutlineColor(sf::Color(255, 255, 255, 150));

		// Space them 12px apart, starting just inside the left color bar
		redCard.setPosition({ scorePos.x + 20.f + (i * 12.f), scorePos.y - 13.f });
		t_window.draw(redCard);
	}

	// Draw Away Red Cards (Resting on the top-right edge)
	for (int i = 0; i < displayAwayReds; ++i) {
		sf::RectangleShape redCard(sf::Vector2f(8.f, 12.f));
		redCard.setFillColor(sf::Color(220, 20, 20, 255));
		redCard.setOutlineThickness(1.f);
		redCard.setOutlineColor(sf::Color(255, 255, 255, 150));

		// Space them 12px apart, building backwards from the right color bar
		redCard.setPosition({ scorePos.x + scoreWidth - 28.f - (i * 12.f), scorePos.y - 13.f });
		t_window.draw(redCard);
	}

	// --- SCORE TEXT ---
	int hScore = m_referee.getHomeScore();
	int aScore = m_referee.getAwayScore();

	std::string homeName = m_homeTeamData.shortName.empty() ? "HOME" : m_homeTeamData.shortName;
	std::string awayName = m_awayTeamData.shortName.empty() ? "AWAY" : m_awayTeamData.shortName;

	std::string scoreString = homeName + "  " + std::to_string(hScore) + " - " + std::to_string(aScore) + "  " + awayName;

	sf::Text scoreText(m_font, scoreString, 22);
	scoreText.setFillColor(sf::Color::White);

	float textX = scorePos.x + (scoreWidth - scoreText.getLocalBounds().size.x) / 2.f;
	scoreText.setPosition({ textX, scorePos.y + 10.f });
	t_window.draw(scoreText);

	// 3. Format the Match Timer Text
	float matchTime = m_referee.getMatchMinute();
	int mins = static_cast<int>(matchTime);
	int secs = static_cast<int>((matchTime - mins) * 60.0f);

	// Add leading zeroes if numbers are < 10 (e.g., "05:09")
	std::string timeString = (mins < 10 ? "0" : "") + std::to_string(mins) + ":" +
		(secs < 10 ? "0" : "") + std::to_string(secs);

	sf::Text timeText(m_font, timeString, 18);
	timeText.setFillColor(sf::Color::Yellow);

	// Center the time text below the score
	float timeX = scorePos.x + (scoreWidth - timeText.getLocalBounds().size.x) / 2.f;
	timeText.setPosition({ timeX, scorePos.y + 35.f });
	t_window.draw(timeText);

	// ==========================================
	// --- E. PLAYER INFO PANEL (Bottom Right) ---
	// ==========================================
	if (m_userPlayer) {
		// 1. Fetch Player State
		float currentStamina = m_userPlayer->getCurrentStamina();
		float maxStamina = m_userPlayer->getMaxStamina();
		bool hasYellow = m_userPlayer->getYellowCards() > 0; // Make sure you have this getter in Player.h!
		bool hasRed = m_userPlayer->isSentOff();
		bool hasCard = hasYellow || hasRed;

		// 2. Setup Text
		std::string playerNum = std::to_string(m_userPlayer->getSquadNumber());
		std::string playerName = m_userPlayer->getName();
		std::string playerInfoStr = playerNum + "   " + playerName;

		sf::Text playerInfoText(m_font, playerInfoStr, 24);
		playerInfoText.setFillColor(sf::Color::White);

		// 3. Layout Dimensions
		float infoPadding = 20.f;
		float textWidth = playerInfoText.getLocalBounds().size.x;
		float cardSpacing = hasCard ? 25.f : 0.f; // Add 25px of width if we need to draw a card
		float infoBoxWidth = textWidth + 50.f + cardSpacing;
		float infoBoxHeight = 50.f;
		float staminaBarHeight = 6.f; // Thin bar attached to the bottom

		// Shift the Y position UP slightly to accommodate the new stamina bar below it
		sf::Vector2f infoBoxPos(
			t_window.getSize().x - infoBoxWidth - infoPadding,
			t_window.getSize().y - (infoBoxHeight + staminaBarHeight) - infoPadding
		);

		// 4. Draw Main Background
		sf::RectangleShape infoBg(sf::Vector2f(infoBoxWidth, infoBoxHeight));
		infoBg.setPosition(infoBoxPos);
		infoBg.setFillColor(sf::Color(20, 20, 30, 220));
		infoBg.setOutlineThickness(2.f);
		infoBg.setOutlineColor(sf::Color(255, 255, 255, 150));
		t_window.draw(infoBg);

		// 5. Draw User Team Accent Bar
		sf::Color userTeamColor = (m_userPlayer->getTeam() == Team::Home) ? m_homeTeamData.uiColor : m_awayTeamData.uiColor;
		userTeamColor.a = 255; // Force full opacity

		sf::RectangleShape playerAccent(sf::Vector2f(12.f, infoBoxHeight));
		playerAccent.setPosition(infoBoxPos);
		playerAccent.setFillColor(userTeamColor);
		t_window.draw(playerAccent);

		// 6. Draw Card Indicator (If booked)
		if (hasCard) {
			sf::RectangleShape cardIcon(sf::Vector2f(12.f, 18.f));
			cardIcon.setPosition({ infoBoxPos.x + 22.f, infoBoxPos.y + 16.f }); // Nestled next to the accent bar

			if (hasRed) {
				cardIcon.setFillColor(sf::Color(220, 20, 20, 255));
			}
			else {
				cardIcon.setFillColor(sf::Color(220, 200, 20, 255)); // Yellow
			}

			cardIcon.setOutlineThickness(1.f);
			cardIcon.setOutlineColor(sf::Color(255, 255, 255, 150));
			t_window.draw(cardIcon);
		}

		// 7. Draw Text (Pushed further to the right if there is a card present)
		playerInfoText.setPosition({ infoBoxPos.x + 25.f + cardSpacing, infoBoxPos.y + 10.f });
		t_window.draw(playerInfoText);

		// ==========================================
		// --- NEW: STAMINA BAR ---
		// ==========================================
		sf::Vector2f staminaPos(infoBoxPos.x, infoBoxPos.y + infoBoxHeight);

		// A. Background (Empty Gas Tank)
		sf::RectangleShape staminaBg(sf::Vector2f(infoBoxWidth, staminaBarHeight));
		staminaBg.setPosition(staminaPos);
		staminaBg.setFillColor(sf::Color(30, 30, 30, 240));
		staminaBg.setOutlineThickness(2.f);
		staminaBg.setOutlineColor(sf::Color(255, 255, 255, 150));
		t_window.draw(staminaBg);

		// B. Foreground (Current Gas)
		float staminaRatio = std::clamp(currentStamina / maxStamina, 0.0f, 1.0f);

		// Only draw the fill if they actually have stamina left
		if (staminaRatio > 0.01f)
		{
			sf::RectangleShape staminaFill(sf::Vector2f(infoBoxWidth * staminaRatio, staminaBarHeight));
			staminaFill.setPosition(staminaPos);

			// Color gradient: Green -> Yellow -> Red based on exhaustion
			sf::Color stamColor;
			if (staminaRatio > 0.5f) {
				stamColor = sf::Color(40, 200, 40, 255);       // Healthy Green
			}
			else if (staminaRatio > 0.2f) {
				stamColor = sf::Color(220, 200, 20, 255);      // Warning Yellow
			}
			else {
				stamColor = sf::Color(220, 40, 40, 255);       // Danger Red
			}

			staminaFill.setFillColor(stamColor);
			t_window.draw(staminaFill);
		}
	}
	// ==========================================
		// --- F. GOAL BANNER (Broadcast Popup) ---
		// ==========================================
		// Static variables to remember the exact moment the goal happened
	static MatchState s_lastBannerState = MatchState::InPlay;
	static int s_savedGoalMinute = 0;

	if (m_referee.getMatchState() == MatchState::GoalScored)
	{
		// 0. Freeze the time on the exact frame the goal is scored
		if (s_lastBannerState != MatchState::GoalScored) {
			s_savedGoalMinute = static_cast<int>(m_referee.getMatchMinute());
		}

		// 1. Pull the official decision from the Referee
		std::string scorerName = m_referee.getLastGoalScorerName();
		std::string assistName = m_referee.getLastGoalAssistName();
		bool isOwnGoal = m_referee.getLastGoalWasOwnGoal();
		Team scoringTeam = m_referee.getLastGoalScoringTeam();

		if (isOwnGoal) {
			scorerName += " (O.G.)";
		}

		// Use the SCORING team's colors and name
		std::string teamName = (scoringTeam == Team::Home) ? m_homeTeamData.fullName : m_awayTeamData.fullName;
		sf::Color goalColor = (scoringTeam == Team::Home) ? m_homeTeamData.uiColor : m_awayTeamData.uiColor;
		goalColor.a = 255;

		// 2. Setup Text
		sf::Text goalText(m_font, isOwnGoal ? "OWN GOAL!" : "GOAL!", 60);
		goalText.setStyle(sf::Text::Bold);
		goalText.setFillColor(sf::Color::White);

		sf::Text infoText(m_font, scorerName + " (" + std::to_string(s_savedGoalMinute) + "')\n" + teamName, 30);
		infoText.setFillColor(sf::Color(220, 220, 220));
		infoText.setLineSpacing(1.2f);

		// Setup Assist Text (if applicable)
		sf::Text assistText(m_font, "", 22);
		bool hasAssist = (!assistName.empty() && !isOwnGoal);
		if (hasAssist) {
			assistText.setString("Assist: " + assistName);
			assistText.setFillColor(sf::Color(180, 180, 180));
		}

		// 3. Layout Dimensions
		float bannerWidth = 500.f;
		float bannerHeight = hasAssist ? 280.f : 250.f; // Make it slightly taller to fit the assist!
		sf::Vector2f centerPos(t_window.getSize().x * 0.5f, t_window.getSize().y * 0.35f);
		sf::Vector2f topLeft(centerPos.x - bannerWidth * 0.5f, centerPos.y - bannerHeight * 0.5f);

		// 4. Draw Background Shadow
		sf::RectangleShape bannerBg(sf::Vector2f(bannerWidth, bannerHeight));
		bannerBg.setPosition(topLeft);
		bannerBg.setFillColor(sf::Color(10, 10, 15, 230));
		bannerBg.setOutlineThickness(3.f);
		bannerBg.setOutlineColor(sf::Color(255, 255, 255, 100));
		t_window.draw(bannerBg);

		// 5. Draw Team Color Header Bar
		sf::RectangleShape headerBar(sf::Vector2f(bannerWidth, 10.f));
		headerBar.setPosition(topLeft);
		headerBar.setFillColor(goalColor);
		t_window.draw(headerBar);

		// 6. Position and Draw Text
		goalText.setPosition({
			centerPos.x - goalText.getLocalBounds().size.x * 0.5f,
			topLeft.y + 15.f
			});
		t_window.draw(goalText);

		infoText.setPosition({
			centerPos.x - infoText.getLocalBounds().size.x * 0.5f,
			topLeft.y + 85.f
			});
		t_window.draw(infoText);

		if (hasAssist) {
			assistText.setPosition({
				centerPos.x - assistText.getLocalBounds().size.x * 0.5f,
				topLeft.y + 180.f // Placed neatly below the team name
				});
			t_window.draw(assistText);
		}
	}

	// Update the tracking state at the end of the frame
	s_lastBannerState = m_referee.getMatchState();

	// ==========================================
		// --- G. SUBSTITUTION POPUP (Top Left) ---
		// ==========================================
	float startY = 100.f; // Base position under the scoreboard
	float subHeight = 110.f;
	float subSpacing = 15.f; // Gap between multiple popups

	// We use an iterator so we can safely delete expired events from the vector mid-loop
	for (auto it = m_activeSubEvents.begin(); it != m_activeSubEvents.end(); ) {
		it->timer -= 1.0f / 60.0f; // Approximate dt

		if (it->timer <= 0.f) {
			// Timer is dead, erase it from the list and move to the next one!
			it = m_activeSubEvents.erase(it);
		}
		else {
			float subWidth = 350.f;

			// Figure out which number in line this popup is (0, 1, 2, etc.)
			int index = std::distance(m_activeSubEvents.begin(), it);

			// Dynamically push the Y position down based on how many are above it!
			sf::Vector2f subPos(20.f, startY + (index * (subHeight + subSpacing)));

			// Background
			sf::RectangleShape subBg(sf::Vector2f(subWidth, subHeight));
			subBg.setPosition(subPos);
			subBg.setFillColor(sf::Color(20, 20, 30, 220));
			subBg.setOutlineThickness(2.f);
			subBg.setOutlineColor(sf::Color(255, 255, 255, 150));
			t_window.draw(subBg);

			// Accent Color
			sf::Color accent = it->teamColor; accent.a = 255;
			sf::RectangleShape subAccent(sf::Vector2f(12.f, subHeight));
			subAccent.setPosition(subPos);
			subAccent.setFillColor(accent);
			t_window.draw(subAccent);

			// Title
			sf::Text titleText(m_font, "SUBSTITUTION - " + it->teamName, 16);
			titleText.setFillColor(sf::Color(200, 200, 200));
			titleText.setPosition({ subPos.x + 25.f, subPos.y + 10.f });
			t_window.draw(titleText);

			// Red Down Arrow
			sf::RectangleShape redArrow(sf::Vector2f(10.f, 15.f));
			redArrow.setFillColor(sf::Color(220, 40, 40));
			redArrow.setPosition({ subPos.x + 25.f, subPos.y + 40.f });
			t_window.draw(redArrow);

			// Player Off
			sf::Text offText(m_font, std::to_string(it->numOff) + "  " + it->playerOff, 20);
			offText.setFillColor(sf::Color::White);
			offText.setPosition({ subPos.x + 45.f, subPos.y + 35.f });
			t_window.draw(offText);

			// Green Up Arrow
			sf::RectangleShape greenArrow(sf::Vector2f(10.f, 15.f));
			greenArrow.setFillColor(sf::Color(40, 220, 40));
			greenArrow.setPosition({ subPos.x + 25.f, subPos.y + 75.f });
			t_window.draw(greenArrow);

			// Player On
			sf::Text onText(m_font, std::to_string(it->numOn) + "  " + it->playerOn, 20);
			onText.setFillColor(sf::Color::White);
			onText.setPosition({ subPos.x + 45.f, subPos.y + 70.f });
			t_window.draw(onText);

			++it; // Move to the next popup
		}
	}

	// ==========================================
	// 3. RESTORE WORLD VIEW
	// ==========================================
	t_window.setView(worldView);
}

void MatchEngine::triggerForfeit(bool isHomeForfeit)
{
	m_gameOver = true;
	m_referee.applyForfeitScore(isHomeForfeit);

	std::string teamName = isHomeForfeit ? m_homeTeamData.fullName : m_awayTeamData.fullName;

	std::string forfeitStr = "MATCH ABANDONED\n\n" + teamName + " has fewer than 7 players.\nMatch forfeited (3 - 0).\n\nPress SPACE to return to Menu";

	m_gameOverText.setString(forfeitStr);

	// Recenter the text perfectly on the screen
	sf::FloatRect textSize = m_gameOverText.getGlobalBounds();
	float xpos = (1920.f / 2.f) - (textSize.size.x / 2.f);
	m_gameOverText.setPosition({ xpos, 1080.f * 0.4f });
}

void MatchEngine::executePlayerSwitch(Player* targetNPC, MatchEnvironment& env)
{
	// Safety checks
	if (!targetNPC || targetNPC == m_userPlayer.get() || targetNPC->getTeam() != m_userPlayer->getTeam()) return;
	if (targetNPC->getPositionRole() == PositionRole::Goalkeeper) return; // Don't let user accidentally possess the GK!

	// ==========================================
	// --- THE SOUL SWAP ---
	// ==========================================
	// Perform the clean, native C++ swap directly in the base class
	m_userPlayer->swapIdentityWith(targetNPC);

	// Fix Ball Ownership Tracker
	// Because the pointers didn't move but the 'hasPossession' bool did, 
	// we need to tell the Ball object who its new owner pointer is.
	if (m_ball->getOwner() == targetNPC) {
		m_ball->possess(m_userPlayer.get(), env);
	}
	else if (m_ball->getOwner() == m_userPlayer.get()) {
		m_ball->possess(targetNPC, env);
	}
}

void MatchEngine::powerBarUpdate()
{
	// 1. Get the current power (0.0 to 1.0)
	float power = m_userController->getKickStrength();

	// 2. Scale the width based on the power
	// If power is 0.5, the width becomes 100px (half of 200px)
	barFill.setSize(sf::Vector2f(barSize.x * power, barSize.y));

	// 3. Optional: Dynamic Color (Green -> Red)
	// As power increases, the bar gets "hotter"
	int red = static_cast<int>(255 * power);
	int green = static_cast<int>(255 * (1.0f - power));
	barFill.setFillColor(sf::Color(red, green, 0));
}

void MatchEngine::powerBarDraw(sf::RenderWindow& t_window)
{
	t_window.setView(t_window.getDefaultView());

	// Position the bar (e.g., bottom center)
	sf::Vector2f uiPos(t_window.getSize().x / 2.f - barSize.x / 2.f,
		t_window.getSize().y - 75.f);
	barBackground.setPosition(uiPos);
	barFill.setPosition(uiPos);

	t_window.draw(barBackground);
	t_window.draw(barFill);
}

void MatchEngine::drawMatchStats(sf::RenderWindow& t_window)
{
	// 1. Setup the semi-transparent background panel
	sf::Vector2u winSize = t_window.getSize();
	float panelWidth = 800.f;
	float panelHeight = 600.f;

	sf::RectangleShape panel(sf::Vector2f(panelWidth, panelHeight));
	panel.setFillColor(sf::Color(15, 15, 20, 230)); // Dark, slightly transparent
	panel.setOutlineThickness(2.f);
	panel.setOutlineColor(sf::Color(100, 100, 100, 200));

	// Center it on the screen
	sf::Vector2f panelPos((winSize.x - panelWidth) / 2.f, (winSize.y - panelHeight) / 4.f);
	panel.setPosition(panelPos);
	t_window.draw(panel);

	// 2. Setup Typography
	sf::Text text(m_font);
	text.setFont(m_font); // Ensure m_font is loaded!
	text.setFillColor(sf::Color::White);

	// --- DRAW TITLE ---
	text.setCharacterSize(40);
	text.setString(m_referee.getMatchState() == MatchState::FullTime ? "FULL TIME" :
		(m_referee.getMatchState() == MatchState::HalfTime ? "HALF TIME" : "MATCH PAUSED"));

	// Center the title
	sf::FloatRect textBounds = text.getLocalBounds();
	text.setPosition({ panelPos.x + (panelWidth - textBounds.size.x) / 2.f, panelPos.y + 20.f });
	t_window.draw(text);

	// --- DRAW TEAM NAMES & SCORE ---
	text.setCharacterSize(50);
	std::string scoreStr = std::to_string(m_matchStats.home.goals) + " - " + std::to_string(m_matchStats.away.goals);

	// Home Team Name (Left)
	text.setString(m_homeTeamData.shortName);
	text.setPosition({ panelPos.x + 100.f, panelPos.y + 80.f });
	t_window.draw(text);

	// Score (Center)
	text.setString(scoreStr);
	textBounds = text.getLocalBounds();
	text.setPosition({panelPos.x + (panelWidth - textBounds.size.x) / 2.f, panelPos.y + 80.f});
	t_window.draw(text);

	// Away Team Name (Right)
	text.setString(m_awayTeamData.shortName);
	textBounds = text.getLocalBounds();
	text.setPosition({ panelPos.x + panelWidth - 100.f - textBounds.size.x, panelPos.y + 80.f });
	t_window.draw(text);

	// --- DRAW GOALSCORERS ---
	text.setCharacterSize(18);
	text.setFillColor(sf::Color(200, 200, 200));

	float homeScorerY = panelPos.y + 150.f;
	for (const auto& event : m_matchStats.home.goalEvents) {
		text.setString(event);
		text.setPosition({ panelPos.x + 100.f, homeScorerY });
		t_window.draw(text);
		homeScorerY += 25.f;
	}

	float awayScorerY = panelPos.y + 150.f;
	for (const auto& event : m_matchStats.away.goalEvents) {
		text.setString(event);
		textBounds = text.getLocalBounds();
		text.setPosition({ panelPos.x + panelWidth - 100.f - textBounds.size.x, awayScorerY });
		t_window.draw(text);
		awayScorerY += 25.f;
	}

	// --- HELPER LAMBDA TO DRAW STAT ROWS ---
	float currentY = panelPos.y + 250.f;
	auto drawStatRow = [&](const std::string& statName, const std::string& homeVal, const std::string& awayVal) {
		text.setCharacterSize(24);
		text.setFillColor(sf::Color::White);

		// Center Label
		text.setString(statName);
		sf::FloatRect bounds = text.getLocalBounds();
		text.setPosition({ panelPos.x + (panelWidth - bounds.size.x) / 2.f, currentY });
		t_window.draw(text);

		// Home Value (Left)
		text.setString(homeVal);
		text.setPosition({ panelPos.x + 150.f, currentY });
		t_window.draw(text);

		// Away Value (Right)
		text.setString(awayVal);
		bounds = text.getLocalBounds();
		text.setPosition({ panelPos.x + panelWidth - 150.f - bounds.size.x, currentY });
		t_window.draw(text);

		currentY += 45.f; // Spacing for next row
		};

	// --- DRAW STAT ROWS ---
	// 1. Possession
	float totalTime = m_matchStats.getTotalPossessionTime();
	char homePoss[16], awayPoss[16];
	snprintf(homePoss, sizeof(homePoss), "%.0f%%", m_matchStats.home.getPossessionPercent(totalTime));
	snprintf(awayPoss, sizeof(awayPoss), "%.0f%%", m_matchStats.away.getPossessionPercent(totalTime));
	drawStatRow("Possession", homePoss, awayPoss);

	// 2. Shots
	std::string homeShots = std::to_string(m_matchStats.home.shotsOnTarget + m_matchStats.home.shotsOffTarget)
		+ " (" + std::to_string(m_matchStats.home.shotsOnTarget) + ")";
	std::string awayShots = std::to_string(m_matchStats.away.shotsOnTarget + m_matchStats.away.shotsOffTarget)
		+ " (" + std::to_string(m_matchStats.away.shotsOnTarget) + ")";
	drawStatRow("Shots (On Target)", homeShots, awayShots);

	// 3. Passes
	char homePass[32], awayPass[32];
	snprintf(homePass, sizeof(homePass), "%d (%.0f%%)", m_matchStats.home.passesAttempted, m_matchStats.home.getPassCompletion());
	snprintf(awayPass, sizeof(awayPass), "%d (%.0f%%)", m_matchStats.away.passesAttempted, m_matchStats.away.getPassCompletion());
	drawStatRow("Passes (Completed %)", homePass, awayPass);

	// 4. Fouls
	drawStatRow("Fouls", std::to_string(m_matchStats.home.fouls), std::to_string(m_matchStats.away.fouls));
}

void MatchEngine::initPauseMenuButtons()
{
	if (!m_buttonTxt.loadFromFile("ASSETS/IMAGES/button.png")) {
		std::cout << "Can't load button texture in GamePlay\n";
	}

	sf::String btnStrings[] = { "Game Plan", "Return to Menu" };
	sf::IntRect txtRect({ 0, 0 }, { static_cast<int>(m_buttonTxt.getSize().x), static_cast<int>(m_buttonTxt.getSize().y) });

	for (int i = 0; i < 2; i++) {
		auto& sprite = m_pauseButtons.emplace_back(m_buttonTxt);
		sprite.setTextureRect(txtRect);
		sprite.setScale({ m_btnWidth / m_buttonTxt.getSize().x, m_btnHeight / m_buttonTxt.getSize().y });
		sprite.setColor(sf::Color{ 0, 100, 0, 255 }); // Dark green base

		auto& text = m_pauseTexts.emplace_back(m_font);
		text.setString(btnStrings[i]);
		text.setFillColor(sf::Color::White);
		text.setCharacterSize(40);
	}
}

void MatchEngine::updatePauseMenu(sf::RenderWindow& t_window)
{
	if (m_showGamePlan) return;

	sf::Vector2i mousePixel = sf::Mouse::getPosition(t_window);
	sf::Vector2f mouseLocation = t_window.mapPixelToCoords(mousePixel, t_window.getDefaultView());

	sf::Vector2u winSize = t_window.getSize();
	float xOffset = (winSize.x / 2.f) - (m_btnWidth / 2.f);
	float statsBoardBottom = ((winSize.y) / 4.f) + 450.f;
	float yOffset = statsBoardBottom + 20.f;

	bool isFullTime = (m_referee.getMatchState() == MatchState::FullTime);
	m_showWarning = false; // Reset warning flag every frame

	for (int i = 0; i < 2; i++)
	{
		// 1. Hide Game Plan at Full Time
		if (isFullTime && i == 0) continue;

		// 2. Position the elements
		float currentY = yOffset + (m_btnSpacing * i);

		// If it's Full Time, slide the "Return" button up to the top slot!
		if (isFullTime && i == 1) currentY = yOffset;

		m_pauseButtons[i].setPosition({ xOffset, currentY });

		sf::FloatRect textSize = m_pauseTexts[i].getLocalBounds();
		float textXOffset = (m_btnWidth - textSize.size.x) / 2.f;
		m_pauseTexts[i].setPosition({ xOffset + textXOffset, currentY + 15.f });

		m_pauseButtons[i].setColor(sf::Color{ 0, 100, 0, 255 });
		m_pauseTexts[i].setFillColor(sf::Color::White);

		// 3. Hover & Click Logic
		if (mouseLocation.x > xOffset && mouseLocation.x < xOffset + m_btnWidth &&
			mouseLocation.y > currentY && mouseLocation.y < currentY + m_btnHeight)
		{
			m_pauseButtons[i].setColor(sf::Color{ 0, 50, 0, 255 });

			// THE FIX: Show warning if hovering over "Return" while match is active
			if (i == 1 && !isFullTime) {
				m_showWarning = true;

				// Center the warning text directly below the button
				sf::FloatRect warnBounds = m_warningText.getLocalBounds();
				m_warningText.setPosition({ (winSize.x - warnBounds.size.x) / 2.f, currentY + m_btnHeight + 20.f });
			}

			if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
			{
				if (i == 0) m_showGamePlan = true;
				if (i == 1) m_exitRequested = true; // Raise the flag!
			}
		}
	}
}

void MatchEngine::drawGamePlan(sf::RenderWindow& t_window)
{
	if (!m_showGamePlan || !m_userPlayer) return;

	ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2((t_window.getSize().x - 800) / 2.f, (t_window.getSize().y - 600) / 2.f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Game Plan", &m_showGamePlan))
	{
		TeamData* myTeam = (m_userPlayer->getTeam() == Team::Home) ? &m_homeTeamData : &m_awayTeamData;
		std::vector<Player*>& liveTeam = (m_userPlayer->getTeam() == Team::Home) ? m_homeTeam : m_awayTeam;
		int subsUsed = (m_userPlayer->getTeam() == Team::Home) ? m_homeSubsUsed : m_awaySubsUsed;

		if (myTeam && ImGui::BeginTabBar("GamePlanTabs"))
		{
			// ==========================================
			// TAB 1: TACTICS
			// ==========================================
			if (ImGui::BeginTabItem("Tactics"))
			{
				ImGui::Text("Team Tactics: %s", myTeam->fullName.c_str());
				ImGui::Separator();

				TeamTactics& tactics = myTeam->defaultTactics;

				const char* formations[] = { "4-4-2", "4-3-3", "4-2-4", "5-3-2", "5-2-3", "5-4-1" };
				int currentFmtIdx = 0;
				for (int i = 0; i < IM_ARRAYSIZE(formations); i++) {
					if (tactics.formationName == formations[i]) currentFmtIdx = i;
				}
				if (ImGui::Combo("Formation", &currentFmtIdx, formations, IM_ARRAYSIZE(formations))) {
					tactics.formationName = formations[currentFmtIdx];
				}

				ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
				ImGui::TextDisabled("0 = Safe/Deep | 100 = Aggressive/High");
				ImGui::SliderInt("Defensive Depth", &tactics.defensiveDepth, 0, 100);
				ImGui::SliderInt("Pressing Intensity", &tactics.pressingIntensity, 0, 100);

				ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
				ImGui::TextDisabled("0 = Short/Slow | 100 = Long/Fast");
				ImGui::SliderInt("Passing Length", &tactics.passingLength, 0, 100);
				ImGui::SliderInt("Passing Speed", &tactics.passingSpeed, 0, 100);
				ImGui::SliderInt("Attacking Speed", &tactics.attackingSpeed, 0, 100);

				ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
				ImGui::SliderInt("Attacking Width", &tactics.attackingWidth, 0, 100);
				ImGui::SliderInt("Positional Freedom", &tactics.positionalFreedom, 0, 100);

				ImGui::EndTabItem();
			}

			// ==========================================
			// TAB 2: TEAM MANAGEMENT (SUBS)
			// ==========================================
			if (ImGui::BeginTabItem("Team Management"))
			{
				static int selectedStarter = -1;
				static int selectedSub = -1;

				ImGui::Text("Substitutions Used: %d / %d", subsUsed, MAX_SUBS);
				ImGui::Separator();

				// Pitch Players Column
				ImGui::BeginChild("Pitch", ImVec2(360, 450), true);
				ImGui::Text("On Pitch");
				ImGui::Separator();

				// Helper to group players by their physical line
				auto getCat = [](PositionRole r) {
					if (r == PositionRole::Goalkeeper) return 0;
					if (r == PositionRole::CenterBack || r == PositionRole::LeftBack || r == PositionRole::RightBack || r == PositionRole::LeftWingBack || r == PositionRole::RightWingBack) return 1;
					if (r == PositionRole::Striker || r == PositionRole::CenterForward || r == PositionRole::LeftWing || r == PositionRole::RightWing) return 3;
					return 2; // Midfielders
					};

				const char* catNames[] = { "GOALKEEPERS", "DEFENDERS", "MIDFIELDERS", "ATTACKERS" };

				// Loop through the 4 tactical lines
				for (int cat = 0; cat < 4; ++cat) {
					bool hasPrintedHeader = false;

					for (int i = 0; i < 11; ++i) {
						Player* p = liveTeam[i];
						if (!p || getCat(p->getPositionRole()) != cat) continue;

						// Print the category header once
						if (!hasPrintedHeader) {
							ImGui::Spacing();
							ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", catNames[cat]);
							hasPrintedHeader = true;
						}

						bool isQueued = false;
						for (const auto& sub : m_pendingSubsQueue) {
							if (sub.team == m_userPlayer->getTeam() && sub.pitchIndex == i) {
								isQueued = true; break;
							}
						}

						std::string label = roleToString(p->getPositionRole()) + "  |  " + p->getName();
						if (p->getState() == PlayerState::Injured) label += " [INJURED]";
						if (p->getYellowCards() > 0) label += " [YELLOW]";
						if (p->isSentOff()) label += " [RED CARD]";
						if (isQueued) label += " [QUEUED]";

						// Disable if sent off or already queued
						if (p->isSentOff() || isQueued) {
							ImGui::TextDisabled("%s", label.c_str());
						}
						else {
							// Notice we still use 'i' as the selectedStarter so the array index is perfectly preserved!
							if (ImGui::Selectable(label.c_str(), selectedStarter == i)) {
								selectedStarter = i;
							}
						}

						float stamPercent = p->getCurrentStamina() / p->getMaxStamina();
						ImVec4 barColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
						if (stamPercent < 0.5f) barColor = ImVec4(0.8f, 0.8f, 0.2f, 1.0f);
						if (stamPercent < 0.25f) barColor = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);

						ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
						ImGui::ProgressBar(stamPercent, ImVec2(150.f, 6.f), "");
						ImGui::PopStyleColor();
						ImGui::Spacing();
					}
				}
				ImGui::EndChild();

				ImGui::SameLine();

				// Bench Players Column
				ImGui::BeginChild("Bench", ImVec2(360, 450), true);
				ImGui::Text("Bench");
				ImGui::Separator();

				// Grab the correct burn list for the user's team
				std::vector<std::string>& subbedOutList = (m_userPlayer->getTeam() == Team::Home) ? m_homeSubbedOutIds : m_awaySubbedOutIds;

				for (int i = 0; i < myTeam->bench.size(); ++i) {

					bool isQueued = false;
					for (const auto& sub : m_pendingSubsQueue) {
						if (sub.team == m_userPlayer->getTeam() && sub.benchIndex == i) {
							isQueued = true; break;
						}
					}

					// ==========================================
					// --- THE FIX: CHECK THE BURN LIST ---
					// ==========================================
					bool isSubbedOut = std::find(subbedOutList.begin(), subbedOutList.end(), myTeam->bench[i].id) != subbedOutList.end();

					std::string label = roleToString(myTeam->bench[i].positionRole) + "  |  " + myTeam->bench[i].name;
					if (isQueued) label += " [QUEUED]";
					if (isSubbedOut) label += " [SUBBED OUT]";

					// Disable the bench player if they are queued OR if they have already been subbed out!
					if (isQueued || isSubbedOut) {
						ImGui::TextDisabled("%s", label.c_str());
					}
					else {
						if (ImGui::Selectable(label.c_str(), selectedSub == i)) {
							selectedSub = i;
						}
					}
				}
				ImGui::EndChild();

				ImGui::Spacing();

				// Execution Button
				if (selectedStarter != -1 && selectedSub != -1) {
					// Check if we have enough subs left considering the ones already in the queue!
					if (subsUsed + m_pendingSubsQueue.size() < MAX_SUBS) {
						if (ImGui::Button("Queue Substitution", ImVec2(730, 40))) {
							// THE FIX: Call queueSubstitution instead!
							queueSubstitution(m_userPlayer->getTeam(), selectedStarter, selectedSub);
							selectedStarter = -1;
							selectedSub = -1;
						}
					}
					else {
						ImGui::TextColored(ImVec4(1, 0, 0, 1), "Out of substitutions!");
					}
				}
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		else if (!myTeam) {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: Could not load Team Data.");
		}
	}
	ImGui::End();
}

void MatchEngine::performSubstitution(Team team, int pitchIndex, int benchIndex)
{
	std::vector<Player*>& liveTeam = (team == Team::Home) ? m_homeTeam : m_awayTeam;
	TeamData& teamData = (team == Team::Home) ? m_homeTeamData : m_awayTeamData;
	int& subsUsed = (team == Team::Home) ? m_homeSubsUsed : m_awaySubsUsed;

	// Safety check: Don't exceed the sub limit
	if (subsUsed >= MAX_SUBS) return;

	Player* pitchPlayer = liveTeam[pitchIndex];
	PlayerData subData = teamData.bench[benchIndex];

	// ==========================================
	// --- THE ANTI-CLONING VAULT ---
	// ==========================================
	// DO NOT rely on secondary arrays that might be out of sync.
	// Ask the physical player object who they are, and pull their exact 
	// original file from the Database to drop onto the bench!
	PlayerData* outgoingData = m_db->getPlayer(pitchPlayer->getId());
	if (!outgoingData) return; // Safety abort if the player somehow doesn't exist

	// ==========================================
	// --- THE FIX: RECORD APPEARANCE STATS ---
	// ==========================================
	int currentMinute = static_cast<int>(m_referee.getMatchMinute());
	std::string teamIdStr = (team == Team::Home) ? m_matchInfo.getHomeTeamId() : m_matchInfo.getAwayTeamId();

	// Clock out the old player, clock in the new player!
	m_matchInfo.recordAppearanceEnd(outgoingData->id, currentMinute);
	m_matchInfo.recordAppearanceStart(subData.id, teamIdStr, currentMinute);

	// ==========================================
	// --- BURN THE PLAYER ID ---
	// ==========================================
	// Record the outgoing player so they can take no further part in the match!
	if (team == Team::Home) m_homeSubbedOutIds.push_back(outgoingData->id);
	else m_awaySubbedOutIds.push_back(outgoingData->id);

	PositionRole preservedRole = pitchPlayer->getPositionRole();

	// 1. Send the accurate outgoing player data to the bench
	teamData.bench[benchIndex] = *outgoingData;

	// 2. Re-skin the physics object with the incoming bench player
	pitchPlayer->applySubstitution(subData, teamData);

	// 3. Force the new player to inherit the tactical responsibilities
	pitchPlayer->setPositionRole(preservedRole);

	// ==========================================
	// --- EVENT UI QUEUE ---
	// ==========================================
	SubEvent newEvent;
	newEvent.team = team;
	newEvent.timer = 6.5f; // Show for 4 seconds
	newEvent.teamName = teamData.fullName;
	newEvent.teamColor = teamData.uiColor;
	newEvent.playerOff = outgoingData->name; // Real outgoing name!
	newEvent.numOff = outgoingData->squadNumber;
	newEvent.playerOn = subData.name;
	newEvent.numOn = subData.squadNumber;

	m_activeSubEvents.push_back(newEvent);

	// Ensure Home team subs display on top if simultaneous
	std::stable_sort(m_activeSubEvents.begin(), m_activeSubEvents.end(),
		[](const SubEvent& a, const SubEvent& b) {
			return a.team == Team::Home && b.team == Team::Away;
		});

	subsUsed++;
}

void MatchEngine::handleAISubstitutions()
{
	// ==========================================
	// --- 1. WAIT FOR DEAD BALL ---
	// ==========================================
	// Do absolutely nothing if the ball is rolling! 
	if (m_referee.getMatchState() == MatchState::InPlay) return;

	// ==========================================
	// --- 2. EXECUTE USER SUBS ---
	// ==========================================
	// The referee blew the whistle. Execute any subs the user queued up!
	for (auto& sub : m_pendingSubsQueue) {
		performSubstitution(sub.team, sub.pitchIndex, sub.benchIndex);
	}
	m_pendingSubsQueue.clear();

	// ==========================================
	// --- 3. EXECUTE AI SUBS ---
	// ==========================================
	Team aiTeam = (m_userPlayer && m_userPlayer->getTeam() == Team::Home) ? Team::Away : Team::Home;
	int& subsUsed = (aiTeam == Team::Home) ? m_homeSubsUsed : m_awaySubsUsed;

	if (subsUsed >= MAX_SUBS) return;

	std::vector<Player*>& liveTeam = (aiTeam == Team::Home) ? m_homeTeam : m_awayTeam;
	TeamData& aiTeamData = (aiTeam == Team::Home) ? m_homeTeamData : m_awayTeamData;

	// Helper lambda to categorize roles
	auto getCategory = [](PositionRole r) {
		if (r == PositionRole::Goalkeeper) return 0;
		if (r == PositionRole::CenterBack || r == PositionRole::LeftBack || r == PositionRole::RightBack || r == PositionRole::LeftWingBack || r == PositionRole::RightWingBack) return 1;
		if (r == PositionRole::Striker || r == PositionRole::CenterForward || r == PositionRole::LeftWing || r == PositionRole::RightWing) return 3;
		return 2; // Midfielders
		};

	for (int i = 0; i < 11; i++) {
		Player* p = liveTeam[i];
		if (p->isSentOff()) continue;

		bool needsSub = false;

		// ==========================================
		// --- THE FIX 1 & 2: FATIGUE & 2ND HALF ---
		// ==========================================
		float matchMinute = m_referee.getMatchMinute();

		// Condition 1: INJURY (Immediate)
		if (p->getState() == PlayerState::Injured) {
			needsSub = true;
		}
		// Condition 2: TACTICAL PROTECTION (Defender on a yellow late in the game)
		else if (matchMinute > 60.0f && p->getYellowCards() > 0) {
			if (getCategory(p->getPositionRole()) == 1) { // Is Defender
				needsSub = true;
			}
		}
		// Condition 3: EXTREME FATIGUE (2nd Half Only)
		else if (matchMinute > 45.0f) {
			// Calculate the percentage of stamina remaining
			float stamPercent = (p->getCurrentStamina() / p->getMaxStamina()) * 100.0f;

			// If they are completely dead on their feet (under 25%), sub them!
			// If they are getting tired (under 40%) but it's very late in the game (80+ min), sub them!
			if (stamPercent < 25.0f || (matchMinute > 80.0f && stamPercent < 40.0f)) {
				// Don't waste a sub on a tired Goalkeeper unless it's an injury
				if (p->getPositionRole() != PositionRole::Goalkeeper) {
					needsSub = true;
				}
			}
		}

		if (needsSub) {
			int targetCategory = getCategory(p->getPositionRole());
			int bestBenchIndex = -1;

			// Grab the correct burn list for the AI
			std::vector<std::string>& aiSubbedOutList = (aiTeam == Team::Home) ? m_homeSubbedOutIds : m_awaySubbedOutIds;

			// Find a bench player that matches the category
			for (size_t b = 0; b < aiTeamData.bench.size(); ++b) {
				// THE FIX: Skip players who have already played!
				if (std::find(aiSubbedOutList.begin(), aiSubbedOutList.end(), aiTeamData.bench[b].id) != aiSubbedOutList.end()) continue;

				if (getCategory(aiTeamData.bench[b].positionRole) == targetCategory) {
					bestBenchIndex = b;
					break;
				}
			}

			// Fallback: If no exact match, just throw on the first available outfielder
			if (bestBenchIndex == -1 && targetCategory != 0) {
				for (size_t b = 0; b < aiTeamData.bench.size(); ++b) {
					// THE FIX: Skip players who have already played!
					if (std::find(aiSubbedOutList.begin(), aiSubbedOutList.end(), aiTeamData.bench[b].id) != aiSubbedOutList.end()) continue;

					if (aiTeamData.bench[b].positionRole != PositionRole::Goalkeeper) {
						bestBenchIndex = b; break;
					}
				}
			}

			if (bestBenchIndex != -1) {
				performSubstitution(aiTeam, i, bestBenchIndex);
				return;
			}
		}
	}
}

void MatchEngine::queueSubstitution(Team team, int pitchIndex, int benchIndex) {
	// Prevent duplicates! If the user changes their mind and queues a different sub 
	// for the same spot, we remove the old request so the arrays don't get corrupted.
	m_pendingSubsQueue.erase(std::remove_if(m_pendingSubsQueue.begin(), m_pendingSubsQueue.end(),
		[&](const PendingSub& s) {
			return s.team == team && (s.pitchIndex == pitchIndex || s.benchIndex == benchIndex);
		}),
		m_pendingSubsQueue.end());

	m_pendingSubsQueue.push_back({ team, pitchIndex, benchIndex });
}

void MatchEngine::drawDebugHitboxes(sf::RenderWindow& window) {
	// 1. Gather all active players
	std::vector<Player*> allPlayers;
	allPlayers.push_back(m_userPlayer.get());
	for (auto& tm : m_homeside) allPlayers.push_back(tm.get());
	for (auto& opp : m_awayside) allPlayers.push_back(opp.get());

	for (Player* p : allPlayers) {
		if (p->isSentOff()) continue;

		// ==========================================
		// 1. CORE BOUNDING BOX (Yellow)
		// ==========================================
		sf::FloatRect bounds = p->getBoundingBox();

		sf::RectangleShape boundingBoxShape(bounds.size);
		boundingBoxShape.setPosition(bounds.position);
		boundingBoxShape.setFillColor(sf::Color::Transparent);
		boundingBoxShape.setOutlineThickness(2.0f);

		// Make Goalkeeper hitboxes Cyan when diving so you can see them expand!
		if (p->getPositionRole() == PositionRole::Goalkeeper && p->getState() == PlayerState::Diving) {
			boundingBoxShape.setOutlineColor(sf::Color::Cyan);
		}
		else {
			boundingBoxShape.setOutlineColor(sf::Color::Yellow);
		}

		window.draw(boundingBoxShape);

		// ==========================================
		// 2. TACKLE REACH BOX (Red)
		// ==========================================
		// We only draw the tackle box if they are actively tackling, 
		// otherwise the screen will be covered in red squares!
		if (p->getState() == PlayerState::Tackling) {
			sf::FloatRect tackleBounds = p->getTackleHitbox();

			sf::RectangleShape tackleBoxShape(tackleBounds.size);
			tackleBoxShape.setPosition(tackleBounds.position);

			// Fill it with a very faint red so you can see the active threat area
			tackleBoxShape.setFillColor(sf::Color(255, 0, 0, 80));
			tackleBoxShape.setOutlineThickness(2.0f);
			tackleBoxShape.setOutlineColor(sf::Color::Red);

			window.draw(tackleBoxShape);
		}
	}
}

void MatchEngine::drawDebugOffsideLines(sf::RenderWindow& window) {
	// We make them semi-transparent (150 alpha) so they don't blind you
	// Note: SFML 3 allows initializing colors with sf::Color(rgba) or sf::Color(r,g,b,a)
	sf::Color homeColor(0, 255, 0, 150); // Green for Home
	sf::Color awayColor(255, 0, 0, 150); // Red for Away

	// ==========================================
	// 1. HOME TEAM OFFSIDE LINE 
	// (The line the Away Strikers must stay behind)
	// ==========================================
	float homeLineX = m_homeTeamAI->getOffsideLineX();

	// FIX: Using sf::PrimitiveType::Lines for SFML 3
	sf::VertexArray homeLine(sf::PrimitiveType::Lines, 2);
	homeLine[0].position = sf::Vector2f(homeLineX, m_pitch.margin);
	homeLine[1].position = sf::Vector2f(homeLineX, m_pitch.totalHeight - m_pitch.margin);
	homeLine[0].color = homeColor;
	homeLine[1].color = homeColor;

	window.draw(homeLine);

	// ==========================================
	// 2. AWAY TEAM OFFSIDE LINE 
	// (The line the Home Strikers must stay behind)
	// ==========================================
	float awayLineX = m_awayTeamAI->getOffsideLineX();

	// FIX: Using sf::PrimitiveType::Lines for SFML 3
	sf::VertexArray awayLine(sf::PrimitiveType::Lines, 2);
	awayLine[0].position = sf::Vector2f(awayLineX, m_pitch.margin);
	awayLine[1].position = sf::Vector2f(awayLineX, m_pitch.totalHeight - m_pitch.margin);
	awayLine[0].color = awayColor;
	awayLine[1].color = awayColor;

	window.draw(awayLine);
}

void MatchEngine::drawDebugNames(sf::RenderWindow& window, const sf::Font& font) {
	// 1. Setup the Text Object
	// In SFML 3, the constructor takes the font directly
	sf::Text nameText(font);
	nameText.setCharacterSize(32); // Keep it small so the screen doesn't get cluttered
	nameText.setFillColor(sf::Color::White);

	// Add a black outline so the names are readable against the green grass
	nameText.setOutlineColor(sf::Color::Black);
	nameText.setOutlineThickness(1.5f);

	// 2. Gather all players
	std::vector<Player*> allPlayers;
	if (m_userPlayer)
	{
		allPlayers.push_back(m_userPlayer.get());
	}
	for (auto& tm : m_homeside) allPlayers.push_back(tm.get());
	for (auto& opp : m_awayside) allPlayers.push_back(opp.get());

	// 3. Draw names above their heads
	for (Player* p : allPlayers) {
		// Skip players who are sent off (if they are teleported off-screen)
		if (p->isSentOff()) continue;

		nameText.setString(p->getName());

		// SFML 3 RECTANGLE SYNTAX: Center the text perfectly
		sf::FloatRect bounds = nameText.getLocalBounds();
		nameText.setOrigin({
			bounds.position.x + (bounds.size.x / 2.0f),
			bounds.position.y + bounds.size.y
			});
		nameText.setRotation(sf::degrees(90.f));
		// Position it just above the player's coordinates. 
		// You might need to tweak the -60.f depending on how tall your sprites are!
		nameText.setPosition(p->getPosition() + sf::Vector2f(100.f, 0.f));

		window.draw(nameText);
	}
}

float MatchEngine::distance(sf::Vector2f a, sf::Vector2f b)
{
	sf::Vector2f d = a - b;
	return std::sqrt(d.x * d.x + d.y * d.y);
}

// Helper: Normalize
sf::Vector2f MatchEngine::normalize(sf::Vector2f source) {
	float length = std::sqrt(source.x * source.x + source.y * source.y);
	if (length != 0) return source / length;
	return source;
}

#include "GamePlay.h"
#include "Game.h"
#include "imgui-1.92.6/imgui.h"

GamePlay::GamePlay() : m_pauseText(m_font), m_gameOverText(m_font), m_gameWonText(m_font)
{
}

GamePlay::~GamePlay()
{
}

/// <summary>
/// Initialise text
/// </summary>
/// <param name="t_font"></param>
void GamePlay::initialise(sf::Font& t_font)
{
	m_font = t_font;
	m_pauseText.setFont(m_font); // Text seen on the screen
	m_pauseText.setString("Game Paused\nPress Esc to Unpause\n Press Space to return to Main Menu");
	m_pauseText.setCharacterSize(42);
	m_pauseText.setFillColor(sf::Color::Red);
	m_pauseText.setStyle(sf::Text::Bold);

	sf::FloatRect textSize = m_pauseText.getGlobalBounds(); // will be used to put the text in the middle
	float xpos = (1920 / 2) - (textSize.size.x / 2);
	m_pauseText.setPosition({ xpos, 1080 * 0.5f });

	m_gameOverText.setFont(m_font); // Text seen on the screen
	m_gameOverText.setString("");
	m_gameOverText.setCharacterSize(42);
	m_gameOverText.setFillColor(sf::Color::Red);
	m_gameOverText.setStyle(sf::Text::Bold);
	textSize = m_gameOverText.getGlobalBounds(); // will be used to put the text in the middle
	xpos = (1920 / 2) - (textSize.size.x / 2);
	m_gameOverText.setPosition({ xpos, 1080 * 0.5f });

	m_gameWonText.setFont(m_font); // Text seen on the screen
	m_gameWonText.setString("");
	m_gameWonText.setCharacterSize(42);
	m_gameWonText.setFillColor(sf::Color::Red);
	m_gameWonText.setStyle(sf::Text::Bold);
	textSize = m_gameWonText.getGlobalBounds(); // will be used to put the text in the middle
	xpos = (1920 / 2) - (textSize.size.x / 2);
	m_gameWonText.setPosition({ xpos, 1080 * 0.5f });


	m_animServer.init("ASSETS/PLAYER/player_run_ing.png");

}

/// <summary>
/// handle user and system events / inputs
/// get key pressed, mouse moves etc. from OS
/// </summary>
void GamePlay::processEvents(sf::Event& t_event, sf::RenderWindow& t_window)
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
void GamePlay::processKeys(sf::Event t_event)
{
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape))
	{
		if (!m_pause && !m_gameOver)
		{
			m_pause = true;
		}
		else if (m_pause)
		{
			m_pause = false;
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
void GamePlay::update(sf::Time& t_deltaTime, sf::RenderWindow& t_window)
{
	float dt = t_deltaTime.asSeconds();


	// ==========================================
	// 1. THE SMOKE AND MIRRORS REPLAY MEDIATOR
	// ==========================================
	if (m_referee.getMatchState() == MatchState::RequestReplay)
	{
		m_replayEngine.startReplay(0.7f);

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
		// The isReplaying() check just became false this exact frame.
		// Release the hold and blow the whistle!
		m_referee.resumeFromReplay();
	}

    // 1. Gather all players
	std::vector<Player*> homeFriends;
	if (m_userPlayer)
	{
		homeFriends.push_back(m_userPlayer.get());
	}
	for (auto& npc : m_homeside) homeFriends.push_back(npc.get());

	std::vector<Player*> homeEnemies;
	for (auto& opp : m_awayside) homeEnemies.push_back(opp.get());
	
    std::vector<Player*> allPlayers = homeFriends;
	allPlayers.insert(allPlayers.end(), homeEnemies.begin(), homeEnemies.end());

	m_homeTeamAI->update(homeEnemies, *m_ball, m_pitch);
	m_awayTeamAI->update(homeFriends, *m_ball, m_pitch);

	if (!m_pause && !m_gameOver)
	{
		// 2. THE REFEREE UPDATES THE CONTEXTS FIRST
		m_referee.update(*m_ball, m_pitch, allPlayers, dt, m_homeGoal, m_awayGoal, m_soundManager);
		m_referee.checkOffsideLogic(*m_ball, allPlayers, m_homeTeamAI->getOffsideLineX(), m_awayTeamAI->getOffsideLineX(), m_pitch, m_soundManager);
		// 3. RUN THE SIMULATION
        // This handles InPlay, ThrowIns, Corners, AND Celebrations automatically now!
        runStandardSystems(dt, t_window);

		// 4. Update visuals
		updateCamera(t_window);
		if ( m_userPlayer)
		{
			powerBarUpdate();
		}
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
void GamePlay::render(sf::RenderWindow& t_window)
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

	if (m_userController && !m_replayEngine.isReplaying()) {
		m_userController->draw(t_window);
	}

	// 3. Sort Entities by Depth
	std::sort(m_entities.begin(), m_entities.end(), [](Entity* a, Entity* b) {
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

		m_replayEngine.render(t_window);
	}
	else {
		// A. Draw the Floor & Netting FIRST (Always behind the players)
		m_homeGoal.drawFloor(t_window); m_awayGoal.drawFloor(t_window);
		m_homeGoal.drawNet(t_window);   m_awayGoal.drawNet(t_window);

		// Grab the specific Goal Line depths for the Mid Layer
		float homeGoalDepth = m_homeGoal.center.x;
		float awayGoalDepth = m_awayGoal.center.x;

		bool homePostsDrawn = false;
		bool awayPostsDrawn = false;

		// B. Draw Live Game Entities & Interleave the Posts
		for (Entity* entity : m_entities) {
			if (entity == nullptr) continue;

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

		// Catch-all: If the posts were somehow lower depth than ALL entities
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
		powerBarDraw(t_window);
	}

	t_window.setView(t_window.getDefaultView());

	if (m_pause || m_gameOver)
	{
		sf::RectangleShape overlay(sf::Vector2f(t_window.getSize()));
		overlay.setFillColor(sf::Color(20, 20, 20, 200));
		t_window.draw(overlay);

		if (m_pause && !m_gameOver) {
			t_window.draw(m_pauseText);
		}
		else if (m_gameOver) {
			t_window.draw(m_gameOverText);
		}
	}
}

void GamePlay::renderPlayerEntity(sf::RenderWindow& t_window, Entity* entity)
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
	}

	// ==========================================
	// 2B. CORE PLAYER SHADOW (Ambient Occlusion)
	// ==========================================
	sf::CircleShape shadow(20.f);
	shadow.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(100 * airFade)));
	shadow.setOrigin({ 20.f, 20.f });
	shadow.setPosition(feetPos);
	shadow.setScale({ shadowScale, shadowScale });
	t_window.draw(shadow);

	// ==========================================
	// 3. ELEVATED PLAYER VISUALS
	// ==========================================
	sf::Vector2f visualPos = { groundPos.x + (z / 1.5f), groundPos.y };
	sf::Sprite visualSprite = entity->getSprite();
	visualSprite.setPosition(visualPos);

	float scaleMultiplier = 1.0f + (z / 750.f);
	visualSprite.setScale({ visualSprite.getScale().x * scaleMultiplier, visualSprite.getScale().y * scaleMultiplier });

	t_window.draw(visualSprite);
}

void GamePlay::setupMatch(GameDatabase& db, const std::string& homeTeamId, const std::string& awayTeamId, const std::string& userPlayerId)
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

	// ==========================================
	// --- SPECTATOR BYPASS ---
	// ==========================================
	if (userPlayerId != "SPECTATOR") {
		m_userPlayer = std::make_unique<UserPlayer>((m_animServer.getPlayerTexture()));
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

	spawnTeamDynamic(m_homeside, m_entities, m_homeTeamData, true, userPlayerId);
	spawnTeamDynamic(m_awayside, m_entities, m_awayTeamData, false, "");

	m_ball->setPosition(m_pitch.centerSpot);

	barBackground.setSize(barSize);
	barBackground.setFillColor(sf::Color(50, 50, 50, 200));
	barBackground.setOutlineThickness(2.f);
	barBackground.setOutlineColor(sf::Color::White);

	barFill.setSize(barSize);
	barFill.setFillColor(sf::Color::Green);

	m_homeGoal.initialize(sf::Vector2f{ m_pitch.margin, 3500.f }, true);
	m_awayGoal.initialize(sf::Vector2f{ m_pitch.totalWidth - m_pitch.margin, 3500.f }, false);

	// Safely center the camera on the ball if there is no user
	m_playerCam.setCenter(m_userPlayer ? m_userPlayer->getPosition() : m_pitch.centerSpot);
	m_playerCam.setSize({ 1920, 1080 });
	m_playerCam.zoom(0.50f);

	std::vector<Player*> allPlayers;
	if (m_userPlayer) allPlayers.push_back(m_userPlayer.get()); // Only push if exists!
	for (auto& tm : m_homeside) allPlayers.push_back(tm.get());
	for (auto& opp : m_awayside) allPlayers.push_back(opp.get());

	m_soundManager.loadAllSounds();
	m_soundManager.playCrowd("ASSETS/SOUNDS/CROWD/stadium_noise.ogg", 80.f);

	m_referee.startMatch(*m_ball, m_pitch, allPlayers, m_soundManager);
}

void GamePlay::spawnTeamDynamic(std::vector<std::unique_ptr<NPCPlayer>>& team, std::vector<Entity*>& entities, TeamData& teamData, bool isHomeSide, const std::string& userPlayerId)
{
	bool userAssigned = false;
	auto layout = getFormationLayout(teamData.defaultTactics.formationName);

	// --- THE FIX: Check if Player 8 is actually in the starting XI ---
	bool hasPlayer8 = false;
	if (userPlayerId.empty() && userPlayerId != "SPECTATOR") {
		for (const auto& [slotId, pId] : teamData.defaultTactics.startingXI) {
			if (pId == "8") {
				hasPlayer8 = true;
				break;
			}
		}
	}

	for (const auto& [slotId, playerId] : teamData.defaultTactics.startingXI)
	{
		PlayerData* pData = m_db->getPlayer(playerId);
		if (!pData) continue;

		PositionRole matchRole = PositionRole::CenterMid;
		for (const auto& line : layout) {
			for (const auto& slot : line) {
				if (slot.first == slotId) {
					matchRole = slot.second;
					break;
				}
			}
		}

		bool isTargetUser = false;

		// ONLY attempt to hijack if we are NOT in spectator mode
		if (userPlayerId != "SPECTATOR" && isHomeSide && !userAssigned) {
			if (!userPlayerId.empty()) {
				isTargetUser = (playerId == userPlayerId);
			}
			else {
				// Auto-pick Player 8! 
				if (hasPlayer8) {
					isTargetUser = (playerId == "8");
				}
				// THE FIX: If Player 8 isn't there, grab the first player who is NOT a Goalkeeper!
				else if (matchRole != PositionRole::Goalkeeper) {
					isTargetUser = true;
				}
			}
		}

		if (isTargetUser && m_userPlayer)
		{
			m_userPlayer->loadFromData(*pData);
			m_userPlayer->setTeam(Team::Home);
			m_userPlayer->setPositionRole(matchRole);

			sf::Vector2f basePos = m_userPlayer->getBaseTacticalCoordinate(true, slotId, layout);
			m_userPlayer->setBaseHomePosition(basePos);
			m_userPlayer->setPosition(basePos);
			m_userPlayer->setKitColor(teamData.shirt.primaryColor);

			userAssigned = true;
			continue;
		}

		auto player = std::make_unique<NPCPlayer>((m_animServer.getPlayerTexture()));
		player->loadFromData(*pData);
		player->setTeam(isHomeSide ? Team::Home : Team::Away);
		player->setPositionRole(matchRole);

		sf::Vector2f basePos = player->getBaseTacticalCoordinate(isHomeSide, slotId, layout);
		player->setBaseHomePosition(basePos);
		player->setPosition(player->getHomePosition());

		if (matchRole == PositionRole::Goalkeeper) {
			player->setKitColor(sf::Color(50, 200, 50));
		}
		else {
			player->setKitColor(teamData.shirt.primaryColor);
		}

		entities.push_back(player.get());
		team.push_back(std::move(player));
	}
}

void GamePlay::updateCamera(sf::RenderWindow& t_window)
{
	sf::Vector2f ballPos = m_ball->getPosition();
	sf::Vector2f targetCenter;
	float zoomFactor = 1.0f;

	if (m_userPlayer) {
		// --- PLAYER CAMERA ---
		sf::Vector2f playerPos = m_userPlayer->getPosition();
		sf::Vector2f mouseWorldPos = t_window.mapPixelToCoords(sf::Mouse::getPosition(t_window), m_playerCam);

		sf::Vector2f aimVec = mouseWorldPos - playerPos;
		sf::Vector2f ballVec = ballPos - playerPos;

		float aimDist = std::sqrt(aimVec.x * aimVec.x + aimVec.y * aimVec.y);
		float ballDist = std::sqrt(ballVec.x * ballVec.x + ballVec.y * ballVec.y);

		float maxAimPull = 1200.f;
		if (aimDist > maxAimPull) {
			aimVec = (aimVec / aimDist) * maxAimPull;
			aimDist = maxAimPull;
		}

		targetCenter = playerPos + (aimVec * 0.35f) + (ballVec * 0.15f);
		zoomFactor = 1.0f + (aimDist * 0.00015f) + (ballDist * 0.0001f);
		zoomFactor = std::clamp(zoomFactor, 1.0f, 1.35f);
	}
	else {
		// --- TV BROADCAST CAMERA (Spectator) ---
		// Lead the camera slightly ahead of the ball's momentum
		sf::Vector2f ballVel = m_ball->getVelocity();
		targetCenter = ballPos + (ballVel * 0.3f);

		// Zoom out slightly more depending on how fast the ball is moving
		float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);
		zoomFactor = 1.15f + (ballSpeed * 0.0001f);
		zoomFactor = std::clamp(zoomFactor, 1.15f, 1.4f);
	}

	float baseSizeX = 1920.f * 2.5f;
	float baseSizeY = 1080.f * 2.5f;
	sf::Vector2f targetSize(baseSizeX * zoomFactor, baseSizeY * zoomFactor);

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

void GamePlay::powerBarUpdate()
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

void GamePlay::powerBarDraw(sf::RenderWindow& t_window)
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

float GamePlay::distance(sf::Vector2f a, sf::Vector2f b)
{
	sf::Vector2f d = a - b;
	return std::sqrt(d.x * d.x + d.y * d.y);
}

// Helper: Normalize
sf::Vector2f GamePlay::normalize(sf::Vector2f source) {
	float length = std::sqrt(source.x * source.x + source.y * source.y);
	if (length != 0) return source / length;
	return source;
}

void GamePlay::runStandardSystems(float dt, sf::RenderWindow& t_window)
{
	// ==========================================
	// --- 1. DETERMINE EFFECTIVE POSSESSION ---
	// ==========================================
	Player* effectiveOwner = m_ball->getOwner();
	if (effectiveOwner == nullptr) {
		effectiveOwner = m_ball->getLastOwner(); // The magic fix for passes!
	}

	TeamState homeState = TeamState::Defending;
	TeamState awayState = TeamState::Defending;

	if (effectiveOwner != nullptr) {
		homeState = (effectiveOwner->getTeam() == Team::Home) ? TeamState::Attacking : TeamState::Defending;
		awayState = (homeState == TeamState::Attacking) ? TeamState::Defending : TeamState::Attacking;
	}
	else {
		// Failsafe for the literal first frame before any touches
		homeState = TeamState::Attacking;
		awayState = TeamState::Defending;
	}

	// --- 2. UPDATE HUMAN USER ---
	// (These are already perfectly guarded!)
	if (m_userController) {
		m_userController->update(dt, *this);
		m_userController->mouseAiming(mouseWorld, t_window, m_playerCam);
	}
	if (m_userPlayer)
	{
		m_userPlayer->update(dt, m_animServer);
	}

	// --- 3. GATHER ACTIVE LISTS & FIND FIRST RESPONDERS ---
	std::vector<Player*> homeFriends;

	// THE FIX: Explicitly check if m_userPlayer exists before asking if they are sent off!
	if (m_userPlayer && !m_userPlayer->isSentOff()) {
		homeFriends.push_back(m_userPlayer.get());
	}

	for (auto& npc : m_homeside) {
		if (!npc->isSentOff()) homeFriends.push_back(npc.get());
	}

	std::vector<Player*> homeEnemies;
	for (auto& opp : m_awayside) {
		if (!opp->isSentOff()) homeEnemies.push_back(opp.get());
	}

	// --- NEW: ABANDONMENT CHECK (LESS THAN 7 PLAYERS) ---
	if (!m_gameOver) {
		if (homeFriends.size() < 7) {
			triggerForfeit(true);  // Home forfeits
			return; // Stop processing physics for this frame!
		}
		else if (homeEnemies.size() < 7) {
			triggerForfeit(false); // Away forfeits
			return;
		}
	}

	Player* homeFirstResponder = findFirstResponder(homeFriends);
	Player* awayFirstResponder = findFirstResponder(homeEnemies);

	// --- 4. UPDATE AI BRAINS ---
	for (auto& npc : m_homeside) {
		// FIX: Lock them in the dressing room! Don't let the AI brain move them back to the pitch.
		if (npc->isSentOff()) continue;

		// THE FIX: Pass m_userPlayer as a pointer (m_userPlayer.get()) so it safely passes nullptr in Spectator Mode!
		m_npcController->update(*npc, m_userPlayer.get(), *m_ball, homeFriends, homeEnemies, m_pitch, homeState, dt, homeFirstResponder, m_referee, *m_homeTeamAI, m_soundManager);
		npc->update(dt, m_animServer); // Internal motor physics
	}

	for (auto& npc : m_awayside) {
		// FIX: Lock them in the dressing room!
		if (npc->isSentOff()) continue;

		// THE FIX: Pass m_userPlayer as a pointer here as well!
		m_npcController->update(*npc, m_userPlayer.get(), *m_ball, homeEnemies, homeFriends, m_pitch, awayState, dt, awayFirstResponder, m_referee, *m_awayTeamAI, m_soundManager);
		npc->update(dt, m_animServer);
	}

	// --- 5. WORLD PHYSICS & COLLISIONS ---
	m_ball->update(dt);

	// Collect everyone for collisions
	std::vector<Player*> allPlayers = homeFriends;
	allPlayers.insert(allPlayers.end(), homeEnemies.begin(), homeEnemies.end());

	// Let the physics engine resolve overlapping bodies!
	PhysicsEngine::resolvePlayerPlayerCollisions(allPlayers, *m_ball, m_referee, m_animServer, m_pitch, m_soundManager);
	for (Player* p : allPlayers) {
		PhysicsEngine::resolvePlayerPitchBoundaries(*p, m_pitch);
	}
	for (Player* p : allPlayers) {
		PhysicsEngine::resolvePlayerGoalCollisions(*p, m_homeGoal);
		PhysicsEngine::resolvePlayerGoalCollisions(*p, m_awayGoal);
	}

	// 3. Resolve Ball interacting with the world
	PhysicsEngine::resolveBallPitchBoundaries(*m_ball, m_pitch, m_soundManager);
	PhysicsEngine::resolveBallGoalCollisions(*m_ball, m_homeGoal, m_soundManager);
	PhysicsEngine::resolveBallGoalCollisions(*m_ball, m_awayGoal, m_soundManager);

	// 4. Resolve Ball interacting with Players
	PhysicsEngine::resolveGoalkeeperBallCollisions(*m_ball, allPlayers);
	PhysicsEngine::resolveBallPlayerCollisions(*m_ball, allPlayers);

}

Player* GamePlay::findFirstResponder(const std::vector<Player*>& t_team) {
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

void GamePlay::drawUI(sf::RenderWindow& t_window)
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

		// 1. Identify the Scorer (The last player to touch the ball)
		Player* scorer = m_ball->getLastOwner();

		if (scorer)
		{
			// The team taking the Kick-Off is the team that CONCEDED.
			// Therefore, the scoring team is the opposite.
			Team scoringTeam = (m_referee.getAwardedTo() == Team::Home) ? Team::Away : Team::Home;

			// If the player who touched it last is not on the scoring team, it's an Own Goal!
			bool isOwnGoal = (scorer->getTeam() != scoringTeam);

			std::string scorerName = scorer->getName();
			if (isOwnGoal) {
				scorerName += " (O.G.)";
			}

			// Use the SCORING team's colors and name, not necessarily the scorer's!
			std::string teamName = (scoringTeam == Team::Home) ? m_homeTeamData.fullName : m_awayTeamData.fullName;
			sf::Color goalColor = (scoringTeam == Team::Home) ? m_homeTeamData.uiColor : m_awayTeamData.uiColor;
			goalColor.a = 255;

			// 2. Setup Text
			sf::Text goalText(m_font, isOwnGoal ? "OWN GOAL!" : "GOAL!", 60);
			goalText.setStyle(sf::Text::Bold);
			goalText.setFillColor(sf::Color::White);

			// Use our frozen timer!
			sf::Text infoText(m_font, scorerName + " (" + std::to_string(s_savedGoalMinute) + "')\n" + teamName, 30);
			infoText.setFillColor(sf::Color(220, 220, 220)); // Off-white
			infoText.setLineSpacing(1.2f);

			// 3. Layout Dimensions
			float bannerWidth = 500.f;
			float bannerHeight = 250.f;
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
			// Center "GOAL!"
			goalText.setPosition({
				centerPos.x - goalText.getLocalBounds().size.x * 0.5f,
				topLeft.y + 15.f
				});
			t_window.draw(goalText);

			// Center Player/Team Info
			infoText.setPosition({
				centerPos.x - infoText.getLocalBounds().size.x * 0.5f,
				topLeft.y + 85.f
				});
			t_window.draw(infoText);
		}
	}

	// Update the tracking state at the end of the frame
	s_lastBannerState = m_referee.getMatchState();


	// ==========================================
	// 3. RESTORE WORLD VIEW
	// ==========================================
	t_window.setView(worldView);
}

void GamePlay::triggerForfeit(bool isHomeForfeit)
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

void GamePlay::drawDebugOffsideLines(sf::RenderWindow& window) {
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

void GamePlay::drawDebugNames(sf::RenderWindow& window, const sf::Font& font) {
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

void GamePlay::executePlayerSwitch(Player* targetNPC) 
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
		m_ball->possess(m_userPlayer.get());
	}
	else if (m_ball->getOwner() == m_userPlayer.get()) {
		m_ball->possess(targetNPC);
	}
}

void GamePlay::drawDebugHitboxes(sf::RenderWindow& window) {
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
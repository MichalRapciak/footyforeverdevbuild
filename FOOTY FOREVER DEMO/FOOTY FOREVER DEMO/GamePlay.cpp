#include "GamePlay.h"
#include "Game.h"

GamePlay::GamePlay() : m_pauseText(m_font), m_gameOverText(m_font), m_gameWonText(m_font)
{
	setupGame();
	m_playerCam.setCenter(m_userPlayer->getPosition());
	m_playerCam.setSize({ 1920,1080 });
	m_playerCam.zoom(2.5f);
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


	m_animServer.init("ASSETS/PLAYER/player_run.png");

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
		if (!m_pause)
			m_userController->inputHandler(t_event);
	}
	if (const auto keyReleased = t_event.getIf<sf::Event::KeyReleased>())
	{
		if (!m_pause)
			m_userController->inputHandler(t_event);
	}
	if (const auto buttonPressed = t_event.getIf<sf::Event::MouseButtonPressed>())
	{
		if (!m_pause)
			m_userController->inputHandler(t_event);
	}
	if (const auto buttonReleased = t_event.getIf<sf::Event::MouseButtonReleased>())
	{
		if (!m_pause)
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
	if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
	{

	}

	if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right))
	{
		if (m_ball->hasOwner())
		{
			//sf::Vector2f dir = m_player->getAimDirection();
			//m_ball.shoot(dir, m_player->getPower());
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
	
    // 1. Gather all players
	std::vector<Player*> homeFriends;
	homeFriends.push_back(m_userPlayer.get());
	for (auto& npc : m_teammates) homeFriends.push_back(npc.get());

	std::vector<Player*> homeEnemies;
	for (auto& opp : m_opponents) homeEnemies.push_back(opp.get());
	
    std::vector<Player*> allPlayers = homeFriends;
	allPlayers.insert(allPlayers.end(), homeEnemies.begin(), homeEnemies.end());

    // 2. THE REFEREE UPDATES THE CONTEXTS FIRST
	m_referee.update(*m_ball, m_pitch, allPlayers, dt);

	if (!m_pause && !m_gameOver)
	{
		// 3. RUN THE SIMULATION
        // This handles InPlay, ThrowIns, Corners, AND Celebrations automatically now!
        runStandardSystems(dt, t_window);
		// 4. Update visuals
		updateCamera(t_window);
		powerBarUpdate();
	}
}

/// <summary>
/// Draw Frames and Switch Buffers
/// </summary>
void GamePlay::render(sf::RenderWindow& t_window)
{
	t_window.setView(m_playerCam); // Set Camera to player camera
	t_window.draw(m_level1.getLevelBG());
	std::sort(m_entities.begin(), m_entities.end(), [](Entity* a, Entity* b) {
		// Now it compares the Ball's center to the Player's feet!
		return a->getSortDepth() > b->getSortDepth();
		});
	for (Entity* entity : m_entities)
	{
		if (entity == nullptr) continue;

		// Check if this entity IS the ball
		if (entity == m_ball.get())
		{
			// Use the Ball's specialized draw
			m_ball->draw(t_window);
		}
		else
		{
			// 1. Get the actual ground position and Z height
			sf::Vector2f groundPos = entity->getPosition();

			// Replace this with your actual Z getter (e.g., entity->getZ() or dynamic cast)
			float z = entity->z;

			// 2. Draw standard Player Shadow at the ACTUAL ground position
			sf::CircleShape shadow(20.f);
			shadow.setFillColor(sf::Color(0, 0, 0, 50));
			shadow.setOrigin({ 20.f, 20.f });
			shadow.setScale({ 1.0f, 1.0f });


			// Shadow offset: If you want the shadow slightly offset from their feet, 
			// you might want to shift it on the Y axis (-10.f) rather than X, 
			// depending on where your imaginary "sun" is!
			shadow.setPosition({ groundPos.x - 40.f, groundPos.y });
			t_window.draw(shadow);

			// 3. Calculate the visual (elevated) position
			// Because +X is UP on your screen, jumping adds Z to X!
			sf::Vector2f visualPos = { groundPos.x + (z / 1.5f), groundPos.y };

			// 4. Move and draw the VISUALS only
			// Making a local copy of the sprite keeps the real collision box on the ground
			sf::Sprite visualSprite = entity->getSprite();
			visualSprite.setPosition(visualPos);

			// Optional: Make the player scale up slightly as they jump closer to the camera
			 float scaleMultiplier = 1.0f + (z / 750.f); 
			 visualSprite.setScale({ visualSprite.getScale().x * scaleMultiplier, visualSprite.getScale().y * scaleMultiplier });

			 float t = std::min(z / 100.f, 1.f);
			 float scale = scaleMultiplier;
			 shadow.setPosition({ groundPos.x - 10.f, groundPos.y });
			 shadow.setScale({ 1.f - t * 0.5f, 1.f });

			t_window.draw(visualSprite);
		}
	}
	m_homeGoal.draw(t_window);
	m_awayGoal.draw(t_window);
	m_userController->draw(t_window);
	drawUI(t_window);
	powerBarDraw(t_window);
	if (m_pause)
	{
		t_window.setView(t_window.getDefaultView());
		sf::RectangleShape overlay;
		overlay.setSize(sf::Vector2f(t_window.getSize()));
		overlay.setFillColor(sf::Color(100, 100, 100, 150));
		t_window.draw(overlay);
		t_window.draw(m_pauseText);
		t_window.setView(m_playerCam);
	}
	if (m_gameOver)
	{
		t_window.setView(t_window.getDefaultView());
		sf::RectangleShape overlay;
		overlay.setSize(sf::Vector2f(t_window.getSize()));
		overlay.setFillColor(sf::Color(50, 50, 50, 100));
		t_window.draw(overlay);
		t_window.draw(m_gameOverText);
		t_window.setView(m_playerCam);
	}
	if (m_gameWon)
	{
		t_window.setView(t_window.getDefaultView());
		sf::RectangleShape overlay;
		overlay.setSize(sf::Vector2f(t_window.getSize()));
		overlay.setFillColor(sf::Color(50, 50, 50, 100));
		t_window.draw(overlay);
		t_window.draw(m_gameWonText);
		t_window.setView(m_playerCam);
	}
}

/// <summary>
/// Setting player up, temporarily setting up enemies
/// </summary>
void GamePlay::setupGame()
{
	// --- 1. SETUP USER (As you have it) ---
	m_userPlayer = std::make_unique<UserPlayer>((m_animServer.getPlayerTexture()));
	m_entities.push_back(m_userPlayer.get());
	m_userController = std::make_unique<UserController>(*m_userPlayer);

	m_ball = std::make_unique<Ball>();
	m_entities.push_back(m_ball.get());

	// --- 2. SETUP BRAINS ---
	m_npcController = std::make_unique<NPCController>();

	spawnTeam(m_teammates,m_entities,true);
	spawnTeam(m_opponents,m_entities,false);

	m_ball->setPosition(m_pitch.centerSpot);

	// The Background (The "Container")
	barBackground.setSize(barSize);
	barBackground.setFillColor(sf::Color(50, 50, 50, 200)); // Dark grey, semi-transparent
	barBackground.setOutlineThickness(2.f);
	barBackground.setOutlineColor(sf::Color::White);

	// The Foreground (The "Value")
	barFill.setSize(barSize);
	barFill.setFillColor(sf::Color::Green); // Start with green

	// 1. Construct the Home Goal (Left side, pos.x = margin)
	// We pass 'true' because it's the home side (net goes to the left)
	m_homeGoal.initialize(sf::Vector2f{ m_margin, m_goalCenterY }, true);

	// 2. Construct the Away Goal (Right side, pos.x = width - margin)
	// We pass 'false' because net goes to the right
	m_awayGoal.initialize(sf::Vector2f{ m_pitchWidth - m_margin, m_goalCenterY }, false);
}

void GamePlay::handlePlayerCollisions(std::vector<Player*>& players, const Pitch& pitch)
{
	// --- 0. DEAD BALL CHECK ---
	if (m_referee.getMatchState() != MatchState::InPlay) return;

	// --- 1. PITCH BOUNDARY COLLISIONS (Walls) ---
	for (Player* p : players)
	{
		resolvePlayerGoalCollision(*p, m_homeGoal);
		resolvePlayerGoalCollision(*p, m_awayGoal);
		sf::Vector2f pos = p->getPosition();
		sf::Vector2f vel = p->getVelocity();
		float radius = p->getCollisionRadius();
		float bounce = 0.4f;

		float minX = 0 + radius;
		float maxX = (pitch.totalWidth) - radius;
		float minY = 0 + radius;
		float maxY = (pitch.totalHeight) - radius;

		if (pos.x < minX) { p->setPosition({ minX, pos.y }); if (vel.x < 0) p->setVelocity(sf::Vector2f(-vel.x * bounce, vel.y)); }
		else if (pos.x > maxX) { p->setPosition({ maxX, pos.y }); if (vel.x > 0) p->setVelocity(sf::Vector2f(-vel.x * bounce, vel.y)); }

		if (pos.y < minY) { p->setPosition({ pos.x, minY }); if (vel.y < 0) p->setVelocity(sf::Vector2f(vel.x, -vel.y * bounce)); }
		else if (pos.y > maxY) { p->setPosition({ pos.x, maxY }); if (vel.y > 0) p->setVelocity(sf::Vector2f(vel.x, -vel.y * bounce)); }
	}

	// ==========================================
	// --- 1.5 TACKLE VS BALL COLLISIONS ---
	// ==========================================
	for (Player* tackler : players)
	{
		if (!tackler->isTackling()) continue;

		sf::FloatRect tackleArea = tackler->getTackleHitbox();
		sf::FloatRect ballGroundBounds = m_ball->getShadow().getGlobalBounds();

		if (tackleArea.findIntersection(ballGroundBounds) && m_ball->z < 40.f) {

			if (m_ball->hasOwner()) {
				Player* owner = m_ball->getOwner();

				// NO FRIENDLY FIRE: Only stumble the owner if they are on the opposing team
				if (owner != tackler && owner->getTeam() != tackler->getTeam()) {
					owner->setStumbled(0.5f); // Clean tackle! Just stumble the opponent
				}
			}
			m_ball->release();

			sf::Vector2f tackleImpulse = tackler->getVelocity() * 1.2f;
			m_ball->applyImpulse(tackleImpulse);

			tackler->setVelocity(tackler->getVelocity() * 0.97f);
		}
	}

	// ==========================================
	// --- 2. PLAYER-TO-PLAYER COLLISIONS ---
	// ==========================================
	for (size_t i = 0; i < players.size(); ++i)
	{
		for (size_t j = i + 1; j < players.size(); ++j)
		{
			Player* p1 = players[i];
			Player* p2 = players[j];

			// ==========================================
			// GHOSTING (NO TEAMMATE COLLISIONS)
			// ==========================================
			// If they are on the same team, skip ALL physics and foul checks. 
			// They will smoothly pass through each other.
			if (p1->getTeam() == p2->getTeam()) continue;


			// ==========================================
			// A. HANDLE PLAYER VS PLAYER TACKLES (FOULS)
			// ==========================================
			auto processTackleHit = [&](Player* tackler, Player* victim) -> bool
				{
					if (!tackler->isTackling()) return false;

					// Friendly fire check removed here since the loop 'continue' handles it globally

					if (tackler->getTackleHitbox().findIntersection(victim->getBoundingBox())) {

						// EXCLUSIVE BALL CARRIER CHECK
						if (m_ball->getOwner() == victim) {
							// Did they get the man before the ball?
							sf::Vector2f toBall = m_ball->getPosition() - tackler->getPosition();
							float distToBall = std::sqrt(toBall.x * toBall.x + toBall.y * toBall.y);

							// If the ball is more than 60px away during player contact, it's a foul!
							if (distToBall > 60.f) {
								victim->setState(PlayerState::Stunned);
								victim->resetTackleCooldown();

								tackler->setState(PlayerState::Normal); // Snap tackler out of animation
								tackler->setVelocity({ 0.f, 0.f });

								FoulEvent foul;
								foul.type = FoulType::Sliding;
								foul.location = victim->getPosition();
								foul.offender = tackler;
								m_referee.awardFoul(foul, pitch, *m_ball, players, victim);
								return true; // FOUL CALLED
							}
						}
					}
					return false;
				};

			if (processTackleHit(p1, p2)) return;
			if (processTackleHit(p2, p1)) return;

			// ==========================================
			// B. HANDLE BODY COLLISIONS (Shoulder to Shoulder & Shoves)
			// ==========================================
			sf::Vector2f delta = p1->getPosition() - p2->getPosition();
			float distanceSq = delta.x * delta.x + delta.y * delta.y;
			float combinedRadius = p1->getCollisionRadius() + p2->getCollisionRadius();

			if (distanceSq < combinedRadius * combinedRadius && distanceSq > 0.0001f)
			{
				float distance = std::sqrt(distanceSq);
				sf::Vector2f normal = delta / distance;
				float overlap = combinedRadius - distance;

				float w1 = p1->getWeight();
				float s1 = p1->getBodyStrength() / 100.f;
				float b1 = p1->getBalancing() / 100.f;

				float w2 = p2->getWeight();
				float s2 = p2->getBodyStrength() / 100.f;
				float b2 = p2->getBalancing() / 100.f;

				float m1 = w1 * (1.0f + b1);
				float m2 = w2 * (1.0f + b2);

				float invM1 = 1.0f / m1;
				float invM2 = 1.0f / m2;
				float sumInvMass = invM1 + invM2;

				float ratio1 = invM1 / sumInvMass;
				float ratio2 = invM2 / sumInvMass;

				p1->move(normal * (overlap * ratio1));
				p2->move(-normal * (overlap * ratio2));

				sf::Vector2f relativeVel = p1->getVelocity() - p2->getVelocity();
				float velAlongNormal = (relativeVel.x * normal.x + relativeVel.y * normal.y);

				if (velAlongNormal < 0)
				{
					float restitution = 0.4f;
					float strengthFactor = 1.0f + ((s1 + s2) * 0.5f);

					float j = -(1.0f + restitution) * velAlongNormal;
					j /= sumInvMass;
					j *= strengthFactor;

					sf::Vector2f impulse = j * normal;

					p1->setVelocity(p1->getVelocity() + (invM1 * impulse));
					p2->setVelocity(p2->getVelocity() - (invM2 * impulse));

					float deltaV1 = std::sqrt((invM1 * impulse).x * (invM1 * impulse).x + (invM1 * impulse).y * (invM1 * impulse).y);
					float deltaV2 = std::sqrt((invM2 * impulse).x * (invM2 * impulse).x + (invM2 * impulse).y * (invM2 * impulse).y);

					float stumbleThreshold1 = 250.f * (1.0f + b1);
					float stumbleThreshold2 = 250.f * (1.0f + b2);

					// --- SHOVE FOUL DETECTION (Obstruction) ---
					// Team check removed here since teammates never reach this point anyway
					float v1Sq = p1->getVelocity().x * p1->getVelocity().x + p1->getVelocity().y * p1->getVelocity().y;
					float v2Sq = p2->getVelocity().x * p2->getVelocity().x + p2->getVelocity().y * p2->getVelocity().y;

					Player* aggressor = nullptr;
					Player* victim = nullptr;
					float vicDeltaV = 0.f;
					float vicThresh = 0.f;

					if (v1Sq > v2Sq + 40000.f) { aggressor = p1; victim = p2; vicDeltaV = deltaV2; vicThresh = stumbleThreshold2; }
					else if (v2Sq > v1Sq + 40000.f) { aggressor = p2; victim = p1; vicDeltaV = deltaV1; vicThresh = stumbleThreshold1; }

					if (aggressor && victim && vicDeltaV > vicThresh * 5.0f) {

						// EXCLUSIVE BALL CARRIER CHECK
						if (m_ball->getOwner() == victim) {
							victim->setState(PlayerState::Stunned);

							FoulEvent foul;
							foul.type = FoulType::Obstruction;
							foul.location = victim->getPosition();
							foul.offender = aggressor;
							m_referee.awardFoul(foul, pitch, *m_ball, players, victim);
							return; // FOUL CALLED
						}
					}
					// -----------------------------------------

					float stumbleDuration1 = std::clamp((deltaV1 / stumbleThreshold1) * 0.4f, 0.3f, 0.8f);
					float stumbleDuration2 = std::clamp((deltaV2 / stumbleThreshold2) * 0.4f, 0.3f, 0.8f);

					if (deltaV1 > stumbleThreshold1) p1->setStumbled(stumbleDuration1);
					if (deltaV2 > stumbleThreshold2) p2->setStumbled(stumbleDuration2);
				}
			}
		}
	}
}

void GamePlay::spawnTeam(std::vector<std::unique_ptr<NPCPlayer>>& team, 
                         std::vector<Entity*>& entities, // Pass your render vector
                         bool isHomeSide)
{
    std::vector<PositionRole> formation = {
        PositionRole::Goalkeeper,
        PositionRole::LeftBack, PositionRole::LCenterBack, PositionRole::RCenterBack, PositionRole::RightBack,
        PositionRole::DefensiveMid, PositionRole::CenterMid, PositionRole::AttackingMid,
        PositionRole::LeftWing, PositionRole::RightWing, PositionRole::Striker
    };

    for (PositionRole role : formation)
    {
        auto player = std::make_unique<NPCPlayer>((m_animServer.getPlayerTexture()));
        player->setPositionRole(role);
        player->setTeam(isHomeSide ? Team::Home : Team::Away);
        player->setPosition(player->getHomePosition(isHomeSide, TeamState::Neutral));

		// ==========================================
		// DYNAMIC STAT ASSIGNMENT
		// ==========================================
		PlayerStats generatedStats = PlayerStats::createFromRole(role);

		// Add slightly randomized genetics (+/- 3 points) to physical stats
		// This ensures the players feel organic and not like copy-pasted clones
		float speedVariance = ((rand() % 7) - 3.0f);
		float accelVariance = ((rand() % 7) - 3.0f);

		generatedStats.topSpeed += speedVariance;
		generatedStats.acceleration += accelVariance;

		// Assuming your Player/NPCPlayer class has a setter for stats!
		player->setStats(generatedStats);

		if (isHomeSide) {
			player->getSprite().setColor(sf::Color::White); // Home Kit
		}
		else {
			// Give opponents a distinct color so you know who to tackle!
			player->getSprite().setColor(sf::Color::Blue); // Blue kit
		}

        // 1. Add to the rendering/update vector (raw pointer)
        entities.push_back(player.get());

        // 2. Add to the ownership vector (moves unique_ptr)
        team.push_back(std::move(player));
    }
}

void GamePlay::processTackle(Player* tackler, Player* victim, float dt) {
	// 1. Logic Checks: Are they lunging? AND is their cooldown ready?
	if (!tackler->isTackling() || !tackler->canTackle()) return;

	sf::FloatRect tackleArea = tackler->getTackleHitbox();
	sf::FloatRect ballBounds = m_ball->getSprite().getGlobalBounds();
	sf::Vector2f tackleImpulse = tackler->getVelocity() * 1.2f;

	bool hitSomething = false;

	// --- A. BALL INTERACTION ---
	if (tackleArea.findIntersection(ballBounds)) {
		m_ball->release();
		m_ball->applyImpulse(tackleImpulse);
		tackler->setVelocity(tackler->getVelocity() * 0.95f);
		hitSomething = true;
	}

	// --- B. PLAYER INTERACTION ---
	if (tackleArea.findIntersection(victim->getBoundingBox())) {
		if (victim->getState() != PlayerState::Stunned) {
			if (victim->getBallPossession()) m_ball->release();
			m_ball->applyImpulse(tackleImpulse);
			victim->setState(PlayerState::Stunned);
			tackler->setVelocity(tackler->getVelocity() * 0.80f);
			hitSomething = true;
		}
	}

	// 2. TRIGGER COOLDOWN
	// If we made contact, start the 2-second timer
	if (hitSomething) {
		tackler->startTackleCooldown();
	}
}

void GamePlay::handleBallPlayerPhysics(std::vector<Player*>& players, Ball& ball) {

	// ==========================================
	// 1. KEEPER DIVING COLLISIONS (Unchanged)
	// ==========================================
	for (Player* p : players)
	{
		if (p->getState() == PlayerState::Diving)
		{
			if (p->getBoundingBox().findIntersection(ball.getBoundingBox()))
			{
				if (std::abs(p->z - ball.z) < p->height)
				{
					NPCPlayer* npcKeeper = dynamic_cast<NPCPlayer*>(p);
					if (npcKeeper != nullptr) {
						m_npcController->resolveSaveOutcome(*npcKeeper, ball);
					}
				}
			}
		}
	}

	// ==========================================
	// 2. UNIFIED OUTFIELD COLLISIONS
	// ==========================================
	// We handle ALL speeds here now. No more "speed > 1000" threshold.
	// This ensures the ball is pushed out gracefully before applyImpulse can ever fire.

	if (ball.z < 80.f) {

		for (Player* p : players) {

			if (p->getBallPossession()) continue;

			// Optional: Don't bounce off the person who JUST kicked it to avoid self-blocks
			// if (ball.getLastOwner() == p) continue; 

			if (std::abs(p->z - ball.z) < p->height)
			{
				sf::Vector2f ballPos = ball.getPosition();
				sf::Vector2f playerPos = p->getPosition();
				sf::Vector2f delta = ballPos - playerPos;

				float distSq = delta.x * delta.x + delta.y * delta.y;
				float combineRadius = ball.getShadow().getRadius() + p->getCollisionRadius();

				if (distSq < combineRadius * combineRadius && distSq > 0.0001f) {

					float distance = std::sqrt(distSq);
					sf::Vector2f normal = delta / distance; // Direction pushing AWAY from player

					// --- 1. OVERLAP RESOLUTION ---
					// Instantly move the ball out of the player's body.
					// This completely stops applyImpulse() from firing and multiplying forces!
					float overlap = combineRadius - distance;
					ball.setPosition(ballPos + normal * (overlap + 1.0f));

					// --- 2. RELATIVE VELOCITY CALCULATION ---
					// We must fetch the ball's velocity dynamically here in case it already bounced this frame
					sf::Vector2f currentBallVel = ball.getVelocity();
					sf::Vector2f playerVel = p->getVelocity();
					sf::Vector2f relVel = currentBallVel - playerVel;

					float dot = relVel.x * normal.x + relVel.y * normal.y;

					// Only bounce if the ball is moving TOWARDS the player
					if (dot < 0) {
						float relSpeed = std::sqrt(relVel.x * relVel.x + relVel.y * relVel.y);

						// --- 3. DYNAMIC RESTITUTION (BOUNCINESS) ---
						// If it's a slow bump, bounciness is nearly 0 (ball dies at their feet).
						// If it's a fast shot, it bounces off at 25% speed.
						float restitution = (relSpeed > 800.f) ? 0.25f : 0.05f;

						// Elastic reflection formula based on relative velocity
						sf::Vector2f reflection = relVel - (1.0f + restitution) * dot * normal;

						// Apply the reflection AND add the player's momentum back
						sf::Vector2f finalVel = playerVel + reflection;

						// --- 4. HARD SPEED CAP ---
						// Physically impossible to exceed 1000px/s off a body deflection
						float finalSpeed = std::sqrt(finalVel.x * finalVel.x + finalVel.y * finalVel.y);
						if (finalSpeed > 1000.f) {
							finalVel = (finalVel / finalSpeed) * 1000.f;
						}

						ball.setVelocity(finalVel);

						// Stun check using the original incoming speed
						float originalIncomingSpeed = std::sqrt(currentBallVel.x * currentBallVel.x + currentBallVel.y * currentBallVel.y);
						if (originalIncomingSpeed > 2000.f) {
							p->setState(PlayerState::Stunned);
						}
					}
				}
			}
		}
	}
}

void GamePlay::updateCamera(sf::RenderWindow& t_window)
{
	// 1. Get world positions
	sf::Vector2f playerPos = m_userPlayer->getPosition();
	sf::Vector2f mouseWorldPos = t_window.mapPixelToCoords(sf::Mouse::getPosition(t_window), m_playerCam);

	// 2. Calculate a target point that is 20% of the way toward the mouse
	// Increase 0.2f to 0.4f if you want the camera to follow the mouse further
	float leanStrength = 0.5f;
	sf::Vector2f targetCenter = playerPos + (mouseWorldPos - playerPos) * leanStrength;

	// 3. Smooth the camera movement (Interpolation)
	// This prevents the camera from "snapping" and makes it feel professional
	sf::Vector2f currentCenter = m_playerCam.getCenter();
	float lerpFactor = 0.025f; // Between 0.0 and 1.0 (lower is smoother)

	sf::Vector2f smoothedCenter = currentCenter + (targetCenter - currentCenter) * lerpFactor;

	// 4. Apply to your view~
	m_playerCam.setCenter(smoothedCenter);
	m_playerCam.setRotation(sf::degrees(90));
	t_window.setView(m_playerCam);
}

/// <summary>
/// Function in charge of deleting dead enemies and entities and updating their vectors
/// </summary>
void GamePlay::refreshEntities()
{
	if (m_userPlayer)
	{
		m_entities.push_back(m_userPlayer.get());
	}
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

void GamePlay::handleGoalPhysics(Goal& goal) {
	sf::Vector2f bPos = m_ball->getPosition();
	sf::Vector2f velocity = m_ball->getVelocity();
	float ballZ = m_ball->z;
	float bRadius = 12.f;

	float crossbarZ = 244.f;
	float goalX = goal.center.x;
	float topY = goal.center.y - 366.f;
	float bottomY = goal.center.y + 366.f;
	float netDepth = 225.f;
	float backX = goal.isHomeGoal ? (goalX - netDepth) : (goalX + netDepth);
	// --- A. THE ROOF CATCH (Falling onto the net) ---
	// Check if ball is within the 2D "Rectangle" of the goal net
	bool isOverNetX = goal.isHomeGoal ? (bPos.x < goalX && bPos.x > backX)
		: (bPos.x > goalX && bPos.x < backX);
	bool isOverNetY = (bPos.y > topY && bPos.y < bottomY);

	if (isOverNetX && isOverNetY) {
		// If ball is falling (vz is positive in your downward gravity logic) 
		// and it's hitting the roof from above
		if (m_ball->vz > 0.f && ballZ <= crossbarZ + 5.f && ballZ >= crossbarZ - 5.f) {

			// 1. Snap to roof height
			m_ball->z = crossbarZ;

			// 2. Kill vertical velocity (stops it from falling through)
			m_ball->vz = 0.f;

			// 3. Apply extreme friction (ball rolls slowly on the mesh)
			m_ball->setVelocity(velocity * 0.95f);

			// Optional: If it hits hard, give it a tiny 'mesh bounce'
			// m_ball->vz = -m_ball->vz * 0.1f; 
		}
	}

	// --- A. THE POSTS (3D Circles) ---
	auto checkPost = [&](sf::CircleShape& post) {
		if (ballZ > 244.f + bRadius) return; // Fly over
		sf::Vector2f pPos = post.getPosition();
		sf::Vector2f diff = bPos - pPos;
		float distSq = (diff.x * diff.x) + (diff.y * diff.y);
		float minDist = bRadius + post.getRadius();

		if (distSq < minDist * minDist) {
			sf::Vector2f normal = normalize(diff);
			m_ball->setVelocity(m_ball->reflect(velocity, normal) * 0.7f);
			m_ball->setPosition(pPos + normal * (minDist + 1.f));
		}
		};
	checkPost(goal.topPost);
	checkPost(goal.bottomPost);

	// --- B. THE NET WALLS (Physical Obstacles) ---
	// Only collide with net if ball is below crossbar height
	if (ballZ < 244.f) {
		// 1. BACK NET (X-Wall)
		// If ball is between the posts Y-wise
		if (bPos.y > topY && bPos.y < bottomY) {
			bool isHittingBack = goal.isHomeGoal ? (bPos.x < backX + bRadius) : (bPos.x > backX - bRadius);
			if (isHittingBack) {
				m_ball->setPosition({ goal.isHomeGoal ? backX + bRadius : backX - bRadius, bPos.y });
				m_ball->setVelocity({ -velocity.x * 0.2f, velocity.y * 0.5f }); // Dampen heavily
			}
		}

		// 2. SIDE NETS (Y-Walls)
		// Only if the ball is "behind" the goal line (in the net area)
		bool isBehindLine = goal.isHomeGoal ? (bPos.x < goalX) : (bPos.x > goalX);
		bool isInsideDepth = goal.isHomeGoal ? (bPos.x > backX) : (bPos.x < backX);

		if (isBehindLine && isInsideDepth) {
			// Top Side
			if (std::abs(bPos.y - topY) < bRadius) {
				float sideSign = (bPos.y < topY) ? -1.f : 1.f;
				m_ball->setPosition({ bPos.x, topY + (sideSign * bRadius) });
				m_ball->setVelocity({ velocity.x * 0.5f, -velocity.y * 0.2f });
			}
			// Bottom Side
			else if (std::abs(bPos.y - bottomY) < bRadius) {
				float sideSign = (bPos.y < bottomY) ? -1.f : 1.f;
				m_ball->setPosition({ bPos.x, bottomY + (sideSign * bRadius) });
				m_ball->setVelocity({ velocity.x * 0.5f, -velocity.y * 0.2f });
			}
		}
	}

	// --- C. GOAL SCORING TRIGGER ---
	// (Keep your 'Axe Logic' crossing check here)
}

void GamePlay::resolvePlayerGoalCollision(Player& player, Goal& goal) {
	sf::Vector2f pPos = player.getPosition();
	sf::Vector2f pVel = player.getVelocity();
	float pRadius = 25.f;

	float netDepth = 225.f;
	float goalWidth = 732.f;
	float topY = goal.center.y - (goalWidth / 2.f);
	float bottomY = goal.center.y + (goalWidth / 2.f);
	float goalX = goal.center.x;
	float backX = goal.isHomeGoal ? (goalX - netDepth) : (goalX + netDepth);

	// --- 1. THE SIDE WALLS (Top and Bottom) ---
	// Condition: Player is X-wise "Inside" the net area (between back and front)
	bool isInsideX = goal.isHomeGoal ? (pPos.x < goalX + pRadius && pPos.x > backX - pRadius)
		: (pPos.x > goalX - pRadius && pPos.x < backX + pRadius);

	if (isInsideX) {
		// TOP SIDE COLLISION
		if (std::abs(pPos.y - topY) < pRadius) {
			// If pPos.y < topY, they are OUTSIDE the goal. If pPos.y > topY, they are INSIDE.
			float pushDir = (pPos.y < topY) ? -1.0f : 1.0f;
			player.setPosition({ pPos.x, topY + (pushDir * pRadius) });
			player.setVelocity({ pVel.x * 0.5f, -pVel.y * 0.2f }); // Bounce Y
		}
		// BOTTOM SIDE COLLISION
		else if (std::abs(pPos.y - bottomY) < pRadius) {
			float pushDir = (pPos.y > bottomY) ? 1.0f : -1.0f;
			player.setPosition({ pPos.x, bottomY + (pushDir * pRadius) });
			player.setVelocity({ pVel.x * 0.5f, -pVel.y * 0.2f }); // Bounce Y
		}
	}

	// --- 2. THE BACK WALL ---
	// Condition: Player is Y-wise "Inside" the net (between top and bottom)
	if (pPos.y > topY - pRadius && pPos.y < bottomY + pRadius) {
		bool hittingBack = goal.isHomeGoal ? (std::abs(pPos.x - backX) < pRadius)
			: (std::abs(pPos.x - backX) < pRadius);

		if (hittingBack) {
			// Determine if they are hitting the back from the pitch side or the outside
			float pushDir;
			if (goal.isHomeGoal) {
				pushDir = (pPos.x < backX) ? -1.0f : 1.0f; // Left of back wall or Right?
			}
			else {
				pushDir = (pPos.x > backX) ? 1.0f : -1.0f; // Right of back wall or Left?
			}

			player.setPosition({ backX + (pushDir * pRadius), pPos.y });
			player.setVelocity({ -pVel.x * 0.2f, pVel.y * 0.5f }); // Bounce X
		}
	}
	// --- WALL C: The Posts (Circular Bounces) ---
	auto collideWithPost = [&](sf::CircleShape& post) {
		sf::Vector2f postPos = post.getPosition();
		sf::Vector2f diff = pPos - postPos;
		float distSq = (diff.x * diff.x) + (diff.y * diff.y);
		float minDist = pRadius + post.getRadius();

		if (distSq < minDist * minDist) {
			float dist = std::sqrt(distSq);
			sf::Vector2f normal = diff / dist;
			player.setPosition(postPos + normal * (minDist + 1.f));

			// Reflected bounce
			float dot = pVel.x * normal.x + pVel.y * normal.y;
			sf::Vector2f reflection = pVel - 2.f * dot * normal;
			player.setVelocity(reflection * 0.3f); // 70% speed loss on post hit
		}
		};

	collideWithPost(goal.topPost);
	collideWithPost(goal.bottomPost);
}

void GamePlay::runStandardSystems(float dt, sf::RenderWindow& t_window)
{
	// --- 1. DETERMINE TEAM STATES ---
	TeamState homeState = TeamState::Neutral;
	TeamState awayState = TeamState::Neutral;
	Player* owner = m_ball->getOwner();

	if (owner != nullptr) {
		homeState = (owner->isTeammate()) ? TeamState::Attacking : TeamState::Defending;
		awayState = (homeState == TeamState::Attacking) ? TeamState::Defending : TeamState::Attacking;
	}
	else {
		homeState = (m_ball->getPosition().x > m_pitch.halfwayLineX) ? TeamState::Attacking : TeamState::Defending;
		awayState = (homeState == TeamState::Attacking) ? TeamState::Defending : TeamState::Attacking;
	}

	// --- 2. UPDATE HUMAN USER ---
	m_userController->update(dt, *this);
	m_userController->mouseAiming(mouseWorld, t_window, m_playerCam);
	m_userPlayer->update(dt, m_animServer);

	// --- 3. GATHER ACTIVE LISTS & FIND FIRST RESPONDERS ---
	std::vector<Player*> homeFriends;
	homeFriends.push_back(m_userPlayer.get());
	for (auto& npc : m_teammates) homeFriends.push_back(npc.get());

	std::vector<Player*> homeEnemies;
	for (auto& opp : m_opponents) homeEnemies.push_back(opp.get());

	Player* homeFirstResponder = findFirstResponder(homeFriends);
	Player* awayFirstResponder = findFirstResponder(homeEnemies);

	// --- 4. UPDATE AI BRAINS ---
	for (auto& npc : m_teammates) {
		m_npcController->update(*npc, *m_userPlayer, *m_ball, homeFriends, homeEnemies, m_pitch, homeState, dt, homeFirstResponder, m_referee);
		npc->update(dt, m_animServer); // Internal motor physics
	}
	for (auto& npc : m_opponents) {
		m_npcController->update(*npc, *m_userPlayer, *m_ball, homeEnemies, homeFriends, m_pitch, awayState, dt, awayFirstResponder, m_referee);
		npc->update(dt, m_animServer);
	}

	// --- 5. WORLD PHYSICS & COLLISIONS ---
	m_ball->update(dt);

	// Collect everyone for collisions
	std::vector<Player*> allPlayers = homeFriends;
	allPlayers.insert(allPlayers.end(), homeEnemies.begin(), homeEnemies.end());

	handleBallPlayerPhysics(allPlayers, *m_ball);
	handlePlayerCollisions(allPlayers, m_pitch);

	handleGoalPhysics(m_homeGoal);
	handleGoalPhysics(m_awayGoal);
	
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
		if (p->getPositionRole() == PositionRole::LCenterBack || p->getPositionRole() == PositionRole::RCenterBack || p->getPositionRole() == PositionRole::LeftBack || p->getPositionRole() == PositionRole::RightBack) {
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

	// A CircleShape with 3 points is a triangle!
	sf::CircleShape indicator(20.f, 3);
	indicator.setFillColor(sf::Color(255, 255, 0, 130)); // Semi-transparent yellow
	indicator.setOrigin({ 20.f, 20.f });
	indicator.setRotation(sf::degrees(270.f)); // Rotate 180 degrees to point down

	// Position it hovering just above the player's head
	sf::Vector2f playerPos = m_userPlayer->getPosition();
	indicator.setPosition({ playerPos.x + 100.f, playerPos.y });

	t_window.draw(indicator);

	// ==========================================
	// 2. SCREEN-SPACE UI (Minimap & Scoreboard)
	// ==========================================
	// Save the camera view, then switch to the static screen view
	sf::View worldView = t_window.getView();
	t_window.setView(t_window.getDefaultView());

	// --- A. MINIMAP BACKGROUND ---
	float mapWidth = 300.f;
	float mapHeight = 210.f; // 10:7 aspect ratio to match your 10000x7000 pitch
	float padding = 20.f;

	// Top right corner calculation
	sf::Vector2f mapPos(t_window.getSize().x - mapWidth - padding, padding);

	sf::RectangleShape mapBg(sf::Vector2f(mapWidth, mapHeight));
	mapBg.setPosition(mapPos);
	mapBg.setFillColor(sf::Color(50, 50, 50, 150)); // See-through dark grey
	mapBg.setOutlineThickness(2.f);
	mapBg.setOutlineColor(sf::Color(255, 255, 255, 200));
	t_window.draw(mapBg);

	// --- B. PITCH LINES ---
	// Halfway Line
	sf::RectangleShape halfway(sf::Vector2f(2.f, mapHeight));
	halfway.setPosition({ mapPos.x + (mapWidth / 2.f) - 1.f, mapPos.y });
	halfway.setFillColor(sf::Color(255, 255, 255, 100));
	t_window.draw(halfway);

	// Penalty Boxes (Proportionally scaled to 16.5m x 40m)
	float boxW = mapWidth * 0.165f;
	float boxH = mapHeight * 0.575f;
	float boxY = mapPos.y + (mapHeight - boxH) / 2.f;

	sf::RectangleShape leftBox(sf::Vector2f(boxW, boxH));
	leftBox.setPosition({ mapPos.x, boxY });
	leftBox.setFillColor(sf::Color::Transparent);
	leftBox.setOutlineThickness(1.f);
	leftBox.setOutlineColor(sf::Color(255, 255, 255, 100));
	t_window.draw(leftBox);

	sf::RectangleShape rightBox(sf::Vector2f(boxW, boxH));
	rightBox.setPosition({ mapPos.x + mapWidth - boxW, boxY });
	rightBox.setFillColor(sf::Color::Transparent);
	rightBox.setOutlineThickness(1.f);
	rightBox.setOutlineColor(sf::Color(255, 255, 255, 100));
	t_window.draw(rightBox);

	// --- C. PLAYER & BALL DOTS ---
	auto drawMinimapDot = [&](sf::Vector2f worldPos, sf::Color color, float radius = 3.f) {
		// 1. Calculate the actual dimensions inside the chalk lines
		float playWidth = m_pitch.totalWidth - (2.f * m_pitch.margin);
		float playHeight = m_pitch.totalHeight - (2.f * m_pitch.margin);

		// 2. Subtract the margin so the touchlines map perfectly to 0.0 and 1.0
		float normX = (worldPos.x - m_pitch.margin) / playWidth;
		float normY = (worldPos.y - m_pitch.margin) / playHeight;

		// 3. Clamp keeps players on the edge of the minimap if they step out of bounds
		normX = std::clamp(normX, 0.0f, 1.0f);
		normY = std::clamp(normY, 0.0f, 1.0f);

		sf::CircleShape dot(radius);
		dot.setOrigin({ radius, radius });
		dot.setPosition({ mapPos.x + (normX * mapWidth), mapPos.y + (normY * mapHeight) });
		dot.setFillColor(color);
		t_window.draw(dot);
		};

	for (const auto& tm : m_teammates) drawMinimapDot(tm->getPosition(), sf::Color::White);
	for (const auto& opp : m_opponents) drawMinimapDot(opp->getPosition(), sf::Color::Blue);

	drawMinimapDot(m_userPlayer->getPosition(), sf::Color::Yellow, 4.f);
	if (m_ball) drawMinimapDot(m_ball->getPosition(), sf::Color(255, 165, 0), 4.f);

	// ==========================================
	// --- D. TV BROADCAST SCOREBOARD ---
	// ==========================================
	float scoreWidth = 260.f;
	float scoreHeight = 65.f;
	sf::Vector2f scorePos(20.f, 20.f); // Top Left Corner

	// 1. Scoreboard Background
	sf::RectangleShape scoreBg(sf::Vector2f(scoreWidth, scoreHeight));
	scoreBg.setPosition(scorePos);
	scoreBg.setFillColor(sf::Color(20, 20, 30, 200)); // Dark navy blue broadcast feel
	scoreBg.setOutlineThickness(2.f);
	scoreBg.setOutlineColor(sf::Color(255, 255, 255, 150));
	t_window.draw(scoreBg);

	// 2. Format the Match Score Text
	int hScore = m_referee.getHomeScore();
	int aScore = m_referee.getAwayScore();
	std::string scoreString = "HOME  " + std::to_string(hScore) + " - " + std::to_string(aScore) + "  AWAY";

	// In SFML 3.0, the font is passed directly into the constructor
	sf::Text scoreText(m_font, scoreString, 22);
	scoreText.setFillColor(sf::Color::White);

	// Center the score text horizontally inside the box
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
	// 3. RESTORE WORLD VIEW
	// ==========================================
	// Put the camera back to normal so the next frame renders the pitch correctly!
	t_window.setView(worldView);
}
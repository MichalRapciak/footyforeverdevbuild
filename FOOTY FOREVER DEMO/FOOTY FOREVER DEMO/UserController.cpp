#include "UserController.h"
#include "UserPlayer.h"
#include "Ball.h"
#include "MatchReferee.h"
#include "GamePlay.h"
#include "PhysicsEngine.h"
#include "AimAssist.h"
#include "Pitch.h"

UserController::UserController(UserPlayer& player) : m_userPlayer(player)
{
	m_targetHighlight.setFillColor(sf::Color::Transparent);
	m_targetHighlight.setOutlineColor(sf::Color(255, 255, 255, 180)); // Semi-transparent white
	m_targetHighlight.setOutlineThickness(2.0f);
	m_targetHighlight.setRadius(80.f);
	m_targetHighlight.setOrigin({ 80.f, 80.f });
}
UserController::~UserController()
{
}

/// <summary>
/// Handling Input through Player Controller class, since within menus etc. you won't need to move the player it's good to keep these separate
/// </summary>
/// <param name="t_event"></param>
/// <param name="isPressed"></param>
void UserController::inputHandler(const sf::Event t_event)
{
	if (const auto keyPressed = t_event.getIf<sf::Event::KeyPressed>()) // This checks what key is pressed and tells the updater which action to do
	{
		if (keyPressed->scancode == sf::Keyboard::Scancode::W)
		{
			m_up = true;
		}
		if (keyPressed->scancode == sf::Keyboard::Scancode::S)
		{
			m_down = true;
		}
		if (keyPressed->scancode == sf::Keyboard::Scancode::A)
		{
			m_left = true;
			// ==========================================
			// --- NEW: DOUBLE TAP A (LEFT BARGE) ---
			// ==========================================
			if (!m_wasAPressed) {
				if (m_tapTimerA > 0.f) {
					// Double tap detected!
					triggerBargeLeft = true;
					m_tapTimerA = 0.f; // Consume tap
				}
				else {
					m_tapTimerA = 0.25f; // 250ms window for the second tap
				}
			}
			m_wasAPressed = true;
		}
		if (keyPressed->scancode == sf::Keyboard::Scancode::D)
		{
			m_right = true;
			// ==========================================
			// --- NEW: DOUBLE TAP D (RIGHT BARGE) ---
			// ==========================================
			if (!m_wasDPressed) {
				if (m_tapTimerD > 0.f) {
					// Double tap detected!
					triggerBargeRight = true;
					m_tapTimerD = 0.f; // Consume tap
				}
				else {
					m_tapTimerD = 0.25f; // 250ms window
				}
			}
			m_wasDPressed = true;
		}
		if (keyPressed->scancode == sf::Keyboard::Scancode::LShift)
		{
			isSprinting = true;
		}
		if (keyPressed->scancode == sf::Keyboard::Scancode::LControl)
		{
			if (m_userPlayer.getBallPossession())
			{
				m_userPlayer.changeFoot();
			}
			else if (m_userPlayer.canTackle())
			{
				m_userPlayer.startTackle(m_userPlayer.getAimDirection());
			}
		}// --- NEW: FOOT SWITCH ON 'Q' ---
		if (keyPressed->scancode == sf::Keyboard::Scancode::Q)
		{
			isPressing = true;
		}
		if (keyPressed->scancode == sf::Keyboard::Scancode::Space)
		{
			if (m_userPlayer.getPositionRole() == PositionRole::Goalkeeper)
			{
				// --- MANUAL GOALKEEPER DIVE ---
				if (m_userPlayer.z <= 0.0f && m_userPlayer.getState() != PlayerState::Diving)
				{
					// Dive speed scales with GK Reactions stat
					float diveSpeed = 600.f + (m_userPlayer.getGkReactions() / 100.f) * 500.f;
					m_userPlayer.setVelocity(m_userPlayer.getAimDirection() * diveSpeed);
					m_userPlayer.vz = 140.f; // Medium hop height
					m_userPlayer.setState(PlayerState::Diving);
					m_userPlayer.deductStaminaAction(2.0f);
				}
			}
			else
			{
				// --- NORMAL PLAYER JUMP ---
				if (m_userPlayer.z <= 0.0f && m_userPlayer.getState() != PlayerState::Tackling)
				{
					float jumpingNorm = m_userPlayer.getJumpingStrength() / 100.0f;
					float jumpVz = 240.f + (jumpingNorm * 160.f);

					m_userPlayer.deductStaminaAction(1.5f);
					m_userPlayer.vz = jumpVz;
					m_userPlayer.setState(PlayerState::Jumping);
				}
			}
		}
		if (keyPressed->scancode == sf::Keyboard::Scancode::E)
		{
			switchPressed = true;
		}
	}

	if (const auto keyReleased = t_event.getIf<sf::Event::KeyReleased>())
	{
		if (keyReleased->scancode == sf::Keyboard::Scancode::W) // These check what key is released, telling the updated to decelerate
		{
			m_up = false;
		}
		if (keyReleased->scancode == sf::Keyboard::Scancode::S)
		{
			m_down = false;
		}
		if (keyReleased->scancode == sf::Keyboard::Scancode::A)
		{
			m_left = false;
			m_wasAPressed = false; // Reset state for next press
		}
		if (keyReleased->scancode == sf::Keyboard::Scancode::D)
		{
			m_right = false;
			m_wasDPressed = false; // Reset state for next press
		}
		if (keyReleased->scancode == sf::Keyboard::Scancode::LShift)
		{
			isSprinting = false;
		}
		if (keyReleased->scancode == sf::Keyboard::Scancode::Q)
		{
			isPressing = false;
		}
	}
	if (const auto buttonPressed = t_event.getIf<sf::Event::MouseButtonPressed>())
	{
		if (buttonPressed->button == sf::Mouse::Button::Left) {

			kickPressed = true;
			isHighKick = false; // We are doing a ground action
			isShooting = true;
		}
		if (buttonPressed->button == sf::Mouse::Button::Right) {
			kickPressed = true;
			isHighKick = true;  // We are doing an aerial action
			isShooting = true;
		}
	}
	if (const auto buttonReleased = t_event.getIf<sf::Event::MouseButtonReleased>())
	{
		if (buttonReleased->button == sf::Mouse::Button::Left ||
			buttonReleased->button == sf::Mouse::Button::Right)
		{
			isChasingLooseBall = false;
			isJockeying = false;
			kickPressed = false;
			isShooting = false;
			// Note: charging = false will be handled in playerShooting
		}
	}
}

/// <summary>
/// updating player movement - can be turned off if within menus.
/// </summary>
/// <param name="isPressed"></param>
/// <summary>
/// updating player movement - can be turned off if within menus.
/// </summary>
/// <param name="isPressed"></param>
void UserController::update(float dt, GamePlay& game)
{
	// ==========================================
	// --- THE FIX: USER INJURY EJECTION ---
	// ==========================================
	if (m_userPlayer.getState() == PlayerState::Injured) {
		// Bleed momentum
		PhysicsEngine::applyPlayerIdleFriction(m_userPlayer, dt);
		PhysicsEngine::updatePlayerAirPhysics(m_userPlayer, dt);

		// Find a healthy teammate to auto-switch to!
		std::vector<Player*> myTeam;
		auto& npcList = (m_userPlayer.getTeam() == Team::Home) ? game.m_homeside : game.m_awayside;

		for (auto& npc : npcList) {
			if (!npc->isSentOff() && npc->getState() != PlayerState::Injured && npc->getPositionRole() != PositionRole::Goalkeeper) {
				myTeam.push_back(npc.get());
			}
		}

		if (!myTeam.empty()) {
			// Hijack the nearest healthy defender
			Player* rescueTarget = findBestDefensiveSwitch(m_userPlayer, myTeam, *game.m_ball, game.m_pitch);
			sf::Vector2f tempAim = m_userPlayer.getPlayerAim();
			game.executePlayerSwitch(rescueTarget);
			game.m_referee.notifyPlayerSwap(&m_userPlayer, rescueTarget);
			resetInputs();
			m_userPlayer.updateAim(tempAim);
		}
		return; // Halt the update loop for the injured player
	}


	MatchState state = game.m_referee.getMatchState();
	Player* currentOwner = game.m_ball->getOwner();

	bool hasPossession = (currentOwner == &m_userPlayer);
	bool userTeamHasPossession = (currentOwner != nullptr && currentOwner->getTeam() == m_userPlayer.getTeam());

	// ==========================================
	// --- AUTOMATIC OFFENSIVE SWITCHING ---
	// ==========================================
	if (state == MatchState::InPlay && userTeamHasPossession && !hasPossession) {
		if (currentOwner->getPositionRole() != PositionRole::Goalkeeper) {
			sf::Vector2f tempAim = m_userPlayer.getPlayerAim();
			game.executePlayerSwitch(currentOwner);
			game.m_referee.notifyPlayerSwap(&m_userPlayer, currentOwner);
			resetInputs();
			m_userPlayer.updateAim(tempAim);
			hasPossession = true;
		}
	}

	// ==========================================
	// --- MANUAL SWITCHING (Offense & Defense) ---
	// ==========================================
	if (switchPressed) {
		switchPressed = false;

		std::vector<Player*> myTeam;
		auto& npcList = (m_userPlayer.getTeam() == Team::Home) ? game.m_homeside : game.m_awayside;

		for (auto& npc : npcList) {
            // THE FIX: Added `npc->getState() != PlayerState::Injured`
            if (!npc->isSentOff() && npc->getState() != PlayerState::Injured && npc->getPositionRole() != PositionRole::Goalkeeper) {
                myTeam.push_back(npc.get());
            }
        }

		Player* targetPlayer = nullptr;

		if (!userTeamHasPossession) {
			targetPlayer = findBestDefensiveSwitch(m_userPlayer, myTeam, *game.m_ball, game.m_pitch);
		}
		else {
			float minDist = 999999.f;
			sf::Vector2f ballPos = game.m_ball->getPosition();

			for (Player* teammate : myTeam) {
				if (teammate == &m_userPlayer) continue;

				float dx = teammate->getPosition().x - ballPos.x;
				float dy = teammate->getPosition().y - ballPos.y;
				float distSq = (dx * dx) + (dy * dy);

				if (distSq < minDist) {
					minDist = distSq;
					targetPlayer = teammate;
				}
			}
		}

		if (targetPlayer && targetPlayer != &m_userPlayer) {
			sf::Vector2f tempAim = m_userPlayer.getPlayerAim();
			game.executePlayerSwitch(targetPlayer);
			game.m_referee.notifyPlayerSwap(&m_userPlayer, targetPlayer);
			resetInputs();
			m_userPlayer.updateAim(tempAim);
		}
	}

	// ==========================================
	// --- BUG FIX: INPUT BLEED INTERCEPTORS ---
	// ==========================================
	if (state != m_lastMatchState) resetInputs();
	if (hasPossession && !m_hadPossessionLastFrame) resetInputs();

	m_lastMatchState = state;
	m_hadPossessionLastFrame = hasPossession;

	PhysicsEngine::updatePlayerAirPhysics(m_userPlayer, dt);

	bool isTaker = (game.m_referee.getSetPieceTaker() == &m_userPlayer);

	if (state != MatchState::InPlay && m_userPlayer.getState() == PlayerState::Tackling) {
		m_userPlayer.setState(PlayerState::Normal);
	}

	// ==========================================
	// 3. MATCH PAUSED & CELEBRATION STATES
	// ==========================================
	if (state == MatchState::HalfTime || state == MatchState::FullTime || state == MatchState::GoalScored)
	{
		m_userPlayer.updateStamina(dt, false);
		m_userPlayer.setVelocity(m_userPlayer.getVelocity() * 0.85f);
		return;
	}

	// ==========================================
	// 4. DEAD BALL OVERRIDE (Set Pieces)
	// ==========================================
	if (state != MatchState::InPlay)
	{
		// --- STAMINA HOOK: Catching breath while setting up the play ---
		m_userPlayer.updateStamina(dt, false);
		updateTargetScanning(game);

		if (isTaker) {
			// THE FIX: Kick-offs, Throw-ins, and Penalties do not use the free-roaming run-up!
			bool needsRunUp = (state == MatchState::FreeKick || state == MatchState::Corner || state == MatchState::GoalKick);
			m_isSetPieceRunUp = needsRunUp;

			if (game.m_referee.isWhistleBlown()) {
				// 1. ALLOW MOVEMENT FOR THE TAKER
				m_speedVector = m_userPlayer.getVelocity();
				playerMovement(dt, game);

				if (needsRunUp) {
					// 2. THE TETHER (3 Meters / 300px)
					sf::Vector2f ballPos = game.m_ball->getPosition();
					sf::Vector2f myPos = m_userPlayer.getPosition();
					float distToBall = dist(myPos, ballPos);

					if (distToBall > 300.f) {
						sf::Vector2f toPlayer = myPos - ballPos;
						float len = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);

						if (len > 0.001f) {
							sf::Vector2f normal = toPlayer / len;
							sf::Vector2f tetherPos = ballPos + (normal * 300.f);
							m_userPlayer.setPosition(tetherPos);

							// THE FIX: Kill outward momentum so they don't get stuck in molasses!
							sf::Vector2f currentVel = m_userPlayer.getVelocity();
							float outwardSpeed = (currentVel.x * normal.x) + (currentVel.y * normal.y);

							// If velocity is pushing them outside the circle, strip it away!
							// (This leaves tangential velocity intact, allowing smooth orbiting)
							if (outwardSpeed > 0.f) {
								m_userPlayer.setVelocity(currentVel - (normal * outwardSpeed));
							}
						}
					}
				}
				else {
					// 3. INSTANT POSSESSION (Kick-Offs, Throw-Ins, Penalties)
					// If we don't need a run-up, automatically put the ball at our feet!
					if (game.m_ball->getOwner() != &m_userPlayer && dist(m_userPlayer.getPosition(), game.m_ball->getPosition()) < 70.f) {
						game.m_ball->possess(&m_userPlayer);
					}
				}

				playerShooting(dt, game);
			}
			else {
				m_userPlayer.setVelocity({ 0.f, 0.f }); // Freeze before the whistle
			}
		}
		else {
			m_isSetPieceRunUp = false;
			if (state == MatchState::KickOff && !game.m_referee.isWhistleBlown()) {
				m_userPlayer.setVelocity({ 0.f, 0.f });
			}
			else {
				m_speedVector = m_userPlayer.getVelocity();
				playerMovement(dt, game);
			}
		}
		return;
	}
	else {
		m_isSetPieceRunUp = false;
	}

	// ==========================================
	// 5. NORMAL OPEN PLAY
	// ==========================================
	m_speedVector = m_userPlayer.getVelocity();
	updateTargetScanning(game);
	playerMovement(dt, game);
	playerShooting(dt, game);
}

void UserController::draw(sf::RenderWindow& window)
{
	if (m_currentTarget) {
		if (charging) {
			// Grow the circle based on the live kickStrength
			float pulse = 80.f + (kickStrength * 20.f);
			m_targetHighlight.setRadius(pulse);
			m_targetHighlight.setOrigin({ pulse, pulse });

			// Visual feedback: Orange while waiting to strike, Green while actively charging
			if (!kickPressed) {
				m_targetHighlight.setOutlineColor(sf::Color(255, 165, 0, 200)); // Orange/Gold
			}
			else {
				m_targetHighlight.setOutlineColor(sf::Color(100, 255, 100, 200)); // Green
			}
		}
		else {
			m_targetHighlight.setRadius(40.f);
			m_targetHighlight.setOrigin({ 40.f, 40.f });
			m_targetHighlight.setOutlineColor(sf::Color(255, 255, 255, 150));
		}
		window.draw(m_targetHighlight);
	}
}

/// <summary>
/// Calculating Player Aim and facing Direction sending it to the Player
/// </summary>
/// <param name="t_mouseWorld"></param>
void UserController::mouseAiming(sf::Vector2f t_mouseWorld, sf::RenderWindow& t_window, sf::View t_view)
{
	// The view is rotated 90 degrees, so mapPixelToCoords automatically handles
	// translating the screen's X/Y mouse position into the correct rotated World X/Y!
	sf::Vector2f mouseWorldPos = t_window.mapPixelToCoords(sf::Mouse::getPosition(t_window), t_view);

	// Pass the actual world position to the player
	// (Notice we don't need to calculate angles here anymore)
	m_userPlayer.updateAim(mouseWorldPos);
}

/// <summary>
/// PLAYER MOVEMENT
/// </summary>
/// <param name="dt"></param>
/// <param name="game"></param>
void UserController::playerMovement(float dt, GamePlay& game)
{
	bool canMove = !m_userPlayer.isTackling();

	// 1. Tick down the double-tap windows
	if (m_tapTimerA > 0.f) m_tapTimerA -= dt;
	if (m_tapTimerD > 0.f) m_tapTimerD -= dt;

	// ==========================================
	// --- EXECUTE USER BARGE ---
	// ==========================================
	if (triggerBargeLeft || triggerBargeRight && canMove && !m_userPlayer.getBallPossession()) {

		Player* bestTarget = nullptr;
		float minDist = 150.f; // Max reach of a barge (1.5 meters)

		sf::Vector2f myVel = m_userPlayer.getVelocity();
		float speed = std::sqrt(myVel.x * myVel.x + myVel.y * myVel.y);

		// Default to facing down the X-axis if standing perfectly still
		sf::Vector2f myDir = (speed > 10.f) ? (myVel / speed) : sf::Vector2f(1.f, 0.f);

		// Grab the opposing team list
		auto& oppTeam = (m_userPlayer.getTeam() == Team::Home) ? game.m_awayside : game.m_homeside;

		for (auto& oppPtr : oppTeam) {
			Player* opp = oppPtr.get();
			if (opp->getState() == PlayerState::Injured || opp->isSentOff()) continue;

			sf::Vector2f toOpp = opp->getPosition() - m_userPlayer.getPosition();
			float oppDist = std::sqrt(toOpp.x * toOpp.x + toOpp.y * toOpp.y);

			if (oppDist < minDist) {
				// The 2D Cross Product tells us if the opponent is to our local left or right!
				float cross = (myDir.x * toOpp.y) - (myDir.y * toOpp.x);

				// If Left (A) was double-tapped, we only care about opponents where Cross < 0
				if (triggerBargeLeft && cross < 0.f) {
					minDist = oppDist;
					bestTarget = opp;
				}
				// If Right (D) was double-tapped, we only care about opponents where Cross > 0
				else if (triggerBargeRight && cross > 0.f) {
					minDist = oppDist;
					bestTarget = opp;
				}
			}
		}

		if (bestTarget) {
			m_userPlayer.executeShoulderBarge(bestTarget);
		}

		// Reset triggers so they don't fire every frame
		triggerBargeLeft = false;
		triggerBargeRight = false;
	}

	sf::Vector2f forwardDir(m_userPlayer.getAimDirection());
	sf::Vector2f rightDir(-forwardDir.y, forwardDir.x);
	directionInput = { 0.f, 0.f };

	if (canMove)
	{
		// --- 1. RAW USER INPUT ---
		if (m_up)    directionInput += forwardDir;
		if (m_down)  directionInput -= forwardDir;
		if (m_left)  directionInput -= rightDir;
		if (m_right) directionInput += rightDir;

		// --- 2. CONTEXT AWARENESS (Dribble/Jockey/Chase) ---
		if (isPressing)
		{
			if (!game.m_ball->hasOwner()) {
				isChasingLooseBall = true;
			}
			else if (game.m_ball->getOwner()->getTeam() != m_userPlayer.getTeam()) {
				isJockeying = true;
			}
		}

		// --- 3. STAMINA INTERCEPTOR ---
		bool isEffectivelySprinting = isSprinting || isChasingLooseBall || isJockeying;

		if (m_userPlayer.getCurrentStamina() < 2.0f)
		{
			// The player is completely dead on their feet. Cancel all intense actions!
			isEffectivelySprinting = false;
			isSprinting = false;
			isChasingLooseBall = false;
			isJockeying = false;
		}	

		// Update the gauge based on their final verified effort level
		m_userPlayer.updateStamina(dt, isEffectivelySprinting);


		// --- 4. AUTOMATED ASSISTS (Target Lock-on & Orbital Damping) ---
		if (isChasingLooseBall || isJockeying)
		{
			sf::Vector2f targetPos;

			if (isChasingLooseBall)
			{
				targetPos = game.m_ball->getPosition();
			}
			else if (isJockeying)
			{
				Player* threat = game.m_ball->getOwner();
				sf::Vector2f threatPos = threat->getPosition();

				bool isHomeSide = (m_userPlayer.getTeam() == Team::Home);
				sf::Vector2f myGoalPos = isHomeSide ? sf::Vector2f(500.f, 3500.f) : sf::Vector2f(9500.f, 3500.f);

				sf::Vector2f toGoal = myGoalPos - threatPos;
				float toGoalLen = std::sqrt(toGoal.x * toGoal.x + toGoal.y * toGoal.y);
				if (toGoalLen > 0.001f) toGoal /= toGoalLen;

				float aggressionNorm = m_userPlayer.getAggression() / 100.0f;
				float awarenessNorm = m_userPlayer.getAwareness() / 100.0f;

				float dynamicBuffer = 150.f - (aggressionNorm * 100.f);
				float distToThreat = std::sqrt(std::pow(m_userPlayer.getPosition().x - threatPos.x, 2) +
					std::pow(m_userPlayer.getPosition().y - threatPos.y, 2));

				float commitThreshold = 80.f + ((1.0f - awarenessNorm) * 60.f);

				if (distToThreat < commitThreshold && m_userPlayer.canTackle()) {
					float tackleStep = 40.f - (aggressionNorm * 20.f);
					targetPos = threatPos + (toGoal * tackleStep);
				}
				else {
					targetPos = threatPos + (toGoal * dynamicBuffer);
				}
			}

			sf::Vector2f toTarget = targetPos - m_userPlayer.getPosition();
			float dist = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);

			if (dist > 10.f)
			{
				sf::Vector2f targetDir = toTarget / dist;

				// --- PHYSICS ENGINE: KILL ORBITAL DRIFT ---
				float dampingStrength = isJockeying ? 10.0f : 6.0f;
				PhysicsEngine::applyTangentialVelocityDamping(m_userPlayer, targetDir, dampingStrength, dt);

				float pullStrength = isJockeying ? 0.85f : 0.55f;
				if (directionInput.x == 0.f && directionInput.y == 0.f) {
					directionInput = targetDir;
				}
				else {
					directionInput = (directionInput * (1.0f - pullStrength)) + (targetDir * pullStrength);
				}
			}
		}

		// Normalize final intent vector
		if (directionInput.x != 0.f || directionInput.y != 0.f)
		{
			float length = std::sqrt(directionInput.x * directionInput.x + directionInput.y * directionInput.y);
			directionInput /= length;
		}
	}

	// ==========================================
	// 5. DISPATCH TO PHYSICS ENGINE
	// ==========================================
	if (m_userPlayer.getPositionRole() == PositionRole::Goalkeeper && m_userPlayer.getState() != PlayerState::Diving) {
		attemptSave(dt, game);
	}

	if (m_userPlayer.getState() == PlayerState::Tackling) {
		PhysicsEngine::applySlideTackleFriction(m_userPlayer, dt);
	}
	else if (m_userPlayer.getState() == PlayerState::Diving) {
		// --- REPLACE MANUAL DRAG WITH ENGINE CALL ---
		PhysicsEngine::applyKeeperDiveFriction(m_userPlayer, dt);
	}
	else {
		// Determine maximum permitted speed based on context
		float currentMax = m_userPlayer.getTopSpeed() * 10.0f;

		if (m_isSetPieceRunUp) {
			// THE FIX: Boost base movement speed during run-ups for snappy adjustments!
			currentMax = isSprinting ? currentMax : currentMax * 0.85f;
		}
		else if (isChasingLooseBall) {
			currentMax = m_userPlayer.getTopSpeed() * 10.0f;
		}
		else if (isJockeying) {
			float aggressionNorm = m_userPlayer.getAggression() / 100.0f;
			currentMax *= (0.70f + (aggressionNorm * 0.30f));
		}
		else {
			currentMax = isSprinting ? currentMax : currentMax * 0.5f;
		}

		// Apply Movement Forces
		if (directionInput.x == 0.f && directionInput.y == 0.f) {
			PhysicsEngine::applyPlayerIdleFriction(m_userPlayer, dt);
		}
		else {
			PhysicsEngine::applyPlayerLocomotion(m_userPlayer, directionInput, currentMax, dt);
		}
	}

	// Optional: Keep the user strictly inside the stadium grass
	PhysicsEngine::resolvePlayerPitchBoundaries(m_userPlayer, game.m_pitch);

	// Sync local variables (used by camera / UI tracking)
	m_speedVector = m_userPlayer.getVelocity();
	speed = std::sqrt(m_speedVector.x * m_speedVector.x + m_speedVector.y * m_speedVector.y);
}


/// <summary>
/// PLAYER BALL KICKING
/// </summary>
/// <param name="dt"></param>
/// <param name="game"></param>
void UserController::playerShooting(float dt, GamePlay& game)
{
	// --- 1. COOLDOWNS & AUTO-POSSESS ---
	if (kickCooldownTimer > 0.f) {
		justKicked = false;
		kickCooldownTimer -= dt;
	}
	if (kickCooldownTimer < 0.f) kickCooldownTimer = 0.f;

	float distToBall = game.distance(m_userPlayer.getPosition(), game.m_ball->getPosition());
	bool hasPossession = (game.m_ball->getOwner() == &m_userPlayer);

	// Prevent auto-possess during a Set Piece run-up so the ball stays perfectly still!
	if (!hasPossession && kickCooldownTimer <= 0.f && !m_isSetPieceRunUp) {
		if (distToBall < 70.f && m_userPlayer.getState() != PlayerState::Tackling && m_userPlayer.getState() != PlayerState::Stunned && m_userPlayer.getState() != PlayerState::Stumbled && m_userPlayer.getState() != PlayerState::FallOver && game.m_ball->z < 40.f) {
			game.m_ball->possess(&m_userPlayer);
			hasPossession = true;
		}
	}

	bool canContestAir = (game.m_ball->z > 40.f && distToBall < 150.f);

	// ==========================================
	// --- 2. UNIFIED CHARGING SYSTEM ---
	// ==========================================
	if (kickPressed) {
		justKicked = false;
		charging = true;
		m_preChargeTimer = 1.0f; // Refresh the 1-second forgiveness buffer

		// Standard Oscillating Charge
		if (increasing) {
			kickStrength += kickSpeed * dt;
			if (kickStrength >= 1.f) { kickStrength = 1.f; increasing = false; }
		}
		else {
			kickStrength -= kickSpeed * dt;
			if (kickStrength <= 0.f) { kickStrength = 0.f; increasing = true; }
		}
	}
	else {
		// The button was released!
		if (charging) {

			bool canStrikeNow = (hasPossession || canContestAir) && !m_isSetPieceRunUp;
			bool canSetPieceStrikeNow = m_isSetPieceRunUp && (distToBall < 100.f);

			// A. If we have the ball (or just walked up to a free kick), fire immediately!
			if (canStrikeNow || canSetPieceStrikeNow) {
				executeKickRelease(game);
				m_preChargeTimer = 0.0f;
				if (canSetPieceStrikeNow) m_isSetPieceRunUp = false;
			}
			// B. If we don't have the ball yet, we are PRE-CHARGING.
			else {
				// The power bar is frozen. Tick down the expiration timer!
				if (m_preChargeTimer > 0.0f) {
					m_preChargeTimer -= dt;

					if (m_preChargeTimer <= 0.0f) {
						// Pre-charge expired after 1 second!
						kickStrength = 0.0f;
						charging = false;
						increasing = true;
					}
				}
			}
		}
	}
}

// ==========================================
// --- KICK PIPELINE HELPERS ---
// ==========================================

void UserController::executeKickRelease(GamePlay& game)
{
	sf::Vector2f aimDir = m_userPlayer.getAimDirection();
	float basePower = m_userPlayer.getKickPower();
	float finalPower = basePower * kickStrength;
	float vzPower = 0.f;
	float errorAngle = 0.f;
	float finalBackspin = 0.f;

	// 1. Trajectory Calculation
	if (game.m_ball->z > 40.f) {
		if (!calculateAerialKick(game, finalPower, vzPower, errorAngle, finalBackspin)) {
			kickStrength = 0.f; charging = false; increasing = true; kickPressed = false;
			return;
		}
	}
	else {
		calculateGroundKick(basePower, finalPower, vzPower, errorAngle, finalBackspin);
	}

	// ==========================================
	// --- WEAK FOOT INJECTION ---
	// ==========================================
	bool isRightFooted = (m_userPlayer.getPreferredFoot() == "Right");
	bool usingRight = m_userPlayer.usingRightFoot();
	bool isWeakFoot = (isRightFooted && !usingRight) || (!isRightFooted && usingRight);

	if (isWeakFoot) {
		float wfPowerMod = 1.0f;
		float wfErrorMod = 1.0f;
		float extraShank = getWeakFootPenalty(m_userPlayer.getWeakFootAccuracy(), wfPowerMod, wfErrorMod);

		finalPower *= wfPowerMod;
		errorAngle = (errorAngle * wfErrorMod) + extraShank;
	}

	// ==========================================
	// --- 2. APPLY RAW STAT ERROR FIRST ---
	// ==========================================
	// Inject a specific intrinsic penalty if the user's finishing is bad.
	if (m_currentTarget == nullptr && !isHighKick) {
		// Up to 15 degrees of pure inaccuracy for a 0 Finishing player.
		// Hitting it with maximum power makes the shank even wider!
		float finishingMiss = (1.0f - (m_userPlayer.getFinishing() / 100.f)) * 30.0f;
		errorAngle += finishingMiss * (0.5f + kickStrength);
	}

	// Apply the error directly to the raw mouse aim
	float randError = ((rand() % 100) / 100.f - 0.5f) * errorAngle;
	float rad = randError * 3.14159f / 180.f;

	// aimDir is now mathematically shanked
	aimDir = sf::Vector2f(
		aimDir.x * std::cos(rad) - aimDir.y * std::sin(rad),
		aimDir.x * std::sin(rad) + aimDir.y * std::cos(rad)
	);


	// ==========================================
	// --- 3. APPLY ASSISTS (The Corrector) ---
	// ==========================================
	// Elite players will use their magnetism to pull the shanked aimDir back on target.
	// Poor players have no magnetism, so the shanked aimDir stays wild!
	if (m_currentTarget != nullptr) {
		AimAssist::applyPassAssist(m_userPlayer, m_currentTarget, aimDir, finalPower, isHighKick, false);
		game.m_matchStats.recordPassAttempt(m_userPlayer.getTeam());
	}
	else if (!isHighKick) {
		AimAssist::applyShotAssist(m_userPlayer, aimDir, vzPower, finalPower, game.m_pitch);

		// ==========================================
		// --- THE FIX: GEOMETRIC SHOT TRACKING ---
		// ==========================================
		bool isHomeSide = (m_userPlayer.getTeam() == Team::Home);
		float targetLineX = isHomeSide ? (game.m_pitch.totalWidth - game.m_pitch.margin) : game.m_pitch.margin;

		bool onTarget = false;
		sf::Vector2f playerPos = m_userPlayer.getPosition();

		// Prevent divide-by-zero if shooting perfectly vertical
		if (std::abs(aimDir.x) > 0.001f) {
			// Calculate the multiplier 't' required to reach the goal line's X coordinate
			float t = (targetLineX - playerPos.x) / aimDir.x;

			// If t is positive, the ball is actually traveling TOWARD the goal!
			if (t > 0.f) {
				// Project the Y coordinate using 't'
				float intersectY = playerPos.y + (aimDir.y * t);

				float goalCenterY = 3500.f;
				float halfGoalWidth = 366.f;

				// Did the trajectory cross the line between the two posts?
				if (intersectY > goalCenterY - halfGoalWidth && intersectY < goalCenterY + halfGoalWidth) {
					onTarget = true;
				}
			}
		}

		game.m_matchStats.recordShot(m_userPlayer.getTeam(), onTarget);
	}

	// 4. Calculate Spin (Dampen curl for weak foot)
	float spin = 0.f;
	if (m_left) spin = usingRight ? -(m_userPlayer.getCurl() / 2.f) : m_userPlayer.getCurl();
	if (m_right) spin = usingRight ? -m_userPlayer.getCurl() : (m_userPlayer.getCurl() / 2.f);

	// Weak foot can't curl the ball as well
	if (isWeakFoot) spin *= (0.4f + (m_userPlayer.getWeakFootAccuracy() / 5.0f) * 0.6f);
	spin *= (1.1f + kickStrength / 2.f);

	// 5. Execute Audio & Shot
	float kickVol = std::clamp(0.f + ((finalPower / m_userPlayer.getKickPower()) * 40.0f), 10.f, 100.f);
	game.m_soundManager.playRandomSound("kick", 3, kickVol, 0.15f);

	// Notice we just pass aimDir here now, as it has already been corrected by the AimAssist
	game.m_ball->shoot(aimDir, finalPower, spin, vzPower, finalBackspin);

	kickStrength = 0.f;
	charging = false;
	increasing = true;
	justKicked = true;
	kickCooldownTimer = kickCooldown;
}

bool UserController::calculateAerialKick(GamePlay& game, float& finalPower, float& vzPower, float& errorAngle, float& finalBackspin)
{
	float distToBall = game.distance(m_userPlayer.getPosition(), game.m_ball->getPosition());
	if (distToBall > 120.f) return false; // Whiffed distance

	float relZ = game.m_ball->z - m_userPlayer.z;
	bool isHeader = (relZ >= 140.f && relZ <= 240.f);
	bool isVolley = (relZ >= 40.f && relZ < 140.f);

	if (!isHeader && !isVolley) return false; // Whiffed timing

	float stat = isHeader ? m_userPlayer.getHeading() : m_userPlayer.getFinishing();
	errorAngle = (1.0f - (stat / 100.f)) * (isHeader ? 15.0f : 10.0f);

	if (isHeader) {
		finalPower = (40.f + (stat * 0.6f)) * std::max(0.4f, kickStrength);
		vzPower = isHighKick ? (150.f + (stat * 1.5f)) : (100.f - (stat * 3.0f));
		finalBackspin = 10.f;
	}
	else {
		finalPower = m_userPlayer.getKickPower() * kickStrength * 1.2f;
		float techniqueError = (1.0f - (stat / 100.f));
		vzPower = 100.f + (techniqueError * 350.f);
		finalBackspin = 30.f;
	}
	return true;
}

void UserController::calculateGroundKick(float basePower, float& finalPower, float& vzPower, float& errorAngle, float& finalBackspin)
{
	if (isHighKick) {
		bool isPassing = (m_currentTarget != nullptr);
		float stat = isPassing ? m_userPlayer.getLongPassing() : m_userPlayer.getFinishing();
		errorAngle = (1.0f - (stat / 100.f)) * 8.0f;
		float statDampening = (stat / 100.f) * 80.f;

		float floatMultiplier = 1.1f - (kickStrength * 0.4f);
		finalPower = basePower * kickStrength * floatMultiplier;

		if (isPassing) {
			vzPower = 1150.f - (kickStrength * 300.f) - statDampening;
			finalBackspin = 60.f + (stat * 0.4f) + (kickStrength * 60.f);
		}
		else {
			vzPower = 880.f - (kickStrength * 200.f) - statDampening;
			finalBackspin = 80.f + (stat * 0.5f) + (kickStrength * 40.f);
		}
	}
	else {
		float stat = m_currentTarget ? m_userPlayer.getShortPassing() : m_userPlayer.getFinishing();
		errorAngle = (1.0f - (stat / 100.f)) * 5.0f;

		finalPower = basePower * kickStrength * 1.1f;
		vzPower = 10.f + (std::pow(kickStrength, 2.f) * 850.f);
		finalBackspin = 0.f;
	}
}

float UserController::dist(sf::Vector2f p1, sf::Vector2f p2) 
{
	float dx = p1.x - p2.x;
	float dy = p1.y - p2.y;
	return std::sqrt(dx * dx + dy * dy);
}

void UserController::updateTargetScanning(GamePlay& game)
{
	// Build a quick list of raw pointers for the assist engine
	std::vector<Player*> teammatesRaw;

	// THE FIX: Dynamically grab the correct team list!
	auto& myTeam = (m_userPlayer.getTeam() == Team::Home) ? game.m_homeside : game.m_awayside;
	for (auto& tm : myTeam) {
		// Don't add yourself to the pass target list!
		{
			teammatesRaw.push_back(tm.get());
		}
	}

	m_currentTarget = AimAssist::getTargetLock(m_userPlayer.getPosition(), m_userPlayer.getAimDirection(), teammatesRaw);

	if (m_currentTarget) {
		m_targetHighlight.setPosition({ m_currentTarget->getPosition().x - 30 , m_currentTarget->getPosition().y });
	}
}

void UserController::resetInputs() {
	// 1. Reset all shooting and charging variables
	kickStrength = 0.f;
	charging = false;
	increasing = true;
	m_preChargeTimer = 0.0f; // <-- Added to kill the timer!

	// 2. Force the buttons to 'unpress'
	kickPressed = false;
	isPressing = false;
	isShooting = false;

	// 3. Reset automated movement states
	isChasingLooseBall = false;
	isJockeying = false;
}

void UserController::attemptSave(float dt, GamePlay& game)
{
	sf::Vector2f ballVel = game.m_ball->getVelocity();
	float ballSpeed = std::sqrt(ballVel.x * ballVel.x + ballVel.y * ballVel.y);

	// Only trigger auto-saves on actual fast shots, not slow passes
	if (ballSpeed < 300.0f) return;

	sf::Vector2f ballPos = game.m_ball->getPosition();
	sf::Vector2f keeperPos = m_userPlayer.getPosition();

	// Make sure the ball is actually moving TOWARDS our goal
	bool isHomeSide = (m_userPlayer.getTeam() == Team::Home);
	if (isHomeSide && ballVel.x >= -10.0f) return;
	if (!isHomeSide && ballVel.x <= 10.0f) return;

	// Calculate Time-To-Intercept (TTI)
	float ballTTI = std::abs((keeperPos.x - ballPos.x) / ballVel.x);
	if (ballTTI < 0.0f || ballTTI > 1.5f) return;

	// Project the 3D height of the ball when it reaches the goal line
	float gravity = 980.f;
	float interceptZ = game.m_ball->z + (game.m_ball->vz * ballTTI) - (0.5f * gravity * ballTTI * ballTTI);
	interceptZ = std::max(0.f, interceptZ);

	float interceptY = ballPos.y + (ballVel.y * ballTTI);
	sf::Vector2f interceptPoint(keeperPos.x, interceptY);
	float diveDistance = std::abs(keeperPos.y - interceptY);

	// Determine max dive speed based on stats
	float distToBall = game.distance(keeperPos, ballPos);
	float activeStat = (distToBall < 600.0f) ? m_userPlayer.getGkBlocking() : m_userPlayer.getGkReactions();
	float maxDiveSpeed = 600.0f + ((activeStat / 100.0f) * 1000.0f);

	float keeperTTI = diveDistance / maxDiveSpeed;
	if (ballTTI > keeperTTI + 0.15f) return; // Cannot reach it in time

	float attemptDiveRadius = 800.0f;

	// If it's within reach, pull the trigger!
	if (diveDistance <= attemptDiveRadius && interceptZ <= (m_userPlayer.height + 120.0f))
	{
		float optimalSpeed = maxDiveSpeed;
		if (ballTTI > 0.05f) optimalSpeed = diveDistance / ballTTI;
		float finalSpeed = std::clamp(optimalSpeed, 150.0f, maxDiveSpeed);

		// Execute the Dive
		sf::Vector2f diveDir = game.normalize(interceptPoint - keeperPos);
		m_userPlayer.setVelocity(diveDir * finalSpeed);

		// Context-aware jump height
		if (interceptZ < 40.f) m_userPlayer.vz = 0.f;
		else if (interceptZ < 120.f) m_userPlayer.vz = 140.f;
		else m_userPlayer.vz = 200.f + (interceptZ * 0.2f);

		m_userPlayer.setState(PlayerState::Diving);
	}
}

Player* UserController::findBestDefensiveSwitch(Player& currentPlayer, const std::vector<Player*>& team, Ball& ball, const Pitch& pitch) {
	if (team.empty()) return &currentPlayer;

	sf::Vector2f ballPos = ball.getPosition();
	bool isHome = (currentPlayer.getTeam() == Team::Home);

	Player* bestOption = nullptr;
	float bestScore = 999999.f; // Lower is better

	for (Player* p : team) {
		// Skip the player we are already controlling and the Goalkeeper
		if (p == &currentPlayer || p->getPositionRole() == PositionRole::Goalkeeper || p->isSentOff()) continue;

		sf::Vector2f pPos = p->getPosition();
		float distToBall = dist(pPos, ballPos);
		float score = distToBall;

		// ==========================================
		// --- 1. THE "EMERGENCY ZONE" (3m Rule) ---
		// ==========================================
		// Since 100px = 1m, 3 meters = 300px.
		// If a player is this close, they are the most relevant player on the pitch.
		bool inEmergencyZone = (distToBall < 300.f);

		if (inEmergencyZone) {
			// Apply a massive "Proximity Bonus" to ensure they beat anyone further away.
			// We effectively ignore the goal-side check here because they are close enough to act.
			score -= 5000.f;
		}
		else {
			// ==========================================
			// --- 2. THE "GOAL-SIDE" CHECK ---
			// ==========================================
			// If they AREN'T in the emergency zone, we only want them if they are between
			// the ball and our goal.
			bool isCaughtUpfield = isHome ? (pPos.x > ballPos.x) : (pPos.x < ballPos.x);

			if (isCaughtUpfield) {
				// Massive 2000px penalty for trailing the play.
				score += 2000.f;
			}
		}

		// ==========================================
		// --- 3. ANIMATION LOCK PENALTY ---
		// ==========================================
		// If a player is on the floor or tackling, we really don't want to switch to them.
		if (p->getState() != PlayerState::Normal) {
			score += 1000.f;
		}

		if (score < bestScore) {
			bestScore = score;
			bestOption = p;
		}
	}

	return bestOption ? bestOption : &currentPlayer;
}
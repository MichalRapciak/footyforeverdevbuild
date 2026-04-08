#include "UserController.h"
#include "UserPlayer.h"
#include "Ball.h"
#include "MatchReferee.h"
#include "GamePlay.h"

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
		}
		if (keyPressed->scancode == sf::Keyboard::Scancode::D)
		{
			m_right = true;
		}
		if (keyPressed->scancode == sf::Keyboard::Scancode::LShift)
		{
			isSprinting = true;
		}
		if (keyPressed->scancode == sf::Keyboard::Scancode::LControl)
		{
			if (m_userPlayer.canTackle())
			{
				m_userPlayer.startTackle(m_userPlayer.getAimDirection());
			}
		}
		// --- NEW: FOOT SWITCH ON 'Q' ---
		if (keyPressed->scancode == sf::Keyboard::Scancode::Q)
		{
			if (m_userPlayer.getBallPossession())
			{
				m_userPlayer.changeFoot();
			}
		}
		if (keyPressed->scancode == sf::Keyboard::Scancode::Space)
		{
			// Only jump if they are on the ground and not in a tackle animation
			if (m_userPlayer.z <= 0.0f && m_userPlayer.getState() != PlayerState::Tackling)
			{
				float jumpingNorm = m_userPlayer.getJumpingStrength() / 100.0f;

				// 0 Stat = ~240px/s. 99 Stat = ~400px/s jump velocity
				float jumpVz = 240.f + (jumpingNorm * 160.f);

				m_userPlayer.deductStaminaAction(1.5f);
				m_userPlayer.vz = jumpVz;
				m_userPlayer.setState(PlayerState::Jumping);
			}
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
		}
		if (keyReleased->scancode == sf::Keyboard::Scancode::D)
		{
			m_right = false;
		}
		if (keyReleased->scancode == sf::Keyboard::Scancode::LShift)
		{
			isSprinting = false;
		}
	}
	if (const auto buttonPressed = t_event.getIf<sf::Event::MouseButtonPressed>())
	{
		if (buttonPressed->button == sf::Mouse::Button::Left) {
			dribblePressed = true;
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
			dribblePressed = false;
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
	MatchState state = game.m_referee.getMatchState();
	bool hasPossession = (game.m_ball->getOwner() == &m_userPlayer);

	// ==========================================
	// --- BUG FIX: INPUT BLEED INTERCEPTORS ---
	// ==========================================
	// 1. Did the match state just change? (e.g., InPlay -> FoulDelay)
	if (state != m_lastMatchState) {
		resetInputs();
	}

	// 2. Did we just pick up the ball this exact frame while chasing?
	if (hasPossession && !m_hadPossessionLastFrame) {
		resetInputs();
	}

	// Save for next frame
	m_lastMatchState = state;
	m_hadPossessionLastFrame = hasPossession;
	// ==========================================

	// 1. Process Gravity and Jumping FIRST
	updatePlayerAirPhysics(m_userPlayer, dt);

	bool isTaker = (game.m_referee.getSetPieceTaker() == &m_userPlayer);

	// --- CANCEL TACKLES ON THE WHISTLE ---
	if (state != MatchState::InPlay && m_userPlayer.getState() == PlayerState::Tackling) {
		m_userPlayer.setState(PlayerState::Normal);
	}

	// ==========================================
	// 3. MATCH PAUSED & CELEBRATION STATES
	// ==========================================
	if (state == MatchState::HalfTime || state == MatchState::FullTime || state == MatchState::GoalScored)
	{
		// --- STAMINA HOOK: Catching breath while play is dead ---
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
			m_userPlayer.setVelocity({ 0.f, 0.f });

			if (game.m_ball->getOwner() != &m_userPlayer) {
				game.m_ball->possess(&m_userPlayer);
			}

			if (game.m_referee.isWhistleBlown()) {
				playerShooting(dt, game);
			}
		}
		else {
			if (state == MatchState::KickOff && !game.m_referee.isWhistleBlown()) {
				bool isHome = (m_userPlayer.getTeam() == Team::Home);
				float startX = game.m_pitch.totalWidth / 2.f + (isHome ? -400.f : 400.f);
				float startY = game.m_pitch.totalHeight / 2.f + 200.f;

				m_userPlayer.setPosition({ startX, startY });
				m_userPlayer.setVelocity({ 0.f, 0.f });
			}
			else {
				m_speedVector = m_userPlayer.getVelocity();
				playerMovement(dt, game);
			}
		}

		return;
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
			float pulse = 80.f + (kickStrength * 20.f); // Grows from 40 to 60 radius
			m_targetHighlight.setRadius(pulse);
			m_targetHighlight.setOrigin({ pulse, pulse });
			m_targetHighlight.setOutlineColor(sf::Color(100, 255, 100, 200)); // Turns green when charging
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
	sf::Vector2f forwardDir(m_userPlayer.getAimDirection());
	sf::Vector2f rightDir(-forwardDir.y, forwardDir.x);
	directionInput = { 0.f, 0.f };

	if (canMove)
	{
		if (m_up)    directionInput += forwardDir;
		if (m_down)  directionInput -= forwardDir;
		if (m_left)  directionInput -= rightDir;
		if (m_right) directionInput += rightDir;

		if (dribblePressed)
		{
			if (!game.m_ball->hasOwner())
			{
				isChasingLooseBall = true;
			}
			else if (game.m_ball->getOwner()->getTeam() != m_userPlayer.getTeam())
			{
				isJockeying = true;
			}
		}

		// ==========================================
		// --- STAMINA EXHAUSTION INTERCEPTOR ---
		// ==========================================
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
		// ==========================================

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
				sf::Vector2f currentVel = m_userPlayer.getVelocity();
				float velocityTowardTarget = (currentVel.x * targetDir.x + currentVel.y * targetDir.y);
				sf::Vector2f tangentialVel = currentVel - (targetDir * velocityTowardTarget);

				float dampingStrength = isJockeying ? 10.0f : 6.0f;
				m_speedVector -= tangentialVel * dampingStrength * dt;

				float pullStrength = isJockeying ? 0.85f : 0.55f;
				if (directionInput.x == 0.f && directionInput.y == 0.f) {
					directionInput = targetDir;
				}
				else {
					directionInput = (directionInput * (1.0f - pullStrength)) + (targetDir * pullStrength);
				}
			}
		}

		if (directionInput.x != 0.f || directionInput.y != 0.f)
		{
			float length = std::sqrt(directionInput.x * directionInput.x + directionInput.y * directionInput.y);
			directionInput /= length;
		}
		else
		{
			directionInput = { 0.f, 0.f };
		}
	}

	if (m_userPlayer.getState() == PlayerState::Normal)
	{
		float inputForward = (directionInput.x * forwardDir.x + directionInput.y * forwardDir.y);
		float inputRight = (directionInput.x * rightDir.x + directionInput.y * rightDir.y);

		speed = std::sqrt(m_speedVector.x * m_speedVector.x + m_speedVector.y * m_speedVector.y);
		sprintRatio = speed / (m_userPlayer.getTopSpeed() * 10.f);
		accelMultiplier = 1.f + (1.f - sprintRatio) * 12.f;

		float fwdAccel = m_userPlayer.getAcceleration() * accelMultiplier;
		float sideAccel = m_userPlayer.getAgility() * accelMultiplier;

		sf::Vector2f accelVec = (forwardDir * inputForward * fwdAccel) +
			(rightDir * inputRight * sideAccel);

		forwardSpeed = m_speedVector.x * directionInput.x + m_speedVector.y * directionInput.y;

		if (isChasingLooseBall)
		{
			maxSpeed = m_userPlayer.getTopSpeed() * 10.0f;
		}
		else if (isJockeying)
		{
			float aggressionNorm = m_userPlayer.getAggression() / 100.0f;
			float jockeyClamp = 0.70f + (aggressionNorm * 0.30f);
			maxSpeed = (m_userPlayer.getTopSpeed() * 10.0f) * jockeyClamp;
		}
		else
		{
			maxSpeed = isSprinting ? (m_userPlayer.getTopSpeed() * 10.0f) : (m_userPlayer.getTopSpeed() * 10.0f) * 0.5f;
		}

		if (forwardSpeed < maxSpeed)
		{
			m_speedVector += accelVec * dt;
		}
	}
	else if (m_userPlayer.getState() == PlayerState::Tackling)
	{
		float slideDecel = 1500.f;
		speed = std::sqrt(m_speedVector.x * m_speedVector.x + m_speedVector.y * m_speedVector.y);
		if (speed > 0.f)
		{
			float newSpeed = std::max(0.f, speed - (slideDecel * dt));
			m_speedVector = (m_speedVector / speed) * newSpeed;
		}
	}

	if (m_userPlayer.getState() == PlayerState::Normal)
	{
		if (directionInput.x == 0.f && directionInput.y == 0.f)
		{
			if (speed > 0.1f)
			{
				float speedRatio = speed / (m_userPlayer.getTopSpeed() * 10.0f);
				float momentumFactor = 0.3f + (1.0f - speedRatio) * 0.7f;
				float totalDecel = (m_userPlayer.getAgility() * 2.f) * momentumFactor;
				float decelAmount = totalDecel * dt;

				float newSpeed = std::max(0.f, speed - decelAmount);
				m_speedVector = (m_speedVector / speed) * newSpeed;
			}
		}

		if (m_userPlayer.getState() == PlayerState::Normal)
		{
			float currentFwdSpeed = (m_speedVector.x * forwardDir.x + m_speedVector.y * forwardDir.y);
			float currentSideSpeed = (m_speedVector.x * rightDir.x + m_speedVector.y * rightDir.y);

			if (speed > 5.0f)
			{
				if (m_left && currentSideSpeed > 0.f) {
					currentSideSpeed *= 0.75f;
				}
				else if (m_right && currentSideSpeed < 0.f) {
					currentSideSpeed *= 0.75f;
				}
				else if (!m_left && !m_right) {
					currentSideSpeed *= 0.90f;
				}

				if (m_up && currentFwdSpeed < 0.f) {
					currentFwdSpeed *= 0.85f;
				}
				else if (m_down && currentFwdSpeed > 0.f) {
					currentFwdSpeed *= 0.80f;
				}
				else if (!m_up && !m_down) {
					currentFwdSpeed *= 0.98f;
				}

				m_speedVector = (forwardDir * currentFwdSpeed) + (rightDir * currentSideSpeed);
			}
		}

		if (!isSprinting && speed > (m_userPlayer.getTopSpeed() * 10.0f) * 0.5f)
		{
			m_speedVector *= 0.98f;
		}

		if (m_down && speed > ((m_userPlayer.getTopSpeed() * 10.0f) * 0.50f)) m_speedVector *= 0.96f;
		if ((m_left || m_right) && speed > ((m_userPlayer.getTopSpeed() * 10.0f) * 0.70f) && !m_up) m_speedVector *= 0.96f;
		if ((m_left || m_right) && speed > ((m_userPlayer.getTopSpeed() * 10.0f) * 0.95f) && m_up) m_speedVector *= 0.97f;


		if (isChasingLooseBall)
		{
			sf::Vector2f currentVel = m_userPlayer.getVelocity();
			sf::Vector2f toBall = game.m_ball->getPosition() - m_userPlayer.getPosition();
			float dist = std::sqrt(toBall.x * toBall.x + toBall.y * toBall.y);

			if (dist > 10.f) {
				sf::Vector2f ballDir = toBall / dist;
				float velocityTowardBall = (currentVel.x * ballDir.x + currentVel.y * ballDir.y);
				sf::Vector2f tangentialVel = currentVel - (ballDir * velocityTowardBall);

				m_speedVector -= tangentialVel * 8.0f * dt;
			}
		}
	}

	float currentMax;
	if (isChasingLooseBall) {
		currentMax = m_userPlayer.getTopSpeed() * 10.0f;
	}
	else if (isJockeying) {
		float aggressionNorm = m_userPlayer.getAggression() / 100.0f;
		currentMax = (m_userPlayer.getTopSpeed() * 10.0f) * (0.70f + (aggressionNorm * 0.30f));
	}
	else {
		currentMax = isSprinting ? (m_userPlayer.getTopSpeed() * 10.0f) : (m_userPlayer.getTopSpeed() * 10.0f) * 0.95f;
	}

	speed = std::sqrt(m_speedVector.x * m_speedVector.x + m_speedVector.y * m_speedVector.y);
	if (m_userPlayer.getState() != PlayerState::Tackling && speed > currentMax)
	{
		m_speedVector = (m_speedVector / speed) * currentMax;
	}

	if (!m_up && !m_down && !m_left && !m_right)
	{
		if (std::sqrt(m_speedVector.x * m_speedVector.x + m_speedVector.y * m_speedVector.y) < m_speedNearlyZero)
		{
			m_speedVector = { 0.f, 0.f };
		}
	}

	m_userPlayer.setVelocity(m_speedVector);
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

	if (!game.m_ball->hasOwner() && kickCooldownTimer <= 0.f) {
		float dist = game.distance(m_userPlayer.getPosition(), game.m_ball->getPosition());
		if (dist < 70.f && m_userPlayer.getState() != PlayerState::Tackling && m_userPlayer.getState() != PlayerState::Stunned && m_userPlayer.getState() != PlayerState::Stumbled && m_userPlayer.getState() != PlayerState::FallOver && game.m_ball->z < 40.f) {
			game.m_ball->possess(&m_userPlayer);
		}
	}

	// --- 2. INPUT & CHARGING STATE ---
	float distToBall = game.distance(m_userPlayer.getPosition(), game.m_ball->getPosition());
	bool hasPossession = (game.m_ball->getOwner() == &m_userPlayer);
	bool canContestAir = (game.m_ball->z > 40.f && distToBall < 150.f);

	if (kickPressed && (hasPossession || canContestAir)) {
		justKicked = false;
		charging = true;

		if (increasing) {
			kickStrength += kickSpeed * dt;
			if (kickStrength >= 1) { kickStrength = 1; increasing = false; }
		}
		else {
			kickStrength -= kickSpeed * dt;
			if (kickStrength <= 0.f) { kickStrength = 0.f; increasing = true; }
		}
	}
	else if (charging) {
		// The trigger was released, execute the kick pipeline!
		executeKickRelease(game);
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

	// 1. Calculate Base Trajectories
	if (game.m_ball->z > 40.f) {
		// If calculateAerialKick returns false, we whiffed the ball entirely. Abort.
		if (!calculateAerialKick(game, finalPower, vzPower, errorAngle, finalBackspin)) {
			kickStrength = 0.f; charging = false; increasing = true; kickPressed = false;
			return;
		}
	}
	else {
		calculateGroundKick(basePower, finalPower, vzPower, errorAngle, finalBackspin);
	}

	// 2. Apply AI Assists (Magnetism & Power Correction)
	if (m_currentTarget != nullptr) {
		applyPassingAssistance(game, aimDir, finalPower, basePower);
	}
	else if (!isHighKick) {
		applyShootingAimbot(game, aimDir, vzPower, finalPower);
	}

	// 3. Apply Final Stat Error
	float randError = ((rand() % 100) / 100.f - 0.5f) * errorAngle;
	float rad = randError * 3.14159f / 180.f;
	sf::Vector2f finalDir(
		aimDir.x * std::cos(rad) - aimDir.y * std::sin(rad),
		aimDir.x * std::sin(rad) + aimDir.y * std::cos(rad)
	);

	// 4. Calculate Magnus Spin
	float spin = 0.f;
	bool isRightFoot = m_userPlayer.usingRightFoot();
	if (m_left) spin = isRightFoot ? -(m_userPlayer.getCurl() / 2.f) : m_userPlayer.getCurl();
	if (m_right) spin = isRightFoot ? -m_userPlayer.getCurl() : (m_userPlayer.getCurl() / 2.f);
	spin *= (1.1f + kickStrength / 2.f);

	// 5. Execute & Reset
	game.m_ball->shoot(finalDir, finalPower, spin, vzPower, finalBackspin);

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

void UserController::applyPassingAssistance(GamePlay& game, sf::Vector2f& aimDir, float& finalPower, float basePower)
{
	sf::Vector2f playerPos = m_userPlayer.getPosition();
	sf::Vector2f targetPos = m_currentTarget->getPosition();
	sf::Vector2f targetVel = m_currentTarget->getVelocity();

	float rawDist = game.distance(playerPos, targetPos);
	float stat = isHighKick ? m_userPlayer.getLongPassing() : ((rawDist < 1500.f) ? m_userPlayer.getShortPassing() : m_userPlayer.getLongPassing());
	float passingNorm = stat / 100.f;

	// Physics Prediction
	float arrivalSpeed = 500.f - (std::clamp(rawDist / 4000.f, 0.f, 1.f) * 300.f);
	float v0_est = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * 800.f * rawDist));
	float travelTime = (rawDist > 1200.f) ? (rawDist / v0_est) + 0.3f : rawDist / ((v0_est + arrivalSpeed) * 0.5f);

	sf::Vector2f predictedPos = targetPos + (targetVel * travelTime);
	sf::Vector2f dirToPredicted = game.normalize(predictedPos - playerPos);
	float leadAmount = (stat == m_userPlayer.getAwareness()) ? 250.f : 80.f;

	sf::Vector2f aimSpot = predictedPos + (dirToPredicted * leadAmount);
	sf::Vector2f perfectPassDir = game.normalize(aimSpot - playerPos);
	float perfectDist = game.distance(playerPos, aimSpot);

	// Aim Magnetism
	float aimDot = (aimDir.x * perfectPassDir.x) + (aimDir.y * perfectPassDir.y);
	if (aimDot > 0.5f) {
		float magnetism = 0.4f + (passingNorm * 0.5f);
		aimDir = game.normalize((aimDir * (1.0f - magnetism)) + (perfectPassDir * magnetism));
	}

	// Power Assistance
	float idealPowerWorld = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * 800.f * perfectDist));
	float idealPowerAssisted = idealPowerWorld / 52.0f;
	idealPowerAssisted *= isHighKick ? 0.75f : 1.1f;

	float powerMagnetism = 0.3f + (passingNorm * 0.6f);
	finalPower = (finalPower * (1.0f - powerMagnetism)) + (idealPowerAssisted * powerMagnetism);
	finalPower = std::min(finalPower, basePower);
}

void UserController::applyShootingAimbot(GamePlay& game, sf::Vector2f& aimDir, float& vzPower, float finalPower)
{
	sf::Vector2f playerPos = m_userPlayer.getPosition();
	bool isHome = (m_userPlayer.getTeam() == Team::Home);
	float goalX = isHome ? game.m_pitch.totalWidth - game.m_pitch.margin : game.m_pitch.margin;

	bool aimingAtGoal = (isHome && aimDir.x > 0) || (!isHome && aimDir.x < 0);
	if (!aimingAtGoal) return;

	float distToGoalX = std::abs(goalX - playerPos.x);
	float intersectY = playerPos.y + (aimDir.y / std::abs(aimDir.x)) * distToGoalX;
	float topPostY = 3500.f - 366.f;
	float bottomPostY = 3500.f + 366.f;

	if (intersectY > topPostY - 200.f && intersectY < bottomPostY + 200.f)
	{
		float targetY = (intersectY < 3500.f) ? topPostY + 40.f : bottomPostY - 40.f;
		sf::Vector2f perfectDir = game.normalize(sf::Vector2f(goalX, targetY) - playerPos);

		float magnetism = (m_userPlayer.getFinishing() / 100.f) * 0.6f;
		aimDir = game.normalize((aimDir * (1.0f - magnetism)) + (perfectDir * magnetism));

		float horizontalSpeed = std::max(finalPower * 52.0f, 1.f);
		float timeToGoal = distToGoalX / horizontalSpeed;

		// 3D VZ Dip Calculation
		float perfectVz = (180.f + (0.5f * 980.f * timeToGoal * timeToGoal)) / timeToGoal;
		vzPower = (vzPower * (1.0f - magnetism)) + (perfectVz * magnetism);
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
    m_currentTarget = nullptr;
    sf::Vector2f aimDir = m_userPlayer.getAimDirection();
    sf::Vector2f playerPos = m_userPlayer.getPosition();
    float bestMatch = 0.98f; // Threshold for "locking on"

    for (const auto& teammatePtr : game.m_teammates) 
    {
        Player* teammate = teammatePtr.get();
        sf::Vector2f toTeammate = teammate->getPosition() - playerPos;
        float d = std::sqrt(toTeammate.x * toTeammate.x + toTeammate.y * toTeammate.y);
        
        if (d < 10.f || d > 4500.f) continue; 
        sf::Vector2f dirToTeammate = toTeammate / d;
        
        float alignment = (aimDir.x * dirToTeammate.x + aimDir.y * dirToTeammate.y);
        
        if (alignment > bestMatch) {
            bestMatch = alignment;
            m_currentTarget = teammate;
        }
    }

    // Update the visual circle position
    if (m_currentTarget) {
        m_targetHighlight.setPosition(m_currentTarget->getPosition());
    }
}

void UserController::updatePlayerAirPhysics(UserPlayer& user, float dt) {
	// If the player is in the air, or has upward velocity
	if (user.z > 0.0f || user.vz != 0.0f) {
		// Apply Gravity (9.8m/s^2 = 980px/s^2)
		user.vz -= 980.f * dt;
		user.z += user.vz * dt;

		// Hit the ground
		if (user.z <= 0.0f) {
			user.z = 0.0f;
			user.vz = 0.0f;
			user.setVelocity(user.getVelocity() * 0.60f);
			// Return to normal state (unless they were tackling/stunned)
			if (user.getState() == PlayerState::Jumping) {
				user.setState(PlayerState::Normal);
			}
		}
	}
}

void UserController::resetInputs() {
	// 1. Reset all shooting and charging variables
	kickStrength = 0.f;
	charging = false;
	increasing = true;

	// 2. Force the buttons to 'unpress' so holding the mouse 
	// through a cutscene doesn't instantly shoot when play resumes
	kickPressed = false;
	dribblePressed = false;
	isShooting = false;

	// 3. Reset automated movement states
	isChasingLooseBall = false;
	isJockeying = false;
}
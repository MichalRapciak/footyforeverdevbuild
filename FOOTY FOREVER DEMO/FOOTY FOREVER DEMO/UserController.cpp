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
		// --- MANUAL DEFENSIVE SWITCHING ---
		// ==========================================
	if (switchPressed) {
		switchPressed = false; // Consume input

		// Only allow manual switching if we DO NOT have the ball
		if (game.m_ball->getOwner() != &m_userPlayer) {

			// Gather all teammates
			std::vector<Player*> myTeam;
			for (auto& npc : game.m_homeside) {
				if (!npc->isSentOff()) myTeam.push_back(npc.get());
			}

			// Call the AI to find the smartest goal-side defender
			Player* bestDefender = findBestDefensiveSwitch(m_userPlayer, myTeam, *game.m_ball, game.m_pitch);
			sf::Vector2f tempAim = m_userPlayer.getPlayerAim();
			if (bestDefender && bestDefender != &m_userPlayer) {
				game.executePlayerSwitch(bestDefender);
				resetInputs(); // Prevent the new player from instantly sliding if you were holding Ctrl!

				// Keep the mouse aiming stable after the teleport
				m_userPlayer.updateAim(tempAim);
			}
		}
	}

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
	PhysicsEngine::updatePlayerAirPhysics(m_userPlayer, dt);

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
		// --- 1. RAW USER INPUT ---
		if (m_up)    directionInput += forwardDir;
		if (m_down)  directionInput -= forwardDir;
		if (m_left)  directionInput -= rightDir;
		if (m_right) directionInput += rightDir;

		// --- 2. CONTEXT AWARENESS (Dribble/Jockey/Chase) ---
		if (dribblePressed)
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

		if (isChasingLooseBall) {
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

    // 1. Trajectory Calculation
    if (game.m_ball->z > 40.f) {
        if (!calculateAerialKick(game, finalPower, vzPower, errorAngle, finalBackspin)) {
            kickStrength = 0.f; charging = false; increasing = true; kickPressed = false;
            return;
        }
    } else {
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

    // 2. Apply Assists
    if (m_currentTarget != nullptr) {
        AimAssist::applyPassAssist(m_userPlayer, m_currentTarget, aimDir, finalPower, isHighKick, false);
    } else if (!isHighKick) {
        AimAssist::applyShotAssist(m_userPlayer, aimDir, vzPower, finalPower, game.m_pitch);
    }

    // 3. Apply Final Stat Error (Now much larger if Weak Foot is low)
    float randError = ((rand() % 100) / 100.f - 0.5f) * errorAngle;
    float rad = randError * 3.14159f / 180.f;
    sf::Vector2f finalDir(
        aimDir.x * std::cos(rad) - aimDir.y * std::sin(rad),
        aimDir.x * std::sin(rad) + aimDir.y * std::cos(rad)
    );

    // 4. Calculate Spin (Dampen curl for weak foot)
    float spin = 0.f;
    if (m_left) spin = usingRight ? -(m_userPlayer.getCurl() / 2.f) : m_userPlayer.getCurl();
    if (m_right) spin = usingRight ? -m_userPlayer.getCurl() : (m_userPlayer.getCurl() / 2.f);
    
    // Weak foot can't curl the ball as well
    if (isWeakFoot) spin *= (0.4f + (m_userPlayer.getWeakFootAccuracy() / 5.0f) * 0.6f);
    spin *= (1.1f + kickStrength / 2.f);

    game.m_ball->shoot(finalDir, finalPower, spin, vzPower, finalBackspin);

	kickStrength = 0.f;
	charging = false;
	increasing = true;
	justKicked = true;
	kickCooldownTimer = kickCooldown;

	// ==========================================
	// --- AUTOMATIC OFFENSIVE SWITCH ---
	// ==========================================
	// If we successfully aimed a ground or high pass at a teammate, switch to them immediately!
	if (m_currentTarget != nullptr) {
		game.executePlayerSwitch(m_currentTarget);
		m_currentTarget = nullptr; // Clear the target since WE are now the target!
		resetInputs();
	}
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
	for (auto& tm : game.m_homeside) teammatesRaw.push_back(tm.get());

	m_currentTarget = AimAssist::getTargetLock(m_userPlayer.getPosition(), m_userPlayer.getAimDirection(), teammatesRaw);

	if (m_currentTarget) {
		m_targetHighlight.setPosition(m_currentTarget->getPosition());
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

		// --- THE "GOAL-SIDE" CHECK ---
		// If we are Home, our goal is at X=0. Caught upfield means X > Ball.X
		// If we are Away, our goal is at X=10000. Caught upfield means X < Ball.X
		bool isCaughtUpfield = isHome ? (pPos.x > ballPos.x) : (pPos.x < ballPos.x);

		float score = distToBall;

		if (isCaughtUpfield) {
			// Massive penalty for trailing the play. The AI will prefer a Center Back 
			// 800px away over a Striker who is 200px away but behind the ball!
			score += 2000.f;
		}

		// Penalty for players currently locked in an animation
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
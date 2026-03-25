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
	// 1. Process Gravity and Jumping FIRST
	updatePlayerAirPhysics(m_userPlayer, dt);

	// 2. ASK THE REFEREE FOR THE MATCH STATE
	MatchState state = game.m_referee.getMatchState();
	bool isTaker = (game.m_referee.getSetPieceTaker() == &m_userPlayer);

	// --- CANCEL TACKLES ON THE WHISTLE ---
	// If the referee blows the play dead while you are sliding, snap out of it
	if (state != MatchState::InPlay && m_userPlayer.getState() == PlayerState::Tackling) {
		m_userPlayer.setState(PlayerState::Normal);
	}

	// ==========================================
	// 3. MATCH PAUSED & CELEBRATION STATES
	// ==========================================
	if (state == MatchState::HalfTime || state == MatchState::FullTime || state == MatchState::GoalScored)
	{
		// Apply heavy friction so you naturally slide to a halt
		m_userPlayer.setVelocity(m_userPlayer.getVelocity() * 0.85f);
		return; // Skip normal movement and shooting entirely
	}

	// ==========================================
	// 4. DEAD BALL OVERRIDE (Set Pieces)
	// ==========================================
	if (state != MatchState::InPlay)
	{
		updateTargetScanning(game); // Let the user look around and aim

		if (isTaker) {
			// THE USER IS THE TAKER
			// Keep them locked to the spot while waiting for the whistle
			m_userPlayer.setVelocity({ 0.f, 0.f });

			// FORCE POSSESSION: Ensure the physics engine knows the taker "has" the ball
			if (game.m_ball->getOwner() != &m_userPlayer) {
				game.m_ball->possess(&m_userPlayer);
			}

			// If the whistle blew, let them shoot!
			if (game.m_referee.isWhistleBlown()) {
				playerShooting(dt, game);
			}
		}
		else {
			// THE USER IS *NOT* THE TAKER
			// Let the user move freely to get into the box for a header or form a wall!
			m_speedVector = m_userPlayer.getVelocity();
			playerMovement(dt, game);

			// NOTE: We deliberately do NOT call playerShooting here so you can't 
			// accidentally swing your leg while waiting for the NPC to cross it.
		}

		return; // Skip the open play block
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
	// 1. Get where the player is ON THE MONITOR (Screen Pixels)
	// We use mapCoordsToPixel to account for the camera's current offset
	sf::Vector2i playerPixelPos = t_window.mapCoordsToPixel(m_userPlayer.getPosition(), t_view);

	// 2. Get the mouse position relative to the window
	sf::Vector2i mousePixelPos = sf::Mouse::getPosition(t_window);

	// 3. Vector from Player Pixel to Mouse Pixel
	sf::Vector2f aimDir = sf::Vector2f(mousePixelPos.x - playerPixelPos.x,
		mousePixelPos.y - playerPixelPos.y);

	// 4. Calculate and set rotation
	if (std::abs(aimDir.x) > 0.5f || std::abs(aimDir.y) > 0.5f) {
		float angleRad = std::atan2(aimDir.y, aimDir.x);
		float angleDeg = angleRad * 180.f / 3.14159265f;
		// Use m_player.setRotation if you have a wrapper, or the sprite directly
		m_userPlayer.getSprite().setRotation(sf::degrees(angleDeg));
		m_userPlayer.updateAim(mousePixelPos, angleDeg);
	}
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
                isJockeying = true; // Opponent has it, form the wall!
            }
        }

        if (isChasingLooseBall || isJockeying)
        {
            sf::Vector2f targetPos;

            if (isChasingLooseBall)
            {
                // 1. LOOSE BALL: Sprint directly to the ball coordinate
                targetPos = game.m_ball->getPosition();
            }
            else if (isJockeying)
            {
                // 2. JOCKEYING: Calculate the standoff line
                Player* threat = game.m_ball->getOwner();
                sf::Vector2f threatPos = threat->getPosition();
                
                // Set the Goal Position we are defending (adjust X coordinates based on your pitch margins!)
                bool isHomeSide = (m_userPlayer.getTeam() == Team::Home);
                sf::Vector2f myGoalPos = isHomeSide ? sf::Vector2f(500.f, 3500.f) : sf::Vector2f(9500.f, 3500.f);
                
                sf::Vector2f toGoal = myGoalPos - threatPos;
                float toGoalLen = std::sqrt(toGoal.x * toGoal.x + toGoal.y * toGoal.y);
                if (toGoalLen > 0.001f) toGoal /= toGoalLen; // Normalize

                // Pull player stats
                float aggressionNorm = m_userPlayer.getAggression() / 100.0f;
                float awarenessNorm = m_userPlayer.getAwareness() / 100.0f;

                // Standoff buffer (150px baseline)
                float dynamicBuffer = 150.f - (aggressionNorm * 100.f);
                float distToThreat = std::sqrt(std::pow(m_userPlayer.getPosition().x - threatPos.x, 2) + 
                                               std::pow(m_userPlayer.getPosition().y - threatPos.y, 2));

                // Auto-commit threshold
                float commitThreshold = 80.f + ((1.0f - awarenessNorm) * 60.f);

                if (distToThreat < commitThreshold && m_userPlayer.canTackle()) {
                    float tackleStep = 40.f - (aggressionNorm * 20.f);
                    targetPos = threatPos + (toGoal * tackleStep);
                } else {
                    targetPos = threatPos + (toGoal * dynamicBuffer);
                }
            }

            // --- HOMING IN ON THE TARGET ---
            sf::Vector2f toTarget = targetPos - m_userPlayer.getPosition();
            float dist = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);

            if (dist > 10.f)
            {
                sf::Vector2f targetDir = toTarget / dist;

                // Orbit Killer (Velocity Correction)
                sf::Vector2f currentVel = m_userPlayer.getVelocity();
                float velocityTowardTarget = (currentVel.x * targetDir.x + currentVel.y * targetDir.y);
                sf::Vector2f tangentialVel = currentVel - (targetDir * velocityTowardTarget);

                // Snappier sideways braking when Jockeying so they don't slide past the attacker
                float dampingStrength = isJockeying ? 10.0f : 6.0f; 
                m_speedVector -= tangentialVel * dampingStrength * dt;

                // Blend Input
                // Pull harder towards the defensive spot when jockeying
                float pullStrength = isJockeying ? 0.85f : 0.55f; 
                if (directionInput.x == 0.f && directionInput.y == 0.f) {
                    directionInput = targetDir;
                } else {
                    directionInput = (directionInput * (1.0f - pullStrength)) + (targetDir * pullStrength);
                }
            }
        }
	
		// --- NORMALIZE FINAL COMBINED INPUT ---
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
		// 1. PROJECT INPUT onto local axes
		float inputForward = (directionInput.x * forwardDir.x + directionInput.y * forwardDir.y);
		float inputRight = (directionInput.x * rightDir.x + directionInput.y * rightDir.y);

		// Calculate the ratio based directly on the sprint speed getter
		speed = std::sqrt(m_speedVector.x * m_speedVector.x + m_speedVector.y * m_speedVector.y);
		sprintRatio = speed / (m_userPlayer.getTopSpeed() * 10.f);
		accelMultiplier = 1.f + (1.f - sprintRatio) * 12.f;

		// 2. CALCULATE SEPARATE ACCELERATIONS
		float fwdAccel = m_userPlayer.getAcceleration() * accelMultiplier;
		float sideAccel = m_userPlayer.getAgility() * accelMultiplier;

		sf::Vector2f accelVec = (forwardDir * inputForward * fwdAccel) +
			(rightDir * inputRight * sideAccel);

		// 3. APPLY ACCELERATION
		forwardSpeed = m_speedVector.x * directionInput.x + m_speedVector.y * directionInput.y;

		// Determine maxSpeed
		if (isChasingLooseBall)
		{
			maxSpeed = m_userPlayer.getTopSpeed() * 10.0f; // Full sprint to win the ball!
		}
		else if (isJockeying)
		{
			// Same as the NPC logic: 0 Aggression = 70% speed. 99 Aggression = 100% sprint.
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
		// --- TACKLE LOGIC REMAINS THE SAME ---
		float slideDecel = 1500.f;
		speed = std::sqrt(m_speedVector.x * m_speedVector.x + m_speedVector.y * m_speedVector.y);
		if (speed > 0.f)
		{
			float newSpeed = std::max(0.f, speed - (slideDecel * dt));
			m_speedVector = (m_speedVector / speed) * newSpeed;
		}
	}

	// --- DECELERATION AND MOMENTUM MANAGEMENT ---
	if (m_userPlayer.getState() == PlayerState::Normal)
	{
		// Friction when no keys are pressed
		if (directionInput.x == 0.f && directionInput.y == 0.f)
		{
			if (speed > 0.1f)
			{
				// 1. Calculate the speed ratio (0.0 to 1.0)
				float speedRatio = speed / (m_userPlayer.getTopSpeed() * 10.0f);

				// 2. INVERT THE FACTOR
				// When speed is 100%, momentumFactor is 0.3 (Slower deceleration)
				// When speed is 0%, momentumFactor is 1.0 (Faster deceleration)
				float momentumFactor = 0.3f + (1.0f - speedRatio) * 0.7f;

				// 3. Apply the scaled deceleration
				float totalDecel = (m_userPlayer.getAgility() * 2.f) * momentumFactor;
				float decelAmount = totalDecel * dt;

				float newSpeed = std::max(0.f, speed - decelAmount);
				m_speedVector = (m_speedVector / speed) * newSpeed;
			}
		}

		if (m_userPlayer.getState() == PlayerState::Normal)
		{
			// 1. PROJECT current velocity onto your local axes
			float currentFwdSpeed = (m_speedVector.x * forwardDir.x + m_speedVector.y * forwardDir.y);
			float currentSideSpeed = (m_speedVector.x * rightDir.x + m_speedVector.y * rightDir.y);

			// ONLY apply counter-steering friction if we actually have meaningful speed
			if (speed > 5.0f)
			{
				// 2. SIDE-TO-SIDE (Lateral) COUNTER-STEER
				if (m_left && currentSideSpeed > 0.f) {
					currentSideSpeed *= 0.75f;
				}
				else if (m_right && currentSideSpeed < 0.f) {
					currentSideSpeed *= 0.75f;
				}
				else if (!m_left && !m_right) {
					currentSideSpeed *= 0.90f;
				}

				// 3. FORWARD-BACK (Longitudinal) COUNTER-STEER
				if (m_up && currentFwdSpeed < 0.f) {
					currentFwdSpeed *= 0.85f;
				}
				else if (m_down && currentFwdSpeed > 0.f) {
					currentFwdSpeed *= 0.80f;
				}
				else if (!m_up && !m_down) {
					currentFwdSpeed *= 0.98f;
				}

				// 4. RECONSTRUCT the speed vector
				m_speedVector = (forwardDir * currentFwdSpeed) + (rightDir * currentSideSpeed);
			}
		}

		// --- MOMENTUM BLEEDING ---
		// If we stop sprinting, bleed speed down to the 50% walk threshold
		if (!isSprinting && speed > (m_userPlayer.getTopSpeed() * 10.0f) * 0.5f)
		{
			m_speedVector *= 0.98f;
		}

		// Turning/Backwards penalties relative to sprint speed
		if (m_down && speed > ((m_userPlayer.getTopSpeed() * 10.0f) * 0.50f)) m_speedVector *= 0.96f;
		if ((m_left || m_right) && speed > ((m_userPlayer.getTopSpeed() * 10.0f) * 0.70f) && !m_up) m_speedVector *= 0.96f;
		if ((m_left || m_right) && speed > ((m_userPlayer.getTopSpeed() * 10.0f) * 0.95f) && m_up) m_speedVector *= 0.97f;


		// --- PRESSURE / CONTAIN VELOCITY DAMPING (The "Orbit Killer") ---
		if (isChasingLooseBall)
		{
			sf::Vector2f currentVel = m_userPlayer.getVelocity();
			sf::Vector2f toBall = game.m_ball->getPosition() - m_userPlayer.getPosition();
			float dist = std::sqrt(toBall.x * toBall.x + toBall.y * toBall.y);

			if (dist > 10.f) {
				sf::Vector2f ballDir = toBall / dist;
				float velocityTowardBall = (currentVel.x * ballDir.x + currentVel.y * ballDir.y);
				sf::Vector2f tangentialVel = currentVel - (ballDir * velocityTowardBall);

				// Aggressively counter sideways momentum to snap toward the ball
				m_speedVector -= tangentialVel * 8.0f * dt;
			}
		}
	}

	// --- FINAL HARD CAPS ---
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

	// --- STOP NEAR-ZERO SPEED ---
	if (!m_up && !m_down && !m_left && !m_right)
	{
		if (std::sqrt(m_speedVector.x * m_speedVector.x + m_speedVector.y * m_speedVector.y) < m_speedNearlyZero)
		{
			m_speedVector = { 0.f, 0.f };
		}
	}

	// --- APPLY NEW VELOCITY TO PLAYER ---
	m_userPlayer.setVelocity(m_speedVector);

}


/// <summary>
/// PLAYER BALL KICKING
/// </summary>
/// <param name="dt"></param>
/// <param name="game"></param>
void UserController::playerShooting(float dt, GamePlay& game)
{
	if (kickCooldownTimer > 0.f)
	{
		justKicked = false;
		kickCooldownTimer -= dt;
	}
	if (kickCooldownTimer < 0.f) kickCooldownTimer = 0.f;

	// --- 1. AUTO-POSSESS (No button needed!) ---
	// If the ball has no owner, and we are close enough, grab it!
	if (!game.m_ball->hasOwner() && kickCooldownTimer <= 0.f)
	{
		float dist = game.distance(m_userPlayer.getPosition(), game.m_ball->getPosition());
		if (dist < 70.f && m_userPlayer.getState() != PlayerState::Tackling)
		{
			game.m_ball->possess(&m_userPlayer);
		}
	}

	float distToBall = game.distance(m_userPlayer.getPosition(), game.m_ball->getPosition());
	bool hasPossession = (game.m_ball->getOwner() == &m_userPlayer);
	bool canContestAir = (game.m_ball->z > 40.f && distToBall < 150.f);

	if (kickPressed && (hasPossession || canContestAir))
	{
		justKicked = false;
		charging = true;

		// Oscillate kick power
		if (increasing) {
			kickStrength += kickSpeed * dt;
			if (kickStrength >= 1) { kickStrength = 1; increasing = false; }
		}
		else {
			kickStrength -= kickSpeed * dt;
			if (kickStrength <= 0.f) { kickStrength = 0.f; increasing = true; }
		}
	}
	else if (charging)
	{
		// --- THE DECISION ENGINE ---
		sf::Vector2f aimDir = m_userPlayer.getAimDirection();
		sf::Vector2f playerPos = m_userPlayer.getPosition();

		// 2. Select the Stat and Trajectory
		float finalPower = m_userPlayer.getKickPower() * kickStrength;
		float vzPower = 0.f;
		float errorAngle = 0.f;
		float finalBackspin = 0.f;

		if (game.m_ball->z > 40.f)
		{
			// 1. Check if we whiffed the distance
			if (distToBall > 120.f) {
				kickStrength = 0.f; charging = false; increasing = true; kickPressed = false; return;
			}

			// 2. The Timing Window (Skill check!)
			float relZ = game.m_ball->z - m_userPlayer.z;
			bool isHeader = (relZ >= 140.f && relZ <= 240.f);
			bool isVolley = (relZ >= 40.f && relZ < 140.f);

			// If we missed the timing window (jumped too early/late), we whiff.
			if (!isHeader && !isVolley) {
				kickStrength = 0.f; charging = false; increasing = true; kickPressed = false; return;
			}

			// 3. Set up aerial math
			float stat = isHeader ? m_userPlayer.getHeading() : m_userPlayer.getFinishing();
			errorAngle = (1.0f - (stat / 100.f)) * (isHeader ? 15.0f : 10.0f);

			if (isHeader) {
				// Header power is stat-based, combined with how much you charged
				finalPower = (40.f + (stat * 0.6f)) * std::max(0.4f, kickStrength);

				if (isHighKick) {
					// Right Click: Looped header pass
					vzPower = 150.f + (stat * 1.5f);
				}
				else {
					// Left Click: Driven downward header shot (Spike)
					vzPower = 100.f - (stat * 3.0f); // Drives it into the ground!
				}
				finalBackspin = 10.f;
			}
			else {
				// Volley power is purely leg power (can hit absolute rockets)
				finalPower = m_userPlayer.getKickPower() * kickStrength * 1.2f;
				float techniqueError = (1.0f - (stat / 100.f));
				vzPower = 100.f + (techniqueError * 350.f); // Bad technique flies into the stands
				finalBackspin = 30.f;
			}
		}
		else
		{
			if (isHighKick) {
				// --- HIGH PASS / CROSS / CHIP (INVERTED LOGIC) ---
				bool isPassing = (m_currentTarget != nullptr);
				float stat = isPassing ? m_userPlayer.getLongPassing() : m_userPlayer.getFinishing();

				// Accuracy: Better stats = tighter aim
				errorAngle = (1.0f - (stat / 100.f)) * 8.0f;

				// THE INVERSION: 
				// Tap (0.1 power) -> High arc (approx 530 vz)
				// Full (1.0 power) -> Driven arc (approx 350 vz)
				float maxLoft = 850.f;
				float heightInversionFactor = 200.f; // The "Aggression" of the inversion
				vzPower = maxLoft - (kickStrength * heightInversionFactor);

				// Stat Bonus: Elite players can hit even flatter, more aggressive balls
				float statDampening = (stat / 100.f) * 80.f;
				vzPower -= statDampening;

				// --- BACKSPIN CALCULATION ---
				if (isPassing) {
					// Long powerful pings need backspin to "bite" on landing
					finalBackspin = (stat * 0.5f) + (kickStrength * 45.f);
				}
				else {
					float baseSpin = 50.f; // Guaranteed spin even on medium shots
					float spinIntensity = 120.f; // How much the power-drop affects the spin

					// This makes the spin drop off much faster as you add power
					finalBackspin = (m_userPlayer.getFinishing() * 0.8f) + (spinIntensity * (1.0f - kickStrength));
				}
			}
			else {
				// --- GROUND PASS / THROUGH BALL / DRIVEN SHOT ---
				float stat;
				if (m_currentTarget) {
					stat = (dist(playerPos, m_currentTarget->getPosition()) < 1500.f)
						? m_userPlayer.getShortPassing()
						: m_userPlayer.getLongPassing();
				}
				else {
					stat = m_userPlayer.getFinishing();
				}

				errorAngle = (1.0f - (stat / 100.f)) * 5.0f;

				// Keep ground shots on the floor, but add a tiny pop if it's a "Driven" shot
				vzPower = 5.f + (kickStrength * 50.f);
				finalPower *= 1.2f;
				finalBackspin = (m_currentTarget && stat == m_userPlayer.getShortPassing()) ? 10.f : 0.f;
			}
		}

		if (m_currentTarget != nullptr)
		{
			sf::Vector2f targetPos = m_currentTarget->getPosition();
			sf::Vector2f targetVel = m_currentTarget->getVelocity();
			float rawDist = game.distance(playerPos, targetPos);

			// 1. Identify the Passing Stat
			float stat = isHighKick ? m_userPlayer.getLongPassing() :
				((rawDist < 1500.f) ? m_userPlayer.getShortPassing() : m_userPlayer.getLongPassing());
			float passingNorm = stat / 100.f;

			// 2. Physics Prediction (The "Perfect" Pass)
			const float friction = 800.f;
			const float engineMultiplier = 52.0f;

			float arrivalSpeed = 500.f - (std::clamp(rawDist / 4000.f, 0.f, 1.f) * 300.f);
			float v0_est = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * friction * rawDist));
			float travelTime = (rawDist > 1200.f) ? (rawDist / v0_est) + 0.3f : rawDist / ((v0_est + arrivalSpeed) * 0.5f);

			// 3. Find the Lead Spot
			sf::Vector2f predictedPos = targetPos + (targetVel * travelTime);
			sf::Vector2f dirToPredicted = game.normalize(predictedPos - playerPos);

			// Through balls lead the player into space (250px), passes to feet are tighter (80px)
			float leadAmount = (stat == m_userPlayer.getAwareness()) ? 250.f : 80.f;
			sf::Vector2f aimSpot = predictedPos + (dirToPredicted * leadAmount);

			sf::Vector2f perfectPassDir = game.normalize(aimSpot - playerPos);
			float perfectDist = game.distance(playerPos, aimSpot);

			// 4. AIM MAGNETISM (Blend User Aim with Perfect Aim)
			// Check if the user is aiming roughly toward the target (within ~60 degrees)
			float aimDot = (aimDir.x * perfectPassDir.x) + (aimDir.y * perfectPassDir.y);
			if (aimDot > 0.5f)
			{
				// 99 Passing = 90% aimbot lock-on. 0 Passing = 40% lock-on.
				float magnetism = 0.4f + (passingNorm * 0.5f);
				aimDir = (aimDir * (1.0f - magnetism)) + (perfectPassDir * magnetism);
				aimDir = game.normalize(aimDir);
			}

			// 5. POWER ASSISTANCE (Blend User Charge with Perfect Power)
			float idealPowerWorld = std::sqrt((arrivalSpeed * arrivalSpeed) + (2.f * friction * perfectDist)) * 1.5f;
			float idealPowerAssisted = idealPowerWorld / engineMultiplier;

			if (isHighKick) idealPowerAssisted *= 1.15f; // Looped passes need a little extra juice

			// 99 Passing highly corrects your power bar. 0 passing relies entirely on your manual charge.
			float powerMagnetism = 0.3f + (passingNorm * 0.6f);
			finalPower = (finalPower * (1.0f - powerMagnetism)) + (idealPowerAssisted * powerMagnetism);

			// Cap it to physical limits
			finalPower = std::min(finalPower, m_userPlayer.getKickPower());
		}

		// --- 3. APPLY ERROR & FINISHING AIMBOT ---

				// Determine the goal position based on team
		bool isHome = (m_userPlayer.getTeam() == Team::Home);
		// Assuming Goal center Y is 3500 and posts are at +/- 366
		float goalX = isHome ? game.m_pitch.totalWidth - game.m_pitch.margin : game.m_pitch.margin;
		float topPostY = 3500.f - 366.f;
		float bottomPostY = 3500.f + 366.f;

		// Check if we are aiming towards the opponent's goal area
		// (A simple check to see if the aim vector eventually intersects the goal line)
		bool aimingAtGoal = false;
		if (isHome && aimDir.x > 0) aimingAtGoal = true;
		else if (!isHome && aimDir.x < 0) aimingAtGoal = true;

		// Only apply "Aimbot" if we are NOT passing and we are aiming at the goal side
		if (m_currentTarget == nullptr && aimingAtGoal)
		{
			float finishingNorm = m_userPlayer.getFinishing() / 100.f;

			// Find the point on the goal line where the raw aim is currently pointing
			float distToGoalX = std::abs(goalX - playerPos.x);
			float intersectY = playerPos.y + (aimDir.y / std::abs(aimDir.x)) * distToGoalX;

			// If the raw aim is within or very near the goal width
			if (intersectY > topPostY - 200.f && intersectY < bottomPostY + 200.f)
			{
				// Determine which corner is closer to the current aim
				float targetY = (intersectY < 3500.f) ? topPostY + 40.f : bottomPostY - 40.f;

				// Calculate the vector to that perfect corner
				sf::Vector2f perfectDir = game.normalize(sf::Vector2f(goalX, targetY) - playerPos);

				// Magnetism Strength: How much we pull the aim. 
				// 99 Finishing = 40% pull. 20 Finishing = 5% pull.
				float magnetism = finishingNorm * 0.6f;

				// Blend the raw aim with the perfect corner aim
				aimDir = (aimDir * (1.0f - magnetism)) + (perfectDir * magnetism);
				aimDir = game.normalize(aimDir);
			}
		}

		// Apply random stat-based error (after the aimbot nudge)
		float randError = ((rand() % 100) / 100.f - 0.5f) * errorAngle;
		float rad = randError * 3.14159f / 180.f;
		sf::Vector2f finalDir(
			aimDir.x * cos(rad) - aimDir.y * sin(rad),
			aimDir.x * sin(rad) + aimDir.y * cos(rad)
		);

		// Calculate Spin (Existing logic)
		float spin = 0.f;
		bool isRightFoot = m_userPlayer.usingRightFoot();
		if (m_left) spin = isRightFoot ? -(m_userPlayer.getCurl() / 2.f) : m_userPlayer.getCurl();
		if (m_right) spin = isRightFoot ? -m_userPlayer.getCurl() : (m_userPlayer.getCurl() / 2.f);
		spin *= (1.1f + kickStrength / 2.f);

		// Execute!
		game.m_ball->shoot(finalDir, finalPower, spin, vzPower, finalBackspin);

		// Reset
		kickStrength = 0.f;
		charging = false;
		increasing = true;
		justKicked = true;
		kickCooldownTimer = kickCooldown;
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
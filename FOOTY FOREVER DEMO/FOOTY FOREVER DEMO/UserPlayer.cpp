#include "UserPlayer.h"

/// <summary>
/// Setting up player sprite when the player function is created.
/// </summary>
UserPlayer::UserPlayer(const sf::Texture& texture) : Player(texture)
{
	m_position = { 4000,3500 };
	m_sprite.setPosition(m_position);
	m_stats = { 80.f, 4,80.0f, 80.0f, 80.0f, 88.0f, 80.0f, 80.0f, 80.0f, 88.0f, 88.0f, 88.0f, 80.0f, 80.0f, 80.f, 80.f, 80.f, 80.f, 80.f, 80.f, 80.0f, 80.0f, 80.0f }; 	// FINISHING, CURL, BALL CONTROL, BALANCING, SHORT PASSING, LONG PASSING, THROUGH PASSING, TOP SPEED, ACCELERATION, AGILITY, BODY STRENGTH, KICK POWER, AWARENESS, AGGRESSION, BLOCKING, GK COVER, GK REACT, GK CATCH, GK THROW< GK AWARE, GK BLOCK
	m_team = Team::Home;
}

UserPlayer::~UserPlayer()
{
}

/// <summary>
/// Function used to update player position
/// </summary>
/// <param name="dt"></param>
void UserPlayer::update(float dt, AnimationServer& animServer)
{
    // 1. Tick the base physics and tackle state machine
    Player::update(dt, animServer);
    
    float speed = std::sqrt((m_velocity.x * m_velocity.x) + (m_velocity.y * m_velocity.y));

    // 2. ONLY play running animations if we are standing or running
    if (m_currentState == PlayerState::Normal)
    {
        if (speed > 10.f) 
        {
            // Notice we deleted the visualVector math! 
            // m_currentDirection is already perfectly set by your mouse in updateAim().
            
            // Play the animation and MAINTAIN the frame sync!
            const Animation& runAnim = animServer.getRunningAnimation(m_currentDirection);
            m_animator.playAnimation(&runAnim, true); 
        }
        else 
        {
            // When stopping, we snap back to the standing pose!
            const Animation& runAnim = animServer.getRunningAnimation(m_currentDirection);
            m_animator.playAnimation(&runAnim, false); 
            m_animator.stopAndReset(); 
        }
    }
}

/// <summary>
/// Function used to update player aiming
/// </summary>
/// <param name="t_mousePos"></param>
/// <param name="facingDir"></param>
// Change the signature in UserPlayer.h to: void updateAim(sf::Vector2f t_mouseWorldPos);

void UserPlayer::updateAim(sf::Vector2f t_mouseWorldPos)
{
    // 1. Store the literal world coordinates for gameplay math (passing, shooting)
    m_playerAim = t_mouseWorldPos;

    // 2. Get the vector pointing from the player to the mouse in world space
    sf::Vector2f rawAimVector = m_playerAim - m_position;

    // 3. APPLY 90-DEGREE MATH ROTATION FOR THE ANIMATION
    // Since your camera is rotated 90 degrees, we must rotate the visual 
    // calculation by -90 degrees so "up on screen" maps to "Direction::Up"

    // Formula for rotating a 2D vector by an angle (theta):
    // x' = x * cos(theta) - y * sin(theta)
    // y' = x * sin(theta) + y * cos(theta)

    // For -90 degrees (or -PI/2 radians): cos(-90) = 0, sin(-90) = -1
    // Therefore: x' = -y * (-1) = y
    //            y' = x * (-1) = -x

    sf::Vector2f visualVector;
    visualVector.x = rawAimVector.y;
    visualVector.y = -rawAimVector.x;

    // 4. Ask for the correct sprite direction based on the rotated visual vector
    m_currentDirection = get8WayDirection(visualVector);
}

/// <summary>
/// This function is in charge of shooting
/// </summary>
/// <param name="dt"></param>
void UserPlayer::shooting(float dt, GamePlay& game)
{
}

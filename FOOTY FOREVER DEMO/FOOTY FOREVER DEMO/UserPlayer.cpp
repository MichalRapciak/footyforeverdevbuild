#include "UserPlayer.h"

/// <summary>
/// Setting up player sprite when the player function is created.
/// </summary>
UserPlayer::UserPlayer(const sf::Texture& texture) : Player(texture)
{
    m_position = { 5000.f, 1000.f }; // Default start
    m_sprite.setPosition(m_position);
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
    // 1. Tick the base physics and tackle state machine
    Player::update(dt, animServer);
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

#include "NPCPlayer.h"
#include <cmath>

NPCPlayer::NPCPlayer(const sf::Texture& texture) : Player(texture)
{
    m_position = { 5000.f, 1000.f }; // Default start
    m_sprite.setPosition(m_position);
    // Give them consistent stats
   // m_stats = { 70.0f, 70.0f, 70.0f, 70.0f, 70.0f, 70.0f, 70.0f, 77.0f, 77.0f, 77.0f, 77.0f, 70.0f, 70.f, 70.f, 70.f, 70.f, 70.f, 70.f, 70.0f, 70.0f, 70.0f };	// FINISHING, CURL, BALL CONTROL, BALANCING, SHORT PASSING, LONG PASSING, THROUGH PASSING, TOP SPEED, ACCELERATION, AGILITY, BODY STRENGTH, KICK POWER, AWARENESS, AGGRESSION, BLOCKING, GK COVER, GK REACT, GK CATCH, GK THROW, GK AWARE, GK BLOCK
}
    NPCPlayer::~NPCPlayer()
    {
    }

    void NPCPlayer::update(float dt, AnimationServer& animServer)
    {

        float speed = std::sqrt((m_velocity.x * m_velocity.x) + (m_velocity.y * m_velocity.y));

        // Save the physical direction for the Controller's brakes!
        if (speed > 1.0f) {
            m_physicalHeading = m_velocity / speed;
        }

        // 2. ONLY play running animations if we are standing or running
        if (m_currentState == PlayerState::Normal)
        {
            if (speed > 10.f)
            {
                // Get the direction
                sf::Vector2f visualVector;
                visualVector.x = m_velocity.y;
                visualVector.y = -m_velocity.x;
                m_currentDirection = get8WayDirection(visualVector);

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

    void NPCPlayer::setRotationToward(sf::Vector2f targetPos)
    {
        // 1. Get the physical world vector to the target
        sf::Vector2f direction = targetPos - m_position;

        // 2. APPLY 90-DEGREE MATH ROTATION FOR THE ANIMATION
        sf::Vector2f visualVector;
        visualVector.x = direction.y;
        visualVector.y = -direction.x;

        // 3. Set the direction so the Animator draws the correct frame next tick!
        m_currentDirection = get8WayDirection(visualVector);
    }



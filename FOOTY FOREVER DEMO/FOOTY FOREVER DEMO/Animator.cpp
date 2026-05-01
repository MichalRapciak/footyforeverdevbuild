#include "Animator.h"

Animator::Animator(sf::Sprite& targetSprite)
    : m_sprite(targetSprite),
    m_currentAnimation(nullptr),
    m_currentTime(0.0f),
    m_currentFrameIndex(0)
{
    // 12 FPS = 1.0 second / 12 frames = ~0.0833 seconds per frame
    m_frameTime = 1.0f / 12.0f;
}

void Animator::playAnimation(const Animation* animation, bool maintainFrame, bool loop, int holdFrame) {
    if (m_currentAnimation == animation && !m_isFinished) return;

    m_currentAnimation = animation;
    m_loop = loop;
    m_holdFrame = holdFrame;
    m_isHolding = false;
    m_isFinished = false;

    // ==========================================
    // --- THE FIX 1: RESET ACTION STATE ---
    // ==========================================
    // Whenever a standard locomotion animation is requested, turn off action mode!
    m_isActionPlaying = false;

    if (!maintainFrame) {
        m_currentTime = 0.0f;
        m_currentFrameIndex = 0;
    }
    else if (m_currentAnimation && m_currentFrameIndex >= m_currentAnimation->frames.size()) {
        m_currentFrameIndex = m_currentAnimation->loopStartIndex;
    }

    if (m_currentAnimation && !m_currentAnimation->frames.empty()) {
        m_sprite.setTextureRect(m_currentAnimation->frames[m_currentFrameIndex]);
    }
}

// ==========================================
// --- THE FIX 2: THE ACTION TRIGGER ---
// ==========================================
void Animator::playAction(const Animation* animation, float actionSpeed) {
    // Actions generally shouldn't loop, shouldn't hold a frame, and shouldn't maintain index.
    playAnimation(animation, false, false, -1);

    // Flag this as a strict action so the update loop ignores physical velocity
    m_isActionPlaying = true;
    m_actionSpeed = actionSpeed;
}

void Animator::update(sf::Time dt, float speedMultiplier) {
    if (!m_currentAnimation || m_currentAnimation->frames.empty() || m_isFinished) return;

    // If we are stuck on the sliding frame, do not advance the clock!
    if (m_isHolding) return;

    // ==========================================
    // --- THE FIX 3: SPEED OVERRIDE ---
    // ==========================================
    // If an action is playing, use the fixed internal speed. 
    // Otherwise, use the physical locomotion speed passed by the Player!
    float activeSpeed = m_isActionPlaying ? m_actionSpeed : speedMultiplier;

    m_currentTime += dt.asSeconds() * activeSpeed;

    if (m_currentTime >= m_frameTime) {
        m_currentTime -= m_frameTime;

        m_currentFrameIndex++;

        // Did we hit the designated freeze frame? (Frame 1 of the tackle sequence)
        if (m_currentFrameIndex == m_holdFrame) {
            m_isHolding = true;
        }

        // Did we hit the end of the animation?
        if (m_currentFrameIndex >= m_currentAnimation->frames.size()) {
            if (m_loop) {
                m_currentFrameIndex = m_currentAnimation->loopStartIndex;
            }
            else {
                // Lock it to the final frame and flag it as done
                m_currentFrameIndex = m_currentAnimation->frames.size() - 1;
                m_isFinished = true;

                // --- NEW: Clear action state when finished ---
                m_isActionPlaying = false;
            }
        }

        m_sprite.setTextureRect(m_currentAnimation->frames[m_currentFrameIndex]);
    }
}

void Animator::stopAndReset() {
    m_currentFrameIndex = 0;
    m_isActionPlaying = false; // Reset action state just in case

    if (m_currentAnimation && !m_currentAnimation->frames.empty()) {
        m_sprite.setTextureRect(m_currentAnimation->frames[0]);
    }
}
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

void Animator::update(sf::Time dt, float speedMultiplier) {
    if (!m_currentAnimation || m_currentAnimation->frames.empty() || m_isFinished) return;

    // If we are stuck on the sliding frame, do not advance the clock!
    if (m_isHolding) return;

    m_currentTime += dt.asSeconds() * speedMultiplier;

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
            }
        }

        m_sprite.setTextureRect(m_currentAnimation->frames[m_currentFrameIndex]);
    }
}

void Animator::stopAndReset() {
    m_currentFrameIndex = 0;
    if (m_currentAnimation && !m_currentAnimation->frames.empty()) {
        m_sprite.setTextureRect(m_currentAnimation->frames[0]);
    }
}
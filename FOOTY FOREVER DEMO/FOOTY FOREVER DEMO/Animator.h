#pragma once
#include <SFML/Graphics.hpp>
#include "AnimationServer.h"

class Animator {
public:
    Animator(sf::Sprite& targetSprite);

    // Upgrade the signature to support looping and holding
    void playAnimation(const Animation* animation, bool maintainFrame = false, bool loop = true, int holdFrame = -1);

    void update(sf::Time dt, float speedMultiplier = 1.0f);
    void stopAndReset();

    // --- NEW: Expose the current frame and state ---
    int getCurrentFrameIndex() const { return m_currentFrameIndex; }
    void releaseHold() { m_isHolding = false; m_holdFrame = -1; }
    bool isFinished() const { return m_isFinished; }

private:
    sf::Sprite& m_sprite;
    const Animation* m_currentAnimation;

    float m_currentTime;
    float m_frameTime;
    int m_currentFrameIndex;

    // --- NEW VARIABLES ---
    bool m_loop = true;
    int m_holdFrame = -1;
    bool m_isHolding = false;
    bool m_isFinished = false;
};
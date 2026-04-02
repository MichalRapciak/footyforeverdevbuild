#pragma once
#include <SFML/Graphics.hpp>
#include <map>
#include <vector>
#include <string>
#include "Direction.h"

class AnimationServer {
public:
    AnimationServer();

    // Loads the textures and slices all the rects
    void init(const std::string& texturePath);

    // Retrieves the correct animations
    const Animation& getRunningAnimation(Direction dir) const;
    const Animation& getTackleAnimation(Direction dir, int currentRunFrame) const;

    // Texture getters
    sf::Texture& getPlayerTexture();
    sf::Texture& getTackleTexture();

private:
    sf::Texture m_playerTexture;
    sf::Texture m_tackleTexture;

    std::map<Direction, Animation> m_runningAnimations;
    std::map<Direction, std::vector<Animation>> m_tackleAnimations;

    // Helper to slice a row of frames
    Animation sliceRow(int startY, int numFrames, int frameW, int frameH, bool flipX = false, int loopStart = 0);
};
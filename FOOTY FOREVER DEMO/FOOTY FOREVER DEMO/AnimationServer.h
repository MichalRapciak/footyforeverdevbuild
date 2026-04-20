#pragma once
#include <SFML/Graphics.hpp>
#include <map>
#include <vector>
#include <string>
#include "Direction.h"

class AnimationServer {
public:
    AnimationServer();

    // --- NEW: Global Texture Loader ---
    static void loadMasterTextures();

    // --- NEW: Expose raw textures for the Shader ---
    static sf::Texture& getSkinTexture() { return s_skinTexture; }
    static sf::Texture& getShirtTexture() { return s_shirtTexture; }
    static sf::Texture& getShortsTexture() { return s_shortsTexture; }
    static sf::Texture& getSocksTexture() { return s_socksTexture; }
    static sf::Texture& getTackleTexture() { return s_tackleTexture; }

    static const Animation& getRunningAnimation(Direction dir);
    static const Animation& getTackleAnimation(Direction dir, int currentRunFrame);

private:
    // Shared VRAM Textures
    static sf::Texture s_skinTexture;
    static sf::Texture s_shirtTexture;
    static sf::Texture s_shortsTexture;
    static sf::Texture s_socksTexture;
    static sf::Texture s_tackleTexture;
    static bool s_texturesLoaded;

    static std::map<Direction, Animation> s_runningAnimations;
    static std::map<Direction, std::vector<Animation>> s_tackleAnimations;

    Animation sliceRow(int startY, int numFrames, int frameW, int frameH, bool flipX = false, int loopStart = 0);
    void buildAnimations();
};
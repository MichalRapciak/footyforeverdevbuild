#pragma once
#include <SFML/Graphics.hpp>
#include <map>
#include <vector>
#include <string>
#include "Direction.h"

class AnimationServer {
public:
    // --- STATIC INITIALIZER ---
    static void init();

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

    static sf::Texture* getKitTexture(const std::string& id) {
        if (s_kitTextures.find(id) != s_kitTextures.end()) {
            return &s_kitTextures[id];
        }
        return nullptr; // Returns null if you typed the texture ID wrong in the editor!
    }

private:
    // Shared VRAM Textures
    static sf::Texture s_skinTexture;
    static sf::Texture s_shirtTexture;
    static sf::Texture s_shortsTexture;
    static sf::Texture s_socksTexture;
    static sf::Texture s_tackleTexture;
    static std::map<std::string, sf::Texture> s_kitTextures;
    static bool s_texturesLoaded;

    static std::map<Direction, Animation> s_runningAnimations;
    static std::map<Direction, std::vector<Animation>> s_tackleAnimations;

    // THE FIX: These helper functions must ALSO be static!
    static Animation sliceRow(int startY, int numFrames, int frameW, int frameH, bool flipX = false, int loopStart = 0);
    static void buildAnimations();
};
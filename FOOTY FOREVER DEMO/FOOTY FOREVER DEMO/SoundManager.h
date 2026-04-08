#pragma once
#include <SFML/Audio.hpp>
#include <map>
#include <string>
#include <vector>
#include <iostream>

class SoundManager {
public:
    SoundManager();
    ~SoundManager();

    // Initialization
    void loadSound(const std::string& id, const std::string& filepath);

    // Core Playback
    // pitchVariation of 0.1f means the pitch will randomly shift between 0.9x and 1.1x!
    void playSound(const std::string& id, float volume = 100.f, float pitchVariation = 0.1f);
    void playRandomSound(const std::string& baseId, int count, float volume = 100.f, float pitchVariation = 0.1f);

    // Streaming Audio (Music & Crowd)
    void playMusic(const std::string& filepath, float volume = 50.f);
    void stopMusic();

    void playCrowd(const std::string& filepath, float volume = 50.f);
    void setCrowdVolume(float volume);
    void stopCrowd();

    // Global settings
    void stopAllSounds();

private:
    // Stores the heavy audio data in memory
    std::map<std::string, sf::SoundBuffer> m_buffers;

    // An array of overlapping "voices" so multiple sounds can play at once
    static const int MAX_VOICES = 64;
    std::vector<sf::Sound> m_voices;

    // Dedicated streamers for long tracks
    sf::Music m_music;
    sf::Music m_crowdAmbience;
};
#include "SoundManager.h"
#include <cstdlib> // For rand()

SoundManager::SoundManager() {
    // Pre-allocate 64 voices so we never run out of overlapping channels (e.g. multiple footsteps)
    m_voices.resize(MAX_VOICES);
}

SoundManager::~SoundManager() {
    stopAllSounds();
}

void SoundManager::loadSound(const std::string& id, const std::string& filepath) {
    sf::SoundBuffer buffer;
    if (buffer.loadFromFile(filepath)) {
        m_buffers[id] = buffer;
    }
    else {
        std::cerr << "Audio Error: Could not load " << filepath << "\n";
    }
}

void SoundManager::playSound(const std::string& id, float volume, float pitchVariation) {
    auto it = m_buffers.find(id);
    if (it == m_buffers.end()) return; // Sound not loaded!

    // Find an available voice that isn't currently playing
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (m_voices[i].getStatus() == sf::Sound::Status::Stopped) {

            m_voices[i].setBuffer(it->second);
            m_voices[i].setVolume(volume);

            // Calculate Random Pitch Variation
            if (pitchVariation > 0.0f) {
                // Generates a random float between -1.0 and 1.0, then multiplies by variation
                float randomOffset = ((static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f) * pitchVariation;
                m_voices[i].setPitch(1.0f + randomOffset);
            }
            else {
                m_voices[i].setPitch(1.0f);
            }

            m_voices[i].play();
            return; // Sound played successfully, exit loop
        }
    }
    // If we reach here, all 64 voices are playing at once (very rare!)
}

void SoundManager::playRandomSound(const std::string& baseId, int count, float volume, float pitchVariation) {
    // Expects naming conventions like "kick_1", "kick_2", "kick_3"
    int randomIndex = (rand() % count) + 1;
    std::string finalId = baseId + "_" + std::to_string(randomIndex);
    playSound(finalId, volume, pitchVariation);
}

void SoundManager::playMusic(const std::string& filepath, float volume) {
    if (m_music.openFromFile(filepath)) {
        m_music.setVolume(volume);
        m_music.setLooping(true);
        m_music.play();
    }
}

void SoundManager::stopMusic() {
    m_music.stop();
}

void SoundManager::playCrowd(const std::string& filepath, float volume) {
    if (m_crowdAmbience.openFromFile(filepath)) {
        m_crowdAmbience.setVolume(volume);
        m_crowdAmbience.setLooping(true);
        m_crowdAmbience.play();
    }
}

void SoundManager::setCrowdVolume(float volume) {
    m_crowdAmbience.setVolume(volume);
}

void SoundManager::stopCrowd() {
    m_crowdAmbience.stop();
}

void SoundManager::stopAllSounds() {
    for (auto& voice : m_voices) {
        voice.stop();
    }
    stopMusic();
    stopCrowd();
}
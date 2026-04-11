#include "SoundManager.h"
#include <cstdlib> // For rand()

SoundManager::SoundManager() {
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

    // 1. Look for a voice that has finished playing so we can recycle it
    for (auto& voice : m_voices) {
        if (voice.getStatus() == sf::SoundSource::Status::Stopped) {

            // In SFML 3, we re-assign it with the new buffer directly
            voice = sf::Sound(it->second);
            voice.setVolume(volume);

            if (pitchVariation > 0.0f) {
                float randomOffset = ((static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f) * pitchVariation;
                voice.setPitch(1.0f + randomOffset);
            }
            else {
                voice.setPitch(1.0f);
            }

            voice.play();
            return;
        }
    }

    // 2. If no voices are stopped, and we are under the limit, create a new one!
    if (m_voices.size() < MAX_VOICES) {
        // emplace_back constructs the sf::Sound directly in place using the buffer!
        m_voices.emplace_back(it->second);

        m_voices.back().setVolume(volume);

        if (pitchVariation > 0.0f) {
            float randomOffset = ((static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f) * pitchVariation;
            m_voices.back().setPitch(1.0f + randomOffset);
        }
        else {
            m_voices.back().setPitch(1.0f);
        }

        m_voices.back().play();
    }
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
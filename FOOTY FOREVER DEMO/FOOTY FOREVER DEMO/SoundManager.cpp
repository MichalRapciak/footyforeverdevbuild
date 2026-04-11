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

void SoundManager::loadAllSounds() {
    // --- BALL SOUNDS ---
    loadSound("ball_bounce", "ASSETS/SOUNDS/BALL/ball_bounce.ogg");
    loadSound("kick_1", "ASSETS/SOUNDS/BALL/kick_1.ogg");
    loadSound("kick_2", "ASSETS/SOUNDS/BALL/kick_2.ogg");
    loadSound("kick_3", "ASSETS/SOUNDS/BALL/kick_3.ogg");

    // --- CROWD EVENTS (Streaming is handled separately) ---
    loadSound("crowd_goal", "ASSETS/SOUNDS/CROWD/goal.ogg");
    loadSound("crowd_miss", "ASSETS/SOUNDS/CROWD/shot_miss.ogg");

    // --- GOAL SOUNDS ---
    loadSound("crossbar", "ASSETS/SOUNDS/GOAL/crossbar.ogg");
    loadSound("crossbar_strong", "ASSETS/SOUNDS/GOAL/crossbar_strong.ogg");
    loadSound("net_1", "ASSETS/SOUNDS/GOAL/net_1.ogg");
    loadSound("net_2", "ASSETS/SOUNDS/GOAL/net_2.ogg");
    loadSound("net_3", "ASSETS/SOUNDS/GOAL/net_3.ogg");
    loadSound("post", "ASSETS/SOUNDS/GOAL/post.ogg");
    loadSound("post_strong", "ASSETS/SOUNDS/GOAL/post_strong.ogg");
    loadSound("post_strong2", "ASSETS/SOUNDS/GOAL/post_strong2.ogg");

    // --- REFEREE SOUNDS ---
    loadSound("ref_foul", "ASSETS/SOUNDS/REFEREE/foul.ogg");
    loadSound("ref_fulltime", "ASSETS/SOUNDS/REFEREE/fulltime.ogg");
    loadSound("ref_whistle", "ASSETS/SOUNDS/REFEREE/whistle.ogg");
}

void SoundManager::playSound(const std::string& id, float volume, float pitchVariation) {
    auto it = m_buffers.find(id);

    if (it == m_buffers.end()) return; // Sound not loaded!



    // 1. Look for a voice that has finished playing so we can recycle it safely
    for (auto& voice : m_voices) {
        if (voice.getStatus() == sf::SoundSource::Status::Stopped) {

            // SFML 3 FIX: Overwrite the old sound completely with a new one
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

    // 2. If no voices are stopped, and we are under the limit, allocate a new one!
    if (m_voices.size() < MAX_VOICES) {
        // SFML 3 FIX: Pass the buffer directly into emplace_back to satisfy the constructor!
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
        return;
    }

    // ==========================================
    // 3. ROUND-ROBIN VOICE STEALING (The Savior)
    // ==========================================
    static int stealIndex = 0;

    m_voices[stealIndex].stop();

    // SFML 3 FIX: Overwrite the stolen sound object completely
    m_voices[stealIndex] = sf::Sound(it->second);
    m_voices[stealIndex].setVolume(volume);

    if (pitchVariation > 0.0f) {
        float randomOffset = ((static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f) * pitchVariation;
        m_voices[stealIndex].setPitch(1.0f + randomOffset);
    }
    else {
        m_voices[stealIndex].setPitch(1.0f);
    }

    m_voices[stealIndex].play();

    // Move to the next channel for the next time we need to steal a voice.
    stealIndex = (stealIndex + 1) % MAX_VOICES;
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
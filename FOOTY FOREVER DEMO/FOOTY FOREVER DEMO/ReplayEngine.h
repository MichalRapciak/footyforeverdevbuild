#pragma once
#include <SFML/Graphics.hpp>
#include <deque>
#include <vector>
#include <optional>
#include <cmath>
#include <algorithm>
#include "Player.h"
#include "Ball.h"

struct BodySnapshot {
    std::optional<sf::Sprite> sprite;
    float sortDepth;
};

struct ReplayFrame {
    std::optional<sf::Vector2f> ballPos;
    
    std::optional<sf::CircleShape> ballShadow; 
    std::vector<sf::CircleShape> playerCoreShadows;
    std::vector<sf::VertexArray> playerFloodlights;

    // --- NEW: Unsorted bodies with depth data ---
    std::vector<BodySnapshot> bodies;

    ReplayFrame() = default; 
};

class ReplayEngine {
public:
    ReplayEngine() = default;

    void lockRecording(int postRollFrames = 60) {
        if (!m_isRecordingLocked) {
            m_isRecordingLocked = true;
            m_postRollFrames = postRollFrames;
        }
    }

    void unlockRecording() {
        m_isRecordingLocked = false;
        m_postRollFrames = 0;
    }

    void recordFrame(Ball* ball, const std::vector<Player*>& allPlayers);
    void startReplay(float playbackSpeed = 0.5f);
    void update(float dt);
    void render(sf::RenderWindow& window);
    void replayCam(sf::RenderWindow& window);

    bool isReplaying() const { return m_isReplaying; }

private:
    // --- NEW: DVR FREEZE VARIABLES ---
    bool m_isRecordingLocked = false;
    int m_postRollFrames = 0;
    std::deque<ReplayFrame> m_buffer;
    const size_t MAX_FRAMES = 300;

    float m_blendFactor = 0.0f;
    bool m_isReplaying = false;
    float m_playbackSpeed = 1.0f;
    float m_frameTimer = 0.0f;
    size_t m_playbackIndex = 0;
    sf::Vector2f m_replayCamPos;
};
#pragma once
#include <SFML/Graphics.hpp>
#include <deque>
#include <vector>
#include <optional>
#include <cmath>
#include <algorithm>
#include "Player.h"
#include "Ball.h"

struct KitLayer;

struct BodySnapshot {
    std::optional<sf::Sprite> sprite;
    float sortDepth;

    // --- NEW: Shader data for Replays ---
    bool isPlayer = false;
    sf::Color skinColor;

    // THE FIX: Replace hardcoded colors with the dynamic stack!
    std::vector<KitLayer> kitLayers;
};

struct ReplayFrame {
    std::optional<sf::Vector2f> ballPos;

    std::optional<sf::CircleShape> ballShadow;
    std::vector<sf::CircleShape> playerCoreShadows;
    std::vector<sf::VertexArray> playerFloodlights;

    std::vector<BodySnapshot> bodies;
    bool ballHasOwner = false;
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

    void render(sf::RenderWindow& window, sf::Shader* kitShader);

    void startOffsideReplay(float playbackSpeed, float offsideLineX, sf::Vector2f attackerPos);
    void replayCam(sf::RenderWindow& window);

    bool isReplaying() const { return m_isReplaying; }

private:
    bool m_isRecordingLocked = false;
    int m_postRollFrames = 0;
    std::deque<ReplayFrame> m_buffer;
    const size_t MAX_FRAMES = 600;

    // --- NEW: Delayed Freeze Logic ---
    int m_varFreezeFrameIndex = -1;
    bool m_showVarLines = false;
    sf::Vector2f m_frozenPasserPos;
    float m_currentZoom = 0.8f;

    float m_blendFactor = 0.0f;
    bool m_isReplaying = false;
    float m_playbackSpeed = 1.0f;
    float m_frameTimer = 0.0f;
    size_t m_playbackIndex = 0;
    sf::Vector2f m_replayCamPos;

    bool m_isOffsideReplay = false;
    float m_freezeTimer = 0.0f;
    float m_defenderLineX = 0.0f;     // The Red Line
    sf::Vector2f m_receiverPos;       // The Blue Line
};